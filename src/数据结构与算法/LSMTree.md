# 什么是LSM Tree?

Log-Structured Merge Tree, 是一种分层，有序，面向磁盘的数据结构，其核心思想是充分了利用了，磁盘批量的顺序写要远比随机写性能高出很多。

相对于RB-Tree, B-Tree, 所有的修改操作都是惰性的。

LSM-Tree对于以下情况非常适用：

1. 数据在持久化存储上是不可变的，这使得并发控制更加简单。可以将后台任务（如压缩操作）卸载到远程服务器执行。直接从云原生存储系统（如 S3）中存储和服务数据也是可行的。

2. 更改压缩算法可以使存储引擎在读取、写入和空间放大之间找到平衡。LSM 树结构非常灵活，通过调整压缩参数，我们可以针对不同的工作负载优化 LSM 结构。

## 一个LSM Storage Engine

1. WAL日志机制，用来做数据持久化。

2. 硬盘上的SST(Sorted String Tables)，用来保持LSM Tree的结构。

3. Mem-Tables。是驻留在内存中的数据结构，用于暂存小的写入操作。小写入会被批量写入 Mem-table，等 Mem-table 满了后再刷新到磁盘生成新的 SST 文件。这种方式可以提高写入性能并减少磁盘 I/O 开销。

通常要提供以下操作：

1. Put(Key, Value);

2. Delete(Key);

3. Get(Key);

4. Scan(Range);

为了处理持久化，通常还会有:

Sync()，用来保存sync之前的所有操作都落盘。

# Mem Tables

Mem Tables适用于处理Batch Write批量写的。

## 用SkipList实现Mem Table

用cross-beam的SkipList来实现，因为cross-beam的SkipList支持无锁并发。并且都支持insert, get和iter。

```rust
pub struct MemTable {
    map: Arc<SkipMap<Bytes, Bytes>>,
    wal: Option<Wal>,
    id: usize,
    approximate_size: Arc<AtomicUsize>,
}
```
这是MemTable的结构体。需要实现get和delete方法。

```rust
    /// Get a value by key.
    pub fn get(&self, key: &[u8]) -> Option<Bytes> {
        self.map.get(key).map(|e| e.value().clone())
    }
```

get通过在SkipMap中寻找key来获取对应的值。

```rust
    pub fn put(&self, key: &[u8], value: &[u8]) -> Result<()> {
        //预估总长度
        let estimated_size = key.len() + value.len();
        //向SkipMap中插入键值对
        self.map
            .insert(Bytes::copy_from_slice(key), Bytes::copy_from_slice(value));
        //原子变量执行原子操作
        self.approximate_size
            .fetch_add(estimated_size, std::sync::atomic::Ordering::Relaxed);
        // WAL需要放入，实际上应该先做WAL
        if let Some(ref wal) = self.wal {
            wal.put(key, value)?;
        }
        Ok(())
    }
```
这是put方法的实现，要放入一个键值对到SkipMap中，并且为后面的WAL做准备。

注意，Mem Table不提供delete方法，在这里如果一个键对应的值是空的就视为被删除了。

## 一个MemTable的实现

```rust
/// Represents the state of the storage engine.
#[derive(Clone)]
pub struct LsmStorageState {
    /// The current memtable.
    pub memtable: Arc<MemTable>,
    /// Immutable memtables, from latest to earliest.
    pub imm_memtables: Vec<Arc<MemTable>>,
    /// L0 SSTs, from latest to earliest.
    pub l0_sstables: Vec<usize>,
    /// SsTables sorted by key range; L1 - L_max for leveled compaction, or tiers for tiered
    /// compaction.
    pub levels: Vec<(usize, Vec<usize>)>,
    /// SST objects.
    pub sstables: HashMap<usize, Arc<SsTable>>,
}
```
表示存储引擎的状态，在任何时刻，引擎中只能由一个可变的MemTable。当一个MemTable达到存储上限的时候就会被冻结为一个不可变的MemTable。

在lsm_storage.rs中有两个表示存储引擎的结构：MiniLSM和LsmStorageInner。MiniLSM是对于LsmStorageInner的一层封装。

LsmStorageInner存储了当前的LSM存储引擎的结构。需要在LsmStorageInner中实现get, put和delete方法

## 冻结一个MemTable

当一个MemTable的容量达到上限的时候就要把这个MemTable冻结起来。

