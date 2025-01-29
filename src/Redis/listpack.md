# Listpack数据结构体系

在Redis中，listpack是一种高效的内存数据结构，设计用于存储较小的元素集合，如短字符串或整数。它是Redis 5.0引入的一部分，用于优化内存使用和性能，主要应用于诸如ZSET和LIST等数据结构的底层实现。

```c
/* Each entry in the listpack is either a string or an integer. */
typedef struct {
    /* When string is used, it is provided with the length (slen). */
    unsigned char *sval;
    uint32_t slen;
    /* When integer is used, 'sval' is NULL, and lval holds the value. */
    long long lval;
} listpackEntry;
```

## 特点

1.	紧凑的二进制格式
•	listpack存储的是连续的二进制数据，每个元素紧密相邻，没有额外的指针或间隙。
•	每个元素包括一个长度字段、一个编码字段和实际数据，支持存储小整数或短字符串。
2.	高效的内存利用
•	它通过对元素使用变长编码来减少内存浪费。例如，小整数会以最紧凑的形式存储，而较大的整数或字符串会用更长的格式存储。
3.	快速的遍历和修改
•	listpack通过顺序存储的方式，使得遍历所有元素的性能非常高。
•	添加或删除元素时，需要移动后续元素来保持连续性，但在小型集合中，这种操作代价较低。

## lpNext

获取当前条目的下一个条目的指针。

```c
/* If 'p' points to an element of the listpack, calling lpNext() will return
 * the pointer to the next element (the one on the right), or NULL if 'p'
 * already pointed to the last element of the listpack. */
unsigned char *lpNext(unsigned char *lp, unsigned char *p) {
    assert(p);
    p = lpSkip(p);
    if (p[0] == LP_EOF) return NULL;
    lpAssertValidEntry(lp, lpBytes(lp), p);
    return p;
}
```

这里调用的lpSkip就是跳过当前条目并返回指向下一个条目的指针。


