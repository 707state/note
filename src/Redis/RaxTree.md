<!--toc:start-->
- [什么是Radix Tree?](#什么是radix-tree)
- [实现](#实现)
  - [raxLowWalk](#raxlowwalk)
  - [raxGenericInsert的逻辑](#raxgenericinsert的逻辑)
    - [返回值不等于len并且是一个压缩节点](#返回值不等于len并且是一个压缩节点)
    - [是一个压缩节点并且返回值等于len](#是一个压缩节点并且返回值等于len)
<!--toc:end-->

# 什么是Radix Tree?
我们看到字典树，即一个Trie Tree，但是字典树针对具有相同前缀的结构并没有进行压缩，会导致树的深度非常深，因此在Trie Tree的基础上，对于公共前缀进行了优化，产生了压缩字典树/Radix Tree。

# 实现

```c
typedef struct raxNode {
    uint32_t iskey:1;     /* Does this node contain a key? */
    uint32_t isnull:1;    /* Associated value is NULL (don't store it). */
    uint32_t iscompr:1;   /* Node is compressed. */
    uint32_t size:29;     /* Number of children, or compressed string len. */
    /* Data layout is as follows:
     *
     * If node is not compressed we have 'size' bytes, one for each children
     * character, and 'size' raxNode pointers, point to each child node.
     * Note how the character is not stored in the children but in the
     * edge of the parents:
     *
     * [header iscompr=0][abc][a-ptr][b-ptr][c-ptr](value-ptr?)
     *
     * if node is compressed (iscompr bit is 1) the node has 1 children.
     * In that case the 'size' bytes of the string stored immediately at
     * the start of the data section, represent a sequence of successive
     * nodes linked one after the other, for which only the last one in
     * the sequence is actually represented as a node, and pointed to by
     * the current compressed node.
     *
     * [header iscompr=1][xyz][z-ptr](value-ptr?)
     *
     * Both compressed and not compressed nodes can represent a key
     * with associated data in the radix tree at any level (not just terminal
     * nodes).
     *
     * If the node has an associated key (iskey=1) and is not NULL
     * (isnull=0), then after the raxNode pointers poiting to the
     * children, an additional value pointer is present (as you can see
     * in the representation above as "value-ptr" field).
     */
    unsigned char data[];
} raxNode;

typedef struct rax {
    raxNode *head;
    uint64_t numele;
    uint64_t numnodes;
} rax;
```

在Rax当中，RaxInsert和RaxTryInsert都是对RaxGenericInsert的封装，其中有一个标志位overwrite表示是否覆盖。

在RaxGenericInsert中，核心函数是raxLowWalk，这个函数接受一个rax和一个字符指针（key），传入一个stopnode, plink(parentlink), splitpos作为要获得的结果。

## raxLowWalk

在raxRemove中，如果返回的值与len相等，就代表找到了对应的子节点；否则就意味着出现了字符mismatch。

如果传入参数不为空，那么stopnode就是raxLowWalk停止的位置，plink就是在父节点中对应的link。

如果在一个压缩的节点处搜索停止了，splitpos就会标记压缩节点的index。

如果在一个压缩节点的中间处停止，返回的同样是与len相同的长度，并且splitpos一定是e正数。

如果在压缩节点的中间处停止，且splitpos为0，就意味着当前的节点就是用来表示key的节点，而压缩节点中的字符并不需要。

## raxGenericInsert的逻辑

如果raxLowWalk返回的结果与len相同且返回节点不在中间（是就表示压缩节点）或者这个raxNode被标记为不是压缩节点，就只需要根据overwrite来判断是覆盖数据还是修改数据，这里可能需要分配更大空间。

如果停在一个压缩节点的中间，就意味着需要一个分割。

这里涉及到多个算法：

### 返回值不等于len并且是一个压缩节点

首先保存下一个指针，设置额外长度，创建分割节点，如果OOM就返回（释放刚分配的节点），之后就根据分割位置选择是直接替换旧的节点还是分割原本的压缩节点。

之后创建后缀节点（分割后剩余的部分），根据postfix的长度（可能为0）来决定。

把splitnode的第一个孩子设置为postfix node。

之后就继续插入。

### 是一个压缩节点并且返回值等于len
首先保存下一个节点，然后创建后缀节点，分割当前的压缩节点。
