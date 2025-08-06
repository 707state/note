-   [A Remote Direct Memory Access Protocol
    Specification](#a-remote-direct-memory-access-protocol-specification)
    -   [介绍Introduction](#介绍introduction)

# A Remote Direct Memory Access Protocol Specification

前置知识：

1.  DDP协议：参见RFC5041

## 介绍Introduction

rfc5040定义了RDMAP(Remote Direct Memory Access Protocol)，一个在Direct
Data Placement Protocol基础上进行操作的协议。

RDMAP提供了直接向程序读写和允许数据直接被传输（零拷贝）到ULP(Upper Layer
Protocol)缓冲区的能力。

同时，RDMAP支持Kernel Bypass实现。

RDMAP是建立在DDP协议之上的，并且使用在DDP中可用的双缓冲区模型。DDP应当熟悉。
