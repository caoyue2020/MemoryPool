#pragma once
#include<assert.h>
#include<iostream>
#include<vector>
#include<thread>
#include<mutex>


using std::cout;
using std::endl;
using std::vector;

constexpr size_t FREE_LIST_NUM = 208; // 哈希表中自由链表个数/桶数
constexpr size_t MAX_BYTES = 256*1024; // ThreadCache单次分配给线程最大字节数
constexpr size_t PAGE_NUM = 129; // PageCash中最大的span控制的页数（这里为129是为了下标和桶能直接映射）
constexpr size_t PAGE_SHIFT = 13; // 一页的位数，这里一页设为8K，13位

//获取obj指向的内存块中存储的指针
inline void*& ObjNext(void* obj)
{
    // 这里返回值要加引用，不仅能读取还能修改
    return *(void**)obj;
}

class FreeList // ThreadCache中的自由链表
{
private:
    void* _freeList = nullptr; //这里用void*类型了，之前定长的时候使用char*
    
    // 每个线程的TC的【每个槽位】都有一个这个，这个东西限制了当前线程
    // 申请特定大小最大内存块数
    // 随着慢启动，TC某个大小的块需求越多，这个也会逐渐增加
    // 但显然不可能允许它一直往上加，因此有@SizeClass::NumMoveSize去计算上限
    size_t _maxSize = 1;

public:
    void PushRange(void* start, void* end)
    {
        // TC单次分配只分配桶中的一个块（Pop）
        // 因此触发向CC申请内存时，一定是当前桶没块了
        // 即_freeList = nullptr?
        assert(_freeList == nullptr);
        
        ObjNext(end) = _freeList;
        _freeList = start;
    }
 
    void Push(void* obj) //回收空间
    {
        //头插
        //在obj指向的内存中的“指针”写入_freeList
        //将obj赋值给_freeList
        ObjNext(obj) = _freeList;
        _freeList = obj;
    }

    void* Pop() //分配空间，返回内存指针
    {
        //头删
        //将_freeList赋值为它指向的内存块中的指针
        void* ptr = _freeList;
        _freeList = ObjNext(ptr);
        return ptr;
    }

    bool Empty() //判断哈希桶是否为空
    {
        return _freeList == nullptr;
    }

    size_t& MaxSize() // 注意这里给引用，每次申请之后会让_maxSize++
    {
        return _maxSize;
    }
};

//笔记见MD
class SizeClass
{
public:

	// 计算每个分区对应的对齐后的字节数(大佬写法)
	static size_t _RoundUp(size_t size, size_t alignNum)
	{									// alignNum是size对应分区的对齐数
		return ((size + alignNum - 1) & ~(alignNum - 1));
	}

