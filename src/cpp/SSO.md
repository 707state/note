# Short String Optimization

短字符串优化，C++引入的一个重要优化。

我们看看一个典型的string需要哪些字段？

length, capacity, pointer, 其中，pointer8个字节，length, capacity各8个，这就24个字节了。（pointer指向堆上分配的内存）

这合理吗？假如我有一个std::string s="hello"的字符串，它本身只有6字节长（结尾用一个null），却要用24+6个字节来存放，这显然不合理。

所以，除了必要的length字段，别的几个字段都可以优化掉。

如此一来，就避免在堆上分配内存，直接管理了结构存储。

一个实现如下：
```cpp
class string {
  //
private:
    size_type m_size;
    union {
        class {
            // This is probably better designed as an array-like class
            std::unique_ptr<char[]> m_data;
            size_type m_capacity;
        } m_large;
        std::array<char, sizeof(m_large)> m_small;
    };
};
```

实际的优化要比这复杂地多，因为还要考虑Copy-On-Write来实现额外的内务字段。

# 拓展

## 设想一下另一种场景，大量的小字符串，比较频繁的分配和回收，字符串有一定的相似度（即可能存在长字串的部分被匹配到新的更小的字符串的情况）

这时候或许可以考虑分配一块堆空间，然后自行控制每个字符串的分配。

可以考虑不采用 Ascii 方式存储字符串，而是转而使用头尾指针的方式。注意由于增加了一个额外的指针，所以有更多的管理消耗。但是这样做看似没能从去除字串零结尾的手法中获得利益，但却有利于表达部分匹配的字串片段。所以当整个字串集合中的部分匹配的状况较多时，这么做还是有益的。

如果想象一下扩大整体集合的规模，在大数据的场景中考虑，例如数亿篇文章中进行词汇反排，那么上述的方法仍然有一定的好处。


