cmake_minimum_required(VERSION 4.0)
project(MemoryPool)

set(CMAKE_CXX_STANDARD 14)

include_directories(Include)

add_executable(MemoryPool
        Include/CentralCache.h
        Include/Common.h
        Include/ConcurrentAlloc.h
        Include/ObjectPool.h
        Include/ThreadCache.h
        Source/ThreadCache.cpp
        Test.cpp
        unitTest.cpp
        Source/CentralCache.cpp
        Include/PageCache.h
        Source/PageCache.cpp)
