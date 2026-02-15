//
// Created by CAO on 2026/2/15.
//

#include "CentralCache.h"

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size) {
    // 为什么这里不直接传入index？
    size_t index = SizeClass::Index(size);

    // 获取一个非空的span指针，从该span的frreList上取下连续内存块，整个过程加锁
    {
        std::lock_guard<std::mutex> lg(_spanLists[index].mtx);
        Span* span = getOneSpan(_spanLists[index], size);
        assert(span);
        assert(span->_freeList);

        // 选中内存块链表
        // 1. end 向后挪动batchNum-1个位置
        // 2. 如果span可用的块小于batchNum，需提前结束，即Next(end)!=nullptr
        // 3. 需要对end后移的次数计数以返回实际分配的块数量

        start = end = span-> _freeList;
        size_t actualNum = 1;
        while (actualNum < batchNum && ObjNext(end) != nullptr) {
            end = ObjNext(end);
            actualNum++;
        }


        // 将选中的内存块链表从span的freeLsit上断开
        // 1. span的freeList指向next(end)
        // 2. 将next(end)设为nullptr使其与span断开连接即可
        span->_freeList = ObjNext(end);
        ObjNext(end) = nullptr;

        return actualNum;
    }
}
