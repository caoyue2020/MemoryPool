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

    //TC给线程分配空间
    void* Allocate(size_t size);
    //TC回收线程的空间
    void Deallocate(void* obj, size_t size);

    //TC向CC申请空间
    void* FetchFromCentralCache(size_t index, size_t alignSize);

    /**
     * TC向CC归还空间
     * @param list 自由链表
     * @param size 对齐后的内存块大小
     */
    void ListTooLong(FreeList& list, size_t size);


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

