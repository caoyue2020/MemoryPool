//
// Created by CAO on 2026/2/15.
//

#pragma once
#include "Common.h"
#include <unordered_map>
#include "ObjectPool.h"

class PageCache {
public:
    std::mutex _pageMtx;

    // 单例模式
    static PageCache* getInstance() {
        static PageCache _sInst;
        return &_sInst;
    }



    /**
     * 从PC的第K个桶弹出一个控制K页空间的span
     * 在这个过程中还要对Span的页号和地址进行映射
     * @param k 弹出的span控制的空间页数
     * @return span指针
     */
    Span* NewSpan(size_t k);

    /**
     * 内存地址到span的映射
     * @param obj 内存块指针
     * @return span指针
     */
    Span* MapObjectToSpan(void* obj);

    // 管理CC释放的span，合并span前后空间
    // 整个过程需要加锁，因为Span的状态不能发生变化
    void ReleaseSpanToPageCache(Span* span);

    //DeBug:每个桶中span的数量
    void PrintDebugInfo() {
        std::cout << "=========== PageCache Info ===========" << std::endl;
        // PageCache 的锁是把大锁 (_pageMtx)，通常在外部控制，或者这里为了调试只读一下
        // 注意：PageCache 的 spanLists 数组本身没有独立的锁，而是由 _pageMtx 保护

        // 这种调试打印在多线程高并发下通常需要小心，简单起见我们假设调试时没竞争
        for (size_t i = 1; i < PAGE_NUM; ++i) {
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
    ObjectPool<Span> _spanPool; // Span定长内存池

    // PageID和span地址的映射关系
    // 块地址右移13位可得当前块的页号，
    // 再通过这个哈希表可以直接得到该块所属的span地址
    std::unordered_map<size_t, Span*> _idSpanMap;
};