	static size_t RoundUp(size_t size) // 计算对齐后的字节数，size为线程申请的空间大小
	{
		if (size <= 128)
		{ // [1,128] 8B
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)
		{ // [128+1,1024] 16B
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)
		{ // [1024+1,8*1024] 128B
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{ // [8*1024+1,64*1024] 1024B
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024)
		{ // [64*1024+1,256*1024] 8 * 1024B
			return _RoundUp(size, 8 * 1024);
		}
		else
		{ 
            return -1;
		}
	}

	// 计算映射的哪一个自由链表桶（tc和cc用，二者映射规则一样）
	// 求size对应在哈希表中的下标
    static inline size_t _Index(size_t size, size_t align_shift)
    {							/*这里align_shift是指对齐数的二进制位数。比如size为2的时候对齐数
                                为8，8就是2^3，所以此时align_shift就是3*/
        return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
        //这里_Index计算的是当前size所在区域的第几个下标，所以Index的返回值需要加上前面所有区域的哈希桶的个数
    }

    // 计算映射的哪一个自由链表桶
    static inline size_t Index(size_t size)
    {
        assert(size <= MAX_BYTES);

        // 每个区间有多少个链
        static int group_array[4] = { 16, 56, 56, 56 };
        if (size <= 128)
        { // [1,128] 8B -->8B就是2^3B，对应二进制位为3位
            return _Index(size, 3); // 3是指对齐数的二进制位位数，这里8B就是2^3B，所以就是3
        }
        else if (size <= 1024)
        { // [128+1,1024] 16B -->4位
            return _Index(size - 128, 4) + group_array[0];
        }
        else if (size <= 8 * 1024)
        { // [1024+1,8*1024] 128B -->7位
            return _Index(size - 1024, 7) + group_array[1] + group_array[0];
        }
        else if (size <= 64 * 1024)
        { // [8*1024+1,64*1024] 1024B -->10位
            return _Index(size - 8 * 1024, 10) + group_array[2] + group_array[1]
                + group_array[0];
        }
        else if (size <= 256 * 1024)
        { // [64*1024+1,256*1024] 8 * 1024B  -->13位
            return _Index(size - 64 * 1024, 13) + group_array[3] +
                group_array[2] + group_array[1] + group_array[0];
        }
        else
        {
            assert(false);
            return -1;
        }
        
    }

    // 人为控制单次分配数量上限
    static size_t NumMoveSize(size_t size)
    {
        assert(size > 0);
        int num = MAX_BYTES / size; //MAX_BYTES为TC单次申请上限
        
        /*TODO:如何理解？
        首先，MAX_BYTES=256KB。若TC申请8B，256KB/8B= 2^15,
        但显然这个数量太多了，若允许，TC会得到很多8b小块，
        线程大概率用不完这些块就会产生浪费，因此限制其小于512；
        
        若TC单次申请256KB，则num=1，但若控制CC单次给TC分配上限为1，
        若TC需要给线程分配4*256KB，就会频繁调用，因此这里放宽上限，
        但也不能放宽太多，256KB是非常大的空间了，设为2差不多了

        总结：[2,512]，是人为对分配上限的兜底
        小对象一次批量上限高；大对象一次批量上限低
        */
        if (num > 512) num = 512;
        if (num < 2) num = 2;
        
        return num;
    }

    // 块页匹配
    static size_t NumMovePage(size_t size) {
	    size_t n = NumMoveSize(size); // 计算该块在CC中的单次分配上限
	    size_t npage = n * size; // 计算单次分配上限的总字节数

	    // 这里右移实际上就是除以页大小，即单次分配上限为多少页
	    npage >>= PAGE_SHIFT;

	    // 这里主要考虑到单次分配总字节数小于一页的情况
	    // 此时计算出来的npage为0，强制分配一页
	    if (npage == 0) npage == 1;

	    return npage;
	}
};

struct Span
{
    /*TODO:这里可能有个小问题
    64位下，假设一页为8kb，需共有2^64/2^13=2^51这么多的页数
    */
    size_t _pageId; //页号
    size_t _n = 0; //页数量

    Span* _next = nullptr; // 指向下一个span
    Span* _prev = nullptr; // 指向上一个span

    void* _freeList = nullptr; //span下挂载的内存块链表指针
    size_t _usecount = 0; //内存块使用计数， ==0 说明所有块都还回来了
};

class SpanList //Span为基础元素的双向链表
{
public:
    // PC用于确认当前槽位SpanList是否为空
    // 有必要吗？
    bool Empty() {
        return _head->_next == _head;
    }

    // PC弹出首个非空Span
    // 该Span只能保证非空
    Span* PopFront() {
        Span* front = Begin();
        Erase(front);
        return front;
    }

    // CC遍历槽位获取非空span时需要有链表Begin
    Span* Begin() {
        // 由于_head是一个哨兵节点，不存储实际数据，因此_head->next才是真正的begin
        return _head->_next;
    }
    // CC遍历槽位获取非空span时需要有链表End
    Span* End() {
        // 通常认为end指针是指向最后节点的下一个位置，即end == nullptr
        return _head;
    }

    void Insert(Span* pos, Span* ptr)
    {
        //在Pos前面插入ptr
        //TODO:为什么不是后面？
        assert(pos); assert(ptr);

        //获取pos前一个节点
        Span* prev = pos->_prev;
        // 处理前prev与ptr
        prev->_next = ptr;
        ptr->_prev = prev;
        // 处理pos与ptr
        pos->_prev = ptr;
        ptr->_next = pos;
    }

    void Erase(Span* pos)
    {
        //获取前后
        Span* prev = pos->_prev;
        Span* next = pos->_next;
       
        prev->_next = next;
        next->_prev = prev;

        //TODO:这里并不删除pos节点，而是等待后续回收
        // 回收相关逻辑

    }

    SpanList()
    {
        //构造哨兵位头节点
        _head = new Span;
        //哨兵节点指向自己？
        _head->_next = _head;
        _head->_prev = _head;
    }

public:
    std::mutex mtx;

private:
    Span* _head;

};