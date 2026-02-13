#pragma once
#include "Common.h"

class CentralCache
{
public:
    // 单例模式，局部静态变量模式
    static CentralCache* GetInstance()
    {
        static CentralCache* _sInst;
        return _sInst;
    }

    /**
     * @param start [out] 返回的大内存块起始地址
     * @param end [out] 返回的大内存块终止地址
     * @param n [in] TC请求的内存块数量
     * @param size [in] szie为TC需要的单块内存块字节数
     * @return num 返回CC实际提供的大小（span可用块数量可能小于TC请求的数量）
     */
    size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t size);


private:
    //私有化构造函数，禁用拷贝、直接构造
    CentralCache() = default;
    CentralCache(const CentralCache& copy) = delete;
    CentralCache& operator =(const CentralCache& copy) = delete;

private:
    // 以SpanList为元素的哈希表
    // 除了基础元素不同，其余逻辑与TC中一致
    SpanList _spanLists[FREE_LIST_NUM];

};


