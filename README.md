# 高并发内存池 

## 一、 项目简介 (Overview)

本项目是一个基于 C++11 实现的并发内存分配器，其核心设计思想参考了 Google 的开源项目 **TCMalloc** (Thread-Caching Malloc)。

在多线程高并发场景下，频繁调用系统原生的 `malloc/free` 容易引发全局锁竞争，从而影响程序性能，同时也会带来内存碎片化问题。本项目通过引入**三层缓存架构**与**线程局部存储 (TLS)** 技术，将大部分内存分配操作隔离在线程内部完成，从而有效降低了锁竞争的开销。在特定的多线程压测场景下，相较于原生分配器表现出了更为稳定的分配与释放效率。



## 二、 核心架构设计 (Architecture)
1. **ThreadCache (线程私有缓存)**：按块大小划分为多个独立的哈希桶，负责处理 `size <= 256KB` 的小块内存请求。基于 `thread_local` 实现，每个线程独享。分配和释放内存时**无需加锁**。
2. **CentralCache (中心共享缓存)**：作为所有线程的公共内存池，与ThreadCache以相同的方式划分哈希桶，每个哈希桶包含元素为span的双向链表，每个span挂着自由链表。采用**桶锁**，仅在多个线程同时操作同一个桶时才会产生竞争。CentralCache负责页内存和块内存的转换，既需要切分从PageCache获取的连续页内存，又需要将ThreadCache释放的零散块内存组合为页交付给PageCache。
3. **PageCache (全局页缓存)**：以系统页（通常为 8KB）为单位管理大块内存。持有全局大锁，负责向操作系统申请原始物理内存（128页），然后将内存切分为指定页传给CenterCache。在回收到相邻空闲页时进行合并，以缓解内存碎片问题。


## 三、 数据流转解析 (Data Flow)


### 1. 内存申请逻辑 `ConcurrentAlloc(size)`

```mermaid
flowchart TD
    %% ================= 1. 全局节点提前声明 =================
    Start(["ConcurrentAlloc(size)"])
    SizeCheck{"size > 256KB?"}
    Return(["返回指针给用户"])

    %% ================= 2. 线程私有层 =================
    subgraph TC [1. ThreadCache 层]
        CheckTC{"_freeLists[index]<br>是否有空闲块?"}
        TCPop["从自由链表头删获取 block"]
        FetchCC["FetchFromCentralCache()<br>计算慢启动 batchNum"]
    end

    %% ================= 3. 全局共享层 & 系统底层 (深度嵌套) =================
    subgraph CC [2. CentralCache 层 / 全局共享]
        CCGetOne{"🔒加桶锁 mtx.lock()<br>当前桶是否有空闲 Span?"}
        CCSlice["切分 Span 提取实际所需 block<br>🔓解桶锁 mtx.unlock()<br>1个返回用户，剩余批量挂回TC"]
        ReqPC["🔓先解桶锁 (防交叉死锁!)<br>计算需要向PC申请的页数 k"]:::warnNode

        %% 【深度嵌套 1】：PageCache 属于 CentralCache 的后勤仓库
        subgraph PC [3. PageCache 核心调度]
            PCBig["按页计算对齐大小 k"]
            PCLock["🔒加全局大锁 _pageMtx.lock()"]
            CheckPC{"k > 128页 ?"}
            SearchBucket{"遍历桶 [k] 到 [128]<br>是否有空闲的大 Span?"}
            PCSplit["Span 分裂算法 (切下k页)<br>剩余页组装成新Span挂回哈希桶"]
            
            SetUse["状态机: 标记派发 Span->_isUse = true"]:::warnNode
            
            PCUnlock["🔓解全局大锁 _pageMtx.unlock()"]
            CCLock3["🔒重新加桶锁 mtx.lock()"]:::warnNode
            
            SysAlloc128["向 OS 申请 128 页"]
            SysAllocK["向 OS 申请 k 页"]
            
            PCUnlock2["将大页记录到映射表<br>状态机: 标记 Span->_isUse = true<br>🔓解大锁 _pageMtx.unlock()"]:::warnNode

            %% 【深度嵌套 2】：OS 属于 PageCache 独占的底层资源
            subgraph OS [4. Operating System 底层调用]
                Mem1[(系统物理内存)]:::sysNode
                Mem2[(系统物理内存)]:::sysNode
            end
        end
    end

    %% ================= 4. 集中连线 =================
    Start --> SizeCheck
    SizeCheck -- "否 (≤256KB)" --> CheckTC
    SizeCheck -- "是 (>256KB)" --> PCBig
    
    %% TC 逻辑
    CheckTC -- "是 (Fast Path)" --> TCPop
    TCPop --> Return
    CheckTC -- "否 (Slow Path)" --> FetchCC
    FetchCC --> CCGetOne
    
    %% CC 逻辑
    CCGetOne -- "是" --> CCSlice
    CCSlice --> Return
    CCGetOne -- "否" --> ReqPC
    ReqPC --> PCLock
    
    %% PC 内部逻辑
    PCBig --> PCLock
    PCLock --> CheckPC
    CheckPC -- "否" --> SearchBucket
    SearchBucket -- "是" --> PCSplit
    
    %% 状态机转移链路
    PCSplit --> SetUse
    SetUse --> PCUnlock
    PCUnlock --> CCLock3
    CCLock3 --> CCSlice
    
    %% OS 跨层调用 (被完美限制在 PC 内部)
    SearchBucket -- "否" --> SysAlloc128
    SysAlloc128 -. "mmap" .-> Mem1
    Mem1 --> PCSplit
    
    CheckPC -- "是 (大对象)" --> SysAllocK
    SysAllocK -. "mmap" .-> Mem2
    Mem2 --> PCUnlock2
    PCUnlock2 --> Return

    %% ================= 5. 样式配置 =================
    classDef fastPath fill:#d4edda,stroke:#28a745,stroke-width:2px;
    classDef sysNode fill:#f8d7da,stroke:#dc3545,stroke-width:2px;
    classDef warnNode fill:#fff3cd,stroke:#ffc107,stroke-width:2px;
    class CheckTC,TCPop,Return fastPath;
```



