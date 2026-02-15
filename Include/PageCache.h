//
// Created by CAO on 2026/2/15.
//

#pragma once
#include "Common.h"

class PageCache {
public:
    std::mutex _pageMtx;

    // 单例模式
    static PageCache* getInstance() {
        static PageCache _sInst;
        return &_sInst;
    }

    /* PC给CC分配一个非空Span
     * 情况1：对应槽位有非空Span
     * 情况2：对应槽位无但后续槽位有，弹出后续槽位的span并分裂
     * 返回对应的span，将另一个span挂载到正确槽位
     * 情况3：所有槽位均无，内存申请一个最大槽位的Span，后续同2
     */
    Span* NewSpan(size_t k);



private:
    PageCache() = default;
    SpanList _spanLists[PAGE_NUM];


};