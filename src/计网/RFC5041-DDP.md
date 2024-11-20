# Direct Data Placement over Reliable Transports

前置内容：

1. ULP(Upper Layer Protocol): 并不是某一个协议，而是特指传输层之上的协议，如应用层协议。
2. LLP(Lower Layer Protocol): 也不是一个协议，而是指传输层及以下的协议。

## 介绍

DDP使得ULP能够向数据接收器发送数据，从而不用把数据放置在中间缓冲区，因此当数据到达数据接收器时，网络接口可以直接将数据放置在ULP的缓冲区中，使得接收器消耗的内存带宽大大低于缓冲模型。

此外，与CPU移动数据相比，这样做使得网络协议消耗更少的CPU周期。

DDP保留了ULP记录边界，提供用于传输ULP消息的各种数据传输机制和完成机制。

### 架构的目标

1. Tagged Buffer Model: 提供一个缓冲区，使得本地能向远程标记一个缓冲区，以便远程对等方可以通过网络直接将数据放入远程放入指定位置的缓冲区中。

2. Untagged Buffer Model: 提供第二个接受缓冲区，保留来自对等方的ULP消息边界，并保证本地缓冲区匿名（未标记）。

3. 为两种缓冲区提供可靠的顺序交付语义。

4. 提供ULP消息的分段和重新组装。

5. 允许ULP缓冲区用作中心组装缓冲区，无需复制。这要求协议将包含在传入DDP段中的ULP有效负载的数据放置与完成的ULP消息的数据交付分开。

6. 如果LLP在一个LLP连接中支持多个LLP流，就在每一个LLP流上提供上述功能，并使该功能能够在每个LLP流的基础上导出到ULP。

### 协议概述

DDP支持两种基本的数据传输模型——Tagged Buffer Model和Untagged Buffer Model。

标记缓冲区数据传输模型（Tagged Buffer Model）要求数据接收器向数据源发送ULP缓冲区的标识符，称为转向标签（Steering TAg/STag）。STag使用ULP定义的方法传输到数据源。一旦数据源ULP具有目标ULP缓冲区的STag，它可以通过将STag指定给DDP来请求DDP将ULP数据发送到目标ULP缓冲区。请注意，标记的缓冲区不必从ULP缓冲区开始填充。ULP数据源可以向ULP缓冲区提供任意偏移量。

未标记缓冲区数据传输模型（Untagged Buffer Model）允许数据传输发生，而无需数据接收器向数据源播发ULP缓冲区。数据接收器可以将一系列接收ULP缓冲区排队。来自数据源的未标记DDP消息消耗数据接收器处的未标记缓冲区。因为DDP是面向消息的，所以即使数据源发送的DDP消息有效负载小于接收ULP缓冲区，部分填充的接收ULP缓冲区也会传递到ULP。如果数据源发送的DDP消息有效负载大于接收ULP缓冲区，则会导致错误。

#### Tagged Buffer Model与Untagged Buffer Model之间的关键区别

1. 对于Tagged Buffer Model, 数据源指定接收到的哪个标记缓冲区将用于特定的标记DDP消息（sender-based ULP Buffer management）；对于Untagged Buffer Model, 数据接收器指定接受未标记DDP消息时使用Untagged Budder的顺序（receiver-based ULP Buffer management）。

2. 对于标记缓冲区模型，数据接收器的ULP必须通过ULP特定的机制在数据传送之前向数据源播发指定的ULP缓冲区；对于未标记缓冲区，数据传输可以在没有端到端显示ULP缓冲区播发的情况下发生。有流量控制问题。

3. 对于缓冲区，DDP消息可以从标记缓冲区的任意偏移量为目标；未标记缓冲区中DDP消息只能从偏移量0开始。

4. 标记缓冲区模型允许多个DDP消息以单个ULP缓冲区播发的标记缓冲区为目标。未标记缓冲区模型要求为每个以未标记缓冲区为目标的DDP消息关联接收ULP缓冲区。

两种数据传输模型都将ULP消息放入DDP消息中，然后将每个DDP消息分为DDP段，这是用来适应LLP的最大上层协议数据单元(Maxmum Upper Layer Protocol Data Uni, MULPDU)的，因此，ULP可以发布任意大小的ULP消息，包含2^32-1个ULP有效负载八位字节，并且DDP将ULP消息分为DDP段，这些段在数据接收器重新组装。

DDP为ULP提供顺序交付，但是DDP区分了数据交付和数据放置(Data Dilivery和Data Placement)。DDP在每一个DDP段中提供足够信息，允许每一个入站DDP段有效载荷中的ULP有效载荷直接放入正确的ULP缓冲区，即使DDP段到达时无序。因此，DDP允许在ULP缓冲区内将DDP消息的DDP段中包含的ULP有效负载重新组装为ULP消息，从而消除了从重新组装缓冲区到ULP缓冲区的传统拷贝。

在下面的情况中，DDP消息的有效负载被传送到ULP:

1. DDP消息的所有段都被完全接收，并且DDP消息的有效负载已经被放入到相关的ULP缓冲区中。

2. 已经放置了所有先前的DDP消息并且所有的DDP消息传递都已经执行。

DDP下的LLP支持

