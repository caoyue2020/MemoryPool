#pragma once
#include"ThreadCache.h"

//线程调用这个函数申请内存
void* ConcurrentAlloc(size_t size)
{
    if (pTLSThreadCache == nullptr) 
    {
        /*
        pTLSThreadCache是TLS，每个线程相互独立，
        因此不存在竞争pTLSThreadCache的问题，
        著需要判断当前线程是否初始化过
        */
        pTLSThreadCache = new ThreadCache();
    }
    return pTLSThreadCache->Allocate(size);
}

//线程调用这个函数回收内存
void* ConcurrentFree(void* ptr, size_t size)
{
    assert(ptr); //传入指针不得为空
    pTLSThreadCache->Deallocate(ptr, size);
}
