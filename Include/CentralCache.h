#pragma once
#include "./Include/Common.h"

class CentralCache
{
public:
    // 单例模式，局部静态变量模式
    static CentralCache* GetInstance()
    {
        static CentralCache* _sInst;
        return _sInst;
    }

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


