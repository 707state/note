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

DDP下的LLP可能支持每个连接的单个LLP数据流或每个连接的多个LLP数据流，在这两种情况下，指定DDP时，每个DDP都是杜立德并且映射到单个LLP流。在一个特定DDP流中，LLP流被要求提供一个有序可靠的交付。注意DDP并没有DDP流之间的顺序保证。

DDP协议可能会在可靠交付LLP或不可靠交付LLP上运行，但在这个规范中要求可靠有序的LLP交付。

### DDP分层

DDP要求独立于LLP, 但是是被设计用来和其他几个协议一同工作的。

DDP支持任何ULP的直接数据放置功能，但是经过设计可以与RDMAP配合使用，并且是iWARP组件的一部分。


                       +-------------------+
                       |                   |
                       |     RDMA ULP      |
                       |                   |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                 |                   |
     |      ULP        |       RDMAP       |
     |                 |                   |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                                     |
     |           DDP protocol              |
     |                                     |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                 |                   |
     |       MPA       |                   |
     |                 |                   |
     |                 |                   |
     +-+-+-+-+-+-+-+-+-+       SCTP        |
     |                 |                   |
     |       TCP       |                   |
     |                 |                   |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


如果DDP分层在RDMAP之下，MPA和TCP之上，则相应的报头和有效载荷如下所示（注：为清楚起见，包括MPA报头和CRC，但未显示帧标记）：

    0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                                                               |
    //                           TCP Header                        //
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |         MPA Header            |                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
    |                                                               |
    //                        DDP Header                           //
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                                                               |
    //                        RDMAP Header                         //
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                                                               |
    //                                                             //
    //                        RDMAP ULP Payload                    //
    //                                                             //
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                         MPA CRC                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


## 术语汇编

### General（通用的）

Advertisement: 通知远程对等方本地RDMA缓冲区可用的行为。节点通过通知其RDMA/DDP对等方已标记的缓冲区标识符（STag、基址、长度），为传入的RDMA读或RDMA写访问提供RDMA缓冲区。RDMA/DDP没有定义标记缓冲区信息的播发，而是留给ULP。一种典型的方法是，本地对等方将标记缓冲区的引导标记、地址和长度嵌入到发送给远程对等方的发送消息中。

Data Sink: 数据接收器，接收数据有效负载的对等方。请注意，可以要求数据接收器发送和接收RDMA/DDP消息以传输数据有效负载。

ULP: Upper Layer Protocol。当前引用的协议层之上的协议层，RDMA/DDP的ULP应当是操作系统，应用，适配层或者专有设备。注意RDMA/DDP文档并不指定一个ULP，文档提供了一组语义，允许ULP设计为可以利用RDMA/DDP协议。

ULP Message: ULP消息，转递给特定协议层进行传输的ULP数据，数据边界在通过iWARO传输时被保留。

ULP Payload: ULP有效负载，包含在单个协议段或数据包的ULP数据。

### LLP

LLP: Lower Layer Protocol. 当前引用的协议层之下的协议层，例如对于DDP, LLP是SCTP, DDP Adaption, MPA或者其他传输层协议。对于RDMA, LLP就是DDP。

LLP Connection: 对应于两个节点上对等LLP层之间的LLP传输级别连接。

LLP Stream: 对应于两个节点上对等LLP层之间的单个LLP传输级流。一个或者多个LLP流可以映射到一个传输级LLP连接。对于每个连接支持多个流的传输协议（例如，SCTP），LLP流对应于一个传输级流。

MULPDU: 最大上层协议数据单元（MULPDU）。DDP可以传递给LLP进行传输的记录的当前最大大小。

ULPDU: ULPDU-上层协议数据单元。MPA以上层定义的数据记录。

### Direct Data Placement (DDP)

Data Placement(Placement, Placed, Places): 对于DDP这个词专门描述DDP写入数据缓冲区的过程，DDP段（DDP Segment）携带放置信息，接受DDP实现可以用这个放置信息来执行DDP段ULP有效载荷的数据放置。

DDP Abortive Teardown: DDP拆除。尝试关闭DDP流而不去关闭正在进行中或者挂起的DDP消息。

DDP Graceful Teardown: 关闭DDP流的行为，同时要允许所有正在进行和挂起的DDP消息成功完成。

DDP Control Field: DDP标头中的固定8位的字段。

DDP Header: 所有DDP段中存在的标头，包含控制和放置的字段，用于定义DDP段中携带的ULP有效负载的最终放置位置。

DDP Message: DDP 消息/报文，ULP定义的数据交换单元，细分为一个或多个DDP段。这种分段可能由多重原因引起，包括遵从底层传输协议的最大分段大小而进行的分段。

DDP Segment: DDP段，DDP协议的最小数据单元，包括DDP标头和ULP有效负载。DDP段的大小应适合较低层协议MULPDU。

