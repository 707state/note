# Hashbrown 

Hashbrown 是一个高效的哈希表实现库。它提供了 HashMap 和 HashSet 的数据结构，并且被设计为性能优先。

Hashbrown 使用了一种叫做 SwissTable 的哈希表算法（由 Google 发明，并在 C++ 的 Abseil 库中实现）。

## SwissTable算法

SwissTable在C++的Abseil库中有一个实现，在Hashbrown中的实现细节如下：

### Open Addressing with Quadratic Probing

开放寻址，所有元素放在一个大的数组中，使用二次探测法解决冲突。

### Control Byte Arrays

Hashbrown 引入了一个“控制字节数组”来管理桶的状态。每个桶的控制字节可以表示以下几种状态：

    EMPTY：桶为空。
    DELETED：桶已被删除元素占用。
    FULL：桶中有一个元素。

这种控制字节数组与哈希桶结构分离，允许通过控制字节的状态快速查找、插入和删除元素，而不必直接遍历每个桶。这种方式加速了哈希查找操作，也减少了缓存未命中。

### Robin Hood Hashing

Hashbrown 还使用 Robin Hood Hashing 技术来保持哈希表的负载平衡。Robin Hood Hashing 是一种在插入时考虑“偏移”的哈希方法，插入时尽可能减少偏移量较大的元素。这种方法可以减少查找时的最大偏移量，从而加速查找操作。

当发生冲突时，新元素会比较当前插槽中的元素和自身的“偏移量”（即距离理想位置的距离），优先插入偏移量较小的元素，以减少长距离查找的情况。

### Dynamic Resizing

为了确保哈希表在高负载时仍能保持高性能，Hashbrown 在装载因子超过一定阈值时会进行动态扩展。扩展时会将哈希表的容量翻倍，重新分配内存并重新插入所有元素。通过这种方式，可以减少因过多冲突引起的性能下降。

### SIMD 

SwissTable 通过 SIMD 指令进行组查找，将查找操作并行化处理。例如，若目标插槽与当前位置不符，则使用 SIMD 一次性检查多个桶是否匹配目标的哈希值。这可以显著减少查找和插入的时间开销。

具体来说，每个哈希桶中只存储一部分哈希值，称为哈希指纹（hash fingerprints），这是哈希值的高几位。利用这个简化的哈希值可以在一组桶中快速定位元素，大大提升了哈希表的查找性能。

## 插入

两个步骤：

1. 搜索，如果返回一个结果就是已经有的键，那就把这个值覆盖。
2. 实际上的插入（寻找EMPTY或者DELETED的键）

## 删除

其实就是寻找key然后删除，重点是cleanup（清除）。

实际流程：

1. 从要删除的桶开始；

2. 加载从该桶位置开始的一个组的字节，检查该组中是否有 EMPTY 条目；

3. 记录第一个找到的 EMPTY 位置（称为 “end”）；

4. 再加载删除位置之前的一个字节组，搜索该组中是否有 EMPTY 条目；

5. 记录最后一个找到的 EMPTY 位置（称为 “start”）；

6. 如果 start 和 end 之间的距离大于或等于组的宽度 WIDTH，则将桶设为 DELETED；

7. 否则，将桶设为 EMPTY。

### 桶

桶（bucket） 是用来存储哈希表中每个元素的位置。每个桶可以包含一个元素或为空，它是哈希表用来组织和管理数据的基础单元。在实现上，桶是一个连续的内存位置，用于存储键值对或者表示桶的状态。

```rust
/// A reference to a hash table bucket containing a `T`.
///
/// This is usually just a pointer to the element itself. However if the element
/// is a ZST, then we instead track the index of the element in the table so
/// that `erase` works properly.
pub struct Bucket<T> {
    // Actually it is pointer to next element than element itself
    // this is needed to maintain pointer arithmetic invariants
    // keeping direct pointer to element introduces difficulty.
    // Using `NonNull` for variance and niche layout
    ptr: NonNull<T>,
}
```

#### 桶的结构

每个桶包含两个主要部分：

1. 控制字节

用来标记该桶的状态，Hashbrown 使用控制字节来表明桶是 FULL（已占用）、DELETED（被删除，但保留墓碑）、还是 EMPTY（空的）。

控制字节是一个简化的哈希值或状态标记，用来辅助哈希冲突的解决。

2. 实际的数据存储

当桶状态为 FULL 时，桶中会包含一个键值对（key-value）数据，实际的数据直接存放在桶内。
如果桶被标记为 DELETED 或 EMPTY，则表示该位置可以重新使用。

#### 作用

桶的作用在于优化查找、插入和删除的效率。Hashbrown 使用了 开放寻址（open addressing） 的方式来解决哈希冲突，而不是链式存储。因此，所有数据存储在同一个桶数组（bucket array）中，避免了指针的复杂操作。同时，桶通过控制字节数组与实际数据分离，极大提升了数据的内存局部性，使得查找和插入操作在 CPU 缓存中更有效。

#### 工作原理

插入：将元素放入其对应的哈希桶中，如果目标桶已占用，使用探测算法找到下一个可用的桶。
查找：通过桶的控制字节快速定位元素，如果有哈希冲突则沿着连续的桶组（contiguous group）查找，直到找到匹配元素或遇到空桶为止。
删除：在删除时，通过在桶上放置墓碑标记（DELETED）来保持搜索路径的连续性，必要时调整其他桶的状态。



