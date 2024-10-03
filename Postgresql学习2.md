---
title: "Postgres学习手册: 原理"
author: "jask"
date: "09/26/2024"
output: pdf_document
header-includes:
  - \usepackage{fontspec}
  - \usepackage{xeCJK}
  - \setmainfont{ComicShannsMono Nerd Font}
  - \setCJKmainfont{LXGW WenKai}  # 替换为可用的字体
  - \setCJKmonofont{LXGW WenKai Mono}
  - \usepackage[top=1cm, bottom=1cm, left=1cm, right=1cm]{geometry}
---

# 缓冲区管理器

PostgreSQL缓冲区管理器由缓冲表、缓冲区描述符和缓冲池组成

缓冲池是一个数组，数据的每个槽中存储数据文件的一页。缓冲池数组的序号索引称为buffer_id。

PostgreSQL中的每个数据文件页面都可以分配到唯一的标签，即缓冲区标签。当缓冲区管理器收到请求时，PostgreSQL会用到目标页面的缓冲区标签。

缓冲区标签由三个值组成，分别是关系文件节点、关系分支编号和页面块号。

第一个值分别代表了表空间、数据库和表的oid；第二个值代表关系表的分支号；最后一个值代表页面号。那么该标签表示，在某个表空间（oid=16821）中，某个数据库（oid=16384)的某张表（oid=37721）的0号分支（0代表关系表本体）的第7号页面。再比如，缓冲区标签{(16821,16384,37721),1,3}表示该表空闲空间映射文件的三号页面。关系本体main分支编号为0，空闲空间映射fsm分支编号为1。


## 缓冲区管理器的结构

缓冲表层是一个散列表，它存储着页面的buffer_tag 与描述符的buffer_id 之间的映射关系。

缓冲区描述符层是一个由缓冲区描述符组成的数组。每个描述符与缓冲池槽一一对应，并保存着相应槽的元数据。

缓冲池层是一个数组。每个槽都存储一个数据文件页，数组槽的索引称为buffer_id。

### 缓冲表

缓冲表可以在逻辑上分为三个部分，分别是散列函数、散列桶槽及数据项

内置散列函数将buffer_tag映射到哈希桶槽。即使散列桶槽的数量比缓冲池槽的数量多，冲突也可能会发生。因此缓冲表采用了使用链表的分离链接方法来解决冲突。当数据项被映射至同一个桶槽时，该方法会将这些数据项保存在一个链表

数据项包括两个值，即页面的 buffer_tag和包含页面元数据的描述符的 buffer_id。

### 缓冲区描述符

缓冲区描述符保存着页面的元数据，这些与缓冲区描述符相对应的页面保存在缓冲池槽中。缓冲区描述符的结构由BufferDesc结构定义。

polardb版本
```c 
typedef struct BufferDesc
{
	BufferTag	tag;			/* ID of page contained in buffer */
	int			buf_id;			/* buffer's index number (from 0) */

	/* state of the tag, containing flags, refcount and usagecount */
	pg_atomic_uint32 state;

	int			wait_backend_pid;	/* backend PID of pin-count waiter */
	int			freeNext;		/* link in freelist chain */

	LWLock		content_lock;	/* to lock access to buffer contents */

	/* POLAR */
	int  		flush_next;      /* link to next dirty buffer */
	int  		flush_prev;      /* link to prev dirty buffer */
	XLogRecPtr	oldest_lsn;      /* the first lsn which marked this buffer dirty */
	/*
	 * If a buffer can not be flushed on primary because its latest modification
	 * lsn > oldest apply lsn, in order to advance the consistent lsn, a copy
	 * is made. So copy_buffer is used to point to its copied buffer of the
	 * buffer.
	 */
	CopyBufferDesc	*copy_buffer;
	uint8		polar_flags;
	uint16		recently_modified_count;
	/* POLAR: record buffer redo state */
	pg_atomic_uint32 polar_redo_state;
} BufferDesc;
```