DDP流-DDP消息序列：顺序由LLP定义。对于SCTP, DDP流直接映射到SCTP流，对于MPA, 一个DDP流直接映射到TCP连接，并且支持单个DDP流。注意DDP在DDP流之间没有顺序保证。

DDP Stream Identifier (ID): 对一个DDP Stream的标识符。

Direct Data Placement: 一种机制，能够让DDP段中的数据直接放入到最终的内存位置而不需要ULP的处理。即便没有顺序到达也有可能发生。乱序放置的支持可能需要Data Sink将LLP和DDP作为一个数据块的方式来支持。

Direct Data Placement Protocol (DDP): 一种传输协议，通过将显示内存缓冲区方式信息与LLP有效负载单元关联，支持直接数据放置。

Message Offset (MO): 对于DDP Untagged Buffer Model（未标识缓冲区）, 指定从DDP消息开始的偏移量（以八个字节为单位）。

Message Sequence Number (MSN): 对于Untagged Buffer Model, 指定一个伴随着每一个DDP Message递增的序列号。

Protection Domain (PD): 关联DDP流和STag的机制，在此基础上，如果STag和DDP流具有相同的保护域标识符，则STag在DDP流上的使用是有效的。

Queue Number (QN): 对于DDP未标记缓冲区模型，标识DDP段的目标数据接收器队列。

Steering Tag (STag/转向标记): 节点上标记缓冲区的标识符，在协议规范中定义有效。

Tagged Buffer: 标记缓冲区，通过交换STag. 标记的偏移量和长度显示地通告远程对等方的缓冲区。

Tagged Buffer Model: DDP数据传输模型，用于将标记缓冲区从本地对等点传输到远程对等点。

Tagged DDP Message: 以Tagged Buffer为目标的DDP消息。

Tagged Offset (TO): 节点上标记的缓冲区内的偏移量。

ULP Buffer: DDP层上拥有的缓冲区，作为标记缓冲区或者未标记ULP缓冲区播发给DDP层。

ULP Message Length: ULP Payload包含的DDP消息的总长度，按照8字节对齐。

Untagged Buffer: 并没有显式地通告给远程对等方的缓冲区。

Untagged Buffer Model: 一个DDP数据传输模型，用来将未标记缓冲区从本地对等方发送到远程对等方。

Untagged DDP Message: 一个以Untagged Buffer为目标的DDP消息。

## LLP可靠传输的要求 Reliable Delivery LLP Requirements

任何一个能够用作LLP到DDP的协议都必须满足下面的条件：

1. LLP必须对外暴露MULPDU和MULPDU的修改。

这样做以便DDP层可移植性与MULPDU对齐的分段，并且可以随着MULPDU的变化而调整。在MULPDU变更期间如何处理未完成请求的包含在下面。

2. 人如果MULPDU发生变化，先前已经被DDP发送给LLP的DDP段不可以被LLP要求重新分段。

需要注意的是，在某些极端情况下，LLP 广播的 MULPDU 值可能会比先前提交的 DDP 段发送请求队列清空的速度更频繁地发生变化。

在这种极端情况下，LLP 的发送队列中可能会包含多个 DDP 消息，而这些消息对应的 MULPDU 值在消息提交后已多次更新。因此，队列中的 DDP 段与 LLP 当前的 MULPDU 值之间可能不存在任何直接关联。

3. LLP必须确保，如果他收到一个DDP段，就必须可靠地将其发送到接收方；或者在传输未完成时，返回一个错误状态，说明传输失败。

4. LLP必须在数据接收端（Data Sink）保留 DDP 段（DDP Segment）和消息（Message）的边界。

5. LLP可能会乱序的把到达的段提供给Placement, 但是必须提供发送方指明的顺序。

6. LLP（Lower Layer Protocol，下层协议）必须为至少覆盖 DDP 段（DDP Segment）的数据提供一个强校验机制，其强度至少相当于 CRC32-C。现有的一些数据完整性校验方法被认为不足以满足要求，而直接内存传输语义需要比简单的校验和（如 checksum）更强的校验机制。

7. 在接收数据时，LLP必须提供接收到的DDP段的长度，这确保了 DDP 不需要在其报头中携带长度字段。

8. 如果 LLP不支持独立于其他 LLP 流（LLP Stream）拆除某个 LLP 流的功能，并且某个特定的 DDP 流（DDP Stream）发生错误，则 LLP 必须将相关的 LLP 流标记为错误状态的 LLP 流。在 DDP 请求拆除相关的 DDP 流后，LLP 必须禁止在该 LLP 流上进行任何进一步的数据传输。

9. 对于一个特定的LLP Stream, 这个LLP就必须提供一种机制来推断这个LLP Stream已经被正确拆除/切断/关闭(teardown)。对于一个特定的LLP Connection, 这个LLP必须提供正确判断LLP Connection是否被合理关闭的机制。

10. 对于一个特定的 LLP 连接（LLP Connection），当所有的 LLP 流（LLP Streams）要么被正常关闭，要么被标记为错误状态时，LLP 连接必须被拆除。

