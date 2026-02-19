#pragma once
#include"ThreadCache.h"
#include "PageCache.h"
/**
 * 线程向TC申请内存
 * @param size 线程向TC申请的字节数
 * @return 返回指向内存的指针
 */
inline void* ConcurrentAlloc(size_t size)
{
    // 单次申请大于256KB时，直接向PC申请
    if (size > MAX_BYTES){
        size_t alignSize = SizeClass::RoundUp(size); // 按照页对齐
        size_t k = alignSize >> PAGE_SHIFT; // 计算需要多少页
        void* ptr = nullptr;
        {
            std::unique_lock<std::mutex> pageLg(PageCache::getInstance()->_pageMtx);
            Span* span = PageCache::getInstance()->NewSpan(k);
            ptr = (void*)(span->_pageId << PAGE_SHIFT); // 通过span计算首内存地址
        }
        return ptr;
    }
    else {
        return ThreadCache::getInstance()->Allocate(size);
    }
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
    if (size > MAX_BYTES) {
        // todo:需要加锁
        // newSpan申请空间的时候保证空间首尾地址映射过了
        Span* span = PageCache::getInstance()->MapObjetcToSpan(ptr);
        {// 加page锁
            std::unique_lock<std::mutex> pageLg(PageCache::getInstance()->_pageMtx);
            // 超出256KB小于128页的span依然可以用这个函数释放
            PageCache::getInstance()->ReleaseSpanToPageCache(span);
        }
    }
    else {
        ThreadCache::getInstance()->Deallocate(ptr, size);
    }

}
