//
// Created by CAO on 2026/2/18.
//

#ifndef MEMORYPOOL_UNITTEST_H
#define MEMORYPOOL_UNITTEST_H

#endif //MEMORYPOOL_UNITTEST_H

// 测试用例
void Benchmark();

// void ConcurrentAllocTest();
// void ConcurrentAllocTest2();
// void TestConcurrentFree1();
// void MultiThreadAlloc1();
// void MultiThreadAlloc2();
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds);

void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds);