tag 保存着目标页面的buffer_tag，该页面存储在相应的缓冲池槽中

state表示页面状态。

### 描述符层

缓冲区描述符的集合构成了一个数组，本书称该数组为缓冲区描述符层。

当 PostgreSQL服务器启动时，所有缓冲区描述符的状态都为空。在 PostgreSQL中，这些描述符构成了一个名为freelist的链表

（1）从freelist的头部取一个空描述符，并将其钉住，即将refcount和usage_count增加1。

（2）在缓冲表中插入新项，该缓冲表项保存了页面buffer_tag与所获描述符buffer_id之间的关系。

（3）将新页面从存储器加载至相应的缓冲池槽中。

（4）将新页面的元数据保存至所获取的描述符中。


从freelist中摘出的描述符始终保存着页面的元数据。换言之，仍然在使用的非空描述符不会返还到freelist中。但当下列任一情况出现时，描述符状态将变为“空”，并被重新插入至freelist中。

1.相关表或索引已被删除。

2.相关数据库已被删除。

3.相关表或索引已经被VACUUM FULL命令清理。

## 缓冲区管理器锁

### 缓冲表锁

BufMappingLock保护整个缓冲表的数据完整性。它是一种轻量级的锁，有共享模式与独占模式。在缓冲表中查询条目时，后端进程会持有共享的BufMappingLock。插入或删除条目时，后端进程会持有独占的BufMappingLock。

BufMappingLock会被分为多个分区，以减少缓冲表中的争用（默认为128个分区）。每个BufMappingLock分区都保护着一部分相应的散列桶槽。

缓冲表也需要许多其他锁。例如，在缓冲表内部会使用自旋锁（spin lock）来删除数据项。

### 描述符相关的锁

每个缓冲区描述符都会用到内容锁（content_lock）与IO进行锁（io_in_progress_lock）这两个轻量级锁，以控制对相应缓冲池槽页面的访问。当检查或更改描述符本身字段的值时，就会用到自旋锁。

#### 内容锁

内容锁（content_lock）是一个典型的强制限制访问的锁，它有共享与独占两种模式。

当读取页面时，后端进程以共享模式获取页面相应缓冲区描述符中的content_lock。

执行下列操作之一时，则会获取独占模式的content_lock。

● 将行（即元组）插入页面，或更改页面中元组的 t_xmin/t_xmax 字段时（简单地说，这些字段会在相关元组被删除或更新行时发生更改）。

● 物理移除元组，或压紧页面上的空闲空间。

● 冻结页面中的元组。

#### IO进行锁

IO 进行锁（io_in_progress_lock）用于等待缓冲区上的I/O完成。当PostgreSQL进程加载/写入页面数据时，该进程在访问页面期间，持有对应描述符上独占的 io_in_progres_lock。

#### 自选锁

当检查或更改标记字段与其他字段时，例如refcount和usage_count，会用到自旋锁。下面是两个使用自旋锁的具体例子。

1.钉住缓冲区描述符。

（1）获取缓冲区描述符上的自旋锁。

（2）将其refcount和usage_count的值增加1。

（3）释放自旋锁。

2.将脏位设置为"1"。

（1）获取缓冲区描述符上的自旋锁。

（2）使用位操作将脏位置位为"1"。

（3）释放自旋锁。


后来用原子操作替换了自旋锁。


## 缓冲区管理器的工作原理

### 访问存储在缓冲池中的页面

当从缓冲池槽中的页面里读取行时，PostgreSQL 进程获取相应缓冲区描述符的共享content_lock，因而缓冲池槽可以同时被多个进程读取。

当向页面插入（及更新、删除）行时，该 postgres后端进程获取相应缓冲区描述符的独占content_lock（注意，这里必须将相应页面的脏位置设为"1"）。

访问完页面后，相应缓冲区描述符的引用计数值减1。

