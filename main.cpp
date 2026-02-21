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
    size_t n = 1000; // 轮次
    cout << "==========================================================" << endl;
    // 这里表示8个线程，每个线程申请1万次，执行10轮，总共申请80万次
    BenchmarkMalloc(n, 8, 10000);

    cout << "==========================================================" << endl;
    BenchmarkConcurrentMalloc(n, 8, 10000);


    return 0;
}
