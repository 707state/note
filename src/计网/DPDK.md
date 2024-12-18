-   [Data Plane Development
    Kit](#data-plane-development-kit)
    -   [概述](#概述)
    -   [库](#库)

# Data Plane Development Kit

## 概述

在X86结构中，处理数据包的传统方式是CPU中断方式，即网卡驱动接收到数据包后通过中断通知CPU处理，然后由CPU拷贝数据并交给协议栈。在数据量大时，这种方式会产生大量CPU中断，导致CPU无法运行其他程序。

而DPDK则采用轮询方式实现数据包处理过程：DPDK重载了网卡驱动，该驱动在收到数据包后不中断通知CPU，而是将数据包通过零拷贝技术存入内存，这时应用层程序就可以通过DPDK提供的接口，直接从内存读取数据包。

DPDK通过EAL(Environment Abstraction
Layer)为特定的硬件/软件环境创建了库。通过EAL隐藏实现细节并提供标准接口。

通过DPDK一个人可以完全在用户空间而不用内核和内核到用户的拷贝来实现低开销的![run-to-completion
scheduling](https://en.wikipedia.org/wiki/Run-to-completion_scheduling),流水线，事件驱动的模型。

## 库

DPDK包含一系列data plane libraries和优化的网络适配器：

> 队列管理器：实现了无锁队列

> 缓冲区管理器：预先分配固定大小的缓冲区，用于存储网络数据包

> 内存管理器：内存中分配对象池，并使用环形结构存储空闲对象，确保对象均匀分布于所有DRAM通道上，一优化内存访问模式，提高内存利用率和性能。

> 轮询模式驱动程序：设计为无需异步通知即可工作，可以通过定期检查网络接口控制器的状态来获取数据包，而不是依赖中断。减少了中断处理的开销，提高了数据包处理的速度。
