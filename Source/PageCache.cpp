//
// Created by CAO on 2026/2/15.
//
#include "PageCache.h"


Span *PageCache::NewSpan(size_t k) {
    assert(k>0 && k<PAGE_NUM);

    // ①K号桶有非空Span
    if (!_spanLists[k].Empty()) {

        Span* span = _spanLists[k].PopFront();
        // 分配出去的这些页都是
        for (size_t i=0; i<span->_n; i++) {
            _idSpanMap[span->_pageId + i] = span;
        }

        return _spanLists[k].PopFront();
    }

    // ②K号后面的同有非空Span，记为n
    for (size_t i = k+1; i<PAGE_NUM; i++) {
        if (!_spanLists[i].Empty()) {
            Span* nSpan = _spanLists[i].PopFront();
            // 分裂产生一个新的Span，该Span节点的空间需要新建立而不是用管理的空间
            // 考虑用智能指针？
            Span* kSpan = new Span;
            kSpan->_pageId = nSpan->_pageId;
            kSpan->_n = k;
            nSpan->_pageId += k; // nSpan的pageId后移K
            nSpan->_n -= k;
            // 在n-k桶插入，返回另一个
            _spanLists[i-k].PushFront(nSpan);
            return kSpan;
        }
    }

    // ③K号及后面都没有，向系统申请最大页数（128）
    // 向系统申请内存
    void* ptr = SystemAlloc(PAGE_NUM-1);
    Span* bigSpan = new Span;
    bigSpan->_pageId = (size_t)ptr >> PAGE_SHIFT; // 假设 PAGE_SHIFT 为 13
    bigSpan->_n = PAGE_NUM-1;
    // 将这个大 Span 挂到最大的桶里
    _spanLists[PAGE_NUM-1].PushFront(bigSpan);
    // 递归调用NewSpan
    return NewSpan(k);
}
