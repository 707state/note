<!--toc:start-->
- [Rancher](#rancher)
- [容器实现隔离机制](#容器实现隔离机制)
  - [名字空间](#名字空间)
  - [限制可用资源](#限制可用资源)
- [Kubernetes的优势](#kubernetes的优势)
  - [简化应用程序部署](#简化应用程序部署)
  - [更好地利用硬件](#更好地利用硬件)
  - [健康检查和自修复](#健康检查和自修复)
  - [自动扩容](#自动扩容)
  - [简化应用部署](#简化应用部署)
- [K8S相关概念](#k8s相关概念)
  - [pod](#pod)
  - [为何需要pod?](#为何需要pod)
    - [为什么多个容器比单个容器包含多个进程更好？](#为什么多个容器比单个容器包含多个进程更好)
- [副本机制和其他控制器](#副本机制和其他控制器)
  - [ReplicationController](#replicationcontroller)
  - [DaemonSet](#daemonset)
  - [总结](#总结)
- [客户端发现pod并通信](#客户端发现pod并通信)
  - [通过环境变量](#通过环境变量)
  - [通过DNS发现服务](#通过dns发现服务)
  - [通过FQDN连接服务](#通过fqdn连接服务)
  - [总结](#总结)
- [将磁盘挂载到容器](#将磁盘挂载到容器)
- [StatefulSet：部署有状态的多副本应用](#statefulset部署有状态的多副本应用)
- [K8S原理](#k8s原理)
  - [确保etcd集群一致性](#确保etcd集群一致性)
  - [etcd集群数量为什么应该是奇数？](#etcd集群数量为什么应该是奇数)
  - [高可用的实现](#高可用的实现)
    - [运行多实例来减少宕机可能性](#运行多实例来减少宕机可能性)
    - [对不能水平拓展的应用使用领导选举机制](#对不能水平拓展的应用使用领导选举机制)
<!--toc:end-->

# Rancher

还是Rancher省事，手写命令就是浪费时间。

# 容器实现隔离机制

有两个机制可用：第一个是Linux命名空间，它使每个进程只看到它自己的系统视图（文件、进程、网络接口、主机名等）；第二个是Linux控制组（cgroups），它限制了进程能使用的资源量（CPU、内存、网络带宽等）。

## 名字空间

默认情况下，每个Linux系统最初仅有一个命名空间。所有系统资源（诸如文件系统、用户ID、网络接口等）属于这一个命名空间。但是你能创建额外的命名空间，以及在它们之间组织资源。对于一个进程，可以在其中一个命名空间中运行它。进程将只能看到同一个命名空间下的资源。当然，会存在多种类型的多个命名空间，所以一个进程不单单只属于某一个命名空间，而属于每个类型的一个命名空间。

每种命名空间被用来隔离一组特定的资源。例如，UTS命名空间决定了运行在命名空间里的进程能看见哪些主机名和域名。通过分派两个不同的UTS命名空间给一对进程，能使它们看见不同的本地主机名。

## 限制可用资源

隔离性就是限制容器能使用的系统资源。这通过cgroups来实现。cgroups是一个Linux内核功能，它被用来限制一个进程或者一组进程的资源使用。一个进程的资源（CPU、内存、网络带宽等）使用量不能超出被分配的量。

# Kubernetes的优势

## 简化应用程序部署

## 更好地利用硬件

## 健康检查和自修复

## 自动扩容

## 简化应用部署

开发人员不需要实现他们通常会实现的特性。这包括在集群应用中发现服务和对端。这是由Kubernetes来完成的而不是应用。通常，应用程序只需要查找某些环境变量或执行DNS查询。如果这还不够，应用程序可以直接查询Kubernetes API服务器以获取该信息和其他信息。像这样查询Kubernetes API服务器，甚至可以使开发人员不必实现诸如复杂的集群leader选举机制。

# K8S相关概念

## pod

K8S不直接处理单个容器。相反，它使用多个共存容器的理念。这组容器就叫作pod。

Kubernetes的基本构件是pod。

## 为何需要pod?

### 为什么多个容器比单个容器包含多个进程更好？

容器被设计为每个容器只运行一个进程（除非进程本身产生子进程）。如果在单个容器中运行多个不相关的进程，那么保持所有进程运行、管理它们的日志等将会是我们的责任。

例如，我们需要包含一种在进程崩溃时能够自动重启的机制。同时这些进程都将记录到相同的标准输出中，而此时我们将很难确定每个进程分别记录了什么。

pod是逻辑主机，其行为与非容器世界中的物理主机或虚拟机非常相似。此外，运行在同一个pod中的进程与运行在同一物理机或虚拟机上的进程相似，只是每个进程都封装在一个容器之中。

# 副本机制和其他控制器

## ReplicationController

ReplicationController是一种Kubernetes资源，可确保它的pod始终保持运行状态。如果pod因任何原因消失（例如节点从集群中消失或由于该pod已从节点中逐出），则ReplicationController会注意到缺少了pod并创建替代pod。

ReplicationController会持续监控正在运行的pod列表，并保证相应“类型”的pod的数目与期望相符。

确保一个pod（或多个pod副本）持续运行，方法是在现有pod丢失时启动一个新pod。
集群节点发生故障时，它将为故障节点上运行的所有pod（即受ReplicationController控制的节点上的那些pod）创建替代副本。

## DaemonSet

要在所有集群节点上运行一个pod，需要创建一个DaemonSet对象，这很像一个ReplicationController或ReplicaSet，除了由DaemonSet创建的pod，已经有一个指定的目标节点并跳过Kubernetes调度程序。它们不是随机分布在集群上的。

## 总结

使用存活探针，让Kubernetes在容器不再健康的情况下立即重启它（应用程序定义了健康的条件）。

不应该直接创建pod，因为如果它们被错误地删除，它们正在运行的节点异常，或者它们从节点中被逐出时，它们将不会被重新创建。

ReplicationController始终保持所需数量的pod副本正在运行。

水平缩放pod与在ReplicationController上更改所需的副本个数一样简单。

pod不属于ReplicationController，如有必要可以在它们之间移动。

ReplicationController将从pod模板创建新的pod。更改模板对现有的pod没有影响。

ReplicationController应该替换为ReplicaSet和Deployment，它们提供相同的能力，但具有额外的强大功能。

ReplicationController和ReplicaSet将pod安排到随机集群节点，而DaemonSet确保每个节点都运行一个DaemonSet中定义的pod实例。

执行批处理任务的pod应通过Kubernetes Job资源创建，而不是直接或通过ReplicationController或类似对象创建。

需要在未来某个时候运行的Job可以通过CronJob资源创建。

# 客户端发现pod并通信

## 通过环境变量

在pod开始运行的时候，Kubernetes会初始化一系列的环境变量指向现在存在的服务。如果你创建的服务早于客户端pod的创建，pod上的进程可以根据环境变量获得服务的IP地址和端口号。

## 通过DNS发现服务

运行在pod上的进程DNS查询都会被Kubernetes自身的DNS 服务器响应，该服务器知道系统中运行的所有服务。

## 通过FQDN连接服务

## 总结

在一个固定的IP地址和端口下暴露匹配到某个标签选择器的多个pod
服务在集群内默认是可访问的，通过将服务的类型设置为NodePort或LoadBalancer，使得服务也可以从集群外部访问
让pod能够通过查找环境变量发现服务的IP地址和端口
允许通过创建服务资源而不指定选择器来发现驻留在集群外部的服务并与之通信，方法是创建关联的Endpoint资源
为具有ExternalName服务类型的外部服务提供DNS CNAME别名
通过单个Ingress公开多个HTTP服务（使用单个IP）
使用pod容器的就绪探针来确定是否应该将pod包含在服务endpoints内
通过创建headless服务让DNS发现pod IP
随着对服务的深入理解，也学习到了下面的内容：

故障排查
修改Google Kubernetes/Compute Engine中的防火墙规则
通过kubectl exec在pod容器中执行命令
在现有容器的容器中运行一个bash shell
通过kubectl apply命令修改Kubernetes资源
使用kubectl run--generator=run-pod/v1运行临时的pod

# 将磁盘挂载到容器

使用emptyDir卷存储临时的非持久数据
使用gitRepo卷可以在pod启动时使用Git库的内容轻松填充目录
使用hostPath卷从主机节点访问文件
将外部存储装载到卷中，以便在pod重启之前保持pod数据读写
通过使用持久卷和持久卷声明解耦pod与存储基础架构
为每个持久卷声明动态设置所需（或缺省）存储类的持久卷
当需要将持久卷声明绑定到预配置的持久卷时，防止动态置备程序干扰

# StatefulSet：部署有状态的多副本应用

Statefulset保证了pod在重新调度后保留它们的标识和状态。它让你方便地扩容、缩容。与ReplicaSet类似，Statefulset也会指定期望的副本个数，它决定了在同一时间内运行的宠物的数量。与ReplicaSet类似，pod也是依据Statefulset的pod模板创建的（想象一下曲奇饼干模板）。与ReplicaSet不同的是，Statefulset创建的pod副本并不是完全一样的。每个pod都可以拥有一组独立的数据卷（持久化状态）而有所区别。

# K8S原理

API服务器对外暴露了一个名为ComponentStatus的API资源，用来显示每个控制平面组件的健康状态。

Kubernetes系统组件间只能通过API服务器通信，它们之间不会直接通信。API服务器是和etcd通信的唯一组件。其他组件不会直接和etcd通信，而是通过API服务器来修改集群状态。

## 确保etcd集群一致性

为保证高可用性，常常会运行多个etcd实例。多个etcd实例需要保持一致。这种分布式系统需要对系统的实际状态达成一致。etcd使用RAFT一致性算法来保证这一点，确保在任何时间点，每个节点的状态要么是大部分节点的当前状态，要么是之前确认过的状态。

连接到etcd集群不同节点的客户端，得到的要么是当前的实际状态，要么是之前的状态（在Kubernetes中，etcd的唯一客户端是API服务器，但有可能有多个实例）。

一致性算法要求集群大部分（法定数量）节点参与才能进行到下一个状态。结果就是，如果集群分裂为两个不互联的节点组，两个组的状态不可能不一致，因为要从之前状态变化到新状态，需要有过半的节点参与状态变更。如果一个组包含了大部分节点，那么另外一组只有少量节点成员。第一个组就可以更改集群状态，后者则不可以。当两个组重新恢复连接，第二个组的节点会更新为第一个组的节点的状态。

## etcd集群数量为什么应该是奇数？

etcd通常部署奇数个实例。你一定想知道为什么。让我们比较有一个实例和有两个实例的情况时。有两个实例时，要求另一个实例必须在线，这样才能符合超过半数的数量要求。如果有一个宕机，那么etcd集群就不能转换到新状态，因为没有超过半数。两个实例的情况比一个实例的情况更糟。对比单节点宕机，在有两个实例的情况下，整个集群挂掉的概率增加了 100%。

## 高可用的实现

### 运行多实例来减少宕机可能性

需要你的应用可以水平扩展，不过即使不可以，仍然可以使用Deployment，将复制集数量设为1。如果复制集不可用，会快速替换为一个新的，尽管不会同时发生。让所有相关控制器都发现有节点宕机、创建新的pod复制集、启动pod容器可能需要一些时间。不可避免中间会有小段宕机时间。

### 对不能水平拓展的应用使用领导选举机制

为了避免宕机，需要在运行一个活跃的应用的同时再运行一个附加的非活跃复制集，通过一个快速起效租约或者领导选举机制来确保只有一个是有效的。以防你不熟悉领导者选举算法，提一下，它是一种分布式环境中多应用实例对谁是领导者达成一致的方式。例如，领导者要么是唯一执行任务的那个，其他所有节点都在等待该领导者宕机，然后自己变成领导者；或者是都是活跃的，但是领导者是唯一能够执行写操作的，而其他的只能读数据。这样能保证两个实例不会做同一个任务，否则会因为竞争条件导致不可预测的系统行为。

该机制自身不需要集成到应用中，可以使用一个sidecar容器来执行所有的领导选举操作，通知主容器什么时候它应该活跃起来。
