#pragma once
#include "Common.h"

class ThreadCache
{
private:
    FreeList _freeLists[FREE_LIST_NUM]; //哈希桶，每个桶都是一个自由链表

public:
    void* Allocate(size_t size); //分配内存
    void Deallocate(void* obj, size_t size);//回收内存

    //ThreadCache空间不够用时，向CentralCache申请空间的接口
    void* FetchFromCentralCache(size_t index, size_t alignSize);
};

static thread_local ThreadCache* pTLSThreadCache = nullptr;