//
// Created by CAO on 2026/2/16.
//
#include "CentralCache.h"
#include "PageCache.h"
#include "ThreadCache.h"
#include "./test/unitTest.h"


int main() {
    MultiThreadAlloc1();
    MultiThreadAlloc2();
    ThreadCache::getInstance()->PrintDebugInfo();
    CentralCache::getInstance()->PrintDebugInfo();
    PageCache::getInstance()->PrintDebugInfo();

    //TODO:单次超过256KB的申请？
}