11. LLP在将所有先前的 DDP 段（DDP Segments）及其相关的顺序信息传递给 DDP 层之后，必须确保不会将重复的 DDP 段传递给 DDP 层。


## 头部格式 Header Format

DDP有两种头部格式，一个是到Tagged Buffer的数据放置，另一个是到Untagged Buffer的数据放置。

### DDP Control Field

DDP Header前8个bit携带的是两个格式共有的控制位，如下所示：
 0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                                     +-+-+-+-+-+-+-+-+
                                     |T|L| Rsvd  |DV |
                                     +-+-+-+-+-+-+-+-+

                        Figure 3: DDP Control Field

偏移了16位从而容纳MPA中的头部。只有当DDP建立在MPA上才有这个头部。

T - Tagged flag: 1bit

指明是Tagged还是Untagged Buffer Model. 如果是1, 携带这个DDP段的ULP Payload必须放置到Tagged Buffer内，否则放到Untagged Buffer内。

L - Last flag: 1bit

指明了这个DDP段是不是当前的DDP Message的最后一个段。最后一段必须设置为1，别的段都不能设置为1。

同时，L设置为1的DDP Segment必须在所有当前DDP Message里面其他DDP Segment都被提交到LLP之后才能被提交。对Untagged DDP Message来说，DDP Segment（L字段设置为1的）必须携带最高的MO。

如果L 被设置为1, 这个DDP 消息负载在以下情况之后就要被交付：

1. 所有DDP段和先前的DDP消息都完成放置，并且

2. 每一个DDP消息都已经完成了交付。

如果L设置为0, 那么当前DDP段是一个中间DDP段。

Rsvd - Reserved: 4bits

留着给未来使用，必须被设置为0。

DV - Direct Data Placement Protocol Version: 2bits

标明版本号，这一位必须设置为1，用来确定使用的标准。每一个DDP段必须拥有同样的DV值。

### DDP Tagged Buffer Model Header

如下是所有目标为Tagged Buffer的DDP段的首部：

   0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                                    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                    |T|L| Rsvd  | DV|   RsvdULP     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                              STag                             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                                                               |
    +                               TO                              +
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


T被设置为1。

RsvdULP - Reserved for use by the ULP: 8bits

RsvdULP 字段对于 DDP 协议是不可见的，可以由 ULP（Upper Layer Protocol）以任何方式进行结构化。在 数据源（Data Source）处，DDP 必须将 RsvdULP 字段 设置为 ULP 指定的值，并且该字段从 数据源 到 数据接收端（Data Sink）在传输过程中不得修改。在 数据接收端，当 DDP 消息 被交付时，DDP 必须将 RsvdULP 字段 提供给 ULP。每个特定 DDP 消息 内的所有 DDP 段必须包含该字段的相同值。数据源必须确保每个特定 DDP 消息 中的所有 DDP 段都包含相同的 RsvdULP 字段 值。

STag - Steering Tag: 32bits

Steering Tag用于标识 数据接收端（Data Sink）的 Tagged 缓冲区。STag 必须对该 DDP 流（DDP Stream）有效。STag 与 DDP 流 之间的关联机制超出了 DDP 协议规范的范围。在 数据源处，DDP 必须将 STag 字段 设置为 ULP指定的值。在 数据接收端（Data Sink），当 ULP 消息 被交付时，DDP 必须提供 STag 字段。每个特定 DDP 消息 中的所有 DDP 段必须包含该字段的相同值，并且必须是由 ULP 提供的值。数据源必须确保每个特定 DDP 消息 中的所有 DDP 段 都包含相同的 STag 字段 值。

TO - Tagged Offset: 64bits

Tagged Offset指定了 数据接收端Tagged 缓冲区内的偏移量，以字节为单位，表示 DDP 段中包含的ULP Payload开始放置的位置。一个 DDP 消息可以从 Tagged 缓冲区中的任意TO位置开始。

### DDP Untagged Buffer Model Header

下图是Untagged Buffer中必须用的DDP段，包括上面定义的DDP Control Field。

  0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                                    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                    |T|L| Rsvd  | DV| RsvdULP[0:7]  |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                            RsvdULP[8:39]                      |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                               QN                              |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                              MSN                              |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                              MO                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

T是0

RsvdULP - 给ULP保留的字段: 40bits

RsvdULP 字段对于 DDP 协议是不可见的，可以由 ULP以任何方式进行结构化。在 数据源处，DDP 必须将 RsvdULP 字段 设置为 ULP 指定的值，并且该字段在从 数据源 到 数据接收端的传输过程中不得被修改。在 数据接收端，当 ULP 消息被交付时，DDP 必须将 RsvdULP 字段 提供给 ULP。

每个特定 DDP 消息中的所有 DDP 段必须包含相同的 RsvdULP 字段值。然而，在 数据接收端，DDP 实现不要求验证每个 DDP 段中的 RsvdULP 字段值是否一致。当 ULP 消息被交付时，DDP 可以从接收到的任何一个 DDP 段中提取该字段的值并提供给 ULP。

