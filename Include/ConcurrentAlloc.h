#pragma once
#include"ThreadCache.h"

/**
 * 线程向TC申请内存
 * @param size 线程向TC申请的字节数
 * @return 返回指向内存的指针
 */
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

/**
 * 线程释放空间给TC
 * @param ptr 释放的空间的指针
 * @param size 释放的空间的大小
 * @return
 */
void* ConcurrentFree(void* ptr, size_t size)
{
    assert(ptr); //传入指针不得为空
    pTLSThreadCache->Deallocate(ptr, size);
}
