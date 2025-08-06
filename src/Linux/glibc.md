<!--toc:start-->
- [Glibc](#glibc)
  - [Hook机制](#hook机制)
  - [常见用途](#常见用途)
- [malloc](#malloc)
  - [prerequisite](#prerequisite)
    - [mmap相关系统调用](#mmap相关系统调用)
    - [malloc实现方案](#malloc实现方案)
      - [桶](#桶)
    - [内存分配流程](#内存分配流程)
- [jemalloc相关](#jemalloc相关)
  - [前情回顾之glibc ptmalloc](#前情回顾之glibc-ptmalloc)
    - [多线程支持](#多线程支持)
    - [内存管理](#内存管理)
    - [ptmalloc的缺陷](#ptmalloc的缺陷)
  - [jemalloc实现](#jemalloc实现)
  - [与ptmalloc的比较](#与ptmalloc的比较)
  - [内存碎片处理](#内存碎片处理)
  - [多线程支持](#多线程支持)
  - [总结](#总结)
- [Glibc 实现分析](#glibc-实现分析)
  - [syscall-template.S](#syscall-templates)
- [C封装](#c封装)
  - [INLINE_SYSCALL](#inlinesyscall)
- [文件相关](#文件相关)
  - [文件权限](#文件权限)
  - [修改文件用户ID组和组用户ID](#修改文件用户id组和组用户id)
- [文件系统系统调用](#文件系统系统调用)
- [未实现的系统调用](#未实现的系统调用)
- [分类](#分类)
  - [ptrace](#ptrace)
  - [acct](#acct)
  - [uname](#uname)
  - [sethostname](#sethostname)
  - [swapon](#swapon)
  - [reboot](#reboot)
  - [ioperm](#ioperm)
  - [iopl](#iopl)
  - [vm86(这里考虑的是x86_64平台)](#vm86这里考虑的是x8664平台)
<!--toc:end-->

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

#### 桶

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

# jemalloc相关

## 前情回顾之glibc ptmalloc

### 多线程支持


1. Ptmalloc2有一个主分配区(main arena)， 有多个非主分配区。 非主分配区只能使用mmap向操作系统批发申请HEAP_MAX_SIZE（64位系统为64MB）大小的虚拟内存。 当某个线程调用malloc的时候，会先查看线程私有变量中是否已经存在一个分配区，如果存在则尝试加锁，如果加锁失败则遍历arena链表试图获取一个没加锁的arena， 如果依然获取不到则创建一个新的非主分配区。
2. free()的时候也要获取锁。分配小块内存容易产生碎片，ptmalloc在整理合并的时候也要对arena做加锁操作。在线程多的时候，锁的开销就会增大。

### 内存管理

用户请求分配的内存在ptmalloc中使用chunk表示， 每个chunk至少需要8个字节额外的开销。 用户free掉的内存不会马上归还操作系统，ptmalloc会统一管理heap和mmap区域的空闲chunk，避免了频繁的系统调用。

ptmalloc 将相似大小的 chunk 用双向链表链接起来, 这样的一个链表被称为一个 bin。Ptmalloc 一共 维护了 128 个 bin,并使用一个数组来存储这些 bin(图就像二维数组，或者std::deque底层实现那种感觉，rocksdb arena实现也这样的)

### ptmalloc的缺陷


1. 后分配的内存先释放,因为 ptmalloc 收缩内存是从 top chunk 开始,如果与 top chunk 相邻的 chunk 不能释放, top chunk 以下的 chunk 都无法释放。
2. 多线程锁开销大， 需要避免多线程频繁分配释放。
3. 内存从thread的arena中分配， 内存不能从一个arena移动到另一个arena， 就是说如果多线程使用内存不均衡，容易导致内存的浪费。 比如说线程1使用了300M内存，完成任务后glibc没有释放给操作系统，线程2开始创建了一个新的arena， 但是线程1的300M却不能用了。
4. 每个chunk至少8字节的开销很大
5. 不定期分配长生命周期的内存容易造成内存碎片，不利于回收。 64位系统最好分配32M以上内存，这是使用mmap的阈值。

这里的问题在于arena是全局的 jemalloc和tcmalloc都针对这个做优化

## jemalloc实现

Jemalloc 首先对大小内存块进行了明确的区分，通常以 3.5 个内存页（一般内存页大小为 4KB，不同系统可能有所差异）为默认阈值 。小于这个阈值的内存块被视为小内存块，大于这个阈值的则被视为大内存块。这种区分方式有助于 Jemalloc 针对不同大小的内存块采用不同的分配策略，从而减少内存碎片的产生。对于小内存块，Jemalloc 采用了精细的分级内存分配机制，将小内存块按照大小进一步划分为多个不同的级别，每个级别对应一个特定的内存大小范围 。比如，可能会有一个级别专门管理 8 字节大小的内存块，另一个级别管理 16 字节大小的内存块等等。每个级别都有自己的空闲列表，这样在分配小内存块时，Jemalloc 可以快速地从对应的空闲列表中找到合适的内存块进行分配，大大提高了分配效率。

内存对齐是 Jemalloc 中另一个重要的策略。为了提高内存访问效率，Jemalloc 会对分配的内存块进行对齐操作 。内存对齐是指将内存块的起始地址调整为特定值的整数倍，常见的对齐值有 8 字节、16 字节等。这是因为 CPU 在访问内存时，通常是以特定的字节数为单位进行读取的，如果内存块的起始地址不对齐，可能会导致 CPU 需要多次读取才能获取完整的数据，从而降低了访问效率。例如，假设 CPU 每次读取 8 字节的数据，如果一个 4 字节的数据块起始地址不是 8 的倍数，那么 CPU 可能需要读取两次，先读取包含该数据块的前 8 字节，再读取后 8 字节，才能获取到完整的 4 字节数据。而如果数据块起始地址是 8 的倍数，CPU 一次就可以读取到完整的数据。Jemalloc 通过合理的内存对齐策略，确保了内存访问的高效性。

在内存页管理方面，Jemalloc 引入了 extent 的概念 。extent 是 arena 管理的内存对象，在大内存分配中充当 buddy 算法中的 chunk，在小内存分配中充当 slab。每个 extent 的大小是内存页的整数倍，不同 size 的 extent 会用 buddy 算法来进行拆分和合并 。当申请大内存时，如果没有合适大小的 extent，Jemalloc 会将较大的 extent 按照 buddy 算法进行分裂，直到得到合适大小的内存块；当释放大内存时，Jemalloc 会尝试将相邻的空闲 extent 合并成更大的 extent，以减少内存碎片 。对于小内存分配，Jemalloc 使用 slab 算法，将 extent 划分成大小相同的 slots，用 bitmap 来记录这些 slots 的空闲情况，内存分配时，返回一个 bitmap 中记录的空闲块，释放时，将其标记为忙碌 。这种结合 buddy 算法和 slab 算法的内存页管理方式，使得 Jemalloc 在大小内存分配上都能有效地减少内存碎片，提高内存利用率。

在多线程环境下，线程竞争是一个不可忽视的问题，它就像是多个工人同时争抢仓库里的资源，容易导致效率低下。Jemalloc 采用了一系列策略来处理线程竞争 。一方面，它为每个线程提供了独立的内存缓存（tcache），每个线程在进行内存分配时，首先会在自己的 tcache 中查找是否有可用的内存块 。如果 tcache 中有合适的内存块，就直接从 tcache 中分配，避免了与其他线程竞争全局内存资源，大大提高了分配速度。只有当 tcache 中没有可用内存块时，才会进入全局的内存分配流程 。另一方面，Jemalloc 使用多个 arena 来管理内存，每个 arena 独立管理一部分内存，线程通过某种映射关系（如线程号映射）选择对应的 arena 进行内存分配 。这样，多个线程竞争同一个 arena 的概率就会降低，从而减少了锁竞争，提高了并发性能。

当程序请求分配一个大小为 SIZE 的内存块时，Jemalloc 的分配流程如下：首先，选定一个 arena 或者 tcache 。如果请求的内存块大小不大于 arena 的最小的 bin（不同系统和配置下该值可能不同），那么优先通过线程对应的 tcache 来进行分配 。确定请求大小属于哪一个 tbin（tcache 中的 bin），查找 tbin 中是否有缓存的空间 。如果有，就直接进行分配；如果没有，则为这个 tbin 对应的 arena 的 bin 分配一个 run（run 是 chunk 里的一块区域，大小是 page 的整数倍），然后把 run 里面的部分块的地址依次赋给 tcache 的对应的 bin 的 avail 数组（相当于缓存了一部分内存块），最后从 avail 数组中选取一个地址进行分配 。如果请求 size 大于 arena 的最小的 bin，同时不大于 tcache 能缓存的最大块，也会通过线程对应的 tcache 来进行分配 。首先看 tcache 对应的 tbin 里有没有缓存块，如果有就分配，没有就从 chunk 里直接找一块相应的 page 整数倍大小的空间进行分配（当这块空间后续释放时，会进入相应的 tcache 对应的 tbin 里） 。如果请求 size 大于 tcache 能缓存的最大块，同时不大于 chunk 大小（默认是 4M），具体分配和第 2 类请求相同，区别只是没有使用 tcache 。如果请求大于 chunk 大小，直接通过 mmap 进行分配 。通过这样严谨而细致的分配流程，Jemalloc 能够高效地满足各种内存分配请求。

## 与ptmalloc的比较

1. 内存碎片控制

jemalloc 采用了低地址优先的内存分配策略，优先使用低地址的空闲内存块，从而降低内存碎片化的可能性。这种策略有助于提高内存的利用率，减少内存浪费。

2. 线程局部缓存（Thread-local Caching）

与 tcmalloc 类似，jemalloc 为每个线程提供了本地缓存，允许线程在无需加锁的情况下进行小对象的内存分配和释放。这种设计减少了线程间的锁竞争，提高了多线程程序的性能。

3. 内存分级管理

jemalloc 根据对象大小将内存分为不同的类别（size-class），并为每个类别维护独立的空闲列表。这种分级管理方式使得小对象的分配和释放更加高效，同时减少了内存碎片的产生。

4. 脏页回收策略

jemalloc 优先使用脏页（dirty page）进行内存分配，避免频繁地向操作系统申请新的内存页，从而提高了缓存命中率和内存分配效率。

5. 内存开销控制

jemalloc 的元数据开销约为 2%，相比之下，ptmalloc 的每个内存块至少需要 8 字节的开销。通过精细的内存管理，jemalloc 在保持高性能的同时，控制了额外的内存开销。

## 内存碎片处理

Jemalloc 在内存碎片处理上表现出色。它将内存块按大小分级，每个级别有自己的空闲列表，这种分级内存分配方式可以减少不同大小对象混合产生的碎片 。Jemalloc 还提供了 mallocx 和 rallocx 等函数，可以在内存重新分配时选择更合适的内存块，以减少碎片 。此外，Jemalloc 使用背景线程定期扫描和整理内存碎片，这个线程会将长时间未使用的内存块释放回操作系统，有效减少了碎片 。并且，Jemalloc 对内存块进行对齐操作，采用分块的方式管理小对象，每个块的大小是 2 的幂次，进一步减少了内存碎片的产生 。

## 多线程支持

在多线程支持方面，ptmalloc 最初的设计对多线程并不友好，只有一个主分配区，多线程环境下对主分配区的锁争用非常激烈，严重影响了 malloc 的分配效率 。后来虽然增加了动态分配区，每个分配区利用互斥锁使线程对于该分配区的访问互斥，并且根据系统对分配区的争用情况动态增加动态分配区的数量，但分配区的数量一旦增加就不会再减少，而且多线程之间内存无法实现共享，只能每个线程都独立使用各自的内存，这在内存开销上是有很大浪费的 。

tcmalloc 则是专门为多线程并发的内存管理而设计的 。它的最大特点是带有线程缓存，为每个线程分配了一个局部缓存，对于小对象的分配，可以直接由线程局部缓存来完成，实现了无锁内存分配，大大提高了多线程环境下小对象分配的效率 。对于大对象的分配场景，tcmalloc 尝试采用自旋锁来减少多线程的锁竞争问题，虽然在一定程度上缓解了锁竞争，但在锁冲突严重时，仍然会出现 CPU 飙升的问题 。

Jemalloc 同样对多线程环境有着良好的支持 。它借鉴了 tcmalloc 的线程缓存设计，为每个线程提供独立的内存缓存（tcache），线程优先从自己的 tcache 中分配内存，减少了线程间的锁竞争 。同时，Jemalloc 使用多个 arena 来管理内存，线程通过某种映射关系选择对应的 arena 进行内存分配，降低了多个线程竞争同一个 arena 的概率，进一步减少了锁竞争，提高了多线程环境下的内存分配效率 。在多核多线程的场景下，Jemalloc 能够充分发挥其优势，减少锁竞争带来的性能损耗，提升程序的整体性能 。

## 总结

1. 多 Arena 设计：jemalloc 为不同的线程分配多个独立的 Arena，每个 Arena 管理自己的内存池，减少线程间的锁竞争，提高并发性能。

2. 内存分级管理：根据对象大小，将内存划分为不同的大小类别（Size Classes），并为每个类别维护独立的空闲列表。这种分级管理方式使得小对象的分配和释放更加高效，同时减少了内存碎片的产生。

3. 线程缓存（tcache）：jemalloc 为每个线程提供本地缓存（tcache），用于处理小对象的分配和释放，避免频繁访问全局数据结构，进一步降低锁竞争，提高性能。

4. 内存回收策略：通过延迟回收和批量处理等策略，jemalloc 有效地减少了内存碎片，提高了内存利用率。

# Glibc 实现分析

glibc的大多数系统调用都是采用脚本封装的方式，脚本封装的规则很简单。三种文件生成封装代码。

一 make-syscall.sh文件

二 syscall-template.S文件

三 syscalls.list文件。

make-syscall.sh是shell脚本文件。它读取syscalls.list文件的内容，对文件的每一行进行解析。根据每一行的内容生成一个.S汇编文件，汇编文件封装了一个系统调用。

syscall-template.S是系统调用封装代码的模板文件。生成的.S汇编文件都调用它。

syscalls.list是数据文件，它的内容如下：

```txt
# File name Caller  Syscall name    Args    Strong name Weak names

accept      -   accept      Ci:iBN  __libc_accept   accept
access      -   access      i:si    __access    access
acct        -   acct        i:S acct
adjtime     -   adjtime     i:pp    __adjtime   adjtime
bind        -   bind        i:ipi   __bind      bind
chdir       -   chdir       i:s __chdir     chdir
```

它由许多行组成，每一行可分为6列。File name列指定生成的汇编文件的文件名。Caller指定调用者。Syscall name列指定系统调用的名称，系统调用名称可以转换为系统调用号以标示系统调用。Args列指定系统调用参数类型，个数及返回值类型。Strong name指定系统调用封装函数的函数名。Weak names列指定封装函数的别称，用户可以调用别称来调用封装函数。

make-syscall.sh分析syscalls.list每一行每一列的内容，生成汇编文件。以分析chdir行为例，生成的汇编文件内容为：

```c
#define SYSCALL_NAME chdir
#define SYSCALL_NARGS 1
#define SYSCALL_SYMBOL __chdir
#define SYSCALL_CANCELLABLE 0
#define SYSCALL_NOERRNO 0
#define SYSCALL_ERRVAL 0
#include <syscall-template.S>
weak_alias (__chdir, chdir)
hidden_weak (chdir)
```

SYSCALL_NAME宏定义了系统调用的名字。是从Syscall name列获取。

SYSCALL_NARGS宏定义了系统调用参数的个数。是通过解析Args列获取。

SYSCALL_SYMBOL宏定义了系统调用的函数名称。是从Strong name列获取。

SYSCALL_CANCELLABLE宏在生成的所有汇编文件中都定义为0。

SYSCALL_NOERRNO宏定义为1，则封装代码没有出错返回。用于getpid这些没有出错返回的系统调用。是通过解析Args列设置。

SYSCALL_ERRVAL宏定义为1，则封装代码直接返回错误号，不是返回-1并将错误号放入errno中。生成的所有.S文件中它都定义为0。

weak_alias (__chdir, chdir)定义了__chdir函数的别称，我们可以调用chdir来调用__chdir。 chdir从Weak names列获取。

汇编文件中引用了模板文件syscall-template.S，所有的封装代码都集中在syscall-template.S文件中。

3种文件，make-syscall.sh文件在sysdeps/unix/make-syscall.sh。syscall-template.S文件在sysdeps/unix/syscall-template.S。syscalls.list文件则有多个，分别在sysdeps/unix/syscalls.list，sysdeps/unix/sysv/linux/syscalls.list，sysdeps/unix/sysv/linux/generic/syscalls.list，sysdeps/unix/sysv/linux/i386/syscalls.list。

## syscall-template.S

syscall-template.S作为模板文件，包含了所有封装代码。

```c
#if SYSCALL_CANCELLABLE
# include <sysdep-cancel.h>
#else
# include <sysdep.h>
#endif

#define syscall_hidden_def(SYMBOL)      hidden_def (SYMBOL)

#define T_PSEUDO(SYMBOL, NAME, N)       PSEUDO (SYMBOL, NAME, N)
#define T_PSEUDO_NOERRNO(SYMBOL, NAME, N)   PSEUDO_NOERRNO (SYMBOL, NAME, N)
#define T_PSEUDO_ERRVAL(SYMBOL, NAME, N)    PSEUDO_ERRVAL (SYMBOL, NAME, N)
#define T_PSEUDO_END(SYMBOL)            PSEUDO_END (SYMBOL)
#define T_PSEUDO_END_NOERRNO(SYMBOL)        PSEUDO_END_NOERRNO (SYMBOL)
#define T_PSEUDO_END_ERRVAL(SYMBOL)     PSEUDO_END_ERRVAL (SYMBOL)

#if SYSCALL_NOERRNO

T_PSEUDO_NOERRNO (SYSCALL_SYMBOL, SYSCALL_NAME, SYSCALL_NARGS)
    ret_NOERRNO
T_PSEUDO_END_NOERRNO (SYSCALL_SYMBOL)

#elif SYSCALL_ERRVAL

T_PSEUDO_ERRVAL (SYSCALL_SYMBOL, SYSCALL_NAME, SYSCALL_NARGS)
    ret_ERRVAL
T_PSEUDO_END_ERRVAL (SYSCALL_SYMBOL)

#else

T_PSEUDO (SYSCALL_SYMBOL, SYSCALL_NAME, SYSCALL_NARGS)
    ret
T_PSEUDO_END (SYSCALL_SYMBOL)

#endif

syscall_hidden_def (SYSCALL_SYMBOL)
```

文件开头引入.h文件。如果SYSCALL_CANCELLABLE宏定义为1，则引入<sysdep-cancel.h>文件，否则引入<sysdep.h>文件。SYSCALL_CANCELLABLE宏在所有生成的汇编文件中都定义为0，所以汇编文件都是引用<sysdep.h>文件。<sysdep.h>文件位于sysdeps/unix/sysv/linux/i386/sysdep.h

```c
#if SYSCALL_CANCELLABLE
# include <sysdep-cancel.h>
#else
# include <sysdep.h>
#endif
```

系统调用的封装代码由3种形式。

如果系统调用没有错误返回，则执行

```c
T_PSEUDO_NOERRNO (SYSCALL_SYMBOL, SYSCALL_NAME, SYSCALL_NARGS)
    ret_NOERRNO
T_PSEUDO_END_NOERRNO (SYSCALL_SYMBOL)
```

如果系统调用有错误返回且直接返回错误，则执行

```c
T_PSEUDO_ERRVAL (SYSCALL_SYMBOL, SYSCALL_NAME, SYSCALL_NARGS)
    ret_ERRVAL
T_PSEUDO_END_ERRVAL (SYSCALL_SYMBOL)
```

如果系统调用有错误返回且返回-1，errno设置错误号，则执行

```c
T_PSEUDO (SYSCALL_SYMBOL, SYSCALL_NAME, SYSCALL_NARGS)
    ret
T_PSEUDO_END (SYSCALL_SYMBOL)
```

# C封装

.c封装都是借助嵌入式汇编，按照系统调用的封装规则进行封装的。

可以查看stat64函数的实现，来探究.c封装。

```c
#undef stat64
int
attribute_hidden
stat64 (const char *file, struct stat64 *buf)
{
  return __xstat64 (_STAT_VER, file, buf);
}


int
___xstat64 (int vers, const char *name, struct stat64 *buf)
{
  int result;
  result = INLINE_SYSCALL (stat64, 2, name, buf);
  return result;
}
```

源码中使用了INLINE_SYSCALL 宏封装了stat64系统调用。

## INLINE_SYSCALL

```c
# define INLINE_SYSCALL(name, nr, args...) \
  ({									      \
    unsigned int resultvar = INTERNAL_SYSCALL (name, , nr, args);	      \
    __glibc_unlikely (INTERNAL_SYSCALL_ERROR_P (resultvar, ))		      \
    ? __syscall_error (-INTERNAL_SYSCALL_ERRNO (resultvar, ))		      \
    : (int) resultvar; })
```

INLINE_SYSCALL宏首先执行INTERNAL_SYSCALL宏。INTERNAL_SYSCALL宏调用了系统调用，并将返回值放入resultvar中。然后判断系统调用是否执行错误。

```c
#undef INTERNAL_SYSCALL_ERROR_P
#define INTERNAL_SYSCALL_ERROR_P(val, err) \
  ((unsigned int) (val) >= 0xfffff001u)

#define __glibc_unlikely（cond）（cond）
```

如果执行错误，则调用__syscall_error 设置errno，并返回-1。如果执行成功，则返回resultvar。

# 文件相关

linux支持7种文件：普通文件，目录文件，字符设备文件，块设备文件，管道文件，套接字文件，符号链接文件。每种文件的创建与删除都有对应系统调用。glibc封装了这些系统调用。创建与删除文件的系统调用有：creat，unlink，mkdir，rmdir，mknod，symlink，link。与创建与删除文件的系统调用有关的系统调用有：umask。

其中unlink,mkdir,rmdir,symlink,link,umask是用脚本生成的。

unlink系统调用的封装代码

```c
#define SYSCALL_NAME unlink
#define SYSCALL_NARGS 1
#define SYSCALL_SYMBOL __unlink
#define SYSCALL_CANCELLABLE 0
#define SYSCALL_NOERRNO 0
#define SYSCALL_ERRVAL 0
#include <syscall-template.S>
weak_alias (__unlink, unlink)
hidden_weak (unlink)
```

mkdir的封装代码

```c
#define SYSCALL_NAME mkdir
#define SYSCALL_NARGS 2
#define SYSCALL_SYMBOL __mkdir
#define SYSCALL_CANCELLABLE 0
#define SYSCALL_NOERRNO 0
#define SYSCALL_ERRVAL 0
#include <syscall-template.S>
weak_alias (__mkdir, mkdir)
hidden_weak (mkdir)
```

__lstat函数调用了__lxstat函数。__lxstat函数使用INTERNAL_SYSCALL宏调用了lstat64系统调用。如果调用成功则转化stat64结构数据为stat结构数据。

__fstat函数调用了__fxstat函数。__fxstat函数使用INTERNAL_SYSCALL宏调用了fstat64系统调用。如果调用成功则转化stat64结构数据为stat结构数据。

## 文件权限

linux中关于修改文件权限的系统调用有2个，它们分别是：chmod(15)，fchmod(94)。glibc封装了这两个系统调用。它们都是使用脚本封装的。

## 修改文件用户ID组和组用户ID

linux中关于chown的系统调用有6个，它们分别是lchown（16），fchown（95），chown（182），lchown32（198），fchown32（207），chown32（212）。它们都是用于改变文件的用户ID和组用户ID。chown系列系统调用只能将用户ID和组用户ID改为16位整数。chown32系列系统调用则能将用户ID和组用户ID改为32位整数。glibc封装了3个系统调用，分别封装为chown函数，fchown函数，lchown函数。它们都是通过脚本封装的。

# 文件系统系统调用

linux中rename系统调用可以修改文件名。chdir，fchdir系统调用可以更改用户的工作目录。getcwd系统调用可以获取用户的工作目录。chroot系统调用可以更改用户的根目录。

```c
int rename (const char *old, const char *new)
{
  return INLINE_SYSCALL_CALL (rename, old, new);
}
```

getcwd函数比较特别，它改变了getcwd系统调用的语义。这使得getcwd函数比其他函数更复杂。

```c
char *
__getcwd (char *buf, size_t size)
{
  char *path;
  char *result;
  //确定缓冲区的长度
  size_t alloc_size = size;
  if (size == 0)
    {
      if (buf != NULL)
    {
      __set_errno (EINVAL);
      return NULL;
    }
      alloc_size = MAX (PATH_MAX, __getpagesize ());
    }
  //申请缓冲区
  if (buf == NULL)
    {
      path = malloc (alloc_size);
      if (path == NULL)
        return NULL;
    }
  else
    path = buf;
  int retval;
  //调用系统调用获取当前工作目录
  retval = INLINE_SYSCALL (getcwd, 2, path, alloc_size);
  //获取成功
  if (retval >= 0)
    {
      //重新设置缓冲区
      if (buf == NULL && size == 0)
          buf = realloc (path, (size_t) retval);
      if (buf == NULL)
        buf = path;
      return buf;
    }
   //如果出错了，且出错原因是文件路径太长，则执行generic_getcwd获取当前文件路径。
   //generic_getcwd是内部函数，源码不可见。
  if (errno == ENAMETOOLONG)
    {
      if (buf == NULL && size == 0)
      {
          free (path);
          path = NULL;
      }
      result = generic_getcwd (path, size);
      if (result == NULL && buf == NULL && size != 0)
           free (path);
      return result;
    }
  assert (errno != ERANGE || buf != NULL || size != 0);

  if (buf == NULL)
    free (path);

  return NULL;
}
weak_alias (__getcwd, getcwd)
```

# 未实现的系统调用

glibc实现了大量的系统调用。从文件系统系统调用的open，read，write...到进程系统调用的getpid，fork，exit...。我们都可以简单的调用相应的函数来调用系统调用。但是，glibc并未实现所有的系统调用，那么我们怎么调用未实现的系统调用呢？

glibc提供了函数：

```c
long int syscall (long int callno, ...)
```

参数callno传入系统调用号，后面的参数依次传入系统调用对应的参数。

可以查看syscall函数的源码。源码在sysdeps/unix/sysv/linux/i386/syscall.S文件中。

# 分类

glibc实现了大量的系统调用，按照功能分类：

## ptrace

ptrace系统调用用于调试程序。glibc中ptrace系统调用使用.c文件封装。

```c
long int
ptrace (enum __ptrace_request request, ...)
{
  long int res, ret;
  va_list ap;
  pid_t pid;
  void *addr, *data;

  va_start (ap, request);
  pid = va_arg (ap, pid_t);
  addr = va_arg (ap, void *);
  data = va_arg (ap, void *);
  va_end (ap);

  if (request > 0 && request < 4)
    data = &ret;

  res = INLINE_SYSCALL (ptrace, 4, request, pid, addr, data);
  if (res >= 0 && request > 0 && request < 4)
    {
      __set_errno (0);
      return ret;
    }

  return res;
}
```

## acct

acct系统调用用于开启进程记账功能。glibc中acct系统调用使用脚本文件封装。

## uname

uname系统调用可以获取操作系统的名称，版本信息。glibc中uname系统调用使用脚本封装。

## sethostname

sethostname系统调用用于设置主机名。glibc中sethostname系统调用使用脚本封装。

## swapon

swapon系统调用用于启动系统交换区。glibc中swapon系统调用使用脚本封装。

## reboot

reboot系统调用用于系统关机重启。glibc中reboot系统调用使用.c文件封装。

## ioperm

ioperm系统调用用于设置端口的权限。glibc中ioperm系统调用使用脚本封装。

## iopl

iopl系统调用用于设置端口权限。glibc中iopl系统调用使用脚本封装。

## vm86(这里考虑的是x86_64平台)

vm86系统调用用于进入虚拟8086模式。glibc中vm86系统调用使用脚本封装。