QN - Queue Number: 32 bits

QN 用来标识这个header引用的数据接收器的Untagged Buffer queue。一个特定的DDP消息中每一个DDP 段内都要有一个相同的QN并且要有数据源的ULP提供。数据源必须确保DDP消息中每一个DDP段的包含相同的值。

MSN - Message Sequence Number: 32 bits

消息序列号（Message Sequence Number, MSN）用于指定一个序列号。对于目标是特定 DDP 流中Queue Number的每个 DDP 消息，该序列号必须以 2³² 为模，每次递增 1。MSN 的初始值必须为 1。当 MSN 的值达到 0xFFFFFFFF 时，必须回绕至 0。

在一个特定 DDP 消息中的所有 DDP 段必须包含该字段的相同值。数据源必须确保每个特定 DDP 消息中的所有 DDP 段具有相同的 MSN 字段值。

MO - Message Offset: 32 bits

消息偏移量（Message Offset, MO）指定了以字节为单位的偏移量，表示从由该 DDP 段关联的 DDP 流（DDP Stream）上的消息序列号（MSN）和队列编号（Queue Number）标识的 DDP 消息起始位置的偏移量。DDP 层必须将引用 DDP 消息第一个字节的 MO 设置为 0。

### DDP段格式 DDP Segment Format

每一个DDO段都要包含DDP头部，也要包含ULP Payload。DDP Segment格式如下：

        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |  DDP  |                                       |
        | Header|           ULP Payload (if any)        |
        |       |                                       |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

## 数据传输 Data Transfer

DDP支持多段DDP消息，每一个DDP消息都是由一个或多个DDP段组成的。每一个DDP段包含一个DDP首部，每一个DDP首部包含有接收方所需要的用于放置包含在DDP段中的ULP Payload的信息。

### DDP Tagged Or Untagged Buffer Models

两种基本的缓冲区模型：Tagged Buffer Model和Untagged Buffer Model。

#### Tagged Buffer Model

Tagged Buffer Model用于 数据源将 DDP 消息传输到 数据接收端的一个 Tagged 缓冲区中，而该缓冲区在此之前已经通过某种机制向 数据源进行了声明（Advertised）。一个 STag（Steering Tag）用于标识特定的 Tagged 缓冲区。在使用 Tagged 缓冲区模型放置 DDP 消息时，STag 用于标识缓冲区，TO（Tagged 偏移量）用于标识缓冲区内的偏移位置，ULP 负载（ULP Payload）将传输到此位置。用于声明 Tagged 缓冲区的协议不在本规范的范围内（即，与 ULP 相关）。一个 DDP 消息可以从 Tagged 缓冲区中的任意 TO 位置开始。

此外，一个 Tagged 缓冲区可能会被多次写入。这可能出于错误恢复的目的，或者因为在某种 ULP 特定同步机制之后缓冲区被重新使用。

#### Untagged Buffer Model

Untagged Buffer Model用于 数据源（Data Source）将 DDP 消息传输到 数据接收端（Data Sink）的一个排队的缓冲区中。

DDP 队列号（DDP Queue Number）由 ULP（Upper Layer Protocol）用于将 ULP 消息划分到不同的接收缓冲队列中。例如，如果支持两个队列，ULP 可以使用一个队列来处理从应用层传递给它的缓冲区，而另一个队列则用于仅由 ULP 专用的控制消息所消耗的缓冲区。这种机制允许在使用非标记缓冲区时，将 ULP 控制消息与不透明的 ULP 负载分离开。

DDP 消息序列号（DDP Message Sequence Number）可以被 数据接收端用来标识特定的非标记缓冲区。用于传递排队缓冲区数量的协议超出了本规范的范围。同样，缓冲区队列的具体实现方式也不在本规范的范围之内。

### DDP消息的分段和重组 Segmentation and Reassembly of a DDP Message

在数据源，DDP层必须ULP消息中的数据分组成一系列DDP段，这些DDP 段要包含一个DDP首部和ULP Payload,并且必须不超过由LLP告知的MULPDU值。ULP消息的长度必须比2^32短。

数据源必须发送ULP消息中包含的所有数据，数据接收方的DDP层必须将所有接收到的DDP段的ULP Payload放入到ULP缓冲区中。

数据源处的DDP 消息分组是通过唯一地标识一个DDP消息（与ULP消息一一对应），然后对其中每一个DDP段的ULP消息指定一个1字节的偏移量。

对于一个Untagged DDP Message, QN和MSN的结合唯一地标识一个一个DDP消息。每个Untagged DDP Message里面的1字节偏移量是MO字段。对于Untagged DDP消息的每一个DDP段，MO字段必须被设置为从关联的ULP消息中的第一个字节（被设置为0）到DDP段中的ULP有效负载的第一个字节的偏移量。

