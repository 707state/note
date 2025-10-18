# Probabilistic Data Structure

概率数据结构是一种依赖于概率计算，降低准确度来换取时间和空间效率的数据结构。

## HyperLogLog

HyperLogLog是一种概率数据结构，可以估计集合的基数。

估计基数的核心思路：最大尾零法（Leading Zeros）,设有一个完美随机哈希函数 h(x)，将任意元素映射为均匀分布的二进制串：

h(x) = 01011000...

如果你看很多这样的随机比特串：
出现 0 个前导零的概率 = ½
出现 1 个前导零的概率 = ¼
出现 2 个前导零的概率 = ⅛
出现 3 个前导零的概率 = 1/16
...

那么，如果要在随机样本里面找到一个有R个前导零的哈希值，这个随机样本量平均值有2^R个。这就是估算的前提。

### 取平均

Redis里面为了避免出现被偶然极端值影响的情况，还在哈希的基础上才用了桶：

- 将哈希空间分成很多个桶（m 个）
- 用前几位哈希位决定桶号
- 剩下的位来计算前导零数
- 每个桶独立记录“它见到的最长前导零数”
- 最后综合所有桶的结果，用调和平均计算整体估计

看看Redis里面的实现：

Redis 中的 HyperLogLog 实现是一个 紧凑型的寄存器数组：
1. Redis 的 HyperLogLog 使用 16384 (2¹⁴) 个寄存器（registers）。
2. 每个寄存器保存一个小整数（5~6 bits），表示 “最大尾零数（Leading Zeros）”。

```c
struct hllhdr {
    char magic[4];       /* "HYLL" */
    uint8_t encoding;    /* HLL_DENSE or HLL_SPARSE. */
    uint8_t notused[3];  /* Reserved for future use, must be zero. */
    uint8_t card[8];     /* Cached cardinality, little endian. */
    uint8_t registers[]; /* Data bytes. */
};
```

这里利用了C的struct末尾零长子数组的特性。
