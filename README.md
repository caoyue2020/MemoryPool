# 基于 C++ 的高并发内存池 (Concurrent Memory Pool)

## 一、 项目简介 (Overview)

本项目是一个基于 C++11 实现的并发内存分配器，其核心设计思想参考了 Google 的开源项目 **TCMalloc** (Thread-Caching Malloc)。

在多线程高并发场景下，频繁调用系统原生的 `malloc/free` 容易引发全局锁竞争，从而影响程序性能，同时也会带来内存碎片化问题。本项目通过引入**三层缓存架构**与**线程局部存储 (TLS)** 技术，将大部分内存分配操作隔离在线程内部完成，从而有效降低了锁竞争的开销。在特定的多线程压测场景下，相较于原生分配器表现出了更为稳定的分配与释放效率。



## 二、 核心架构设计 (Architecture)

本内存池采用三级缓存架构，从内到外分别管理不同粒度的内存请求，逐步放大锁的粒度并降低锁的竞争频率：

```mermaid
flowchart TD
    subgraph 线程私有层
        T1((Thread 1)) <--> TC1[ThreadCache]
        T2((Thread 2)) <--> TC2[ThreadCache]
    end

    subgraph 中心调度层
        TC1 <--> |批量申请 / 延迟归还| CC[(CentralCache\n按大小分桶调度)]
        TC2 <--> |批量申请 / 延迟归还| CC
    end

    subgraph 全局页缓存层
        CC <--> |Span 申请 / 回收合并| PC[[PageCache\n以页为单位管理]]
    end

    subgraph 操作系统
        PC <--> |mmap / VirtualAlloc| OS[(System Memory)]
    end

    style TC1 fill:#e1f5fe,stroke:#0288d1
    style TC2 fill:#e1f5fe,stroke:#0288d1
    style CC fill:#fff3e0,stroke:#f57c00
    style PC fill:#e8f5e9,stroke:#388e3c

```

1. **ThreadCache (线程私有缓存)**：负责处理 `size <= 256KB` 的小块内存请求。基于 `thread_local` 实现，每个线程独享。分配和释放内存时**无需加锁**。
2. **CentralCache (中心共享缓存)**：作为所有线程的公共内存池，按块大小（SizeClass）划分为多个独立的哈希桶。采用**细粒度桶锁**，仅在多个线程同时操作同一个桶时才会产生竞争。
3. **PageCache (全局页缓存)**：以系统页（通常为 8KB）为单位管理大块内存。持有全局大锁，负责向操作系统申请原始物理内存，并在回收到相邻空闲页时进行合并，以缓解内存碎片问题。



## 三、 数据流转解析 (Data Flow)


### 1. 内存申请逻辑 `ConcurrentAlloc(size)`

```mermaid
flowchart TD
    Start((申请内存 size)) --> Calc[计算对应的哈希桶索引]
    Calc --> CheckTC{ThreadCache\n当前桶是否有空闲块?}
    
    CheckTC -- "是 (Fast Path)" --> Pop[从对应的自由链表头部取出一个块]
    Pop --> Return((返回指针给用户))
    
    CheckTC -- "否 (Slow Path)" --> RequestCC[向 CentralCache 批量申请]
    RequestCC --> CheckCC{CentralCache\n该桶是否有空闲 Span?}
    
    CheckCC -- "是" --> Slice[切分 Span 并批量返回给 TC]
    Slice --> Return
    
    CheckCC -- "否" --> RequestPC[向 PageCache 申请新 Span]
    RequestPC --> CheckPC{PageCache\n是否有足够连续页?}
    
    CheckPC -- "是" --> ReturnSpan[返回 Span 给 CentralCache]
    ReturnSpan --> Slice
    
    CheckPC -- "否" --> SystemAlloc[向操作系统底层申请内存]
    SystemAlloc --> ReturnSpan
    
    classDef fastPath fill:#d4edda,stroke:#28a745,stroke-width:2px;
    class CheckTC,Pop,Return fastPath;

```

### 2. 内存释放逻辑 `ConcurrentFree(ptr)`

