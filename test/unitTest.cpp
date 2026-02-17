#include "CentralCache.h"
#include "ConcurrentAlloc.h"
#include "PageCache.h"
#include "ThreadCache.h"
#include<thread>

void ConcurrentAllocTest() {
    void* ptr1 = ConcurrentAlloc(5);
    void* ptr2 = ConcurrentAlloc(8);
    void* ptr3 = ConcurrentAlloc(4);
    void* ptr4 = ConcurrentAlloc(6);
    void* ptr5 = ConcurrentAlloc(3);

    ThreadCache::getInstance()->PrintDebugInfo();
    CentralCache::getInstance()->PrintDebugInfo();
    PageCache::getInstance()->PrintDebugInfo();

    cout << ptr1 << endl;
    cout << ptr2 << endl;
    cout << ptr3 << endl;
    cout << ptr4 << endl;
    cout << ptr5 << endl;
}

void ConcurrentAllocTest2() {
    for (int i=0 ; i<1024; i++) {
        void* ptr = ConcurrentAlloc(5);
        cout << ptr << endl;
    }
    cout << " -------Before--------" << endl;
    ThreadCache::getInstance()->PrintDebugInfo();
    CentralCache::getInstance()->PrintDebugInfo();
    PageCache::getInstance()->PrintDebugInfo();

    void * ptr = ConcurrentAlloc(3);

    cout << " -------After--------" << endl;
    ThreadCache::getInstance()->PrintDebugInfo();
    CentralCache::getInstance()->PrintDebugInfo();
    PageCache::getInstance()->PrintDebugInfo();
    cout << " -------END--------" << endl;
}

void TestConcurrentFree1() {
    void* ptr1 = ConcurrentAlloc(5);
    void* ptr2 = ConcurrentAlloc(8);
    void* ptr3 = ConcurrentAlloc(4);
    void* ptr4 = ConcurrentAlloc(6);
    void* ptr5 = ConcurrentAlloc(3);
    void* ptr6 = ConcurrentAlloc(3);
    void* ptr7 = ConcurrentAlloc(3);

    ThreadCache::getInstance()->PrintDebugInfo();
    CentralCache::getInstance()->PrintDebugInfo();
    PageCache::getInstance()->PrintDebugInfo();

    cout << "" <<endl;
    cout << endl;
    cout << endl;
    ConcurrentFree(ptr1, 5);
    ConcurrentFree(ptr2, 8);
    ConcurrentFree(ptr3, 4);
    ConcurrentFree(ptr4, 6);
    ConcurrentFree(ptr5, 3);
    ConcurrentFree(ptr6, 3);
    ConcurrentFree(ptr7, 3);

    ThreadCache::getInstance()->PrintDebugInfo();
    CentralCache::getInstance()->PrintDebugInfo();
    PageCache::getInstance()->PrintDebugInfo();
}

void MultiThreadAlloc1()
{
    std::vector<void*> v;
    for (size_t i = 0; i < 7; ++i) // 申请7次，正好单个线程能走到pc回收cc中span的那一步
    {
        void* ptr = ConcurrentAlloc(6); // 申请的都是8B的块空间
        v.push_back(ptr);
    }

    for (auto e : v)
    {
        ConcurrentFree(e, 6);
    }
}

void MultiThreadAlloc2()
{
    std::vector<void*> v;
    for (size_t i = 0; i < 7; ++i)
    {
        void* ptr = ConcurrentAlloc(16); // 申请的都是16B的块空间
        v.push_back(ptr);
    }

    for (int i = 0; i < 7; ++i)
    {
        ConcurrentFree(v[i], 16);
    }
}

void TestMultiThread()
{
    std::thread t1(MultiThreadAlloc1);
    std::thread t2(MultiThreadAlloc2);

    t1.join();
    t2.join();
}