举个例子，如果ULP Untagged Message有2048个字节，而MULPDU是1500字节，数据源就会生成两个DDP段，其中一个的MO=0, 包含1482个字节的ULP Payload，第二个则包含MO=1048，包含566和字节的ULP Payload。在这里，第一部分的DDP Segment是：

1048=1500(MULPDU)-18(DDP 头部)

对于一个Tagged DDO Message, 其中的STag和TO与LLP的有序传输特性结合起来，用来处理ULP消息的分包与重组。因为初始的1字节偏移量(TO字段)可以是非0的，因此在一般情况下，如果没有额外的ULP消息，就没有办法恢复ULP消息的边界。

> 实现细节：对于某些ULP的实现，例如RDMAP，就选择了不直接支持Tagged DDP Message中ULP消息边界的恢复。例如，即使数据接收方已经告知了一个单独的大Tagged Buffer来接受数据传输，在本地对等方的数据源也可能会用小的缓冲区。这种情况下，这个ULP可能会选择为多个连续的ULP消息使用相同的STag。因此，一个非零初始化的TO和STag的复用会让ULP根据ULP特定地实现分组和重组。具体内容可以看RDMAP。

> 另一种不同的实现可以使用Untagged DDP消息(在Tagged DDP Message之后发送)，用来表明这个标记的STag的初始TO。此外，另一种ULP的实现可以选择将TO初始化为0，这样就不需要额外的消息来传送Tagged DDP Message中的初始TO。


无论ULP是否选择在数据接收器处恢复Tagged DDP消息的原始ULP消息边界，DDP都支持Tagged DDP消息的分段和重新组装。STag用于标识数据接收器处的ULP缓冲区，to用于标识STag引用的ULP缓冲区内的一个字节偏移量。数据源的ULP必须指定ULP消息传递给DDP时的STag和起始TO。

对于Tagged DDP消息的每个DDP段，TO必须设置为从相关ULP消息中的第一个字节到DDP段中包含的ULP有效负载中的第一个字节的1字节偏移量，加上分配给相关ULP消息中第一个字节的TO。

例如，如果ULP标记的消息为2048个字节，起始TO为16384，MULPDU为1500个字节，则数据源将生成两个DDP段：一个TO=16384，包含ULP有效负载的前1486个字节；另一个TO=17870，包含ULP有效负载的562个字节。在此示例中，第一个DDP段的ULP有效负载量计算为：
1486 = 1500 (MULPDU) - 14 (for the DDP Header)

允许使用长度为零的DDP消息，并且必须仅使用一个DDP段。只有DDP控制和RsvdULP字段必须对长度为零的标记DDP段有效。对于长度为零的标记DDP消息，不得检查STag和TO字段。

对于未标记或标记的DDP消息，数据接收器无需验证是否已接收到整个ULP消息。

### DDP消息之间的顺序 Ordering Among DDP Messages

通过DDP传输的消息之间的顺序必须遵循下面的规则：

在数据源：
1. DDP消息必须按照提交到DDP层的顺序发送出去。

2. 传输一个Untagged DDP消息的一个段应该用一个递增的MO，传输一个Tagged DDP消息应该用一个递增的TO顺序。

在数据接收方：
1. 可能会乱序执行DDP段的放置。

2. 可能会不止一次对一个DDP段进行放置。

3. 必须把一个DDP消息至多一次交付给ULP。

4. 必须按照发送给数据源的顺序把DDP消息交付给ULP。

### DDP消息的完成与交付 DDP Message Completion and Delivery

在数据源，当一个可靠有序的LLP传输将要发生时，就认为DDP消息的传输已经结束。请注意，在数据源或者数据接收处限制LLP缓冲数据。因此，在数据源处，DDP消息的完成并不意味着数据接收方已经接收到消息。

在数据接收方，只有在满足以下所有条件时，DDP必须交付一个DDP消息：

1. DDP消息最后一个DDP段的Last被设置

2. 一个DDP消息的所有DDP段已经被放置

3. 所有先前的DDP消息已经被放置

4.每一个先前的DDP消息都已经被交付到ULP

在数据接收方，当一个Untagged DDP消息交付时，DDP必须提供ULP消息的长度给ULP。ULP消息的长度可能是通过把最后一个DDP段MO和ULP Payload长度相加计算出来的（这是在Untagged DDP Message中的）。

## DDP流的设置和拆除 DDP Stream Setup and Teardown

这一部分描述了与DDP Stream建立与拆除相关的LLP问题。

### DDP Stream Setup

ULP被预期使用本文之外的机制来建立LLP连接，而这个LLP连接会支持一个或多个LLP流（MPA/TCP或者SCTP）。LLP在设置好LLP流之后，他会在一个合适时机在特定的LLP流上支持一个DDP Stream。

ULP需要在启用LLP流的两个端点，以便在两个方向上能够同时进行DDP 数据传输。这是必要的，正是这样数据接收段才能合理地辨认出DDP段。

### DDP Stream Teardown

