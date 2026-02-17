//
// Created by CAO on 2026/2/15.
//
#include "PageCache.h"


Span *PageCache::NewSpan(size_t k) {
    /*
     * 情况1：对应槽位有非空Span，弹出首个Span给CC
     * 情况2：对应槽位无但后续槽位有，弹出后续槽位的span并分裂
     * 返回对应的span，将另一个span挂载到正确槽位
     * 情况3：所有槽位均无，内存申请一个最大槽位的Span，后续同2
     */
    assert(k>0 && k<PAGE_NUM);

    // ①K号桶有非空Span
    if (!_spanLists[k].Empty()) {
        Span* span = _spanLists[k].PopFront();

        // 记录【分配出去】的span地址与页号的映射关系
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
            Span* kSpan = new Span;
            kSpan->_pageId = nSpan->_pageId;
            kSpan->_n = k;
            nSpan->_pageId += k; // nSpan的pageId后移K
            nSpan->_n -= k;
            // 在n-k桶插入，返回另一个
            // TODO:这里的i-k有没有可能为0？
            _spanLists[i-k].PushFront(nSpan);
            // 记录nSpan的地址与页号的映射关系
            // 在PC中，只需要记录第一页和最后一页的映射即可
            _idSpanMap[nSpan->_pageId] = nSpan;
            _idSpanMap[nSpan->_pageId+ nSpan->_n -1] = nSpan;

            // 记录Kspan的地址与页号的映射关系
            for (size_t j=0; j<kSpan->_n; j++) {
                _idSpanMap[kSpan->_pageId + j] = kSpan;
            }

            return kSpan;
        }
    }

    // ③K号及后面都没有，向系统申请最大页数（128）
    // 向系统申请内存
    void* ptr = SystemAlloc(PAGE_NUM-1);
    // 需要注意的是，span其实只是记录了页空间的信息，
    // 而不是像自由链表的指针一样占用了块空间
    // 这一点从span需要new就能看出。
    Span* bigSpan = new Span;
    bigSpan->_pageId = (size_t)ptr >> PAGE_SHIFT; // 假设 PAGE_SHIFT 为 13
    bigSpan->_n = PAGE_NUM-1;
    // 将这个大 Span 挂到最大的桶里
    _spanLists[PAGE_NUM-1].PushFront(bigSpan);
    // 递归调用NewSpan
    return NewSpan(k);
}


Span *PageCache::MapObjetcToSpan(void *obj) {
    size_t id = (size_t)obj >> PAGE_SHIFT;
    auto it = _idSpanMap.find(id); // 返回一个迭代器

    if (it != _idSpanMap.end()) {
        return it->second;
    }
    else {
        assert(false);
        return nullptr;
    }
}


void PageCache::ReleaseSpanToPageCache(Span *span) {
    // 1.没找到相邻span停止合并，说明这页空间还没申请
    // 2.Span被CC使用停止合并
    // 3.合并后的数值超过128停止合并，超出了PC维护的大小
    // 4.左右两个方向

    // 向左合并
    while (1) {
        size_t leftID = span->_pageId-1; // 左邻页
        auto it = _idSpanMap.find(leftID);
        if (it == _idSpanMap.end()) break; // 没找到
        Span* leftSpan = it->second; // 左邻span
        if (leftSpan->_isUse) break; // CC在用
        if (leftSpan->_n + span->_n > PAGE_NUM-1) break; //大于128

        // 将leftSpan管理的页交给Span
        span->_pageId = leftSpan->_pageId;
        span->_n += leftSpan->_n;
        // 删除leftSpan
        _spanLists[leftSpan->_n].Erase(leftSpan);// 在对应桶中删除
        delete leftSpan; // 删除对象
    }

    // 向右合并
    while (1) {
        size_t rightID = span->_pageId + span->_n; // 右邻页
        auto it = _idSpanMap.find(rightID);
        if (it == _idSpanMap.end()) break;
        Span* rightSpan = it->second;
        if (rightSpan->_isUse == true) break;
        if (rightSpan->_n + span->_n > PAGE_NUM-1) break; //大于128

        // 将leftSpan管理的页交给Span
        span->_n += rightSpan->_n;
        // 删除leftSpan
        _spanLists[rightSpan->_n].Erase(rightSpan);// 在对应桶中删除
        delete rightSpan; // 删除对象
    }
}
