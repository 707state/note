# Mem Table Iterator

这一部分需要实现Scan函数，Scan函数会通过一个迭代器API按顺序返回一组键值对。

所有的LSM 迭代器都要实现StorageIterator这一个Trait，也就是实现key, value, next和is_valid这四个函数。

## 实现Memtable Iterator

next用来移动到下一个位置，is_valid返回迭代器是否走到终点，当is_valid为真时可以继续移动。

当迭代器无法有效避免用户滥用迭代器时，有FusedIterator会阻塞next调用。

对于memtable iterator, 迭代器并没有与之关联的生命周期，不向SkipMap的iter API会返回带有生命周期的迭代器。

如果迭代器并没有生命周期的泛型参数，我们就要保证任何时候，在迭代器被使用时，对应的SKipList中的对象不会被释放。唯一能保证这点的就是用Arc<SkipMap>。

因此MemtableIterator就要如下定义：

```rust
pub struct MemtableIterator {
    map: Arc<SkipMap<Bytes, Bytes>>,
    iter: SkipMapRangeIter<'???>,
}
```

这里，我们怎么表达迭代器和map有一样的生命周期呢？这就是Rust语言中的一个技巧型的特质：Self-referential Structure。

这里的常见实现方案有Pin和ouroboros。
最终的实现：

```rust
#[self_referencing]
pub struct MemTableIterator {
    /// Stores a reference to the skipmap.
    map: Arc<SkipMap<Bytes, Bytes>>,
    /// Stores a skipmap iterator that refers to the lifetime of `MemTableIterator` itself.
    #[borrows(map)]
    #[not_covariant]
    iter: SkipMapRangeIter<'this>,
    /// Stores the current key-value pair.
    item: (Bytes, Bytes),
}
```

对于几个API的实现如下：

```rust
impl MemTableIterator {
    fn entry_to_item(entry: Option<Entry<'_, Bytes, Bytes>>) -> (Bytes, Bytes) {
        entry
            .map(|x| (x.key().clone(), x.value().clone()))
            .unwrap_or_else(|| (Bytes::from_static(&[]), Bytes::from_static(&[])))
    }
}
impl StorageIterator for MemTableIterator {
    type KeyType<'a> = KeySlice<'a>;

    fn value(&self) -> &[u8] {
        &self.borrow_item().1[..]
    }

    fn key(&self) -> KeySlice {
        KeySlice::from_slice(&self.borrow_item().0[..])
    }

    fn is_valid(&self) -> bool {
        !self.borrow_item().0.is_empty()
    }

    fn next(&mut self) -> Result<()> {
        let entry = self.with_iter_mut(|iter| MemTableIterator::entry_to_item(iter.next()));
        self.with_mut(|x| *x.item = entry);
        Ok(())
    }
}
```

## Merge Iterator

Merge Iterator的主要作用是合并多个有序的迭代器，并为调用方提供一个统一的、案件排序的视图。以便同时查询多个Memtables或者SSTable的内容。

管理的内容：

1. 多个有序迭代器。MergeIterator可以管理一组已经按键排序的迭代器；每个迭代器代表一个数据来源。

2. 优先队列：他维护一个二叉堆，用于追踪当前各个迭代器中键值最小的项；如果同一个键在多个迭代器中出现，MergeIterator可以根据规则选择优先级最高的。

3. 当前结果项：记录当前选中的键值对，每次调用next时，更新当前项并保证堆的正确性。


如果next返回一个错误，就说明这个项已经不再有效，这时需要从堆里面取出这个元素。

对应的实现如下：

<details>


```rust
/// Merge multiple iterators of the same type. If the same key occurs multiple times in some
/// iterators, prefer the one with smaller index.
pub struct MergeIterator<I: StorageIterator> {
    iters: BinaryHeap<HeapWrapper<I>>,
    current: Option<HeapWrapper<I>>,
}

impl<I: StorageIterator> MergeIterator<I> {
    pub fn create(iters: Vec<Box<I>>) -> Self {
        if iters.is_empty() {
            return Self {
                iters: BinaryHeap::new(),
                current: None,
            };
        };
        let mut heap = BinaryHeap::new();
        if iters.iter().all(|x| !x.is_valid()) {
            //全部有效，选择最后一个作为当前的
            let mut iters = iters;
            return Self {
                iters: heap,
                current: Some(HeapWrapper(0, iters.pop().unwrap())),
            };
        }
        for (idx, iter) in iters.into_iter().enumerate() {
            if iter.is_valid() {
                heap.push(HeapWrapper(idx, iter));
            }
        }
        let current = heap.pop().unwrap();
        Self {
            iters: heap,
            current: Some(current),
        }
    }
}

impl<I: 'static + for<'a> StorageIterator<KeyType<'a> = KeySlice<'a>>> StorageIterator
    for MergeIterator<I>
{
    type KeyType<'a> = KeySlice<'a>;

    fn key(&self) -> KeySlice {
        self.current.as_ref().unwrap().1.key()
    }

    fn value(&self) -> &[u8] {
        self.current.as_ref().unwrap().1.value()
    }

    fn is_valid(&self) -> bool {
        self.current
            .as_ref()
            .map(|x| x.1.is_valid())
            .unwrap_or(false)
    }

    fn next(&mut self) -> Result<()> {
        let current = self.current.as_mut().unwrap();
        while let Some(mut inner_iter) = self.iters.peek_mut() {
            if inner_iter.1.key() == current.1.key() {
                //第一种情况，调用next的时候发生错误
                if let e @ Err(_) = inner_iter.1.next() {
                    PeekMut::pop(inner_iter);
                    return e;
                }
                // 第二种情况，iter不再有效
                if !inner_iter.1.is_valid() {
                    PeekMut::pop(inner_iter);
                }
            } else {
                break;
            }
        }
        current.1.next()?;
        //如果不再有效，就从堆里面弹出并选择下一个
        if !current.1.is_valid() {
            if let Some(iter) = self.iters.pop() {
                *current = iter;
            }
            return Ok(());
        }
        //否则，与当前堆的顶部进行比较，如果current较小就要交换
        if let Some(mut inner_iter) = self.iters.peek_mut() {
            if *current < *inner_iter {
                std::mem::swap(&mut *inner_iter, current);
            }
        }
        Ok(())
    }
    fn num_active_iterators(&self) -> usize {
        self.iters
            .iter()
            .map(|x| x.1.num_active_iterators())
            .sum::<usize>()
            + self
                .current
                .as_ref()
                .map(|x| x.1.num_active_iterators())
                .unwrap_or(0)
    }
}
```

