//
// Created by CAO on 2026/2/15.
//

#include "CentralCache.h"
#include "PageCache.h"

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size) {
    // 为什么这里不直接传入index？
    size_t index = SizeClass::Index(size);

    // 获取一个非空的span指针，从该span的frreList上取下连续内存块，整个过程加锁
    {
        std::unique_lock<std::mutex> lg(_spanLists[index].mtx);
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
        // 2. 实际分配了多少块，span的usecount增加多少
        // 3. 将next(end)设为nullptr使其与span断开连接即可
        span->_freeList = ObjNext(end);
        span->_usecount += actualNum;
        ObjNext(end) = nullptr;

        return actualNum;
    }
}

Span* CentralCache::getOneSpan(SpanList &list, size_t size) {
    // 1. 遍历自身
    Span* it = list.Begin();
    while (it != list.End()) {
        if (it->_freeList != nullptr)
            return it; // 找到了管理空间不为空的span
        it = it->_next;
    }
    // 解桶锁，因为后续暂时不需要操作桶
    list.mtx.unlock();

    // 2. 代码执行到这里说明当前span链表中没找到，则向PC申请
    // 但是CC和TC之间的内存管理是以块为单位（size为块的字节数）
    // 而PC和CC之间是以Page为单位
    // 2.1 size和page的转换，按该size在CC的单次分配上限去算
    size_t k = SizeClass::NumMovePage(size);

    // 需要注意，由于NewSpan内部存在递归
    // 函数没出栈lock_guard不解锁，导致递归的函数不能进栈，会发生死锁
    // 因此要在NewSpan外部去加锁
    Span* span = nullptr;
    {
        std::lock_guard<std::mutex> lg(PageCache::getInstance()->_pageMtx);
        span = PageCache::getInstance()->NewSpan(k);
        // 注意，这个操作必须写在PC锁内，否则可能线程A刚申请了span。
        // 另一半因为线程B释放span触发了PC的span合并，导致该span
        // 既被分配给A又被PC管理
        // TODO：释放span的代码没有改回false
        span->_isUse = true;
    }

    // 2.2 按size划分连续内存空间
    char* start = (char*)(span->_pageId << PAGE_SHIFT); // start用char*，方便后续的+=操作
    char* end =  (char*)(start + (span->_n << PAGE_SHIFT));

    span->_freeList = start; // 移动到freeList
    void* tail = start; // 记录已经划分的区域末端

    start += size;
    while (start < end) {
        ObjNext(tail) = start; // 让tail指向start
        tail = start;
        start += size;
    }
    ObjNext(tail) = nullptr; // tail最后需指向null

    // 3. 将这个span插入到当前的spanlist中
    // 要对桶操作了，把桶锁加回来
    list.mtx.lock();
    list.PushFront(span);

    // 4. 注意最后要返回这个span。
    // 因为要从这个span中截取一定的内存块给TC
    return span;
}


void CentralCache::ReleaseListToSpans(void *start, size_t size) {
    // 先根据size找到对应的桶
    // 不过这里找到对应桶是为了对桶上锁，而不是利用index寻找span
    size_t index = SizeClass::Index(size);
    {// 对CC的桶进行操作，需要加锁
        std::unique_lock<std::mutex> CClg(_spanLists[index].mtx);

        // 循环操作弹出链表的每一个block
        while (start) {
            void* next = ObjNext(start);
            // 1. 找到对应的span
            Span* span = PageCache::getInstance()->MapObjetcToSpan(start);
            // 2. 有个比较抽象的地方是TC中使用的是封装过的FreeList类
            // 而CC中span使用的是非常原始的void* _freeList
            // 所以这里对链表的插入操作还得从重写
            ObjNext(start) = span->_freeList;
            span->_freeList = start;
            // 3.还回一个块，useCount减1
            span->_usecount--;
            if (span->_usecount == 0) {//等于0说明span管理的页空间的所有块均被释放
                // 将span从spanList中断开
                _spanLists[index].Erase(span);
                // 清楚span的指针，PC回收span只在意span的pageID和pageNum
                span->_freeList = nullptr;
                span->_next = nullptr;
                span->_prev = nullptr;
                CClg.unlock(); //PC处理归还的span时解除桶锁
                //TODO:循环内重复上锁解锁以及PC的span合并？会不会不太好
                {// 归还span，需对PC上锁
                    std::unique_lock<std::mutex> PClg(PageCache::getInstance()->_pageMtx);
                    PageCache::getInstance()->ReleaseSpanToPageCache(span);
                }
                CClg.lock(); // PC处理完加回桶锁
            }
            // 4.下一个block
            start = next;
        }
    }

}
