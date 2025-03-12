<!--toc:start-->
- [SwissTable](#swisstable)
  - [传统哈希表实现](#传统哈希表实现)
    - [链式](#链式)
    - [线性探测](#线性探测)
    - [二者比较](#二者比较)
  - [核心](#核心)
  - [基本结构](#基本结构)
    - [插入数据](#插入数据)
  - [一些小技巧](#一些小技巧)
    - [找到一个数字的下一个2的倍数](#找到一个数字的下一个2的倍数)
<!--toc:end-->

# SwissTable

参考go的swiss实现。

## 传统哈希表实现

### 链式

如果多个键具有相同的哈希值，就在这个哈希值对应的位置按照链表的形式串联起来，看起来就是这样的：

```cpp
struct ListNode{
    ListNode *next;
    int val;
};
struct HashMap{
    vector<ListNode*> hash_map;
    ...
};
```

### 线性探测

给一个数组，按照哈希值排放元素，如果哈希值的位置上已经有了一个元素，就向后寻找下一个空的位置把元素放进去。

对应的实现会像是这样：

```cpp
class HashMap{
    vector<pair<int,bool>> hash_map;
    public:
    ...
    void insert(int key,int val){
        int hashed_key=hash(key);
        // 表示已经被占用了
        if(hash_map[hashed_key].second){
            int n=hash_map.size();
            for(int i=hashed_key+1;i!=hashed_key;i++){
                if(i>=n){
                    i=(i+1)%n;
                }
                if(!hash_map[i].second){
                    hash_map[hashed_key]={val,true};
                }
            }
        }else{
            hash_map[hashed_key]={val,true};
        }
    }
    ...
};
```

问题在于，开放地址当所有位置都被占用时，就会触发rehash，此时就要调整哈希表大小并重新hash。

### 二者比较

1. 线性探测具有缓存友好、可以使用数组这种紧凑的数据结构的优点；链表法则是实现上较为简单（可以不进行rehash就放下所有的元素），并且当链表数据量大了之后还可以将链表转换为搜索树来减少链表遍历的开销，但是对于缓存不是很友好。
2. 线性探测中，每一个slot的状态比较复杂，有占用、空、删除三种状态，同时，由于冲突的连锁反应，会导致线性探测中rehash相对于链表法更加频繁。

## 核心

Swiss Table是基于线性探测的改进，核心就是通过增强hashtable结构和元数据存储来优化性能和内存使用。Swiss Table可以通过SIMD指令来提高吞吐量。

## 基本结构

```go
// Map is an open-addressing hash map
// based on Abseil's flat_hash_map.
type Map[K comparable, V any] struct {
    ctrl     []metadata
    groups   []group[K, V]
    hash     maphash.Hasher[K]
    resident uint32
    dead     uint32
    limit    uint32
}
```

其中，ctrl是元数据数组，对应于groups，每一个group有8个槽。

```go
// metadata is the h2 metadata array for a group.
// find operations first probe the controls bytes
// to filter candidates before matching keys
type metadata [groupSize]int8
```

哈希值被两部分，第一个部分是H1（57个bit），用来确定groups的起始位置，第二个部分是H2（7个bit），存储在metadata中作为当前密钥的哈希签名，以供后续的搜索和过滤。

```go
const (
    h1Mask    uint64 = 0xffff_ffff_ffff_ff80
    h2Mask    uint64 = 0x0000_0000_0000_007f
    empty     int8   = -128 // 0b1000_0000
    tombstone int8   = -2   // 0b1111_1110
)
```

与传统哈希表相比，优势在于元数据，其中的控制信息包括：
- 槽位是否为空：0b10000000
- 槽位是否被删除：0b11111110
- 槽中的哈希签名：0bh2

这些状态的唯一值允许使用SIMD指令，从而最大限度的加速。

### 插入数据

Put函数。
```go
func (m *Map[K, V]) Put(key K, value V) {
    if m.resident >= m.limit {
        m.rehash(m.nextSize())
    }
    hi, lo := splitHash(m.hash.Hash(key))
    g := probeStart(hi, len(m.groups))
    for { // inlined find loop
        matches := metaMatchH2(&m.ctrl[g], lo)
        for matches != 0 {
            s := nextMatch(&matches)
            if key == m.groups[g].keys[s] { // update
                m.groups[g].keys[s] = key
                m.groups[g].values[s] = value
                return
            }
        }
        // |key| is not in group |g|,
        // stop probing if we see an empty slot
        matches = metaMatchEmpty(&m.ctrl[g])
        if matches != 0 { // insert
            s := nextMatch(&matches)
            m.groups[g].keys[s] = key
            m.groups[g].values[s] = value
            m.ctrl[g][s] = int8(lo)
            m.resident++
            return
        }
        g += 1 // linear probing
        if g >= uint32(len(m.groups)) {
            g = 0
        }
    }
}
func nextMatch(b *bitset) (s uint32) {
    s = uint32(bits.TrailingZeros16(uint16(*b)))
    *b &= ^(1 << s) // clear bit |s|
    return
}
func metaMatchH2(m *metadata, h h2) bitset {
    b := simd.MatchMetadata((*[16]int8)(m), int8(h))
    return bitset(b)
}

func metaMatchEmpty(m *metadata) bitset {
    b := simd.MatchMetadata((*[16]int8)(m), empty)
    return bitset(b)
}
```
1. 先计算哈希值，并把哈希值分裂成h1, h2(hi,lo)两个部分。
2. 然后是使用probeStart结合h1来寻找起始组。如果找到了，就进一步检查匹配的键，如果匹配就更新对应的值。
3. 如果没有找到匹配的键，就使用metaMatchEmpty函数，检查slots当前组是否为空。如果发现空槽，则插入新的键值并更新metadata和resident计数。
4. 如果当前组没有空槽，则使用线性探测来检查下一个groups。

这里，我们可以看出，数据存放是放置在一个个的groups中的，而前面也可以看到，groups是group数组，group则是由key数组和value数组组成的，也就是说，key的哈希值分成了h1(hi), h2(lo)两部分，h1的作用就是找到元素应该放入的group，h2的作用是计算出matches，用来在group中找出槽位。

为了加快找到可用槽位的过程，swiss table使用了这样的方法：

- h2乘以0b01010101得到了一个uint64，允许与8个值同时进行比较ctrl。
- 在meta上执行xor操作，如果h2在metadata中，对应的比特位就应该是0。

```go
func TestMetaMatchH2(t *testing.T) {
    metaData := make([]metadata, 2)
    metaData[0] = [8]int8{0x7f, 0, 0, 0x7f, 0, 0, 0, 0x7f}
    m := &metaData[0]
    h := 0x7f
    metaUint64 := castUint64(m)
    h2Pattern := loBits * uint64(h)
    xorResult := metaUint64 ^ h2Pattern
    fmt.Printf("metaUint64: %b\n", xorResult)
    r := hasZeroByte(xorResult)
    fmt.Printf("r: %b\n", r)
    for r != 0 {
        fmt.Println(nextMatch(&r))
    }
}
```

## 一些小技巧

### 找到一个数字的下一个2的倍数

比如说1的下一个是1,3的下一个是4,5的下一个是8...

```go
func nextPow2(x uint32) uint32 {
    return 1 << (32 - bits.LeadingZeros32(x-1))
}
```
