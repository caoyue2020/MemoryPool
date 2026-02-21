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
    
    // 🌟 修正1：改用 nanoseconds (纳秒) 累加。
    // 如果单轮循环极快（<1ms），强转成毫秒会变成 0，导致总计时间严重偏小。最后打印时再除以一百万转成毫秒。
    std::atomic<long long> malloc_costtime{0};
    std::atomic<long long> free_costtime{0};

    // 🌟 修正2：宏观上帝视角计时
    // 包裹住线程创建、TLS销毁、系统回收的全部生命周期，这才是你的“真实体感等待时间”
    auto global_begin = std::chrono::high_resolution_clock::now();

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
                    size_t size = (16 + i) % 8192 + 1;
                    void* p = malloc(size);
                    
                    // 🌟 修正3：写脏数据 (极其重要)
                    // 强迫操作系统分配真正的物理内存（触发缺页中断），并防止编译器 -O3 优化直接把 malloc 删掉
                    if (p) ((char*)p)[0] = '!'; 
                    
                    v.push_back(p);
                }
                auto end1 = std::chrono::high_resolution_clock::now();

                auto begin2 = std::chrono::high_resolution_clock::now();
                for (size_t i = 0; i < ntimes; i++)
                {
                    free(v[i]);
                }
                auto end2 = std::chrono::high_resolution_clock::now();
                v.clear();

                // 累加纳秒
                malloc_costtime += std::chrono::duration_cast<std::chrono::nanoseconds>(end1 - begin1).count();
                free_costtime += std::chrono::duration_cast<std::chrono::nanoseconds>(end2 - begin2).count();
            }
        });
    }

    for (auto& t : vthread)
    {
        t.join();
    }
    auto global_end = std::chrono::high_resolution_clock::now();
    size_t global_cost = std::chrono::duration_cast<std::chrono::milliseconds>(global_end - global_begin).count();

    // 将纳秒转换回毫秒用于展示
    size_t pure_malloc_ms = malloc_costtime.load() / 1000000;
    size_t pure_free_ms = free_costtime.load() / 1000000;

    printf("==================== Malloc 基准测试 ====================\n");
    printf("%zu个线程并发执行%zu轮次，每轮次操作 %zu次:\n", nworks, rounds, ntimes);
    printf(" -> 纯申请耗时 (多线程内部累计)：%zu ms\n", pure_malloc_ms);
    printf(" -> 纯释放耗时 (多线程内部累计)：%zu ms\n", pure_free_ms);
    printf(" -> 纯操作总计 (CPU执行总时间)：%zu ms\n", pure_malloc_ms + pure_free_ms);
    printf(" 🌟 真实体感总耗时 (挂钟时间，含线程与OS开销)：%zu ms\n", global_cost);
    printf("=========================================================\n\n");
}


// 单轮次申请释放次数 线程数 轮次
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
    std::vector<std::thread> vthread(nworks);
    
    std::atomic<long long> malloc_costtime{0};
    std::atomic<long long> free_costtime{0};

    // 宏观上帝视角计时
    auto global_begin = std::chrono::high_resolution_clock::now();

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
                    size_t size = (16 + i) % 8192 + 1;
                    void* p = ConcurrentAlloc(size);
                    
                    // 写脏数据
                    if (p) ((char*)p)[0] = '!'; 
                    
                    v.push_back(p);
                }
                auto end1 = std::chrono::high_resolution_clock::now();

                auto begin2 = std::chrono::high_resolution_clock::now();
                for (size_t i = 0; i < ntimes; i++)
                {
                    ConcurrentFree(v[i]);
                }
                auto end2 = std::chrono::high_resolution_clock::now();
                v.clear();

                malloc_costtime += std::chrono::duration_cast<std::chrono::nanoseconds>(end1 - begin1).count();
                free_costtime += std::chrono::duration_cast<std::chrono::nanoseconds>(end2 - begin2).count();
            }
        });
    }

    for (auto& t : vthread)
    {
        t.join();
    }
    
    auto global_end = std::chrono::high_resolution_clock::now();
    size_t global_cost = std::chrono::duration_cast<std::chrono::milliseconds>(global_end - global_begin).count();

    size_t pure_malloc_ms = malloc_costtime.load() / 1000000;
    size_t pure_free_ms = free_costtime.load() / 1000000;

    printf("================ ConcurrentAlloc 基准测试 ================\n");
    printf("%zu个线程并发执行%zu轮次，每轮次操作 %zu次:\n", nworks, rounds, ntimes);
    printf(" -> 纯申请耗时 (多线程内部累计)：%zu ms\n", pure_malloc_ms);
    printf(" -> 纯释放耗时 (多线程内部累计)：%zu ms\n", pure_free_ms);
    printf(" -> 纯操作总计 (CPU执行总时间)：%zu ms\n", pure_malloc_ms + pure_free_ms);
    printf(" 🌟 真实体感总耗时 (挂钟时间，含TLS清理与OS开销)：%zu ms\n", global_cost);
    printf("=========================================================\n\n");
}