我们来介绍最简单的情况，即所需页面已经存储在缓冲池中。在这种情况下，缓冲区管理器会执行以下步骤：

（1）创建所需页面的 buffer_tag（在本例中 buffer_tag 是’Tag_C'），并使用散列函数计算与描述符相对应的散列桶槽。

（2）获取相应散列桶槽分区上的BufMappingLock共享锁。

（3）查找标签为’Tag_C’的条目，并从条目中获取buffer_id。本例中buffer_id为2。

（4）将 buffer_id=2的缓冲区描述符钉住，即将描述符的 refcount和usage_count增加1。

（5）释放BufMappingLock。

（6）访问buffer_id=2的缓冲池槽。


### 将页面从存储加载到空槽

在第二种情况下，假设所需页面不在缓冲池中，且freelist中有空闲元素（空描述符）。这时，缓冲区管理器将执行以下步骤：

（1）查找缓冲区表（本节假设页面不存在，找不到对应页面）。

第一，创建所需页面的buffer_tag（本例中buffer_tag为’Tag_E'）并计算其散列桶槽。

第二，以共享模式获取相应分区上的BufMappingLock。

第三，查找缓冲区表（根据假设，这里没找到）。

第四，释放BufMappingLock。

（2）从 freelist 中获取空缓冲区描述符，并将其钉住。在本例中所获的描述符：buffer_id=4。

（3）以独占模式获取相应分区的BufMappingLock（此锁将在步骤（6）中被释放）。

（4）创建一条新的缓冲表数据项：buffer_tag='Tag_E',buffer_id=4，并将其插入缓冲区表中。
（5）将页面数据从存储加载至buffer_id=4的缓冲池槽中，如下所示：

第一，以排他模式获取相应描述符的io_in_progress_lock。

第二，将相应描述符的IO_IN_PROGRESS标记位设置为1，以防其他进程访问。

第三，将所需的页面数据从存储加载到缓冲池插槽中。

第四，更改相应描述符的状态，将IO_IN_PROGRESS标记位设置为"0"，且VALID标记位设置为"1"。

第五，释放io_in_progress_lock。

（6）释放相应分区的BufMappingLock。

（7）访问buffer_id=4的缓冲池槽。


### 将页面从存储加载到受害者缓冲池槽

缓冲区管理器将执行以下步骤：

（1）创建所需页面的buffer_tag并查找缓冲表。在本例中假设buffer_tag是’Tag_M'（且相应的页面在缓冲区中找不到）。

（2）使用时钟扫描算法选择一个受害者缓冲池槽位，从缓冲表中获取包含着受害者槽位buffer_id的旧表项，并在缓冲区描述符层将受害者槽位的缓冲区描述符钉住。本例中受害者槽的buffer_id=5，旧表项为Tag_F,id = 5。时钟扫描将在下一节介绍。

（3）如果受害者页面是脏页，则将其刷盘（write & fsync），否则进入步骤（4）。

在使用新数据覆盖脏页之前，必须将脏页写入存储中。脏页的刷盘步骤如下：

第一，获取 buffer_id=5描述符上的共享 content_lock和独占 io_in_progress_lock。

第二，更改相应描述符的状态：相应IO_IN_PROCESS 位设置为"1",JUST_DIRTIED 位设置为"0"。

第三，根据具体情况，调用XLogFlush()函数将WAL缓冲区上的WAL数据写入当前WAL段文件（WAL和XLogFlush函数将在第9章中介绍）。

第四，将受害者页面的数据刷盘至存储中。

第五，更改相应描述符的状态；将IO_IN_PROCESS位设置为"0"，将VALID位设置为"1"。

第六，释放io_in_progress_lock和content_lock。

（4）以排他模式获取缓冲区表中旧表项所在分区上的BufMappingLock。

（5）获取新表项所在分区上的BufMappingLock，并将新表项插入缓冲表：

第一，创建新表项：由buffer_tag='Tag_M’与受害者的buffer_id组成的新表项。