在这里需要实现一个freeze_memtable的函数，这里有一些会造成性能问题的操作需要[注意](https://skyzh.github.io/mini-lsm/week1-01-memtable.html)。

注意看第三部分，里面解释了freeze_memtable的合适做法。

### Leveled Compaction 分层压缩

<details> 
<summary>展开查看</summary>

广泛应用于分布式数据库（如 RocksDB 和 LevelDB）。这种策略通过将数据分层组织并压缩，优化了查询性能和存储空间利用率。

LSM Tree的基本结构：

在 LSM 树中，数据首先写入内存表（memtable），然后定期刷新到磁盘，形成不可变的文件（称为 SSTable）。这些文件按照不同的层次组织，每一层存储一定范围的数据。

Leveled Compaction的核心思想：

将数据组织成多层（Levels），每一层具有以下特性：

1. 分层结构：从L0到Ln层级逐渐增加。

2. 层的大小递增：每一层的存储容量比上一层更大，通常是上一层的 10 倍。

3. 数据范围无重叠：L0 层的数据可能有重叠，因为它直接存储从内存刷盘的多个 SSTable 文件；L1 及以上，每个 SSTable 文件存储的数据范围不重叠，便于快速定位。

数据流动的过程：

写入L0: 
数据首先写入内存（memtable），然后刷新到磁盘生成 L0 层的 SSTable。

L0到L1的压缩:
当 L0 层文件数量超过阈值（比如 4 个文件），触发压缩操作；将 L0 中的所有文件与 L1 中的文件进行合并，写入新的 L1 文件，确保 L1 中的数据范围不重叠。

更高层次的压缩:
当 L1 的大小超过限制，触发向 L2 层的压缩；同样遵循合并并去重的逻辑，确保 L2 中的数据范围不重叠；这种过程会依次向更高层流动。

#### 优点

高效的点查询：从 L1 层开始，各层的文件之间数据范围不重叠，可以通过范围查找快速定位目标 SSTable；性能比Size-Tiered Compaction更好。

压缩节省空间：每次压缩会移除重复数据，确保存储效率。

适合读密集的场景：由于数据范围清晰，适合有大量点查询或者范围查询的场景。

#### 缺点

高压缩成本：压缩需要将多个文件合并成一个文件；对写入性能有一定影响。

高存储抖动：在压缩过程中，大量数据被压缩。

</details>

> 具体资料可见[RocksDB Wiki](https://github.com/facebook/rocksdb/wiki/Leveled-Compaction)

### Tiered Compaction 分级压缩

<details>
<summary>具体信息</summary>

与 Leveled Compaction 不同，Tiered Compaction 优先优化写入性能，通过将小文件简单归并，减少写入阻塞。它通常用于写密集型工作负载，比如日志存储或批量写入的场景。

核心概念：在 Tiered Compaction 中，数据分为多个层（Tiers），每一层都包含一组 SSTable 文件。这些文件内的数据范围可以重叠，层与层之间也可能有重叠。Tiered Compaction 的压缩操作是基于文件数量而不是数据范围重叠触发的。

数据流动的过程：

写入数据到内存：

1. 新数据首先写入内存；memtable满了之后，刷盘生成一个新的SSTable，这些文件直接写入L0。

2. L0层的文件合并，当 L0 层的文件数量超过预设的阈值（如 4 个文件），触发压缩操作。压缩的方式是将这些文件简单合并，生成更大的文件，写入下一层（L1 层）。L1 层的文件通常存储更多数据，其大小和数量的阈值也更高。

3. 当某一层的文件数量超过阈值时，这些文件会被合并为更大的文件并移到下一层；合并时只是将文件归并到一起，不一定消除重复数据，也不保证数据范围不重叠。

#### 优点

高写入性能：Tiered Compaction 避免了频繁的合并和去重操作，写入过程简单高效。写密集型的场景中，可以显著减少写阻塞。

实现简单：不需要维护分割的逻辑，只要根据文件数量就可以压缩。

#### 缺点

查询效率低：因为文件和层之间可能有重叠，点查询或范围查询时需要检查更多的 SSTable 文件。随着层数增加，文件数量可能显著增多，查询成本上升。

空间利用率低：重复文件的压缩。

读放大效应：由于需要扫描多个文件才能找到目标数据，读放大现象（读取无关数据的比例）更加明显。

#### 适用场景

Tiered Compaction 更适合 写密集型 场景，如：

    日志存储（Log Store）：需要快速写入大量日志数据。
    数据流处理：实时接收和存储大量数据。
    数据备份：批量导入数据的情况。

不适合查询密集型场景，因为查询效率较低。

</details>

> [不同压缩算法之间的比较](https://opensource.docs.scylladb.com/stable/architecture/compaction/compaction-strategies.html)


## 一些思考题

### 为什么Mem Table不需要一个delete API?

LSM Tree的核心思想是追加写入，所有数据的修改都是通过追加写入的方式实现的。

删除操作不会真的从mem table中添加或者在磁盘中删除，而是通过一个特殊标记（通常叫做tombstone）来实现逻辑上的删除。


