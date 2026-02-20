//针对特定对象的定长内存池
//2026.2.11

#include "Common.h"

template<typename T>
class ObjectPool
{
private:
    char* _memory = nullptr; //指向当前可用的内存池的指针
    std::vector<char*> _M; //存放所有申请的内存池
    size_t _remanenetBytes = 0; //内存池剩余量
    void* _freelist = nullptr; //自由链表的头指针
    // 对齐一下，同时防止sizeof(T) < sizeof(void*)，以免切分的内存块无法放入指针
    size_t objSize = SizeClass::RoundUp(sizeof(T));

public:
    //申请一个T类型大小的空间
    T* New()
    {
        T* obj = nullptr;//返回的指针
        
        if(_freelist)
        {
            //从自由链表获取内存，见MD文档
            void* next = *(void**)_freelist; 
            obj = (T*)_freelist;
            _freelist = next;
        }
        else
        {
            if(_remanenetBytes < objSize)//判定空间是否足够
            { 
                _remanenetBytes = 128*1024;  //TODO:开128K。这里128可以考虑用常量表达式
                _memory = (char*)malloc(_remanenetBytes); //TODO:_memory原本剩余的空间呢？
                _M.push_back(_memory);  
                
                //malloc失败不会报错只会返回nullptr
                //故这里需要手动抛出
                if (_memory == nullptr) 
                {
                    throw std::bad_alloc();
                }
            }
    
            //如何分配内存，见MD
            obj = (T*)_memory;
            _memory += objSize; //内存池指针移动
            _remanenetBytes -= objSize;//减少对应可用内存字节
        }
        
        //除了分配空间，还得初始化
        //定位new。详情见MD
        new(obj)T;

        return obj; //返回指向内存的指针
    }

    void Delete(T* obj)
    { 
        
        //显示调用析构函数进行清理
        obj->~T();


        //自由链表回收内存，见MD
        *(void**)obj = _freelist;
        _freelist = obj;
    }

    ~ObjectPool()
    {
       for (char* ptr : _M)
       {
            free(ptr);
       }
    }
};