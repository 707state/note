---
title: "CAP"
author: "jask"
date: "2024-08-11"
output: pdf_document
header-includes:
  - \usepackage{xeCJK}
  - \setCJKmainfont{Noto Sans CJK SC}  # 替换为可用的字体
  - \setCJKmonofont{Noto Sans CJK SC}
  - \setCJKsansfont{Noto Sans CJK SC}
---

# CAP(consistency, availability, partition tolerance)
## 一致性
一致性说的是客户端的每次读操作，不管访问哪个节点，要么读到的都是同一份最新的数据，要么读取失败。 
一致性强调的是各节点间数据一致。在客户端看来，集群和单机在数据一致性上是一样的。
集群毕竟不是单机，当发生分区故障的时候，有时不能仅仅因为节点间出现了通讯问题，节点中的数据会不一致，就拒绝写入新数据，之后在客户端查询数据时，就一直返回给客户端出错信息。
## 可用性
这时候就需要考虑可用性。
可用性说的是任何来自客户端的请求，不管访问哪个节点，都能得到响应数据，但不保证是同一份最新数据。你也可以把可用性看作是分布式系统对访问本系统的客户端的另外一种承诺：我尽力给你返回数据，不会不响应你，但是我不保证每个节点给你的数据都是最新的。这个指标强调的是服务可用，但不保证数据的一致。
## 分区容错性
最后的分区容错性说的是，当节点间出现任意数量的消息丢失或高延迟的时候，系统仍然可以继续提供服务。也就是说，分布式系统在告诉访问本系统的客户端：不管我的内部出现什么样的数据同步问题，我会一直运行，提供服务。这个指标，强调的是集群对分区故障的容错能力。

## CAP不能三角
分区容错性（P）是前提，是必须要保证的。
在选择一致性和可用性时，要么选择一致性，保证数据绝对一致；要么选择可用性，保证服务可用。

当选择了一致性（C）的时候，如果因为消息丢失、延迟过高发生了网络分区，部分节点无法保证特定信息是最新的，那么这个时候，当集群节点接收到来自客户端的写请求时，因为无法保证所有节点都是最新信息，所以系统将返回写失败错误，也就是说集群拒绝新数据写入。

当选择了可用性（A）的时候，系统将始终处理客户端的查询，返回特定信息，如果发生了网络分区，一些节点将无法返回最新的特定信息，它们将返回自己当前的相对新的信息。
（在不存在网络分区的情况下，分布式系统的C和A就能保证）
