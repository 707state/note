<!--toc:start-->
- [什么是SkipList](#什么是skiplist)
- [为什么Redis用SkipList?](#为什么redis用skiplist)
- [跳表节点查询过程](#跳表节点查询过程)
- [1206 设计跳表](#1206-设计跳表)
<!--toc:end-->

# 什么是SkipList

在计算机科学中，跳跃列表是一种数据结构。它使得包含n个元素的有序序列的查找和插入操作的平均时间复杂度都是
\$ O(log(n)) \$ 复杂度。

快速的查询效果是通过维护一个多层次的链表实现的，且与前一层（下面一层）链表元素的数量相比，每一层链表中的元素的数量更少（见右下角示意图）。一开始时，算法在最稀疏的层次进行搜索，直至需要查找的元素在该层两个相邻的元素中间。这时，算法将跳转到下一个层次，重复刚才的搜索，直到找到需要查找的元素为止。跳过的元素的方法可以是随机性选择\[2\]或确定性选择\[3\]，其中前者更为常见。

在查找目标元素时，从顶层列表、头元素起步。算法沿着每层链表搜索，直至找到一个大于或等于目标的元素，或者到达当前层列表末尾。如果该元素等于目标元素，则表明该元素已被找到；如果该元素大于目标元素或已到达链表末尾，则退回到当前层的上一个元素，然后转入下一层进行搜索。

跳跃列表不像平衡树等数据结构那样提供对最坏情况的性能保证：由于用来建造跳跃列表采用随机选取元素进入更高层的方法，在小概率情况下会生成一个不平衡的跳跃列表（最坏情况例如最底层仅有一个元素进入了更高层，此时跳跃列表的查找与普通列表一致）。但是在实际中它通常工作良好，随机化平衡方案也比平衡二叉查找树等数据结构中使用的确定性平衡方案容易实现。跳跃列表在并行计算中也很有用：插入可以在跳跃列表不同的部分并行地进行，而不用对数据结构进行全局的重新平衡。

# 为什么Redis用SkipList?

1. 容易实现
2. 并发效率高
3. 范围查询

Redis 只有 Zset 对象的底层实现用到了跳表，跳表的优势是能支持平均 O(logN) 复杂度的节点查找。

zset
结构体里有两个数据结构：一个是跳表，一个是哈希表。这样的好处是既能进行高效的范围查询，也能进行高效单点查询。

<details><summary>Click to expand</summary>

``` c
typedef struct zset {
    dict *dict;
    zskiplist *zsl;
} zset;
```

</details>
Zset 对象在执行数据插入或是数据更新的过程中，会依次在跳表和哈希表中插入或更新相应的数据，从而保证了跳表和哈希表中记录的信息一致。

Zset 对象能支持范围查询（如 ZRANGEBYSCORE
操作），这是因为它的数据结构设计采用了跳表，而又能以常数复杂度获取元素权重（如
ZSCORE 操作），这是因为它同时采用了哈希表进行索引。

可能很多人会奇怪，为什么我开头说 Zset对象的底层数据结构是「压缩列表」或者「跳表」，而没有说哈希表呢？

Zset 对象在使用跳表作为数据结构的时候，是使用由「哈希表+跳表」组成的
struct zset，但是我们讨论的时候，都会说跳表是 Zset
对象的底层数据结构，而不会提及哈希表，是因为 struct zset
中的哈希表只是用于以常数复杂度获取元素权重，大部分操作都是跳表实现的。

Zset 对象要同时保存「元素」和「元素的权重」，对应到跳表节点结构里就是sds 类型的 ele 变量和 double 类型的 score变量。每个跳表节点都有一个后向指针（struct zskiplistNode\*backward），指向前一个节点，目的是为了方便从跳表的尾节点开始访问节点，这样倒序查找时很方便。

跳表是一个带有层级关系的链表，而且每一层级可以包含多个节点，每一个节点通过指针连接起来，实现这一特性就是靠跳表节点结构体中的zskiplistLevel结构体类型的 level 数组。

level 数组中的每一个元素代表跳表的一层，也就是由 zskiplistLevel结构体表示，比如 leve\[0\] 就表示第一层，leve\[1\]就表示第二层。zskiplistLevel结构体里定义了「指向下一个跳表节点的指针」和「跨度」，跨度时用来记录两个节点之间的距离。

第一眼看到跨度的时候，以为是遍历操作有关，实际上并没有任何关系，遍历操作只需要用前向指针（structzskiplistNode \*forward）就可以完成了。

跨度实际上是为了计算这个节点在跳表中的排位。具体怎么做的呢？

因为跳表中的节点都是按序排列的，那么计算某个节点排位的时候，从头节点到该结点的查询路径上，将沿途访问过的所有层的跨度累加起来，得到的结果就是目标节点在跳表中的排位。

<details><summary>Click to expand</summary>

``` c
typedef struct zskiplist {
    struct zskiplistNode *header, *tail;
    unsigned long length;
    int level;
} zskiplist;
```

</details>
跳表结构里包含了：

    跳表的头尾节点，便于在O(1)时间复杂度内访问跳表的头节点和尾节点；
    跳表的长度，便于在O(1)时间复杂度获取跳表节点的数量；
    跳表的最大层数，便于在O(1)时间复杂度获取跳表中层高最大的那个节点的层数量；

# 跳表节点查询过程

查找一个跳表节点的过程时，跳表会从头节点的最高层开始，逐一遍历每一层。在遍历某一层的跳表节点时，会用跳表节点中的
SDS 类型的元素和元素的权重来进行判断，共有两个判断条件：

如果当前节点的权重「小于」要查找的权重时，跳表就会访问该层上的下一个节点。

如果当前节点的权重「等于」要查找的权重时，并且当前节点的 SDS
类型数据「小于」要查找的数据时，跳表就会访问该层上的下一个节点。

如果上面两个条件都不满足，或者下一个节点为空时，跳表就会使用目前遍历到的节点的
level
数组里的下一层指针，然后沿着下一层指针继续查找，这就相当于跳到了下一层接着查找。

# 1206 设计跳表

<details>

```cpp
class Skiplist {
    constexpr static int MAX_LEVEL = 32;
    constexpr static double P_FACTOR = 0.25;
    struct SkipListNode {
        int val;
        vector<SkipListNode*> forward;
        SkipListNode(int _val, int _max_level = MAX_LEVEL)
            : val(_val), forward(_max_level) {}
    };
    SkipListNode* head;
    int level;
    mt19937 gen{random_device{}()};
    uniform_real_distribution<double> dis;
    int randomLevel() {
        int lv = 1;
        while (dis(gen) < P_FACTOR && lv < MAX_LEVEL) {
            lv++;
        }
        return lv;
    }

public:
    Skiplist() : head(new SkipListNode(-1)), level(0), dis(0, 1) {}

    bool search(int target) {
        auto curr = this->head;
        for (int i = level - 1; i >= 0; i--) {
            while (curr->forward[i] && curr->forward[i]->val < target) {
                curr = curr->forward[i];
            }
        }
        curr = curr->forward[0];
        if (curr && curr->val == target) {
            return true;
        }
        return false;
    }

    void add(int num) {
        vector<SkipListNode*> update(MAX_LEVEL, head);
        auto curr = this->head;
        for (int i = level - 1; i >= 0; i--) {
            while (curr->forward[i] && curr->forward[i]->val < num) {
                curr = curr->forward[i];
            }
            update[i] = curr;
        }
        int lv = randomLevel();
        level = max(level, lv);
        auto newNode = new SkipListNode(num, lv);
        for (int i = 0; i < lv; i++) {
            newNode->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = newNode;
        }
    }

    bool erase(int num) {
        vector<SkipListNode*> update(MAX_LEVEL, nullptr);
        auto curr = this->head;
        for (int i = level - 1; i >= 0; i--) {
            while (curr->forward[i] && curr->forward[i]->val < num) {
                curr = curr->forward[i];
            }
            update[i] = curr;
        }
        curr = curr->forward[0];
        if (!curr || curr->val != num) {
            return false;
        }
        for (int i = 0; i < level; i++) {
            if (update[i]->forward[i] != curr) {
                break;
            }
            update[i]->forward[i] = curr->forward[i];
        }
        delete curr;
        while (level > 1 && !head->forward[level - 1]) {
            level--;
        }
        return true;
    }
};

```

</details>