### 2. 内存释放逻辑 `ConcurrentFree(ptr)`

```mermaid
flowchart TD
    %% ================= 1. 全局节点提前声明 (防拉伸) =================
    Start(["ConcurrentFree(ptr)"])
    Return(["释放结束"])
    
    %% 映射阶段 (利用基数树的 Lock-Free 特性)
    FindSpan["获取 ptr 所属 Span 与 size<br>(基数树无锁查询)"]
    SizeCheck{"size > 256KB?"}

    %% ================= 2. 线程私有层 (保持独立) =================
    subgraph TC [1. ThreadCache 层]
        TCPush["将 ptr 头插法挂回对应自由链表"]
        CheckTCLen{"桶内元素数量<br>>= 批量上限?"}
        PopBatch["ListTooLong()<br>截取一段链表准备还给 CC"]
    end

    %% ================= 3. 全局共享层 & 系统底层 (深度嵌套) =================
    subgraph CC [2. CentralCache 层 / 全局共享]
        CCLock{"🔒加桶锁 mtx.lock()<br>遍历 block 挂回各自的 Span<br>Span->_usecount 减 1"}
        CheckUseCount{"该 Span 的<br>_usecount == 0 ?"}
        CCUnlock1["🔓解桶锁 mtx.unlock()"]
        ExtractSpan["将归零 Span 移入局部链表 emptySpans<br>🔓提前解开桶锁 mtx.unlock() (防死锁!)"]:::warnNode

        %% 【深度嵌套 1】：PageCache 属于 CentralCache 的底层仓库
        subgraph PC [3. PageCache 核心合并调度]
            PCLock["🔒加全局大锁 _pageMtx.lock()"]
            SetUnuse["状态机: 标记 Span->_isUse = false"]
            Merge["循环前后探测相邻物理页 (仅探测 _isUse==false 的页)<br>不断合并出更大的连续 Span"]
            CheckBig{"合并后的页数<br>> 128页 ?"}
            PushPCBucket["将合并后的 Span 挂入对应哈希桶<br>🔓解全局大锁 _pageMtx.unlock()"]
            ReturnOS["🔓解全局大锁 _pageMtx.unlock()<br>触发系统回收"]

            %% 【深度嵌套 2】：OS 属于 PageCache 独占的系统资源
            subgraph OS [4. Operating System 底层回收]
                MemSystem[(系统物理内存)]:::sysNode
            end
        end
    end

    %% ================= 4. 集中连线 =================
    Start --> FindSpan
    FindSpan --> SizeCheck
    
    %% TC 内部与跨界调用
    SizeCheck -- "否 (≤256KB)" --> TCPush
    TCPush --> CheckTCLen
    CheckTCLen -- "否 (Fast Path)" --> Return
    CheckTCLen -- "是 (Slow Path)" --> PopBatch
    
    %% CC 内部跨界调用
    PopBatch --> CCLock
    CCLock --> CheckUseCount
    CheckUseCount -- "否" --> CCUnlock1
    CCUnlock1 --> Return
    CheckUseCount -- "是" --> ExtractSpan
    
    %% PC 内部核心逻辑 (包含大对象直通)
    SizeCheck -- "是 (大对象直通)" --> PCLock
    ExtractSpan --> PCLock
    PCLock --> SetUnuse
    SetUnuse --> Merge
    Merge --> CheckBig
    CheckBig -- "否" --> PushPCBucket
    PushPCBucket --> Return
    CheckBig -- "是" --> ReturnOS
    
    %% OS 跨层回收 (被完美限制在 PC 内部)
    ReturnOS -. "SystemFree (munmap / VirtualFree)" .-> MemSystem
    MemSystem --> Return

    %% ================= 5. 样式配置 =================
    classDef fastPath fill:#d4edda,stroke:#28a745,stroke-width:2px;
    classDef sysNode fill:#f8d7da,stroke:#dc3545,stroke-width:2px;
    classDef warnNode fill:#fff3cd,stroke:#ffc107,stroke-width:2px;
    class FindSpan,SizeCheck,TCPush,CheckTCLen,Return fastPath;
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


## 五、 基准测试 (Benchmarks)

在多线程环境下运行了 `test/benchmark.cpp` 进行基准测试（测试环境已开启 CMake Release 模式及 `-O3` 优化）。

* **测试条件**：8 线程并发，执行 1000 轮次，每轮次动态分配并释放 10000 次大小不一的内存块（模拟内存碎片化场景），总计发生 80,000,000 次分配与释放。

| 分配器类型 | 分配 (Alloc) 耗时 | 释放 (Dealloc) 耗时 | 多线程内部总计耗时 | 挂钟时间 |
| :--- | :---: | :---: | :---: | :---: |
| 系统原生 `malloc/free` | ~3872 ms | ~5543 ms | ~9415 ms | ~1207 ms |
| 本项目 `ConcurrentAlloc` | **~1185 ms** | **~930 ms** | **~2115 ms** | **~270 ms** |

*(注：以上数据受不同 CPU 架构、操作系统缓存状态及编译器优化程度影响，仅供参考。)*

测试结果表明，在持续的高并发小内存分配场景下，该缓存架构能有效减少线程间的阻塞等待，性能相较于原生实现有较明显的改善。


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
