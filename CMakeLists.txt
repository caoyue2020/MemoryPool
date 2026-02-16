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
        Include/PageCache.h

        Source/ThreadCache.cpp
        Source/CentralCache.cpp
        Source/PageCache.cpp

        test/ObjectPoolTest.cpp
        test/unitTest.cpp
        main.cpp
)