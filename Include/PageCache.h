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

    /* PC给CC分配【一个】非空Span
     * 情况1：对应槽位有非空Span
     * 情况2：对应槽位无但后续槽位有，弹出后续槽位的span并分裂
     * 返回对应的span，将另一个span挂载到正确槽位
     * 情况3：所有槽位均无，内存申请一个最大槽位的Span，后续同2
     */
    Span* NewSpan(size_t k);


    //DeBug:每个桶中span的数量
    void PrintDebugInfo() {
        std::cout << "=========== PageCache Info ===========" << std::endl;
        // PageCache 的锁是把大锁 (_pageMtx)，通常在外部控制，或者这里为了调试只读一下
        // 注意：PageCache 的 spanLists 数组本身没有独立的锁，而是由 _pageMtx 保护

        // 这种调试打印在多线程高并发下通常需要小心，简单起见我们假设调试时没竞争
        for (size_t i = 0; i < PAGE_NUM; ++i) {
            if (!_spanLists[i].Empty()) {
                std::cout << "Bucket " << i << " (Pages): "
                          << _spanLists[i].Size() << " spans" << std::endl;
            }
        }
        std::cout << "======================================" << std::endl;
    }

private:
    PageCache() = default;
    SpanList _spanLists[PAGE_NUM];

    // PageID和
    std::unordered_map<size_t, Span*> _idSpanMap;
};