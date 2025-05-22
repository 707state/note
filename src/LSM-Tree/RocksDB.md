## RocksDB基本知识
memtable, sstfile,logfile是RocksDB的三个基本结构。
`Get(key)`, `NewIterator()`, `Put(key, val)`, Delete(key), SingleDelete(key)是RocksDB的基本操作。

### Column Family
RocksDB支持把一个数据库实例分区为多个Column families，默认有一个名为default的column family。多个column families之间具有一致性。
### Gets, Iterators and Snapshots
key和value在rocksdb里面都是纯粹的字节流。Snapshot相当于创建一个View，可以读取数据。
### Prefix Iterator
RocksDB通过bloom filter避免对不包含key的数据文件进行查询。