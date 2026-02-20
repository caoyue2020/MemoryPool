//
// Created by CAO on 2026/2/20.
//

#include "ConcurrentAlloc.h"
#include <thread>
#include <vector>
#include <atomic>
// #include <ctime>
// #include <cstdio>

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
        // 修正2：[&] 已经隐式按引用捕获了外部变量，无需再额外写 k，规范书写
        vthread[k] = std::thread([&]() {
            std::vector<void*> v;
            v.reserve(ntimes);

            for (size_t j = 0; j < rounds; ++j)
            {
                size_t begin1 = clock();
                for (size_t i = 0; i < ntimes; i++)
                {
                    // size_t size = 16;
                    size_t size = (16 + i) % 8192 + 1;
                    // 每一次申请不同桶中的块，模拟真实场景下的内存碎片化申请
                    v.push_back(malloc(size));
                }
                size_t end1 = clock();

                size_t begin2 = clock();
                for (size_t i = 0; i < ntimes; i++)
                {
                    free(v[i]);
                }
                size_t end2 = clock();
                v.clear();

                malloc_costtime += (end1 - begin1);
                free_costtime += (end2 - begin2);
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

//                       单轮次申请释放次数 线程数 轮次
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
    std::vector<std::thread> vthread(nworks);
    std::atomic<size_t> malloc_costtime{0};
    std::atomic<size_t> free_costtime{0};

    for (size_t k = 0; k < nworks; ++k)
    {
        // [](){} :lambda匿名表达式
        vthread[k] = std::thread([&]() {
            std::vector<void*> v;
            v.reserve(ntimes);

            for (size_t j = 0; j < rounds; ++j)
            {
                size_t begin1 = clock();
                for (size_t i = 0; i < ntimes; i++)
                {
                    // size_t size = 16;
                    size_t size = (16 + i) % 8192 + 1;
                    v.push_back(ConcurrentAlloc(size));
                }
                size_t end1 = clock();

                size_t begin2 = clock();
                for (size_t i = 0; i < ntimes; i++)
                {
                    ConcurrentFree(v[i]);
                }
                size_t end2 = clock();
                v.clear();

                malloc_costtime += (end1 - begin1);
                free_costtime += (end2 - begin2);
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