第二，以独占模式获取新表项所在分区上的BufMappingLock。

第三，将新表项插入缓冲区表中。

（6）从缓冲表中删除旧表项，并释放旧表项所在分区的BufMappingLock。

（7）将目标页面数据从存储加载至受害者槽位，然后用 buffer_id=5更新描述符的标识字段，将脏位设置为0，并按流程初始化其他标记位。

（8）释放新表项所在分区上的BufMappingLock。

（9）访问buffer_id=5对应的缓冲区槽位。

### 时钟扫描

该算法是NFU（Not Frequently Used）算法的变体，开销较少，能高效地选出较少使用的页面。

## 环形缓冲区

在读写大表时，PostgreSQL会使用环形缓冲区而不是缓冲池。环形缓冲器是一个很小的临时缓冲区域。当满足下列任一条件时，PostgreSQL将在共享内存中分配一个环形缓冲区。

1.批量读取。当扫描关系读取数据的大小超过缓冲池的四分之一时，环形缓冲区的大小为256 KB。

2.批量写入，当执行下列SQL命令时，环形缓冲区大小为16 MB。

● COPY FROM命令。

● CREATE TABLE AS命令。

● CREATE MATERIALIZED VIEW或 REFRESH MATERIALIZED VIEW命令。

● ALTER TABLE命令。

3.清理过程，当自动清理守护进程执行清理过程时，环形缓冲区大小为256 KB。

分配的环形缓冲区将在使用后被立即释放。

环形缓冲区的好处显而易见，如果后端进程在不使用环形缓冲区的情况下读取大表，则所有存储在缓冲池中的页面都会被移除，这会导致缓存命中率降低。环形缓冲区可以避免此问题。

为什么批量读取和清理过程的默认环形缓冲区大小为256 KB

源代码中缓冲区管理器目录下的README中解释了这个问题。

顺序扫描使用256KB的环形缓冲区，它足够小，因而能放入L2缓存中，从而使得操作系统缓存到共享缓冲区的页面传输变得高效。通常更小一点也可以，但环形缓冲区需要足够大到能同时容纳扫描中被钉住的所有页面。


# 进程与内存架构

## 进程架构

PostgreSQL是一个客户端/服务器风格的关系型数据库管理系统，采用多进程架构，运行在单台主机上。

postgres服务器进程是 PostgreSQL服务器中所有进程的父进程。

带start参数执行pg_ctl实用程序会启动一个postgres服务器进程。它会在内存中分配共享内存区域，启动各种后台进程。

每当接收到来自客户端的连接请求时，它都会启动一个后端进程，然后由启动的后端进程处理该客户端发出的所有查询。

### 后端进程

每个后端进程（也称为“postgres”）由 postgres 服务器进程启动，并处理连接另一侧的客户端发出的所有查询。它通过单条TCP连接与客户端通信，并在客户端断开连接时终止。

因为一条连接只允许操作一个数据库，所以必须在连接到PostgreSQL服务器时显式地指定要连接的数据库。

PostgreSQL允许多个客户端同时连接，配置参数max_connections用于控制最大客户端连接数（默认为100）。

## 内存架构

PostgreSQL的内存架构可以分为两个部分：

 本地内存区域——由每个后端进程分配，供自己使用。

 共享内存区域——供PostgreSQL服务器的所有进程使用。


### 本地内存区域

每个后端进程都会分配一块本地内存区域用于查询处理。该区域会分为几个子区域——子区域的大小有的固定，有的可变。

### 共享内存区域

PostgreSQL服务器启动时会分配共享内存区域，该区域分为几个固定大小的子区域。

PostgreSQL还分配了以下几个区域：

 用于访问控制机制的子区域（例如信号量、轻量级锁、共享和排他锁等）。

 各种后台进程使用的子区域，例如checkpointer和autovacuum。

 用于事务处理的子区域，例如保存点与两阶段提交（2PC）。


# 并发控制

