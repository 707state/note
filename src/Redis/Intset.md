# 整数集合是一种只存储整型数据的集合

采用连续内存布局实现紧凑存储，主要设计目标是节省内存，尤其是在存储小规模整型数据时。

```c
typedef struct intset {
    uint32_t encoding;
    uint32_t length;
    int8_t contents[];
} intset;
```

这是intset的核心结构。

encoding决定所有整数的存储方式，支持INTSET_ENC_INT16（每个整数占2字节），INTSET_ENC_INT32（4个字节），INTSET_ENC_INT64（8字节）。

length表示当前集合中实际存储的整数个数。

contents一个连续内存数组，存储所有整数，按照从小到大的顺序排列。

由于是连续存储，没有指针开销，因此非常节省内存。

## intset的核心思想

1.	连续内存布局：
	•	使用紧凑的数组存储数据，而不是指针链表，节省了内存。

2.	单一类型编码：
	•	Intset 中的所有整数使用统一的编码类型（16 位、32 位或 64 位）。
	•	编码类型会根据需要动态升级，确保能容纳更大的整数。

3.	升序排列：
	•	所有整数按升序存储，可以通过二分查找快速定位元素。


## intsetAdd

插入整数，先检查整数是否存在（二分查找），如果整数已存在，就直接返回，否则继续。

如果整数超过编码范围：

    ·  升级encoding（16位升级到32位）
    ·  升级过程中需要重新分配内存并转换已有数据。

找到插入位置，将整数插入到contents数组中，保持升序排列。

```c
/* Insert an integer in the intset */
intset *intsetAdd(intset *is, int64_t value, uint8_t *success) {
    uint8_t valenc = _intsetValueEncoding(value);
    uint32_t pos;
    if (success) *success = 1;

    /* Upgrade encoding if necessary. If we need to upgrade, we know that
     * this value should be either appended (if > 0) or prepended (if < 0),
     * because it lies outside the range of existing values. */
    if (valenc > intrev32ifbe(is->encoding)) {
        /* This always succeeds, so we don't need to curry *success. */
        return intsetUpgradeAndAdd(is,value);
    } else {
        /* Abort if the value is already present in the set.
         * This call will populate "pos" with the right position to insert
         * the value when it cannot be found. */
        if (intsetSearch(is,value,&pos)) {
            if (success) *success = 0;
            return is;
        }

        is = intsetResize(is,intrev32ifbe(is->length)+1);
        if (pos < intrev32ifbe(is->length)) intsetMoveTail(is,pos,pos+1);
    }

    _intsetSet(is,pos,value);
    is->length = intrev32ifbe(intrev32ifbe(is->length)+1);
    return is;
}
```

## intsetRemove

删除整数时的流程如下：
	1.	通过二分查找找到目标整数的位置。
	2.	如果整数不存在，直接返回；否则继续。
	3.	删除对应的元素，并将其后续元素左移，填补空缺。

```c
/* Delete integer from intset */
intset *intsetRemove(intset *is, int64_t value, int *success) {
    uint8_t valenc = _intsetValueEncoding(value);
    uint32_t pos;
    if (success) *success = 0;

    if (valenc <= intrev32ifbe(is->encoding) && intsetSearch(is,value,&pos)) {
        uint32_t len = intrev32ifbe(is->length);

        /* We know we can delete */
        if (success) *success = 1;

        /* Overwrite value with tail and update length */
        if (pos < (len-1)) intsetMoveTail(is,pos+1,pos);
        is = intsetResize(is,len-1);
        is->length = intrev32ifbe(len-1);
    }
    return is;
}
```

## insetUpgradeAndAdd

将 intset 升级为更高的编码格式，并插入一个新的整数 value。当插入的整数超出当前编码格式的范围时，intset 需要将其所有整数从低位编码（如 INTSET_ENC_INT16）升级为高位编码（如 INTSET_ENC_INT32 或 INTSET_ENC_INT64）。

```c
/* Upgrades the intset to a larger encoding and inserts the given integer. */
static intset *intsetUpgradeAndAdd(intset *is, int64_t value) {
    uint8_t curenc = intrev32ifbe(is->encoding);
    uint8_t newenc = _intsetValueEncoding(value);
    int length = intrev32ifbe(is->length);
    int prepend = value < 0 ? 1 : 0;

    /* First set new encoding and resize */
    is->encoding = intrev32ifbe(newenc);
    is = intsetResize(is,intrev32ifbe(is->length)+1);

    /* Upgrade back-to-front so we don't overwrite values.
     * Note that the "prepend" variable is used to make sure we have an empty
     * space at either the beginning or the end of the intset. */
    while(length--)
        _intsetSet(is,length+prepend,_intsetGetEncoded(is,length,curenc));

    /* Set the value at the beginning or the end. */
    if (prepend)
        _intsetSet(is,0,value);
    else
        _intsetSet(is,intrev32ifbe(is->length),value);
    is->length = intrev32ifbe(intrev32ifbe(is->length)+1);
    return is;
}
```
