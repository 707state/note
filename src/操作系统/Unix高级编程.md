<!--toc:start-->
- [操作系统](#操作系统)
  - [Domain Socket](#domain-socket)
  - [Native Async I/O](#native-async-io)
    - [与POSIX AIO的区别](#与posix-aio的区别)
- [C库](#c库)
  - [__builtin_expect](#builtinexpect)
  - [__builtin_popcount](#builtinpopcount)
    - [C++](#c)
  - [__builtin_clz](#builtinclz)
  - [std::__lg函数](#stdlg函数)
- [通用工具](#通用工具)
  - [autoconf](#autoconf)
  - [qwt](#qwt)
- [slab allocator](#slab-allocator)
  - [Slub allocator](#slub-allocator)
<!--toc:end-->


# 操作系统

## Domain Socket
Unix domain socket 或者 IPC socket是一种终端，可以使同一台操作系统上的两个或多个进程进行数据通信。与管道相比，Unix domain sockets 既可以使用字节流，又可以使用数据队列，而管道通信则只能使用字节流。Unix domain sockets的接口和Internet socket很像，但它不使用网络底层协议来通信。Unix domain socket 的功能是POSIX操作系统里的一种组件。

Unix domain sockets 使用系统文件的地址来作为自己的身份。它可以被系统进程引用。所以两个进程可以同时打开一个Unix domain sockets来进行通信。不过这种通信方式是发生在系统内核里而不会在网络里传播。

## Native Async I/O
这个东西不是POSIX AIO。是标准C库的一部分。

三个主要的函数

```cpp
long io_setup(unsigned int nr_events,aio_context_t *ctx_idp);
int io_submit(aio_context_t ctx_id, long nr, struct iocb **iocbpp);
int syscall(SYS_io_getevents, aio_context_t ctx_id,
                   long min_nr, long nr, struct io_event *events,
                   struct timespec *timeout);
```

### 与POSIX AIO的区别
> POSIX AIO is a user-level implementation that performs normal blocking I/O in multiple threads, hence giving the illusion that the I/Os are asynchronous.

可以看出POSIX AIO并不是真正意义上的异步，而是多线程+阻塞IO。

好处是：
1. 兼容所有的文件系统。
2. 跨平台(glibc是跨平台的)
3. 可以和开启缓冲的文件使用(no O_DIRECT)

> The kernel AIO (i.e. io_submit() et.al.) is kernel support for asynchronous I/O operations, where the io requests are actually queued up in the kernel, sorted by whatever disk scheduler you have, presumably some of them are forwarded (in somewhat optimal order one would hope) to the actual disk as asynchronous operations (using TCQ or NCQ).

Linux AIO是Kernel AIO。

主要的限制是不能在任意的文件系统使用，用O_DIRECT打开的文件有可能会变成阻塞操作。


# C库

## __builtin_expect
```cpp
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
```
这是常见于各种项目中的一个宏定义，作用是分支预测优化。

其中__builtin_expect是一个GCC的函数，
```c
long __builtin_expect(long exp, long c);
```
你期望 exp 表达式的值等于常量 c, 看 c 的值, 如果 c 的值为0(即期望的函数返回值), 那么 执行 if 分支的的可能性小, 否则执行 else 分支的可能性小(函数的返回值等于第一个参数 exp)。

GCC在编译过程中，会将可能性更大的代码紧跟着前面的代码，从而减少指令跳转带来的性能上的下降, 达到优化程序的目的。

返回值为第一个参数exp。

使用时，要与if关键字一起使用。

## __builtin_popcount

返回输入的数据中二进制1的个数

### C++
在C++20中，有了两个attribute: \[\[likely\]\]和 \[\[unlinkely\]\]，放置在if后面的block前面。作用是相同的。


## __builtin_clz

```c
int __builtin_clz(unsigned int x);
```
返回值是X的前导0的数量，x为0时返回值是未定义的。

## std::__lg函数

當我們想快速知道一個整數最前面的 bit 是第幾個的時候，就可以用這個函數。

# 通用工具

## autoconf
用于在GNU/Linux环境下制作编译，打包，安装的配置脚本的工具。通过从模板中生成Makefiule与头文件等从而调整软件包。

autoconf通过检查特性而不是软件版本来确保可移植性。

## qwt
一个绘图库，用来绘制2D图形。

# slab allocator

Slab Allocator 是一种内存管理技术，主要用于分配和管理小块固定大小的内存对象，通常用于操作系统内核中来高效管理内存。这种分配器通过将内存划分为一系列称为“slabs”的固定大小块来减少内存碎片和提升分配效率。每个 slab 包含若干个相同大小的对象，专门用于分配特定类型的对象。

Slab：Slab 表示一块连续的内存，通常由几个虚拟连续的页面组成。Slab 是与包含缓存的特定类型的对象相关的数据的实际容器。

缓存：缓存代表少量非常快的内存。缓存是特定类型对象的存储，例如信号量、进程 描述符、文件对象等。

## Slub allocator

Slub是一种内存管理机制，旨在高效分配内核对象的内存，它具有消除分配和释放造成的碎片化的理想特性。该技术用于保留包含特定类型数据对象的已分配内存，以便在后续分配相同类型的对象时重用。
