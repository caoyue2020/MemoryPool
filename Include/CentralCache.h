#pragma once
#include "./Include/Common.h"

class CentralCache
{
private:
    // 以SpanList为元素的哈希表
    // 除了基础元素不同，其余逻辑与TC中一致
    SpanList _spanLists[FREE_LIST_NUM];
public:
    
};


