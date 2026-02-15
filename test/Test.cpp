#include <chrono>   // C++11 高精度计时库
#include <new>      // 必须包含，用于 placement new
#include "./Include/ObjectPool.h"



struct TreeNode
{
    int _val;
    TreeNode* _left;
    TreeNode* _right;

    TreeNode()
        :_val(0)
        , _left(nullptr)
        , _right(nullptr)
    {}
};

// 使用 chrono 库封装一个毫秒计时器
// 返回值单位：毫秒 (double 类型，保留小数)
double GetTimeMs(const std::chrono::steady_clock::time_point& start, 
                 const std::chrono::steady_clock::time_point& end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

void Benchmark()
{
    // 配置参数
    const size_t Rounds = 5;      // 跑几轮
    const size_t N = 100000;      // 每轮多少次
    
    std::vector<TreeNode*> v;
    v.reserve(N);

    cout << "=========================================" << endl;
    cout << "Benchmark: Rounds=" << Rounds << ", N=" << N << endl;
    cout << "-----------------------------------------" << endl;

    // 1. 测试原生 new/delete
    {
        // 这里的 dummy 变量是为了防止编译器把读取操作优化掉
        long long dummy = 0; 

        auto start = std::chrono::steady_clock::now();
        
        for (size_t j = 0; j < Rounds; ++j)
        {
            // 申请 + 写入
            for (int i = 0; i < N; ++i)
            {
                auto node = new TreeNode;
                node->_val = i; // 【关键】必须写入，模拟真实使用，防止被优化
                v.push_back(node);
            }
            
            // 读取 + 释放
            for (int i = 0; i < N; ++i)
            {
                dummy += v[i]->_val; // 【关键】必须读取
                delete v[i];
            }
            v.clear();
        }
        
        auto end = std::chrono::steady_clock::now();
        cout << "new/delete cost: \t" << GetTimeMs(start, end) << " ms" << endl;
        
        // 打印一下 dummy 防止它被彻底优化删除
        // (虽然大概率不会，但这是个好习惯)
        if (dummy == -1) cout << "ignore" << endl; 
    }

    // 2. 测试 ObjectPool
    {
        ObjectPool<TreeNode> TNPool;
        std::vector<TreeNode*> v2;
        v2.reserve(N);
        long long dummy = 0;

        auto start = std::chrono::steady_clock::now();

        for (size_t j = 0; j < Rounds; ++j)
        {
            // 申请 + 写入
            for (int i = 0; i < N; ++i)
            {
                TreeNode* node = TNPool.New();
                node->_val = i; // 【关键】写入
                v2.push_back(node);
            }

            // 读取 + 释放
            for (int i = 0; i < N; ++i)
            {
                dummy += v2[i]->_val; // 【关键】读取
                TNPool.Delete(v2[i]);
            }
            v2.clear();
        }

        auto end = std::chrono::steady_clock::now();
        cout << "ObjectPool cost: \t" << GetTimeMs(start, end) << " ms" << endl;
        
        if (dummy == -1) cout << "ignore" << endl;
    }
    cout << "=========================================" << endl;
}

int main()
{
    // 可以多跑几次取平均值
    Benchmark();
    return 0;
}





