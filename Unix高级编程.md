---
title: "域套接字"
author: "jask"
date: "2024-08-11"
output: pdf_document
header-includes:
  - \usepackage{xeCJK}
  - \setCJKmainfont{Noto Sans CJK SC}  # 替换为可用的字体
  - \setCJKmonofont{Noto Sans CJK SC}
  - \setCJKsansfont{Noto Sans CJK SC}
---
# 操作系统
Unix domain socket 或者 IPC socket是一种终端，可以使同一台操作系统上的两个或多个进程进行数据通信。与管道相比，Unix domain sockets 既可以使用字节流，又可以使用数据队列，而管道通信则只能使用字节流。Unix domain sockets的接口和Internet socket很像，但它不使用网络底层协议来通信。Unix domain socket 的功能是POSIX操作系统里的一种组件。

Unix domain sockets 使用系统文件的地址来作为自己的身份。它可以被系统进程引用。所以两个进程可以同时打开一个Unix domain sockets来进行通信。不过这种通信方式是发生在系统内核里而不会在网络里传播。 

# C库

## __builtin_expect
```c 
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

### C++
在C++20中，有了两个attribute: [[likely]]和[[unlinkely]]，放置在if后面的block前面。作用是相同的。