```mermaid
flowchart TD
    Start((释放内存 ptr)) --> FindSpan[通过基数树查询 ptr 所属的 Span]
    FindSpan --> PushTC[将内存块挂回 ThreadCache 对应的自由链表]
    
    PushTC --> CheckLen{ThreadCache 桶内\n块数量是否超过水位线?}
    
    CheckLen -- "否 (Fast Path)" --> End((释放完成))
    
    CheckLen -- "是 (Slow Path)" --> ReturnCC[截取部分链表批量还给 CentralCache]
    ReturnCC --> DecUseCount[对应 Span 的 _usecount 递减]
    
    DecUseCount --> CheckUseCount{Span 的 _usecount\n是否归零?}
    
    CheckUseCount -- "否" --> End
    
    CheckUseCount -- "是" --> ReturnPC[说明物理页完整，将其交还给 PageCache]
    ReturnPC --> Merge[PageCache 尝试合并前后相邻的空闲页]
    Merge --> End

    classDef fastPath fill:#d4edda,stroke:#28a745,stroke-width:2px;
    class CheckLen,End fastPath;

```



## 四、 关键技术实现 (Key Implementations)

1. **基数树 (Radix Tree) 的无锁查询**
* **背景**：在释放内存时，需要通过对象地址反查其所属的 `Span`。如果使用传统的 `std::unordered_map`，在高频并发读写时需要加全局锁，容易成为性能瓶颈。
* **实现**：引入 64 位系统下的三层基数树（PageMap）。建立映射时（写操作）由 PageCache 大锁保护；由于树的底层结构静态稳定，硬件可保证指针对齐读写的原子性，因此反查映射时（读操作）实现了**无锁化 (Lock-Free)**，大幅提升了并发释放的效率。


2. **局部链表优化临界区**
* **背景**：CentralCache 向 PageCache 归还 Span 时，如果不提前释放桶锁，容易导致“锁护送”现象。
* **实现**：在 `ReleaseListToSpans` 中使用局部变量 `emptySpans` 暂存需要归还的 Span。提前解除 CentralCache 的桶锁后，再统一获取 PageCache 大锁进行归还。这种设计严格控制了锁的持有时间。


3. **独立的内部对象池 (ObjectPool)**
* **背景**：内存池运行过程中需要管理自身的元数据（如 `Span` 对象），如果依赖原生 `new/delete`，会造成循环调用。
* **实现**：实现了一个基于直接向系统申请大块内存的定长对象池（ObjectPool），专门用于内部元数据的分配，做到了与系统原生分配器的彻底解耦。



---

## 五、 基准测试 (Benchmarks)

在多线程环境下运行了 `test/benchmark.cpp` 进行基准测试（测试环境已开启 CMake Release 模式及 `-O3` 优化）。

* **测试条件**：8 线程并发，执行 10000 轮次，每轮次动态分配并释放 10000 次大小不一的内存块（模拟内存碎片化场景），总计发生 800,000,000 次分配与释放。

| 分配器类型 | 分配 (Alloc) 耗时 | 释放 (Dealloc) 耗时 | 整体总计耗时         |
| --- |---------------|-----------------|----------------|
| 系统原生 `malloc/free**` | ~155722 ms    | ~176661 ms      | ~332383 ms |
| 本项目 `ConcurrentAlloc**` | **~359 ms**   | **~83 ms**      | **~442 ms**    |

*(注：以上数据受不同 CPU 架构、操作系统缓存状态及编译器优化程度影响，仅供参考。)*

测试结果表明，在持续的高并发小内存分配场景下，该缓存架构能有效减少线程间的阻塞等待，性能相较于原生实现有较明显的改善。

---

## 六、 编译与运行 (Quick Start)

本项目使用 `CMake` 作为构建系统，在 Linux / macOS / Windows 环境下均可编译。

```bash
# 1. 克隆项目
git clone https://github.com/caoyue2020/MemoryPool.git
cd MemoryPool

# 2. 创建构建目录
mkdir build && cd build

# 3. 配置 CMake (建议使用 Release 模式以确保性能表现)
cmake -DCMAKE_BUILD_TYPE=Release ..

# 4. 编译可执行文件
make -j4

# 5. 运行基准测试程序
./MemoryPool

```

## 七、Reference
https://gitee.com/yjy_fangzhang/memory-pool-project/tree/master/ConcurrentMemoryPool/ConcurrentMemoryPool
