//针对特定对象的定长内存池
//2026.2.11

#pragma once
#include "Common.h"
#include <vector>

// 定义单次向系统申请的页数 (16页 * 8KB = 128KB)
template<typename T, size_t ALLOC_PAGES = 16>
class ObjectPool
{
private:
    char *_memory = nullptr; //指向当前可用的内存池的指针
    std::vector<char *> _M; //存放所有申请的内存池
    size_t _remanenetBytes = 0; //内存池剩余量
    void *_freelist = nullptr; //自由链表的头指针

    // 对齐一下，同时防止sizeof(T) < sizeof(void*)，以免切分的内存块无法放入指针
    size_t objSize = SizeClass::RoundUp(sizeof(T));


public:
    //申请一个T类型大小的空间
    T *New()
    {
        T *obj = nullptr; //返回的指针

        if (_freelist)
        {
            //从自由链表获取内存，见MD文档
            void *next = *(void **) _freelist;
            obj = (T *) _freelist;
            _freelist = next;
        } else
        {
            if (_remanenetBytes < objSize) //判定空间是否足够
            {
                // 彻底替换 malloc 为底层的 SystemAlloc
                // 16页 * 8192字节 = 131072字节 (128KB)
                _remanenetBytes = ALLOC_PAGES << PAGE_SHIFT;
                _memory = (char *) SystemAlloc(ALLOC_PAGES);
                _M.push_back(_memory);

                // 注意：由于 SystemAlloc 内部如果申请失败已经执行了 throw std::bad_alloc();
                // 所以这里不再需要像之前 malloc 那样手动判断 _memory == nullptr
            }

            //如何分配内存，见MD
            obj = (T *) _memory;
            _memory += objSize; //内存池指针移动
            _remanenetBytes -= objSize; //减少对应可用内存字节
        }

        //除了分配空间，还得初始化
        //定位new。详情见MD
        new(obj)T;

        return obj; //返回指向内存的指针
    }

    void Delete(T *obj)
    {
        //显示调用析构函数进行清理
        obj->~T();

        //自由链表回收内存，见MD
        *(void **) obj = _freelist;
        _freelist = obj;
    }

    ~ObjectPool()
    {
        // 彻底替换 free 为底层的 SystemFree
        // 注意 SystemFree 需要传入分配时的页数
        for (char *ptr: _M)
        {
            SystemFree(ptr, ALLOC_PAGES);
        }
    }
};
