//
// Created by CAO on 2026/2/16.
//
#include "CentralCache.h"
#include "PageCache.h"
#include "ThreadCache.h"


// 测试代码
void Benchmark();
void ConcurrentAllocTest();
void ConcurrentAllocTest2();
void TestConcurrentFree1();
void MultiThreadAlloc1();
void MultiThreadAlloc2();

int main() {
    // Benchmark();
    // ConcurrentAllocTest2();
    MultiThreadAlloc1();
    MultiThreadAlloc2();
    ThreadCache::getInstance()->PrintDebugInfo();
    CentralCache::getInstance()->PrintDebugInfo();
    PageCache::getInstance()->PrintDebugInfo();
}