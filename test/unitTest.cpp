#include "ConcurrentAlloc.h"

void ConcurrentAllocTest() {
    void* ptr1 = ConcurrentAlloc(5);
    void* ptr2 = ConcurrentAlloc(8);
    void* ptr3 = ConcurrentAlloc(4);
    void* ptr4 = ConcurrentAlloc(6);
    void* ptr5 = ConcurrentAlloc(3);

    cout << ptr1 << endl;
    cout << ptr2 << endl;
    cout << ptr3 << endl;
    cout << ptr4 << endl;
    cout << ptr5 << endl;
}
