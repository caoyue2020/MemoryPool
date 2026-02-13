### 使用char*作为内存池的指针
`char* _memory = nullptr; `

这里不使用`void*`，将 `_memory` 定义为 `char*`，当需要移动 $n$ 个字节时，直接 `_memory += n` 即可，逻辑非常直观。
![alt text](ObjectPool/image.png)

### 如何分配内存
```cpp
//将原本内存池首地址给obj，并将其转为T*指针
//即告诉编译器起始地址为_memory
//长度为sizeof(T)
obj = (T*)_memory; 
_memory += sizeof(T); //内存池指针移动
_remanenetBytes -= sizeof(T);//减少对应可用内存字节
```
指针的存储的是内存块的首地址，而指针的类型决定了编译器从首地址往后读取多少个字节。
![alt text](ObjectPool/image-2.png)


### 自由链表回收内存
```cpp
void Delete(T* obj)
    { 
        /*        
        在obj指向的内存中，写入一个指针即_freelist
        问题在于obj指向的内存块存放的是T类型的，无法接受指针赋值
        因此这里将obj强制转换为**void(指针的指针)
        让编译器把 obj 指向的内存当作一个“存放指针的容器”
        */
        *(void**)obj = _freelist;
        _freelist = obj;
    }
```
![alt text](ObjectPool/image-1.png)

### 从自由链表分配内存
```cpp
if(_freelist)
{
    //_freelist不为空则从这里那内存
    //采用头删
    //转为指针的指针再解引用，即以_freelist为首地址读取一个指针，
    //该指针指向的是下一个内存块，将其赋给void* next
    void* next = *(void**)_freelist; 
    obj = (T*)_freelist;
    _freelist = next;
}
```
![alt text](ObjectPool/image-3.png)

### 定位new
```CPP
//除了分配空间，还得初始化
//定位new。详情见MD
new(obj)T;
```
普通的 new 会做两件事：1. 申请内存；2. 调用构造函数。
但在内存池中，内存是你提前申请好的，你只需要在已有的内存空间上初始化对象。
+ 语法:`new (指针) 类型`;

+ 作用:不分配内存，只在“指针”指向的地址处调用构造函数。

+ new(obj)T; 是完全正确的，它保证了 TreeNode 里的 _val 被设为 0，指针被设为 nullptr。


### 关于内存池（大内存块的释放）


```cpp
~ObjectPool()
{
    for (char* ptr : _M)
    {
        free(ptr);
    }
    
}
```
使用vector存放所有的内存池，`std::vector<char*> _M;`，`_M.push_back(_memory);`，在析构函数中循环释放