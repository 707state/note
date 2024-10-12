# 295 数据流的中文数

中位数是有序整数列表中的中间值。如果列表的大小是偶数，则没有中间值，中位数是两个中间值的平均值。

    例如 arr = [2,3,4] 的中位数是 3 。
    例如 arr = [2,3] 的中位数是 (2 + 3) / 2 = 2.5 。

实现 MedianFinder 类:

    MedianFinder() 初始化 MedianFinder 对象。

    void addNum(int num) 将数据流中的整数 num 添加到数据结构中。

    double findMedian() 返回到目前为止所有元素的中位数。与实际答案相差 10-5 以内的答案将被接受。


```c++ 
class MedianFinder {
    priority_queue<int,vector<int>,greater<>>  queMax;
    priority_queue<int,vector<int>,less<>> queMin;
public:
    MedianFinder() {

    }
    
    void addNum(int num) {
        if(queMin.empty()||num<=queMin.top()){
            queMin.push(num);
            if(queMax.size()+1<queMin.size()){
                queMax.push(queMin.top());
                queMin.pop();
            }
        }else{
            queMax.push(num);
            if(queMin.size()<queMax.size()){
                queMin.push(queMax.top());
                queMax.pop();
            }
        }
    }
    
    double findMedian() {
        if(queMin.size()>queMax.size()){
            return queMin.top();
        }
        return (queMin.top()+queMax.top())/2.0;
    }
};
```

# 42 接雨水

给定 n 个非负整数表示每个宽度为 1 的柱子的高度图，计算按此排列的柱子，下雨之后能接多少雨水。

```c++ 
class Solution {
public:
    int trap(vector<int>& height) {
        vector<int> left_max(height.size(),0);
        vector<int> right_max(height.size(),0);
        for(int i=1;i<height.size();i++){
            left_max[i]=std::max(height[i-1],left_max[i-1]);
        }
        for(int i=height.size()-1;i>0;i--){
            right_max[i-1]=std::max(right_max[i],height[i]);
        }
        int sum=0;
        for(int i=1;i<height.size();i++){
            int min_=(std::min(left_max[i],right_max[i]));
            if(min_>height[i]) sum+=(min_-height[i]);
        }
        return sum;
    }
};
```

# 3162 优质数对的总数1 

给你两个整数数组 nums1 和 nums2，长度分别为 n 和 m。同时给你一个正整数 k。

如果 nums1[i] 可以被 nums2[j] * k 整除，则称数对 (i, j) 为 优质数对（0 <= i <= n - 1, 0 <= j <= m - 1）。

返回 优质数对 的总数。


```c++ 
class Solution {
public:
    long long numberOfPairs(vector<int>& nums1, vector<int>& nums2, int k) {
        unordered_map<int, int> cnt;
        for (int x : nums1) {
            if (x % k) {
                continue;
            }
            x /= k;
            for (int d = 1; d * d <= x; d++) { // 枚举因子
                if (x % d) {
                    continue;
                }
                cnt[d]++; // 统计因子
                if (d * d < x) {
                    cnt[x / d]++; // 因子总是成对出现
                }
            }
        }

        long long ans = 0;
        for (int x : nums2) {
            ans += cnt.contains(x) ? cnt[x] : 0;
        }
        return ans;
    }
};
```


# LCR 170 交易逆序对的总数

在股票交易中，如果前一天的股价高于后一天的股价，则可以认为存在一个「交易逆序对」。请设计一个程序，输入一段时间内的股票交易记录 record，返回其中存在的「交易逆序对」总数。

思路：

1. 归并排序

