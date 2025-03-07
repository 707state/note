-   [Block](#block)
    -   [BlockBuilder实现](#blockbuilder实现)
    -   [BlockIterator
        实现](#blockiterator-实现)
    -   [思考题](#思考题)

# Block

这一部分是用来构建LSM
Tree在磁盘上的部分的，先前已经编写了在内存中的部分，这里需要处理日志有关的逻辑。

磁盘结构的基础就是Block, 一个Block通常是4 KB,
Block中存放的是有序的键值对。

一个SST(Sorted String
Table)由多个Block构成的，当MemTables的数量超过限制时，MemTable就会被落盘设置为SST。

## BlockBuilder实现

``` rust
/// Builds a block.
pub struct BlockBuilder {
    /// Offsets of each key-value entries.
    offsets: Vec<u16>,
    /// All serialized key-value pairs in the block.
    data: Vec<u8>,
    /// The expected block size.
    block_size: usize,
    /// The first key in the block
    first_key: KeyVec,
}

fn compute_overlap(first_key: KeySlice, key: KeySlice) -> usize {
    let mut i = 0;
    loop {
        if i >= first_key.len() || i >= key.len() {
            break;
        }
        if first_key.raw_ref()[i] != key.raw_ref()[i] {
            break;
        }
        i += 1;
    }
    i
}

impl BlockBuilder {
    /// Creates a new block builder.
    pub fn new(block_size: usize) -> Self {
        Self {
            offsets: Vec::new(),
            data: Vec::new(),
            block_size,
            first_key: KeyVec::new(),
        }
    }

    fn estimated_size(&self) -> usize {
        SIZEOF_U16 + self.offsets.len() * SIZEOF_U16 + self.data.len()
    }

    /// Adds a key-value pair to the block. Returns false when the block is full.
    #[must_use]
    pub fn add(&mut self, key: KeySlice, value: &[u8]) -> bool {
        assert!(!key.is_empty(), "key must not be empty");
        if self.estimated_size() + key.len() + value.len() + SIZEOF_U16 * 3 > self.block_size
            && !self.is_empty()
        {
            return false;
        }
        self.offsets.push(self.data.len() as u16);
        let overlap = compute_overlap(self.first_key.as_key_slice(), key);
        self.data.put_u16(overlap as u16);
        self.data.put_u16((key.len() - overlap) as u16);
        self.data.put(&key.raw_ref()[overlap..]);
        self.data.put_u16(value.len() as u16);
        self.data.put(value);
        if self.first_key.is_empty() {
            self.first_key = key.to_key_vec();
        }
        true
    }

    /// Check if there is no key-value pair in the block.
    pub fn is_empty(&self) -> bool {
        self.offsets.is_empty()
    }

    /// Finalize the block.
    pub fn build(self) -> Block {
        if self.is_empty() {
            panic!("block should not be empty");
        }
        Block {
            data: self.data,
            offsets: self.offsets,
        }
    }
}
```

这一部分涉及到每一个Block的磁盘格式布局，如下：

``` txt
----------------------------------------------------------------------------------------------------
|             Data Section             |              Offset Section             |      Extra      |
----------------------------------------------------------------------------------------------------
| Entry #1 | Entry #2 | ... | Entry #N | Offset #1 | Offset #2 | ... | Offset #N | num_of_elements |
----------------------------------------------------------------------------------------------------
```

每一个Entry的布局如下：

``` txt
-----------------------------------------------------------------------
|                           Entry #1                            | ... |
-----------------------------------------------------------------------
| key_len (2B) | key (keylen) | value_len (2B) | value (varlen) | ... |
-----------------------------------------------------------------------
```

具体实现体现在add部分。

## BlockIterator 实现

上一部分实现了一个磁盘块的编码，现在要实现迭代，这样就可以在磁盘中寻找key/value。

BlockIterator的代码实现：

<details>

``` rust
/// Iterates on a block.
pub struct BlockIterator {
    /// The internal `Block`, wrapped by an `Arc`
    block: Arc<Block>,
    /// The current key, empty represents the iterator is invalid
    key: KeyVec,
    /// the current value range in the block.data, corresponds to the current key
    value_range: (usize, usize),
    /// Current index of the key-value pair, should be in range of [0, num_of_elements)
    idx: usize,
    /// The first key in the block
    first_key: KeyVec,
}

impl Block {
    fn get_first_key(&self) -> KeyVec {
        let mut buf = &self.data[..];
        buf.get_u16();
        let key_len = buf.get_u16();
        let key = &buf[..key_len as usize];
        KeyVec::from_vec(key.to_vec())
    }
}

impl BlockIterator {
    fn new(block: Arc<Block>) -> Self {
        Self {
            block,
            key: KeyVec::new(),
            value_range: (0, 0),
            idx: 0,
            first_key: KeyVec::new(),
        }
    }

    /// Creates a block iterator and seek to the first entry.
    pub fn create_and_seek_to_first(block: Arc<Block>) -> Self {
        let mut iter = Self::new(block);
        iter.seek_to_first();
        iter
    }

    /// Creates a block iterator and seek to the first key that >= `key`.
    pub fn create_and_seek_to_key(block: Arc<Block>, key: KeySlice) -> Self {
        let mut iter = Self::new(block);
        iter.seek_to_key(key);
        iter
    }

    /// Returns the key of the current entry.
    pub fn key(&self) -> KeySlice {
        debug_assert!(!self.key.is_empty(), "invalid iterator");
        self.key.as_key_slice()
    }

    /// Returns the value of the current entry.
    pub fn value(&self) -> &[u8] {
        debug_assert!(!self.key.is_empty(), "invalid iteraator");
        &self.block.data[self.value_range.0..self.value_range.1]
    }

    /// Returns true if the iterator is valid.
    /// Note: You may want to make use of `key`
    pub fn is_valid(&self) -> bool {
        !self.key.is_empty()
    }

    /// Seeks to the first key in the block.
    pub fn seek_to_first(&mut self) {
        self.seek_to(0);
    }
    /// 找到这个块中第idx个键
    fn seek_to(&mut self, idx: usize) {
        if idx >= self.block.offsets.len() {
            self.key.clear();
            self.value_range = (0, 0);
            return;
        }
        let offset = self.block.offsets[idx] as usize;
        self.idx = idx;
    }

    /// Move to the next key in the block.
    pub fn next(&mut self) {
        self.idx += 1;
        self.seek_to(self.idx);
    }

    fn seek_to_offset(&mut self, offset: usize) {
        let mut entry = &self.block.data[offset..];
        let overlap_len = entry.get_u16() as usize;
        let key_len = entry.get_u16() as usize;
        let key = &entry[..key_len];
        self.key.clear();
        self.key.append(&self.first_key.raw_ref()[..overlap_len]);
        self.key.append(key);
        entry.advance(key_len);
        let value_len = entry.get_u16() as usize;
        let value_offset_begin = offset + SIZEOF_U16 + 3 * SIZEOF_U16 + key_len;
        let value_offset_end = value_offset_begin + value_len;
        self.value_range = (value_offset_begin, value_offset_end);
        entry.advance(value_len);
    }
    /// Seek to the first key that >= `key`.
    /// Note: You should assume the key-value pairs in the block are sorted when being added by
    /// callers.
    pub fn seek_to_key(&mut self, key: KeySlice) {
        let mut low = 0;
        let mut high = self.block.offsets.len();
        while low < high {
            let mid = low + (high - low) / 2;
            self.seek_to(mid);
            assert!(self.is_valid());
            match self.key().cmp(&key) {
                std::cmp::Ordering::Less => low = mid + 1,
                Ordering::Greater => high = mid,
                Ordering::Equal => return,
            }
        }
        self.seek_to(low);
    }
}
```

</details>

BlockIterator可以用Arc`<Block>`{=html}的方式创建，当create_and_seek_to_first调用时，淡灰把当前迭代器放置到Block的第一个键的位置。当create_and_seek_to_key被调用时，迭代器会被放置到第一个Key\>=指定值的位置。

当next调用时，迭代器会移动到下一个位置，如果到达末端，则key会被设置为空并且is_valid返回false。

## 思考题

1.  Block内查询一个Key的时间复杂度：

二分查找：查询键时会在block.offsets中通过二分查找找到目标键的索引。

增量压缩解码键：每次访问具体键时，需要对压缩存储的键进行解码。解码过程包括提取重叠部分（通过索引访问
first_key）和拼接差异部分。如果存储格式支持高效解码（例如简单拷贝），单次解码复杂度为
O(m)O(m)，其中 mm 是目标键的长度。

比较：O(m)。

所以是 O(m \* log n)。

2.  使用Byte和Arc`<u16>`{=html}来取代原来的Block中的数据，将所有迭代器返回&\[u8\]的地方改成Byte，这样做的好处/坏处是什么？

优点

更高效的内存管理 使用 Arc\<\[u16\]\> 代替
Vec`<u16>`{=html}，可以减少对偏移表的内存分配与拷贝，特别是在多线程访问同一个
Block 时。Arc 提供了共享引用计数管理，避免了多次克隆偏移表带来的开销。

降低数据切片的拷贝成本 将数据接口改为 Byte 并通过 Byte::slice
返回切片，可以直接基于底层存储返回视图，避免了创建 &\[u8\]
切片时潜在的拷贝或生命周期管理问题。
例如，在需要频繁切片数据的场景下，这种方法可以提高性能。

更灵活的数据管理 Byte
是一个更高层的抽象，可以封装更多的功能（例如对齐检查、按需加载等），未来扩展性更好。
而 &\[u8\] 是固定的借用切片，无法扩展底层行为。

更简洁的多线程支持 Byte 和 Arc\<\[u16\]\>
的结合使得数据和元数据可以轻松共享到多线程环境，减少锁或拷贝的复杂度。

缺点

接口复杂度增加 Byte 的抽象层次较高，相比直接返回
&\[u8\]，可能导致接口和实现复杂度的增加。 调用者需要学习 Byte
的使用方法，并且可能需要处理不熟悉的问题（如 Byte::slice
的返回值处理、引用生命周期等）。

潜在性能开销 如果 Byte
的实现不是极度轻量（例如增加了很多元数据），可能会引入额外的性能开销，尤其是在高频调用（如迭代器的
key 和 value 方法）中。

生态兼容性问题 许多现有的 Rust 库和工具对 &\[u8\] 的支持非常成熟，使用
Byte 可能需要额外的适配工作，降低了与第三方库的兼容性。

依赖共享引用的开销 使用 Arc\<\[u16\]\>
虽然简化了多线程共享，但对于单线程场景或不需要共享的简单用例，引用计数的管理可能带来不必要的性能损耗。
