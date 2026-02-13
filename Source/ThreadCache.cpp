#include "../Include/ThreadCache.h"

//分配线程内存
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

//回收线程内存
void ThreadCache::Deallocate(void* obj, size_t size)
{
   
    assert(obj); //回收的空间不能为空
    assert(size <= MAX_BYTES); //回收大小不能超过最大值
    
    size_t index = SizeClass::Index(size); //找到对用桶的index
    _freeLists[index].Push(obj);

}

void* FetchFromCentralCache(size_t index, size_t alignSize)
{
    //TODO:
    return nullptr;
}