</details>


## LSM Iterator + Fused Iterator

在LSM Tree的内部，用LSM Iterator作为迭代器，当更多的迭代器加入到系统中是需要修改树的结构。

目前的定义如下：
```rust
type LsmIterator=MergeIterator<MemTableIterator>;
```

为了提高安全性和避免滥用，还需要一层FusedIterator的封装，确保无效的迭代器不会用key, value或者next方法。

LsmIterator用于遍历LSM Tree，对LSM Tree的数据进行统一抽象，提供一个简单的接口用于查询数据，同时支持对删除标记（空值）和边界约束的处理。

核心功能：

1. 封装合并后的迭代器：LsmIteratorInner负责从数据源中合并有序数据；LsmIterator封装了它，加入了额外的逻辑，比如过滤删除标记和边界检查。

2. 处理删除标记：删除标记在LSM树中用作逻辑删除的标识，通常以空值表示；move_to_non_delete方法会跳过所有包含删除标记的键，确保数据是有效的。

3. 支持查询边界：end_bound用来表示查询终止的条件，并且检查是否越过边界。

4. 统一接口：通过实现StorageIterator Trait，提供了key, next, value和num_active_iterators的接口。

具体实现：

```rust
/// Represents the internal type for an LSM iterator. This type will be changed across the tutorial for multiple times.
type LsmIteratorInner = MergeIterator<MemTableIterator>;

pub struct LsmIterator {
    inner: LsmIteratorInner,
    end_bound: Bound<Bytes>,
    is_valid: bool,
}

impl LsmIterator {
    pub(crate) fn new(iter: LsmIteratorInner, end_bound: Bound<Bytes>) -> Result<Self> {
        let mut iter = Self {
            is_valid: iter.is_valid(),
            inner: iter,
            end_bound,
        };
        iter.move_to_non_delete()?;
        Ok(iter)
    }
    fn next_inner(&mut self) -> Result<()> {
        self.inner.next()?;
        if !self.inner.is_valid() {
            self.is_valid = false;
            return Ok(());
        }
        match self.end_bound.as_ref() {
            Bound::Unbounded => {}
            Bound::Included(key) => self.is_valid = self.inner.key().raw_ref() <= key.as_ref(),
            Bound::Excluded(key) => self.is_valid = self.inner.key().raw_ref() < key.as_ref(),
        }
        Ok(())
    }
    fn move_to_non_delete(&mut self) -> Result<()> {
        while self.is_valid() && self.inner.value().is_empty() {
            self.next_inner()?;
        }
        Ok(())
    }
}

impl StorageIterator for LsmIterator {
    type KeyType<'a> = &'a [u8];

    fn is_valid(&self) -> bool {
        self.is_valid
    }

    fn key(&self) -> &[u8] {
        self.inner.key().raw_ref()
    }

    fn value(&self) -> &[u8] {
        self.inner.value()
    }

    fn next(&mut self) -> Result<()> {
        self.next_inner()?;
        self.move_to_non_delete()?;
        Ok(())
    }
    fn num_active_iterators(&self) -> usize {
        self.inner.num_active_iterators()
    }
}
```

相对而言，FusedIterator的实现要简单很多。

```rust
pub struct FusedIterator<I: StorageIterator> {
    iter: I,
    has_errored: bool,
}

impl<I: StorageIterator> FusedIterator<I> {
    pub fn new(iter: I) -> Self {
        Self {
            iter,
            has_errored: false,
        }
    }
}

impl<I: StorageIterator> StorageIterator for FusedIterator<I> {
    type KeyType<'a>
        = I::KeyType<'a>
    where
        Self: 'a;

    fn is_valid(&self) -> bool {
        !self.has_errored && self.iter.is_valid()
    }

    fn key(&self) -> Self::KeyType<'_> {
        if !self.is_valid() {
            panic!("invalid access to the underlying iterator");
        }
        self.iter.key()
    }

    fn value(&self) -> &[u8] {
        if !self.is_valid() {
            panic!("invalid access to the underlying iterator");
        }
        self.iter.value()
    }

    fn next(&mut self) -> Result<()> {
        if self.has_errored {
            bail!("the iterator is tainted");
        }
        if self.iter.is_valid() {
            if let Err(e) = self.iter.next() {
                self.has_errored = true;
                return Err(e);
            }
        }
        Ok(())
    }
}
```