从宽泛的意义上来讲，有三种并发控制技术，分别是多版本并发控制（Multi-Version Concurrency Control,MVCC）、严格两阶段锁定（Strict Two-Phase Locking,S2PL）和乐观并发控制（Optimistic Concurrency Control,OCC），每种技术都有多种变体。在MVCC中，每个写操作都会创建一个新版本的数据项，并保留其旧版本。当事务读取数据对象时，系统会选择其中的一个版本，通过这种方式来确保各个事务间相互隔离。MVCC的主要优势在于“读不会阻塞写，写也不会阻塞读”，相反的例子是，基于S2PL的系统在写操作发生时会阻塞相应对象上的读操作，因为写入者获取了对象上的排他锁。PostgreSQL和一些关系型数据库使用一种MVCC的变体，叫作快照隔离（Snapshot Isolation,SI）。

一些关系型数据库（例如Oracle）使用回滚段来实现快照隔离SI。当写入新数据对象时，旧版本对象先被写入回滚段，随后用新对象覆写至数据区域。PostgreSQL使用更简单的方法，即新数据对象被直接插入相关表页中。读取对象时，PostgreSQL根据可见性检查规则，为每个事务选择合适的对象版本作为响应。

PostgreSQL对DML（SELECT、UPDATE、INSERT、DELETE等命令）使用SSI，对DDL（CREATE TABLE等命令）使用2PL。

## 事务标识

每当事务开始时，事务管理器就会为其分配一个称为事务标识（transaction id,txid）的唯一标识符。PostgreSQL的txid 是一个32位无符号整数，取值空间大小约为42亿。在事务启动后执行内置的txid_current()函数，即可获取当前事务的txid。

PostgreSQL保留以下三个特殊txid：

● 0表示无效的txid。

● 1表示初始启动的txid，仅用于数据库集群的初始化过程。

● 2表示冻结的txid，详情参考第5.10节。

txid 可以相互比较大小。例如对于txid=100的事务，大于100的txid 属于“未来”，且对于txid=100的事务而言都是不可见的，小于100的txid属于“过去”，且对该事务可见。

因为txid在逻辑上是无限的，而实际系统中的txid空间不足（4B整型的取值空间大小约42亿），因此PostgreSQL将txid空间视为一个环。对于某个特定的txid，其前约21亿个txid属于过去，其后约21亿个txid属于未来。

## 元组结构

我们可以将表页中的堆元组分为普通数据元组与TOAST元组两类

堆元组由三个部分组成，即 HeapTupleHeaderData 结构、空值位图及用户数据。


HeapTupleHeaderData结构在src/include/access/htup_details.h中定义。

虽然HeapTupleHeaderData结构包含7个字段，但是后续部分中只需要了解4个字段即可。

● t_xmin保存插入此元组的事务的txid。

● t_xmax 保存删除或更新此元组的事务的 txid。如果尚未删除或更新此元组，则t_xmax设置为0，即无效。

● t_cid 保存命令标识（command id,cid）,cid 的意思是在当前事务中，执行当前命令之前执行了多少SQL命令，从零开始计数。例如，假设我们在单个事务中执行了3条INSERT命令BEGIN;INSERT;INSERT;INSERT;COMMIT;。如果第一条命令插入此元组，则该元组的t_cid会被设置为0。如果第二条命令插入此元组，则其t_cid会被设置为1，以此类推。

● t_ctid保存着指向自身或新元组的元组标识符（tid）。如第1.3节中所述，tid用于标识表中的元组。在更新该元组时，t_ctid 会指向新版本的元组，否则 t_ctid 会指向自己。


### 空闲空间映射

插入堆或索引元组时，PostgreSQL使用表与索引相应的FSM来选择可供插入的页面。

表和索引都有各自的FSM。每个FSM存储着相应表或索引文件中每个页面可用空间容量的信息。

所有FSM都以后缀fsm存储，在需要时它们会被加载到共享内存中。


