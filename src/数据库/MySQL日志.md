<!--toc:start-->
- [MySQL日志](#mysql日志)
  - [undo log](#undo-log)
    - [BufferPool的作用？](#bufferpool的作用)
  - [redo log](#redo-log)
  - [binlog](#binlog)
  - [bin log和redo log的区别？](#bin-log和redo-log的区别)
  - [两阶段提交](#两阶段提交)
    - [两阶段提交的问题？](#两阶段提交的问题)
  - [组提交机制](#组提交机制)
<!--toc:end-->

# MySQL日志

## undo log

InnoDB引擎生成，实现了事务中的原子性，主要用于事务回滚和MVCC。

用于记录每一次对记录时的修改，以便回滚时对数据库进行更新。

每一次记录的更新操作产生的undo log都有一个roll\_pointer指针和一个trx\_id事务id。

- 通过trx_id可以知道记录是哪个事务修改的。
- 通过roll_pointer指针可以将这些undo log串成一个链表，这个链表被称为版本链。

还有一个作用就是通过ReadView + undo log实现MVCC机制。

### BufferPool的作用？

- 当读取数据时，数据已经存在于Buffer Pool中，客户端就会直接读取Buffer Pool中的数据，否则就去磁盘中读取。
- 当修改数据时，如果数据存在于Buffer Pool中，那直接修改Buffer Pool中数据所在的页，然后将页设置为脏页，为了减少磁盘I/O，不会立即将脏页写入磁盘，后续由后台线程选择一个合适实际将脏页写入到磁盘。

InnoDB 会为 Buffer Pool 申请一片连续的内存空间，然后按照默认的16kb的大小划分出一个个的页，BufferPool中的页就叫做缓存页。此时这些缓存页都是空闲的，随着程序的运行，才会有磁盘上的页被缓存到Buffer Pool中。

## redo log

InnoDB引擎生成，实现了事务中的持久性，主要用于掉电等故障恢复。

因为刷盘和修改并不是同步的，数据可能没有保存到磁盘，所以，MySQL采用WAL的技术，先把修改写入redo log，随后等待合适时间刷入磁盘。

对应的BufferPool中的undo页面，也有对应的redo log。

两个作用：
1. 实现了事务的持久性，让MySQL有了crash-safe的能力。
2. 将写操作从随机写变成了顺序写。

## binlog

Server层生成的日志，用于数据备份和主从复制。

记录了所有数据表结构变更和表数据修改的日志，不会记录查询类的操作，比如SELECT和SHOW操作。

## relay log

中继日志，用于主从复制场景下，slave通过io线程拷贝master的bin log后本地生成的日志

## 慢查询日志

用于执行记录执行时间过长的sql，需要设置阈值后手动开启。

## bin log和redo log的区别？

1. 适用对象不同：
- bin log是MySQL Server层的，对所有存储引擎都可用
- redo log是InnoDB实现的日志。

2. 文件格式不同
- bin log有三种格式类型：STATEMENT（默认）、ROW、MIXES

区别在于：

STATEMENT: 每一条修改数据的 SQL 都会被记录到 binlog 中（相当于记录了逻辑操作，所以针对这种格式， binlog 可以称为逻辑日志），主从复制中 slave 端再根据 SQL 语句重现。但 STATEMENT 有动态函数的问题，比如你用了 uuid 或者 now 这些函数，你在主库上执行的结果并不是你在从库执行的结果，这种随时在变的函数会导致复制的数据不一致；

ROW：记录行数据最终被修改成什么样了（这种格式的日志，就不能称为逻辑日志了），不会出现 STATEMENT 下动态函数的问题。但 ROW 的缺点是每行数据的变化结果都会被记录，比如执行批量 update 语句，更新多少行数据就会产生多少条记录，使 binlog 文件过大，而在 STATEMENT 格式下只会记录一个 update 语句而已；

MIXED：包含了 STATEMENT 和 ROW 模式，它会根据不同的情况自动使用 ROW 模式和 STATEMENT 模式；

- redo log是物理日志，记录的是某个数据页做了什么修改。

3. 写入方式不同

- bin log是追加写，写满一个文件就创建一个新的文件继续写，不会覆盖以前的日志，保存的是全量日志。
- redo log是循环写，日志空间大小是固定的，全部写满就从头开始，保存未被刷入磁盘的脏页日志。

4. 用途不同

- bin log用于备份恢复、主从复制。
- redo log用于掉电等故障恢复。

## 两阶段提交

MySQL当中的bin log和redo log可能会出现不一致的状态，为了解决这个问题，MySQL使用了内部XA事务，有bin log作为协调者，存储引擎则是参与者。

当客户端执行commit语句或者是在自动提交的情况下，MySQL内存开启一个XA事务，分两阶段完成XA事务的提交。

事务的提交被拆分为两部分，也就是将redo log的写入拆分成了两个步骤：prepare和commit,中间穿插写入bin log。

- prepare：将XID（内部XA事务的ID）写入到redo log，同时将redo log对应的事务状态设置为prepare，然后将redo log持久化到磁盘。

- commit：把XID写入到binlog,然后将bin log持久化到磁盘，接着调用引擎的提交事务接口，将redo log状态设置为commit，此时该状态不需要持久化到磁盘，只需要write到文件系统的page cache就可以了，因为只要bin log写磁盘成功，就算redo log的状态还是prepare也没有关系，一样被会认为事务执行成功。

两阶段提交是以bin log写成功为事务提交成功的标识。

### 两阶段提交的问题？

1. 磁盘I/O高：对于"双1"配置，每个事务提交都会有两次fsync，一次是redo log刷盘，另一次是bin log。

2. 锁竞争激烈：多事务的情况下，不能保证两者的提交顺序一致，因此，还需要一个锁来保证提交的原子性。

## 组提交机制

MySQL引入了组提交的机制，当有多个事务提交的时候，会将多个bin log刷盘操作合并成一个，从而减少了磁盘I/O操作的次数。

只针对commit阶段进行了修改，分成了三个过程：

1. flush阶段：多个事务按进入的顺序将bin log从cache写入文件（不刷盘）；

在MySQL5.7中，prepare阶段不再让各个事务各自执行redo log刷盘操作，而是推迟到组提交的flush阶段，也就是prepare阶段融合到flush阶段。

这个优化将redo log的刷盘延迟到flush阶段，通过延迟写redo log的方式，为redolog做了一次组写入。

2. sync阶段：对bin log文件做fsync操作（多个事务的bin log合并一次刷盘）；
3. commit阶段：各个事务按照顺序做InnoDB commit操作。