DDP不能独立启动流的拆卸。DDP只可以相应一个由LLP拆除的流或者处理一个来自ULP拆除一个流的请求。DDP流拆卸会关闭两个端点上的功能。对于面向连接的LLP, DDO流拆卸可能会导致底层LLp连接的拆卸。

#### DDP Graceful Teardown

DDP拆卸发生在DDP流两端同时发生是由ULP来保证的。只有这样数据接收方才能停止去尝试翻译DDP段。

如果本地对等方指示正常拆除，本地对等方的DDP层要确保所有ULP数据能够在底层LLP流和连接拆除之前传输所有的ULP数据，并且任何之后的本地对等方数据传输请求都要返回错误。

如果本地对等方的DDP层在接收到LLP正常关闭的请求之后，任何在这个请求之后接收到的数据都被认为是错误并且必须导致这个DDP流被终止拆除。

如果本地对等方LLP支持半关闭LLP流，在接收到DDP流的LLP正常关闭的请求之后，DDP应该向ULP指示半关闭的状态，并且继续正常处理越界的数据传输请求。这个事件发生之后，当本地对等ULP请求正常关闭，DDP必须给LLP指示它应当对LLP流的另一方执行正常关闭。

如果本地对等方LLP支持半关闭LLP流，那么在DDP流受到ULP正常关闭的请求时，DDP应该保持LLP流另一方启用数据接收。

#### DDP Abortive Teardown

正如前面所提到的，DDP并不能杜立德终结一个DDP流。因此，任何下面的发生在DDP流的致命错误都会导致DDP推断出ULP发生了致命错误：
1. 底层的LLP连接或者LLP流已经丢失

2. 底层的LLP报告了一个致命错误

3. DDP头部有一个或多个无效字段。

如果LLP向ULP指示发生了致命错误，DDP层需要向DDP报告这个错误（看下面的DDP Error Numbers）并且用一个错误完成所有的未完成的ULP请求。如果一个底层LLP流仍然完好，则在致命错误指示给ULP之后，DDP应该继续允许ULP去传输额外的DDP消息给正在进行的半连接。这使得ULP能够向远程对等方传输错误。在指示出ULP的致命错误发生后，DDP流必须不能被终止直到本地对等方的ULP指示给DDP层，一个DDP连接应该被终止拆卸。

## 错误语义 Error Semantics

所有报告给DDP的LLP错误都应该被传输给ULP。

### Errors Detected at the Data Sink

对于所有的非零长Untagged DDP 段，这个DDP段都应该在放置前先验证：

1, 对于当前流，QN是合法的。

2. QN和MSN都有允许放置Payload的缓冲区。

> 实现者注意：DDP的实现应该考虑将缺乏关联缓冲区作为系统错误。DDP实现可能会尝试从系统错误中恢复，通过尝试ULP透明的方式。DDP实现不应该保证系统错误频繁/重复的发生。如果没有关联的缓冲区，DDP实现可能会选择禁用接受流并且向数据接收方的ULP报告一个错误

3. MO落在与Untagged Buffer相关联的合法偏移量区间内。

4. DDP段有效负载的长度之和与MO都要落入到Untagged Buffer相关联的合法偏移量区间中。

5. 对于QN定义的队列，Message Sequence Number（MSN）要落入到合法的MSN中。合法区间定义为特定QN的第一个可用缓冲区的MSN值到最后一个可用缓冲区值之间。

> 实现者注意：对于一个典型的QN，MSN的下限由已完成的DDP消息定义。上限由可用于这个队列的可用缓冲区数量来定义。两个数字都是动态的，伴随着新的DDP消息的接受和完成与新的缓冲区的添加。由ULP来确保有足够的缓冲区来处理DDP段。


对于非零长的Tagged DDP段，这个DDP段都应该在放置前先验证：

1. STag应该是对于这个流有效的。

2. 这个STag有一个允许放置Payload的关联缓冲区。

3. TO落入到由STag注册的合法区间中。

4. DDP 段的payload的长度和与TO落入到由STag注册的合法区间中。

5. DDP段有效负载的长度综合和TO不回绕。

如果DDO曾检测到任何上述错误的发生，就必须停止放置剩余的DDP段并且向ULP报告错误。如果可以，DDP层应该在错误报告中包含DDP首部、错误类型和DDP段的长度。

DDP必须静默丢弃任何后续传入的DDP段。由于这些错误中的每一个都表示了发送ULP或者协议的失败，DDP应该让ULP发送额外的DDP消息在关闭DDP流之前。

### DDP Error Numbers

