#include "ThreadCache.h"
#include "CentralCache.h"


/**
 * @param 线程需求的字节数
 * @return void* 内存块指针
 */
void* ThreadCache::Allocate(size_t size)
{
    assert(size <= MAX_BYTES);

    size_t alignSize = SizeClass::RoundUp(size);
    size_t index = SizeClass::Index(size);

    //_freeLists[index]:指定哈希桶
    if(!_freeLists[index].Empty())
    {
        return _freeLists[index].Pop();
    }
    else
    {
        //自由链表为空。像CentralCache申请空间
        return FetchFromCentralCache(index, alignSize);
    }

}

//回收线程内存。当线程释放内存时，会进一步检查TC是否要向CC释放内存
void ThreadCache::Deallocate(void* obj, size_t size)
{
   
    assert(obj); //回收的空间不能为空
    assert(size <= MAX_BYTES); //回收大小不能超过最大值
    
    size_t index = SizeClass::Index(size); //找到对用桶的index
    _freeLists[index].Push(obj);

    // 当TC桶内的块数量大于桶的MaxSize，则释放MaxSize个内存块给CC
    if (_freeLists[index].Size() >= _freeLists[index].MaxSize()) {
        ListTooLong(_freeLists[index], size);
    }
}


void ThreadCache::ListTooLong(FreeList &list, size_t size) {
    void* start = nullptr;
    void* end = nullptr;
    // 弹出数量为MaxSize
    list.PopRange(start, end, list.MaxSize());
    // 归还空间
    CentralCache::getInstance()->ReleaseListToSpans(start, size);

}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t alignSize)
{
    // 通过对应桶的MaxSize和人为设置的上限，双重约束
    size_t batchNum = std::min(_freeLists[index].MaxSize(),
                                SizeClass::NumMoveSize(alignSize));

    // “慢增长”：没有达到上限，MaxSize++
    if(batchNum == _freeLists[index].MaxSize()) 
    {
        _freeLists[index].MaxSize()++; 
    }

    void* start = nullptr;
    void* end = nullptr;

    // 注意这里要先调用GetInstance获取CC指针
    size_t actualNum = CentralCache::getInstance() ->
                        FetchRangeObj(start, end, batchNum, alignSize);

    assert(actualNum >= 1);

    if (actualNum == 1)
    {
        // 如果等于1，那么这个块得直接分配个线程
        return start;
    }
    else
    {
        // 第一个返回给线程，剩余存入TC的FreeList
        _freeLists[index].PushRange(ObjNext(start), end, actualNum-1);
        return start;
    }
}