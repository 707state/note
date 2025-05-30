---
title: "Mysql技术内幕：InnoDB学习 (7)"
author: "jask"
date: "2024-08-11"
output: pdf_document
header-includes:
  - \usepackage{fontspec}
  - \usepackage{xeCJK}
  - \setmainfont{ComicShannsMono Nerd Font} 
  - \setCJKmainfont{Noto Sans CJK SC}  # 替换为可用的字体
  - \setCJKmonofont{Noto Sans CJK SC}
  - \setCJKsansfont{Noto Sans CJK SC}
---

# Mysql技术内幕

## 备份与恢复

可以根据不同的类型来划分备份的方法。根据备份的方法不同可以将备份分为：

Hot Backup（热备）

Cold Backup（冷备）

Warm Backup（温备）

### Hot Backup
Hot Backup是指数据库运行中直接备份，对正在运行的数据库操作没有任何的影响。这种方式在MySQL官方手册中称为Online Backup（在线备份）。

Cold Backup发生在数据库停止的情况下。又叫Offline Backup。

按照备份后文件的内容，备份又可以分为：

逻辑备份

裸文件备份

若按照备份数据库的内容来分，备份又可以分为：

完全备份

增量备份

日志备份

### Cold Backup
对于InnoDB存储引擎的冷备非常简单，只需要备份MySQL数据库的frm文件，共享表空间文件，独立表空间文件（*.ibd），重做日志文件。

冷备的优点是：

备份简单，只要复制相关文件即可。

备份文件易于在不同操作系统，不同MySQL版本上进行恢复。

恢复相当简单，只需要把文件恢复到指定位置即可。

恢复速度快，不需要执行任何SQL语句，也不需要重建索引。

冷备的缺点是：

InnoDB存储引擎冷备的文件通常比逻辑文件大很多，因为表空间中存放着很多其他的数据，如undo段，插入缓冲等信息。

冷备也不总是可以轻易地跨平台。操作系统、MySQL的版本、文件大小写敏感和浮点数格式都会成为问题。

### 热备
ibbackup是InnoDB存储引擎官方提供的热备工具,可以同时备份
MyISAM存储引擎和InnoDB存储引擎表。对于InnoDB存储引擎表其
备份工作原理如下:
1)记录备份开始时,InnoDB存储引擎重做日志文件检查点的LSN。
2)复制共享表空间文件以及独立表空间文件。
3)记录复制完表空间文件后,InnoDB存储引擎重做日志文件检查点
的LSN。
4)复制在备份时产生的重做日志。

#### 使用XtraBackup实现增量备份
MySQL数据库本身提供的工具并不支持真正的增量备份,更准确地
说,二进制日志的恢复应该是point-in-time的恢复而不是增量备份。
而XtraBackup工具支持对于InnoDB存储引擎的增量备份,其工作原
理如下:
1)首选完成一个全备,并记录下此时检查点的LSN。
2)在进行增量备份时,比较表空间中每个页的LSN是否大于上次备
份时的LSN,如果是,则备份该页,同时记录当前检查点的LSN。

#### 快照备份
MySQL数据库本身并不支持快照功能,因此快照备份是指通过文件系
统支持的快照功能对数据库进行备份。备份的前提是将所有数据库文
件放在同一文件分区中,然后对该分区进行快照操作。支持快照功能
的文件系统和设备包括FreeBSD的UFS文件系统,Solaris的ZFS文件
系统,GNU/Linux的逻辑管理器(Logical Volume Manager,
LVM)等。这里以LVM为例进行介绍,UFS和ZFS的快照实现大致和
LVM相似。
LVM是LINUX系统下对磁盘分区进行管理的一种机制。LVM在硬盘和
分区之上建立一个逻辑层,来提高磁盘分区管理的灵活性。管理员可
以通过LVM系统轻松管理磁盘分区,例如,将若干个磁盘分区连接为
一个整块的卷组(Volume Group),形成一个存储池。管理员可以
在卷组上随意创建逻辑卷(Logical Volumes),并进一步在逻辑卷
上创建文件系统。管理员通过LVM可以方便地调整卷组的大小,并且
可以对磁盘存储按照组的方式进行命名、管理和分配。简单地说,用
户可以通过LVM由物理块设备(如硬盘等)创建物理卷,由一个或多
个物理卷创建卷组,最后从卷组中创建任意个逻辑卷(不超过卷组大
小)

### 复制
复制(replication)是MySQL数据库提供的一种高可用高性能的解决
方案,一般用来建立大型的应用。总体来说,replication的工作原理
分为以下3个步骤:
1)主服务器(master)把数据更改记录到二进制日志(binlog)
中。
2)从服务器(slave)把主服务器的二进制日志复制到自己的中继日
志(relay log)中。
3)从服务器重做中继日志中的日志,把更改应用到自己的数据库上,
以达到数据的最终一致性。

复制的工作原理并不复杂,其实就是一个完全备份加上二进制日志备
份的还原。不同的是这个二进制日志的还原操作基本上实时在进行
中。这里特别需要注意的是,复制不是完全实时地进行同步,而是异
步实时。这中间存在主从服务器之间的执行延时,如果主服务器的压
力很大,则可能导致主从服务器延时较大。

  从服务器有2个线程,一个是I/O线程,负责读取主服务器的二进制日
志,并将其保存为中继日志;另一个是SQL线程,复制执行中继日
志。


