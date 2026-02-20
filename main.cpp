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
    // 这里表示4个线程，每个线程申请10万次，总共申请40万次
    BenchmarkConcurrentMalloc(n, 4, 10);
    cout << endl << endl;

    // 这里表示4个线程，每个线程申请10万次，总共申请40万次
    BenchmarkMalloc(n, 4, 10);
    cout << "==========================================================" << endl;

    return 0;
}
