#pragma once
#include"ThreadCache.h"

/**
 * 线程向TC申请内存
 * @param size 线程向TC申请的字节数
 * @return 返回指向内存的指针
 */
inline void* ConcurrentAlloc(size_t size)
{
    return ThreadCache::getInstance()->Allocate(size);
}

/**
 * 线程释放空间给TC
 * @param ptr 释放的空间的指针
 * @param size 释放的空间的大小
 * @return
 */
inline void ConcurrentFree(void* ptr, size_t size)
{
    assert(ptr); //传入指针不得为空
    ThreadCache::getInstance()->Deallocate(ptr, size);
}
