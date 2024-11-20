# RDMA?

remote direct memory access, 远程DMA, 类比rpc, 从一台主机直接访问远程主机的指定内存位置。

## **Wikipidia**

In computing, remote direct memory access (RDMA) is a direct memory access from the memory of one computer into that of another without involving either one's operating system.

This permits high-throughput, low-latency networking, which is especially useful in massively parallel computer clusters. 

简单说，就是从一台电脑直接访问另一台电脑的内存而不需要经过另一台电脑的操作系统。

这允许了高吞吐、低延迟的网络，适用于大规模并行计算机集群。

RDMA 通过使网络适配器能够直接将数据从网络传输到应用内存，或者从应用内存直接传输到网络，支持零拷贝网络通信，消除了在应用内存与操作系统中的数据缓冲区之间复制数据的需求。这样的传输不需要 CPU、缓存或上下文切换执行任何工作，并且传输过程可以与其他系统操作并行进行。这减少了消息传输的延迟。

这种策略带来了几个问题，主要是目标节点不会接收到请求完成的通知（单边通信）。

### 应用

RDMA-over-Converged-Ethernet（RoCE）现在能够在有损或无损基础设施上运行。

iWARP通过使用TCP/IP作为传输协议，在物理层实现了以太网RDMA的实现，将RDMA的性能和低延迟优势与低成本、基于标准的解决方案结合在一起。

RDMA联盟和DAT协作组在开发供标准化组织如互联网工程任务组和互连软件协会考虑的RDMA协议和API方面发挥了关键作用。

硬件供应商已经开始研发更高容量的基于RDMA的网络适配器，据报道速率可达100 Gbit/s。软件供应商，如IBM、Red Hat和Oracle公司，在他们的最新产品中支持这些API，而且自2013年起，工程师们已经开始开发实现在以太网上运行的RDMA网络适配器。Red Hat Enterprise Linux和Red Hat Enterprise MRG均支持RDMA。Microsoft通过SMB Direct在Windows Server 2012中支持RDMA。VMware ESXi自2015年起也支持RDMA。

应用程序通过最初为InfiniBand协议设计的明确定义的API来访问控制结构（尽管这些API可以用于任何底层的RDMA实现）。使用发送队列和完成队列，应用程序通过向提交队列（SQ）提交工作队列条目（WQEs）并从完成队列（CQ）接收响应通知来执行RDMA操作。