下面是报告错误给ULP的错误码，对应前面枚举的错误。每一个错误都分割成4比特的错误类型和8比特的错误码。

  Error    Error
   Type     Code        Description
   ----------------------------------------------------------
   0x0      0x00        Local Catastrophic

   0x1                  Tagged Buffer Error
            0x00        Invalid STag
            0x01        Base or bounds violation
            0x02        STag not associated with DDP Stream
            0x03        TO wrap
            0x04        Invalid DDP version

   0x2                  Untagged Buffer Error
            0x01        Invalid QN
            0x02        Invalid MSN - no buffer available
            0x03        Invalid MSN - MSN range is not valid
            0x04        Invalid MO
            0x05        DDP Message too long for available buffer
            0x06        Invalid DDP version

   0x3      Rsvd        Reserved for the use by the LLP


## 安全考虑 Security Consideration

这一部分讨论每一个协议特定的担忧和将DDP与现有安全机制结合使用。对DDP实现的安全要求在这一部分的结尾提供。更详细的安全问题可以在RDMASEC中讨论过。

对RDDP相关的讨论在RFC 2401中。

### Protocol-Specific Security Consideration

DDP对主动第三方干扰的脆弱性不比通过传输协议（如TCP和SCTP over IP）运行的任何其他协议大。

第三方通过向网络中注入发送到DDP数据接收器的伪造数据包，可以发动各种攻击，利用DDP特定的行为。

由于DDP直接或间接地公开线路上的内存地址，因此在放置任何数据之前，必须验证每个DDP段中携带的放置信息，包括无效的STag和字节级的基数和边界检查。

例如，第三方对手可能会注入看似有效DDP段的随机数据包，并损坏DDP数据接收器上的内存。由于DDP是独立于IP传输协议的，因此可以使用诸如IPsec之类的通信安全机制来防止此类攻击。

### Association of an STag and a DDP Stream

有几种机制将STag和DDP流关联起来。此关联所需的两种机制是保护域（Protection Domain, PD）关联和DDP流关联（DDP Stream associtaion）。

在保护域（PD）关联下，将创建唯一的保护域标识符（PD ID），并在本地使用该标识符将STag与一组DDP流关联。在该机制下，仅允许在与STag具有相同PD ID的DDP流上使用STag。用于网络上Tagged DDP消息的传入DDP段。

DDP流，如果DDP流的PD ID与标记的DDP消息所针对的STag的PD ID不同，则不放置DDP段，并且DDP层必须向ULP显示本地错误。请注意，PD ID是本地定义的，不能由远程对等方直接操作。

在DDP流关联下，DDP流由唯一的DDP流标识符（ID）在本地标识。
STag通过使用DDP流ID与DDP流相关联。
在这种情况下，对于DDP流上Tagged DDP消息的传入DDP段，如果DDP流的DDP流ID与标记的DDP消息所针对的STag的DDP流ID不同，然后不放置DDP段，DDP层必须向ULP显示局部错误。

请注意，DDP流ID是本地定义的，不能由远程对等方直接操作。

ULP应将STag与至少一个DDP流相关联。DDP必须支持保护域关联和DDP流关联机制，以关联STag和DDP流。

### Security Requirements

[RDMASEC]定义了RDMAP/DDP的安全模型和一般假设。本小节规定了DDP实施的安全要求。有关DDP实现的攻击类型、攻击者类型、信任模型和资源共享的更多详细信息，请参阅RDMASEC。

DDP有几种机制来处理一些攻击。这些攻击包括但不限于：

1. 与未经授权或未经验证的端点的连接。

2. 劫持DDP流。

3. 尝试从未经授权的内存区域读取或写入。

4. 另一个应用程序在多用户操作系统的流中注入RDMA消息。

DDP依赖LLP建立LLP流，DDP消息将通过该流进行传输。DDP本身不验证任一端点的LLP流的有效性。ULP负责验证LLP流。由于DDP的性质，这是非常可取的。

劫持DDP流需要劫持底层LLP流。这需要了解告知缓冲区（Advertised Buffer），以便直接将数据放入用户缓冲区。因此，这受到上述相同技术的限制，以防止尝试从未经授权的内存区域读取或写入。

DDP不需要节点打开其缓冲区以抵御DDP流上的任意攻击。它只能在ULP已启用并授权的范围内访问ULP内存。STag访问控制模型在RDMASEC中定义。具体的安全行动包括：

1. STAG仅在ULP建立的确切字节范围内有效。DDP必须为ULP提供一种机制，以建立和撤销与STag引用的ULP缓冲区相关联的TO范围。

2. STAG仅在ULP确定的期限内有效。ULP可根据其自身的上层协议要求随时撤销这些协议。DDP必须为ULP提供建立和撤销STag有效性的机制。

3. DDP必须为ULP提供一种机制，以传递STag和特定DDP流之间的关联。

4. ULP只能将内存暴露给远程访问，直到它本身已经可以访问该内存为止。

5. 如果STag在DDP流上无效，DDP必须将无效访问尝试传递给ULP。ULP可提供用于终止DDP流的机制。

此外，DDP提供了一种机制，可以直接将传入的有效负载放置在用户模式ULP缓冲区中。这避免了以前的解决方案依赖于为传入有效负载公开系统缓冲区的风险。

