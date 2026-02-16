#pragma once
#include "Common.h"

class ThreadCache
{
private:
    FreeList _freeLists[FREE_LIST_NUM]; //哈希桶，每个桶都是一个自由链表

public:
    static ThreadCache* getInstance() {
        static thread_local ThreadCache pTLSThreadCache;
        return &pTLSThreadCache;
    }

    void* Allocate(size_t size); //分配内存
    void Deallocate(void* obj, size_t size);//回收内存

    //ThreadCache空间不够用时，向CentralCache申请空间的接口
    void* FetchFromCentralCache(size_t index, size_t alignSize);

    // DeBug:打印内存块数量
    void PrintDebugInfo() {
        std::cout << "========== ThreadCache Info ==========" << std::endl;
        for (size_t i = 0; i < FREE_LIST_NUM; ++i) {
            if (!_freeLists[i].Empty()) {
                std::cout << "Bucket " << i << ": Has "
                << _freeLists[i].Size() << " blocks(_maxSize=" << _freeLists[i].MaxSize() << ")" << std::endl;
            }
        }
        std::cout << "======================================" << std::endl;
    }
};

// static thread_local ThreadCache* pTLSThreadCache = nullptr;