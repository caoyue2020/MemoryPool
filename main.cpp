//
// Created by CAO on 2026/2/16.
//
#include "CentralCache.h"
#include "PageCache.h"
#include "ThreadCache.h"
#include "ConcurrentAlloc.h"
#include "./test/unitTest.h"

int main()
{
    size_t n = 10000;
    cout << "==========================================================" << endl;

    // 这里表示4个线程，每个线程申请1万次，执行10轮，总共申请40万次
    BenchmarkConcurrentMalloc(n, 8, 10000);

    // 作为对比参照组
    BenchmarkMalloc(n, 8, 10000);

    cout << "==========================================================" << endl;

    return 0;
}