对于DDP实现，必须提供两个组件：支持RDMA的NIC（RNIC）和特权资源管理器（PRM）。

#### RNIC 要求

RNIC必须实现DDP wire协议并执行以下描述的安全语义。

1. RNIC必须确保特定保护域中的特定DDP流不能访问不同保护域中的STag。

2. RNIC必须确保，如果STag的作用域限于单个DDP流，则其他DDP流不能使用该STag。

3. RNIC必须确保远程对等方无法访问STag启用远程访问时指定的缓冲区之外的内存。

4. RNIC必须为ULP提供一种机制，以建立和撤销ULP缓冲区与STag和范围的关联。

5.  RNIC必须为ULP提供一种机制，以建立和撤销对STag引用的ULP缓冲区的读、写或读写访问。

6. RNIC必须确保ULP撤销STag的远程访问权限后，网络接口不能再修改播发缓冲区。

7. RNIC不得允许固件直接从不受信任的本地对等方或远程对等方加载到RNIC上，除非对等方经过适当的身份验证（通过本规范范围外的机制。该机制可能需要验证远程ULP有权执行更新），更新是通过一个安全协议完成的，比如IPsec。

#### Privileged Resources Manager Requirement (PRM)

PRM必须实现下面描述的安全语义。

1. 所有可能影响其他ULP的与RNIC引擎的非特权ULP交互必须使用特权资源管理器作为代理来完成。

2. 稀缺资源的所有ULP资源分配请求也必须使用特权资源管理器完成。

3. 特权资源管理器不得假定不同的ULP共享部分互信，除非有机制确保ULP确实共享部分互信。

4. 如果支持非特权ULP，特权资源管理器必须验证非特权ULP有权访问特定数据缓冲区，然后才能允许ULP有权访问的STag与特定数据缓冲区关联。

5. 特权资源管理器应防止本地对等方分配超过其公平份额的资源。如果RNIC提供跨多个DDP流共享接收缓冲区的能力，则RNIC和特权资源的组合。

Manager必须能够检测远程对等方是否试图消耗超过其公平份额的资源，以便本地对等方可以应用对策来检测和防止攻击。

### Security Services for DDP 

DDP使用基于IP的网络服务；因此，所有交换的DDP段都容易受到欺骗、篡改和信息泄露攻击。如果DDP流可能受到模拟攻击或流劫持攻击，强烈建议对DDP流进行身份验证、完整性保护并防止重播攻击。它可以使用保密保护来防止窃听。

#### Available Security Services

IPsec可用于防止上述数据包注入攻击。由于IPsec设计用于保护任意IP数据包流，包括数据包丢失的数据流，因此DDP可以在IPsec上运行而无需任何更改。

DDP安全还可以从为基于TCP或SCTP的ULP[TLS]提供的SSL或TLS安全服务以及在传输协议下提供的DTLS[DTLS]安全服务中获益。有关这些方法的进一步讨论以及为RDDP协议选择IPsec安全服务的基本原理，请参见[RDMASEC]。

#### DDP的IPSEC要求

IPsec数据包按照接收顺序进行处理（例如，完整性检查并可能解密），DDP数据接收器将以与不安全IP数据包中包含的DDP段相同的方式处理这些数据包中包含的解密DDP段。

IP存储工作组定义了IP存储的标准IPsec要求[RFC3723]。本规范的部分内容适用于DDP。特别是，IPsec服务的兼容实现必须满足[RFC3723]第2.3节中概述的要求。在不复制[RFC3723]中的详细讨论的情况下，这包括以下要求：

1. 实现必须支持IPsec ESP[RFC2406]，以及IPsec的重播保护机制。使用ESP时，必须使用每包数据源身份验证、完整性和重播保护。

2. 它必须在隧道模式下支持ESP，并且可以在传输模式下实施ESP。

3. 它必须支持IKE[RFC2409]，以便使用IPsec DOI[RFC2407]进行对等身份验证、安全关联协商和密钥管理。

4. 它不能将收到IKE delete消息解释为中断DDP流的原因。由于IPsec加速硬件可能只能处理有限数量的活动IPsec安全关联（SA），因此如果活动继续，空闲SA可能会动态关闭，并重新启动新SA。

5. 它必须支持使用预共享密钥的对等身份验证，并且可能支持使用数字签名的基于证书的对等身份验证。不应使用使用公钥加密方法[RFC2409]的对等身份验证。

6. 它必须支持IKE主模式，并且应该支持攻击模式。当任一对等方使用动态分配的IP地址时，不应使用带有预共享密钥身份验证的IKE主模式。

7. 必须适当限制对本地存储的秘密信息（用于数字签名的预共享或私钥）的访问，因为泄露秘密信息会使IKE/IPsec协议的安全属性无效。

8. 它必须遵循[RFC3723]第2.3.4节关于IKE参数设置的指南，以实现高水平的互操作性，而无需大量配置。

此外，DDP IPsec服务的实施和部署应遵循[RFC3723]第5节中概述的安全注意事项。