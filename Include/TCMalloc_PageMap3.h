//
// Created by CAO on 2026/2/20.
//

#ifndef MEMORYPOOL_TCMALLOC_PAGE3_H
#define MEMORYPOOL_TCMALLOC_PAGE3_H

#endif //MEMORYPOOL_TCMALLOC_PAGE3_H

#pragma once
#include "Common.h"
#include "ObjectPool.h"

// 针对 64 位系统，48位虚拟地址空间，8KB(13bits)一页
// PageID 共有 48 - 13 = 35 bits
// 我们将其拆分为三层目录：15 bits, 10 bits, 10 bits
// BITS = 35
template<int BITS>
class TCMalloc_PageMap3
{
private:
    static const int INTERIOR_BITS = (BITS + 2) / 3; // 12
    static const int INTERIOR_BITS2 = (BITS + 1) / 3; // 12
    static const int LEAF_BITS = BITS - INTERIOR_BITS - INTERIOR_BITS2; // 11

    // 第一层：顶层目录长度
    static const int ROOT_LENGTH = 1 << INTERIOR_BITS;
    // 第二层：中间目录长度
    static const int INTERIOR_LENGTH = 1 << INTERIOR_BITS2;
    // 第三层：叶子节点（真正存 Span* 的数组）长度
    static const int LEAF_LENGTH = 1 << LEAF_BITS;

    // 叶子节点结构
    struct Leaf
    {
        void* values[LEAF_LENGTH];
    };

    // 中间节点结构
    struct Node
    {
        Leaf* leafs[INTERIOR_LENGTH];
    };

    // 顶层目录数组
    Node* root_[ROOT_LENGTH];

    ObjectPool<Leaf> _leafPool;
    ObjectPool<Node> _nodePool;

public:
    TCMalloc_PageMap3()
    {
        // 只初始化第一层
        memset(root_, 0, sizeof(root_));
    }

    // 确保某个 PageID 对应的层级结构已经被开辟（需要加全局锁调用）
    void Ensure(size_t pageId)
    {
        // 计算每一层的索引
        const size_t i1 = pageId >> (LEAF_BITS + INTERIOR_BITS2);
        const size_t i2 = (pageId >> LEAF_BITS) & (INTERIOR_LENGTH - 1);

        // 如果第一层对应的中间节点不存在，开辟它
        if (root_[i1] == nullptr)
        {
            // 注意：如果在内核/极底层的内存池开发中，不能用 new，需要用 SystemAlloc
            // Node *n = new Node;
            Node* n = _nodePool.New();
            memset(n, 0, sizeof(*n));
            root_[i1] = n;
        }

        // 如果第二层对应的叶子节点不存在，开辟它
        if (root_[i1]->leafs[i2] == nullptr)
        {
            // Leaf *l = new Leaf;
            Leaf* l= _leafPool.New();
            memset(l, 0, sizeof(*l));
            root_[i1]->leafs[i2] = l;
        }
    }

    // 建立映射关系（写入 Span*），调用前确保该 pageId 的结构已被 Ensure 过
    // 在 PageCache::NewSpan 和 PageCache::ReleaseSpanToPageCache（合并span）时调用
    void set(size_t pageId, void *span)
    {
        Ensure(pageId); // 保底检查
        const size_t i1 = pageId >> (LEAF_BITS + INTERIOR_BITS2); // 第一层地址
        const size_t i2 = (pageId >> LEAF_BITS) & (INTERIOR_LENGTH - 1); // 第二层地址
        const size_t i3 = pageId & (LEAF_LENGTH - 1); //

        root_[i1]->leafs[i2]->values[i3] = span;
    }

    // 获取映射关系（读取 Span*）这个函数是完全无锁的
    void *get(size_t pageId)
    {
        const size_t i1 = pageId >> (LEAF_BITS + INTERIOR_BITS2);
        const size_t i2 = (pageId >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
        const size_t i3 = pageId & (LEAF_LENGTH - 1);

        // 只要树干被建立过了，读取就是原子的
        if (root_[i1] == nullptr || root_[i1]->leafs[i2] == nullptr)
        {
            return nullptr;
        }
        return root_[i1]->leafs[i2]->values[i3];
    }
};
