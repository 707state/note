# Glibc

## Hook机制

Glibc作为大多数Linux发行版所使用的c库，利用了一些GCC的特性，从而使得用户可以非常方便的使用。

Hook机制就是一个很常用的手段，常常用于程序需要管理/修改/访问其他进程。

## 常见用途

1. 与attribute使用
```c
__attribute__((constructor)) xxxxx
```

在这里通常作为在main被调用之前准备环境使用的。

2. 作为动态链接库

准确得说，这里叫做preload hook，通过LD_PRELOAD的方式放置在终端的开始处。

也就是说，对于一个程序，可以用preload hook来进行运行期的链接，可以用来进行运行期的功能测试。

举一个例子：

```cpp main.c
int main(){
    const char a[]="hello\n";
    write(1,a,sizeof(a));
}
```
这里面调用的write是系统调用，但是，如果我们想要write能够被记录（比如说有安全/统计/其他需求），可以通过自己编写write（因为write是一个weak_alias）并用 LD\_PRELOAD的方式添加进来，这样就能够接管write函数。
比如：
```cpp write_hook.c
ssize_t write(int fd,const void* buf,size_t count){
    static ssize_t (*real_write)(int,const void*,size_t)=NULL;
    if(!real_write){
        real_write=(ssize_t(*)(int,const void*,size_t)) dlsym(RTLD_NEXT,\"write\" );
    }
    if(fd==STDOUT_FILENO){
        //拦截标准输出
        printf(xxx);
    }
    return real_write(fd,buf,count);
}
```

在dlsym中用write就可以获取程序加载的下一个动态符号_write_,这就是获取原始的write函数，也是实现hook的关键。

# malloc

## prerequisite

- 用户模式：（32位下）0-3G的内存区域

- 内核模式：（32位下）3-4G的内存区域

- 代码段：存放进程的可执行二进制、字符串字面量、只读变量。

- 数据段：存放已经初始化并且初始值非0的全局变量和局部静态变量。

- 堆：存放由用户动态分配内存存储的变量。

- 栈：局部变量、函数参数、返回值。

- 内存映射段（mmap）：内核将硬盘文件内容直接映射到内存里面，也可以让用户创建匿名内存映射，没有对应文件，可以存放程序数据。

mmap映射区向下拓展，堆向上拓展，两者相对拓展，直到耗尽虚拟地址空间的剩余空间。

### mmap相关系统调用

- brk和sbrk调用

要增加一个进程实际的可用堆大小，需要将break指针向高地址移动，这就需用brk和sbrk系统调用操作break指针。

```c
#include <unistd.h>
int brk(void *addr);
void *sbrk(intptr_t increment);
```
brk函数将break指针直接设置为某个地址，sbrk则是把break指针从当前位置移动到increment所指定的增量。

每个进程有一个rlimit结构体表示当前进程可用的资源上限。

这个限制可以用getrlimit系统调用获得。

申请小内存的时候，malloc使用sbrk分配内存；当申请大内存时，使用mmap函数申请内存，但是这只是分配了虚拟内存，还没有映射到物理内存，当访问申请的内存时，才会因为缺页异常，内核分配物理内存。

### malloc实现方案

如果每一次分配内存都要调用brk/sbrk/mmap, 就会带来非常庞大的系统调用开销；其次，这样做会导致内存容易产生碎片，因为堆是从低地址到高地址，如果低地址的内存不释放，高地址的内存就不能被回收。

鉴于此，malloc采用的是内存池的方案，类似于STL分配器和memcached的内存池，也就是先申请一大块内存，然后将内存分成不同大小的内存块。

malloc将内存分成了大小不同的chunk，然后通过bins来组织起来。malloc将相似大小的chunk用双向链表链接起来，这样一个链表被称为一个bin。malloc一共维护了128个bin，并使用一个数组来存储这些bin。数组中第一个为unsorted bin，数组编号前2到前64的bin为small bins，同一个small bin中的chunk具有相同的大小，两个相邻的small bin中的chunk大小相差8bytes。small bins后面的bin被称作large bins。large bins中的每一个bin分别包含了一个给定范围内的chunk，其中的chunk按大小序排列。large bin的每个bin相差64字节。

### 内存分配流程

1. 如果分配内存<512字节，则通过内存大小定位到smallbins对应的index上(floor(size/8))
- 如果smallbins[index]为空，进入步骤3
- 如果smallbins[index]非空，直接返回第一个chunk

2. 如果分配内存>512字节，则定位到largebins对应的index上
- 如果largebins[index]为空，进入步骤3
- 如果largebins[index]非空，扫描链表，找到第一个大小最合适的chunk，如size=12.5K，则使用chunk B，剩下的0.5k放入unsorted_list中

3. 遍历unsorted_list，查找合适size的chunk，如果找到则返回；否则，将这些chunk都归类放到smallbins和largebins里面
4. index++从更大的链表中查找，直到找到合适大小的chunk为止，找到后将chunk拆分，并将剩余的加入到unsorted_list中
5. 如果还没有找到，那么使用top chunk
6. 或者，内存<128k，使用brk；内存>128k，使用mmap获取新内存。


不是malloc后就马上占用实际内存，而是第一次使用时发现虚存对应的物理页面未分配，产生缺页中断，才真正分配物理页面，同时更新进程页面的映射关系。这也是Linux虚拟内存管理的核心概念之一。
