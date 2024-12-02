-   [SST: Sorted String Table](#sst-sorted-string-table)
    -   [SST Builder](#sst-builder)
    -   [SST Iterator](#sst-iterator)
    -   [Block Cache](#block-cache)
-   [思考题](#思考题)

# SST: Sorted String Table

## SST Builder

SST是存储在磁盘上的数据块和索引块的集合。通常，数据块都是懒加载的。

SST Builder类似于Block Builder, 调用者都使用add函数。在SST Builder需要维护一个BlockBuilder，并且在必要时需要分离Blocks，同时要维护BlockMeta（包含起始和终止Key和每个Block的偏移量）。

编码后的SST如下：

``` txt
-------------------------------------------------------------------------------------------
|         Block Section         |          Meta Section         |          Extra          |
-------------------------------------------------------------------------------------------
| data block | ... | data block |            metadata           | meta block offset (u32) |
-------------------------------------------------------------------------------------------
```

实现如下：

<details>

``` rust
/// Builds an SSTable from key-value pairs.
pub struct SsTableBuilder {
    builder: BlockBuilder,
    first_key: KeyVec,
    last_key: KeyVec,
    data: Vec<u8>,
    pub(crate) meta: Vec<BlockMeta>,
    block_size: usize,
    key_hashes: Vec<u32>,
}

impl SsTableBuilder {
    /// Create a builder based on target block size.
    pub fn new(block_size: usize) -> Self {
        Self {
            data: Vec::new(),
            block_size,
            first_key: KeyVec::new(),
            last_key: KeyVec::new(),
            meta: Vec::new(),
            key_hashes: Vec::new(),
            builder: BlockBuilder::new(block_size),
        }
    }

    /// Adds a key-value pair to SSTable.
    ///
    /// Note: You should split a new block when the current block is full.(`std::mem::replace` may
    /// be helpful here)
    pub fn add(&mut self, key: KeySlice, value: &[u8]) {
        if self.first_key.is_empty() {
            self.first_key.set_from_slice(key);
        }
        self.key_hashes.push(farmhash::fingerprint32(key.raw_ref()));
        if self.builder.add(key, value) {
            self.last_key.set_from_slice(key);
            return;
        }
        self.finish_block();
        assert!(self.builder.add(key, value));
        self.first_key.set_from_slice(key);
        self.last_key.set_from_slice(key);
    }
    fn finish_block(&mut self) {
        let builder = std::mem::replace(&mut self.builder, BlockBuilder::new(self.block_size));
        let encoded_block = builder.build().encode();
        self.meta.push(BlockMeta {
            offset: self.data.len(),
            first_key: std::mem::take(&mut self.first_key).into_key_bytes(),
            last_key: std::mem::take(&mut self.last_key).into_key_bytes(),
        });
        let checksum = crc32fast::hash(&encoded_block);
        self.data.extend(encoded_block);
        self.data.put_u32(checksum);
    }
    /// Get the estimated size of the SSTable.
    ///
    /// Since the data blocks contain much more data than meta blocks, just return the size of data
    /// blocks here.
    pub fn estimated_size(&self) -> usize {
        self.data.len()
    }

    /// Builds the SSTable and writes it to the given path. Use the `FileObject` structure to manipulate the disk objects.
    pub fn build(
        mut self,
        id: usize,
        block_cache: Option<Arc<BlockCache>>,
        path: impl AsRef<Path>,
    ) -> Result<SsTable> {
        self.finish_block();
        let mut buf = self.data;
        let meta_offset = buf.len();
        BlockMeta::encode_block_meta(&self.meta, &mut buf);
        buf.put_u32(meta_offset as u32);
        let bloom = Bloom::build_from_key_hashes(
            &self.key_hashes,
            Bloom::bloom_bits_per_key(self.key_hashes.len(), 0.01),
        );
        let bloom_offset = buf.len();
        bloom.encode(&mut buf);
        buf.put_u32(bloom_offset as u32);
        let file = FileObject::create(path.as_ref(), buf)?;
        Ok(SsTable {
            id,
            file,
            first_key: self.meta.first().unwrap().first_key.clone(),
            last_key: self.meta.first().unwrap().last_key.clone(),
            block_meta: self.meta,
            block_meta_offset: meta_offset,
            block_cache,
            bloom: Some(bloom),
            max_ts: 0,
        })
    }

    #[cfg(test)]
    pub(crate) fn build_for_test(self, path: impl AsRef<Path>) -> Result<SsTable> {
        self.build(0, None, path)
    }
}
```

</details>

核心变量：

1.  builder: BlockBuilder。用于填充当前正在填充的块。

2.  first_key, last_key: 分别记录当前块的第一个和最后一个键，主要用于块元数据的生成。

3.  data: Vec`<u8>`{=html}: 存储所有已完成块的序列化数据和元数据。最终写入磁盘。

4.  meta: Vec`<BlockMeta>`{=html}: 存储每个块的元数据，用于在SSTable构建完成后写入表文件，用于快速查找目标块。

5.  key_hashes: Vec`<u32>`{=html}: 存储每个键的哈希值，主要用于构建布隆过滤器。

## SST Iterator

这一部分就是在SST上能够像其他迭代器一样工作。这里有一个SsTableIterator要实现StorageIterator的Trait, 这样才能与其他迭代器一同使用。

这里的搜索算法大多为二分搜索（重点：Vec的partition_point方法是二分搜索）。

<details>

``` rust
/// An iterator over the contents of an SSTable.
pub struct SsTableIterator {
    table: Arc<SsTable>,
    blk_iter: BlockIterator,
    blk_idx: usize,
}

impl SsTableIterator {
    fn seek_to_first_inner(table: &Arc<SsTable>) -> Result<(usize, BlockIterator)> {
        Ok((
            0,
            BlockIterator::create_and_seek_to_first(table.read_block_cached(0)?),
        ))
    }
    /// Create a new iterator and seek to the first key-value pair in the first data block.
    pub fn create_and_seek_to_first(table: Arc<SsTable>) -> Result<Self> {
        let (blk_idx, blk_iter) = Self::seek_to_first_inner(&table)?;
        let iter = Self {
            blk_iter,
            table,
            blk_idx,
        };
        Ok(iter)
    }

    /// Seek to the first key-value pair in the first data block.
    pub fn seek_to_first(&mut self) -> Result<()> {
        let (blk_idx, blk_iter) = Self::seek_to_first_inner(&self.table)?;
        self.blk_idx = blk_idx;
        self.blk_iter = blk_iter;
        Ok(())
    }

    fn seek_to_key_inner(table: &Arc<SsTable>, key: KeySlice) -> Result<(usize, BlockIterator)> {
        let mut blk_idx = table.find_block_idx(key);
        let mut blk_iter =
            BlockIterator::create_and_seek_to_key(table.read_block_cached(blk_idx)?, key);
        if !blk_iter.is_valid() {
            blk_idx += 1;
            if blk_idx < table.num_of_blocks() {
                blk_iter =
                    BlockIterator::create_and_seek_to_first(table.read_block_cached(blk_idx)?);
            }
        }
        Ok((blk_idx, blk_iter))
    }
    /// Create a new iterator and seek to the first key-value pair which >= `key`.
    pub fn create_and_seek_to_key(table: Arc<SsTable>, key: KeySlice) -> Result<Self> {
        let (blk_idx, blk_iter) = Self::seek_to_key_inner(&table, key)?;
        let iter = Self {
            blk_iter,
            table,
            blk_idx,
        };
        Ok(iter)
    }

    /// Seek to the first key-value pair which >= `key`.
    /// Note: You probably want to review the handout for detailed explanation when implementing
    /// this function.
    pub fn seek_to_key(&mut self, key: KeySlice) -> Result<()> {
        let (blk_idx, blk_iter) = Self::seek_to_key_inner(&self.table, key)?;
        self.blk_iter = blk_iter;
        self.blk_idx = blk_idx;
        Ok(())
    }
}

impl StorageIterator for SsTableIterator {
    type KeyType<'a> = KeySlice<'a>;

    /// Return the `key` that's held by the underlying block iterator.
    fn key(&self) -> KeySlice {
        self.blk_iter.key()
    }

    /// Return the `value` that's held by the underlying block iterator.
    fn value(&self) -> &[u8] {
        self.blk_iter.value()
    }

    /// Return whether the current block iterator is valid or not.
    fn is_valid(&self) -> bool {
        self.blk_iter.is_valid()
    }

    /// Move to the next `key` in the block.
    /// Note: You may want to check if the current block iterator is valid after the move.
    fn next(&mut self) -> Result<()> {
        self.blk_iter.next();
        if !self.blk_iter.is_valid() {
            self.blk_idx += 1;
            if self.blk_idx < self.table.num_of_blocks() {
                self.blk_iter = BlockIterator::create_and_seek_to_first(
                    self.table.read_block_cached(self.blk_idx)?,
                );
            }
        }
        Ok(())
    }
}
```

</details>

这里，查找的block的布局如下：

``` txt
--------------------------------------
| block 1 | block 2 |   block meta   |
--------------------------------------
| a, b, c | e, f, g | 1: a/c, 2: e/g |
--------------------------------------
```

这里，所有的block对应的区间都不重叠。但是如果seek_to_key(d)这样子做，就会到block 1的位置，然后要判断迭代器是否有效。

## Block Cache

Block Cache是通过moka-rs来实现的。

``` rust
pub type BlockCache = moka::sync::Cache<(usize, usize), Arc<Block>>;
```

这里存的是(sst_id, block_id)的结构，可以用try_get_with来从缓存中获取。这里要是实现read_block_cached函数。

``` rust
    /// Read a block from disk, with block cache. (Day 4)
    pub fn read_block_cached(&self, block_idx: usize) -> Result<Arc<Block>> {
        if let Some(ref block_cache) = self.block_cache {
            let blk = block_cache
                .try_get_with((self.id, block_idx), || self.read_block(block_idx))
                .map_err(|e| anyhow!("{}", e))?;
            Ok(blk)
        } else {
            self.read_block(block_idx)
        }
    }
```

# 思考题

1.  查找一个键的时间复杂度：

定位块（find_block_idx 方法）：

    使用 partition_point 方法在 block_meta 中定位键所在的块。
    由于 block_meta 是有序的，partition_point 使用的是二分查找算法。
    时间复杂度为 O(log N)，其中 N 是数据块的数量。

在块内查找键：

    SST 的实现假设每个块内部的键值对也是有序的，因此通常使用二分查找来定位键。
    假设每个块中有 MM 个键值对，则在块内查找的时间复杂度为 O(log M)。

因此查找一个键的时间复杂度是 O( Log N + Log M )。

2.  查找一个不存在的键时，光标停止的位置：

块查找阶段：

    使用 partition_point 方法，找到第一个键大于目标键的块。
    saturating_sub(1) 确保在目标块的范围内（防止越界）。
    若键小于所有块的 first_key，则定位到第一个块。

块内查找阶段：

    如果在块内查找不到目标键，通常光标会停在块中第一个比目标键大的位置，或者块的末尾。

因此，光标的位置取决于：

    若键在某块范围内，则光标停在块内最接近目标键的位置。
    若键比全表所有键都小，则光标停在第一个块。
    若键比全表所有键都大，则光标停在最后一个块。