```c++ 
class Solution {
public:
    int mergeSort(vector<int>& record,vector<int>& tmp,int l,int r){
        if(l>=r) return 0;
        int mid=(l+r)/2;
        int inv_count=mergeSort(record,tmp,l,mid)+mergeSort(record,tmp,mid+1,r);
        int i=l,j=mid+1,pos=l;
        while(i<=mid&&j<=r){
            if(record[i]<=record[j]){
                tmp[pos]=record[i++];
                inv_count+=(j-(mid+1));
            }else{
                tmp[pos]=record[j++];
            }
            ++pos;
        }
        for(int k=i;k<=mid;k++){
            tmp[pos++]=record[k];
            inv_count+=(j-(mid+1));
        }
        for(int k=j;k<=r;k++){
            tmp[pos++]=record[k];
        }
        copy(tmp.begin()+l,tmp.begin()+r+1,record.begin()+l);
        return inv_count;
    }
    int reversePairs(vector<int>& record) {
        int n=record.size();
        vector<int> tmp(n);
        return mergeSort(record,tmp,0,n-1);
    }
};
```

2. 树状数组

## 前置知识

「树状数组」是一种可以动态维护序列前缀和的数据结构，它的功能是：

    单点更新 update(i, v)： 把序列 i 位置的数加上一个值 v，这题 v=1
    区间查询 query(i)： 查询序列 [1⋯i] 区间的区间和，即 i 位置的前缀和

修改和查询的时间代价都是 O(logn)，其中 n 为需要维护前缀和的序列的长度。

记题目给定的序列为 a，我们规定 ai​ 的取值集合为 a 的「值域」。我们用桶来表示值域中的每一个数，桶中记录这些数字出现的次数。假设a={5,5,2,3,6}，那么遍历这个序列得到的桶是这样的：

index  ->  1 2 3 4 5 6 7 8 9
value  ->  0 1 1 0 2 1 0 0 0

我们可以看出它第 i−1 位的前缀和表示「有多少个数比 i 小」。那么我们可以从后往前遍历序列 a，记当前遍历到的元素为 ai​，我们把 ai​ 对应的桶的值自增 1，把 i−1 位置的前缀和加入到答案中算贡献。为什么这么做是对的呢，因为我们在循环的过程中，我们把原序列分成了两部分，后半部部分已经遍历过（已入桶），前半部分是待遍历的（未入桶），那么我们求到的 i−1 位置的前缀和就是「已入桶」的元素中比 ai​ 大的元素的总和，而这些元素在原序列中排在 ai​ 的后面，但它们本应该排在 ai​ 的前面，这样就形成了逆序对。

我们显然可以用数组来实现这个桶，可问题是如果 ai​ 中有很大的元素，比如 109，我们就要开一个大小为 109 的桶，内存中是存不下的。这个桶数组中很多位置是 0，有效位置是稀疏的，我们要想一个办法让有效的位置全聚集到一起，减少无效位置的出现，这个时候我们就需要用到一个方法——离散化。

离散化一个序列的前提是我们只关心这个序列里面元素的相对大小，而不关心绝对大小（即只关心元素在序列中的排名）；离散化的目的是让原来分布零散的值聚集到一起，减少空间浪费。那么如何获得元素排名呢，我们可以对原序列排序后去重，对于每一个 ai​ 通过二分查找的方式计算排名作为离散化之后的值。当然这里也可以不去重，不影响排名。

```c++ 
class BIT{
    vector<int> tree;
    int n;
    public :
    BIT( int _n): n(_n),tree(_n+1){

    }
    static int lowbit(int x){
        return x&(-x);
    }
    int query(int x){
        int ret=0;
        while(x){
            ret+=tree[x];
            x-=lowbit(x);
        }
        return ret;
    }
    void update(int x){
        while(x<=n){
            ++tree[x];
            x+=lowbit(x);
        }
    }
};
class Solution {
public:
    int reversePairs(vector<int>& record) {
        int n=record.size();
        vector<int> tmp=record;
        //离散化
        sort(tmp.begin(),tmp.end());
        for(auto& num: record){
            num=lower_bound(tmp.begin(),tmp.end(),num)-tmp.begin()+1;
        }
        //树状数组统计逆序对
        BIT bit(n);
        int ans=0;
        for(int i=n-1;i>=0;--i){
            ans+=bit.query(record[i]-1);
            bit.update(record[i]);
        }
        return ans;
    }
};
```


