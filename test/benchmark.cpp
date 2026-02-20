//
// Created by CAO on 2026/2/20.
//

#include "ConcurrentAlloc.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>   // 引入高精度计时库
#include <cstdio>   // printf 需要用到
#include <iostream>

using namespace std;

// ntimes 一轮申请和释放内存的次数
// nworks 表示创建多少个线程
// rounds 轮次
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
    std::vector<std::thread> vthread(nworks);
    // 修正1：std::atomic 的拷贝初始化被 delete 了，应使用 {} 或 () 初始化
    std::atomic<size_t> malloc_costtime{0};
    std::atomic<size_t> free_costtime{0};

    for (size_t k = 0; k < nworks; ++k)
    {
        // 修正2：[&] 已经隐式按引用捕获了外部变量
        vthread[k] = std::thread([&]() {
            std::vector<void*> v;
            v.reserve(ntimes);

            for (size_t j = 0; j < rounds; ++j)
            {
                // 使用基于物理挂钟的高精度计时
                auto begin1 = std::chrono::high_resolution_clock::now();
                for (size_t i = 0; i < ntimes; i++)
                {
                    // size_t size = 16;
                    size_t size = (16 + i) % 8192 + 1;
                    // 每一次申请不同桶中的块，模拟真实场景下的内存碎片化申请
                    v.push_back(malloc(size));
                }
                auto end1 = std::chrono::high_resolution_clock::now();

                auto begin2 = std::chrono::high_resolution_clock::now();
                for (size_t i = 0; i < ntimes; i++)
                {
                    free(v[i]);
                }
                auto end2 = std::chrono::high_resolution_clock::now();
                v.clear();

                // 严谨计算毫秒差值并累加
                malloc_costtime += std::chrono::duration_cast<std::chrono::milliseconds>(end1 - begin1).count();
                free_costtime += std::chrono::duration_cast<std::chrono::milliseconds>(end2 - begin2).count();
            }
        });
    }

    for (auto& t : vthread)
    {
        t.join();
    }

    // 修正3：size_t 的格式化控制符应该是 %zu 而不是 %u
    printf("%zu个线程并发执行%zu轮次，每轮次malloc %zu次: 花费：%zu ms\n",
           nworks, rounds, ntimes, malloc_costtime.load());

    printf("%zu个线程并发执行%zu轮次，每轮次free %zu次: 花费：%zu ms\n",
           nworks, rounds, ntimes, free_costtime.load());

    printf("%zu个线程并发malloc&free %zu次，总计花费：%zu ms\n\n",
           nworks, nworks * rounds * ntimes, malloc_costtime.load() + free_costtime.load());
}


// 单轮次申请释放次数 线程数 轮次
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
    std::vector<std::thread> vthread(nworks);
    std::atomic<size_t> malloc_costtime{0};
    std::atomic<size_t> free_costtime{0};

    for (size_t k = 0; k < nworks; ++k)
    {
        vthread[k] = std::thread([&]() {
            std::vector<void*> v;
            v.reserve(ntimes);

            for (size_t j = 0; j < rounds; ++j)
            {
                auto begin1 = std::chrono::high_resolution_clock::now();
                for (size_t i = 0; i < ntimes; i++)
                {
                    // size_t size = 16;
                    size_t size = (16 + i) % 8192 + 1;
                    v.push_back(ConcurrentAlloc(size));
                }
                auto end1 = std::chrono::high_resolution_clock::now();

                auto begin2 = std::chrono::high_resolution_clock::now();
                for (size_t i = 0; i < ntimes; i++)
                {
                    ConcurrentFree(v[i]);
                }
                auto end2 = std::chrono::high_resolution_clock::now();
                v.clear();

                malloc_costtime += std::chrono::duration_cast<std::chrono::milliseconds>(end1 - begin1).count();
                free_costtime += std::chrono::duration_cast<std::chrono::milliseconds>(end2 - begin2).count();
            }
        });
    }

    for (auto& t : vthread)
    {
        t.join();
    }

    printf("%zu个线程并发执行%zu轮次，每轮次concurrent alloc %zu次: 花费：%zu ms\n",
           nworks, rounds, ntimes, malloc_costtime.load());

    printf("%zu个线程并发执行%zu轮次，每轮次concurrent dealloc %zu次: 花费：%zu ms\n",
           nworks, rounds, ntimes, free_costtime.load());

    printf("%zu个线程并发concurrent alloc&dealloc %zu次，总计花费：%zu ms\n\n",
           nworks, nworks * rounds * ntimes, malloc_costtime.load() + free_costtime.load());
}

