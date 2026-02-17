#pragma once
#include <valarray>

#include "Common.h"

class CentralCache
{
public:
    // 单例模式，局部静态变量模式
    static CentralCache* getInstance()
    {
        static CentralCache _sInst;
        return &_sInst;
    }
    CentralCache(const CentralCache& copy) = delete;
    CentralCache& operator =(const CentralCache& copy) = delete;

    /**
     * CC给TC分配空间：从指定桶中选取一个非空span，将该span的自由链表给TC
     * @param start [out] 返回的大内存块起始地址
     * @param end [out] 返回的大内存块终止地址
     * @param batchNum [in] TC请求的内存块数量
     * @param size [in] size为TC需要的单块内存块字节数
     * @return actualNum 返回CC实际提供的大小（span可用块数量可能小于TC请求的数量）
     */
    size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

    // CC获取一个非空的span，从中选取内存分配给TC
    // 两种情况，自身有 or 需向PC申请
    // PC锁
    Span* getOneSpan(SpanList& list, size_t size);

    /**
     * 回收TC弹出的内存块
     * @param start 自由链表头
     * @param size 内存块大小
     */
    void ReleaseListToSpans(void* start, size_t size);

    //DeBug:打印桶中非空span数量，打印非空span内存块数量
    void PrintDebugInfo() {
        std::cout << "========== CentralCache Info =========" << std::endl;
        for (size_t i = 0; i < FREE_LIST_NUM; ++i) {
            // 先判断非空，减少加锁开销
            if (!_spanLists[i].Empty()) {
                std::unique_lock<std::mutex> lock(_spanLists[i].mtx);

                // 打印桶级别的汇总信息
                std::cout << "Bucket " << i << ": " << _spanLists[i].Size() << " spans" << std::endl;

                // 遍历当前桶中的每一个 Span
                Span* span = _spanLists[i].Begin();
                while (span != _spanLists[i].End()) {
                    // --- 计算当前 Span 挂载的自由链表长度 ---
                    size_t blockCount = 0;
                    void* cur = span->_freeList;
                    while (cur) {
                        blockCount++;
                        cur = ObjNext(cur); // 移动到下一个内存块
                    }
                    // ------------------------------------

                    std::cout << "  -> Span [PageId:" << span->_pageId
                              << ", Pages:" << span->_n
                              << ", UseCount:" << span->_usecount
                              << "] FreeBlocks: " << blockCount << std::endl;

                    span = span->_next;
                }
            }
        }
        std::cout << "======================================" << std::endl;
    }
private:
    //私有化构造函数，禁用拷贝、直接构造
    CentralCache() = default;


private:
    // 以SpanList为元素的哈希表
    // 除了基础元素不同，其余逻辑与TC中一致
    SpanList _spanLists[FREE_LIST_NUM];
};


