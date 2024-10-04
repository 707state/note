---
title: "Clocking Cache Algorithm"
author: "jask"
date: "09/30/2024"
output: pdf_document
header-includes:
  - \usepackage{fontspec}
  - \usepackage{xeCJK}
  - \setmainfont{ComicShannsMono Nerd Font}
  - \setCJKmainfont{Noto Sans CJK SC}  # 替换为可用的字体
  - \setCJKmonofont{Noto Sans CJK SC}
  - \setCJKsansfont{Noto Sans CJK SC}
  - \usepackage[top=1cm, bottom=1cm, left=1cm, right=1cm]{geometry}
---

# Clocking Cache 

时钟置换算法是一种性能和开销较均衡的算法，又称CLOCK算法，或最近未用算法（NRU，Not Recently Used）。

## 实现

简单的CLOCK 算法实现方法：为每个页面设置一个访问位，再将内存中的页面都通过链接指针链接成 一个循环队列。当某页被访问时，其访问位置为1。当需要淘汰一个页面时，只需检查页的访问位。 如果是0，就选择该页换出；如果是1，则将它置为0，暂不换出，继续检查下一个页面，若第一轮扫描中所有页面都是1，则将这些页面的访问位依次置为0后，再进行第二轮扫描（第二轮扫描中一定会有访问位为0的页面，因此简单的CLOCK 算法选择一个淘汰页面最多会经过两轮扫描）

## 代码

```rs 
    struct NodeType: u8 {
        const EMPTY     = 0b00001;
        const HOT       = 0b00010;
        const COLD      = 0b00100;
        const TEST      = 0b01000;
        const MASK      = Self::EMPTY.bits() | Self::HOT.bits() | Self::COLD.bits() | Self::TEST.bits();
        const REFERENCE = 0b10000;
    }
```

这个 NodeType 结构体的作用是为时钟算法提供节点状态的表示和管理。通过使用位标志，算法可以有效地存储和处理节点的多种状态。使用位运算提高了存储效率，并简化了状态的检查和更新。


```rs 
struct Node<K, V> {
    key: MaybeUninit<K>,
    value: Option<V>,
    node_type: NodeType,
}
```

key: MaybeUninit<K>:
MaybeUninit 是一个用于未初始化数据的类型，意味着 key 字段在创建时可能还未被初始化。这在需要优化性能或控制初始化时很有用。

value: Option<V>:
Option 是 Rust 中的一个枚举，表示可能存在的值或不存在的值。这里的 value 字段可以是一个 V 类型的值，或者是 None，表示该节点可能没有值。

node_type: NodeType:
node_type 字段是前面提到的 NodeType 枚举，用于标识该节点的状态，比如是否为空、热、冷、测试或引用。


```rs 
pub struct ClockProCache<K, V> {
    capacity: usize,
    test_capacity: usize,
    cold_capacity: usize,
    map: HashMap<K, Token>,
    ring: TokenRing,
    nodes: Vec<Node<K, V>>,
    hand_hot: Token,
    hand_cold: Token,
    hand_test: Token,
    count_hot: usize,
    count_cold: usize,
    count_test: usize,
    inserted: u64,
    evicted: u64,
}
```

ClockProCache 结构体实现了一种基于时钟算法的缓存，使用多种容量控制和节点管理策略，允许在不同频率的访问模式下高效地存储和检索数据。通过将热、冷和测试节点分开管理，它能够优化缓存命中率并有效处理数据。


