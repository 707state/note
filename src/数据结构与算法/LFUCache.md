<!--toc:start-->
- [LFU Cache](#lfu-cache)
- [实现](#实现)
<!--toc:end-->

# LFU Cache

一个缓存结构需要实现如下功能。

    set(key, value)：将记录(key, value)插入该结构
    get(key)：返回key对应的value值

但是缓存结构中最多放K条记录，如果新的第K+1条记录要加入，就需要根据策略删掉一条记录，然后才能把新记录加入。这个策略为：在缓存结构的K条记录中，哪一个key从进入缓存结构的时刻开始，被调用set或者get的次数最少，就删掉这个key的记录；
如果调用次数最少的key有多个，上次调用发生最早的key被删除
这就是LFU缓存替换算法。实现这个结构，K作为参数给出。

# 实现

<details>

``` cpp
class Solution {
  private:
    typedef list<vector<int> >
    vecList; //定义元素为向量的双向链表，向量里为[频次，键，值]
    unordered_map<int, vecList> freq_map; //建立频次到双向链表的哈希表
    unordered_map<int, vecList::iterator>
    key_map; //建立键到双向链表迭代器的哈希表（迭代器可以简单理解为在双向链表中的指针类，通过它可以得到里面的元素）
    int least = 0; //记录当前最小频次
    int K; //记录缓存剩余容量
    void refresh_key(vecList::iterator vList_it, int key,
                     int value) { //更新和加入新key都会刷新
        int num =
            (*vList_it)[0]; //这里迭代器解引用（类似指针）取下标得到频次
        freq_map[num].erase(
            vList_it); //在该频次下的双向链表中擦除迭代器指向位置中的元素
        if (freq_map[num].empty()) { //如果双向链表被擦空了
            freq_map.erase(num); //删除该频次的映射
            if (least == num)
                least++; //如果当前频次为最小频次，最小频次加一
        }
        freq_map[num + 1].push_front({num + 1, key, value}); //插入频次加一的双向链表表头（若无此映射会自动创建空双向链表）
        key_map[key] = freq_map[num +
                                1].begin(); //更新键到双向链表迭代器的映射
    }
    void set(int key, int value) {
        auto it = key_map.find(key); //寻找是否存在该键
        if (it != key_map.end()) //找到时
            refresh_key(it->second, key, value); //更新频次和值
        else { //若找不到
            if (K == 0) { //满容量时
                int old_key = freq_map[least].back()[1]; //取频次最低的键
                freq_map[least].pop_back(); //将该键从对应频次的双向链表末位弹出
                if (freq_map[least].empty()) freq_map.erase(
                        least); //如果双向链表为空则删除
                key_map.erase(old_key); //擦除键到迭代器的映射
            } else K--; //若有空闲则直接加入，容量减1
            least = 1; //最小频次刷新为1
            freq_map[1].push_front({1, key, value}); //在频次1的双向链表表头插入该键
            key_map[key] =
                freq_map[1].begin(); //建立该键到双向链表迭代器的映射
        }
    }
    int get(int key) {
        auto it = key_map.find(key); //寻找是否存在该键
        if (it != key_map.end()) { //找到时
            auto vList_it = it->second; //取双向链表迭代器
            int value = (*vList_it)[2]; //解引用得到值
            refresh_key(vList_it, key, value); //更新频次（这里值其实不变）
            return value; //返回查询值
        } else return -1; //找不到返回-1
    }
  public:
    vector<int> LFU(vector<vector<int> >& operators, int k) {
        K = k; //变为私有成员变量方便操作
        vector<int> res; //记录结果
        for (auto& vec : operators) {
            if (vec[0] == 1) set(vec[1], vec[2]);
            else res.push_back(get(vec[1]));
        }
        return res;
    }
};
```

</details>
