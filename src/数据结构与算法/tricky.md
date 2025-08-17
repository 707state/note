<!--toc:start-->
- [295 数据流的中文数](#295-数据流的中文数)
- [42 接雨水](#42-接雨水)
- [3162 优质数对的总数1](#3162-优质数对的总数1)
- [LCR 170 交易逆序对的总数](#lcr-170-交易逆序对的总数)
  - [前置知识](#前置知识)
- [3200 三角形的最大高度](#3200-三角形的最大高度)
- [71 简化路径](#71-简化路径)
- [54 螺旋矩阵](#54-螺旋矩阵)
- [164 最大间距](#164-最大间距)
- [910 最小差值1](#910-最小差值1)
- [31 下一个排列](#31-下一个排列)
- [287 寻找重复数](#287-寻找重复数)
- [41 缺失的第一个正数](#41-缺失的第一个正数)
- [3222 求出硬币游戏的赢家](#3222-求出硬币游戏的赢家)
- [936 戳印序列](#936-戳印序列)
- [60 排列序列](#60-排列序列)
- [3233 统计不是特殊数字的数字数量](#3233-统计不是特殊数字的数字数量)
- [1739 放置盒子](#1739-放置盒子)
- [UNSOLVED 407 接雨水 2](#unsolved-407-接雨水-2)
- [408 有效单词缩写](#408-有效单词缩写)
- [3208 交替数组](#3208-交替数组)
- [UNSOLVED 3209 子数组按位与值为 K 的数目](#unsolved-3209-子数组按位与值为-k-的数目)
- [782 变为棋盘](#782-变为棋盘)
- [3280 将日期转换为二进制表示](#3280-将日期转换为二进制表示)
- [1706 球会落在何处？](#1706-球会落在何处)
- [348 井字棋](#348-井字棋)
- [1538 找出隐藏数组中出现次数最多的元素](#1538-找出隐藏数组中出现次数最多的元素)
- [1183 矩阵中1的最大数量](#1183-矩阵中1的最大数量)
- [12 整数转罗马数字](#12-整数转罗马数字)
- [59 螺旋矩阵Ⅱ](#59-螺旋矩阵ⅱ)
- [1275 找出井字棋的获胜者](#1275-找出井字棋的获胜者)
- [169 多数元素](#169-多数元素)
- [3644 排序排列](#3644-排序排列)
- [n个物品中选两个，使其和为m的倍数的选法数量](#n个物品中选两个使其和为m的倍数的选法数量)
<!--toc:end-->


# 295 数据流的中文数

中位数是有序整数列表中的中间值。如果列表的大小是偶数，则没有中间值，中位数是两个中间值的平均值。

    例如 arr = [2,3,4] 的中位数是 3 。
    例如 arr = [2,3] 的中位数是 (2 + 3) / 2 = 2.5 。

实现 MedianFinder 类:

    MedianFinder() 初始化 MedianFinder 对象。

    void addNum(int num) 将数据流中的整数 num 添加到数据结构中。

    double findMedian() 返回到目前为止所有元素的中位数。与实际答案相差 10-5 以内的答案将被接受。

<details><summary>Click to expand</summary>

``` cpp
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

</details>

# 42 接雨水
给定 n 个非负整数表示每个宽度为 1
的柱子的高度图，计算按此排列的柱子，下雨之后能接多少雨水。

<details><summary>Click to expand</summary>

``` cpp
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

</details>

# 3162 优质数对的总数1
给你两个整数数组 nums1 和 nums2，长度分别为 n 和 m。同时给你一个正整数
k。

如果 nums1\[i\] 可以被 nums2\[j\] \* k 整除，则称数对 (i, j) 为
优质数对（0 \<= i \<= n - 1, 0 \<= j \<= m - 1）。

返回 优质数对 的总数。

<details><summary>Click to expand</summary>

``` cpp
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

</details>

# LCR 170 交易逆序对的总数

在股票交易中，如果前一天的股价高于后一天的股价，则可以认为存在一个「交易逆序对」。请设计一个程序，输入一段时间内的股票交易记录
record，返回其中存在的「交易逆序对」总数。

思路：

1.  归并排序

<details><summary>Click to expand</summary>

``` cpp
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

</details>

2.  树状数组

## 前置知识

「树状数组」是一种可以动态维护序列前缀和的数据结构，它的功能是：

    单点更新 update(i, v)： 把序列 i 位置的数加上一个值 v，这题 v=1
    区间查询 query(i)： 查询序列 [1⋯i] 区间的区间和，即 i 位置的前缀和

修改和查询的时间代价都是 O(logn)，其中 n 为需要维护前缀和的序列的长度。

记题目给定的序列为 a，我们规定 ai 的取值集合为 a
的「值域」。我们用桶来表示值域中的每一个数，桶中记录这些数字出现的次数。假设a={5,5,2,3,6}，那么遍历这个序列得到的桶是这样的：

index -\> 1 2 3 4 5 6 7 8 9 value -\> 0 1 1 0 2 1 0 0 0

我们可以看出它第 i−1 位的前缀和表示「有多少个数比 i
小」。那么我们可以从后往前遍历序列 a，记当前遍历到的元素为 ai，我们把 ai
对应的桶的值自增 1，把 i−1
位置的前缀和加入到答案中算贡献。为什么这么做是对的呢，因为我们在循环的过程中，我们把原序列分成了两部分，后半部部分已经遍历过（已入桶），前半部分是待遍历的（未入桶），那么我们求到的
i−1 位置的前缀和就是「已入桶」的元素中比
ai大的元素的总和，而这些元素在原序列中排在 ai 的后面，但它们本应该排在
ai 的前面，这样就形成了逆序对。

我们显然可以用数组来实现这个桶，可问题是如果 ai 中有很大的元素，比如
109，我们就要开一个大小为 109
的桶，内存中是存不下的。这个桶数组中很多位置是
0，有效位置是稀疏的，我们要想一个办法让有效的位置全聚集到一起，减少无效位置的出现，这个时候我们就需要用到一个方法------离散化。

离散化一个序列的前提是我们只关心这个序列里面元素的相对大小，而不关心绝对大小（即只关心元素在序列中的排名）；离散化的目的是让原来分布零散的值聚集到一起，减少空间浪费。那么如何获得元素排名呢，我们可以对原序列排序后去重，对于每一个
ai
通过二分查找的方式计算排名作为离散化之后的值。当然这里也可以不去重，不影响排名。

<details><summary>Click to expand</summary>

``` cpp
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

</details>

# 3200 三角形的最大高度
给你两个整数 red 和
blue，分别表示红色球和蓝色球的数量。你需要使用这些球来组成一个三角形，满足第
1 行有 1 个球，第 2 行有 2 个球，第 3 行有 3 个球，依此类推。

每一行的球必须是 相同 颜色，且相邻行的颜色必须 不同。

返回可以实现的三角形的 最大 高度。

<details><summary>Click to expand</summary>

``` cpp
class Solution {
public:
    int maxHeight(int a,int b){
        for(int i=1;;i++){
            if(i%2==1) {
                a-=i;
                if(a<0){
                    return i-1;
                }
            }
            else{
                b-=i;
                if(b<0) return i-1;
            }
        }
    }
    int maxHeightOfTriangle(int red, int blue) {
        return max(maxHeight(red,blue),maxHeight(blue,red));
    }
};
```

</details>

# 71 简化路径
给你一个字符串 path ，表示指向某一文件或目录的 Unix 风格 绝对路径 （以
\'/\' 开头），请你将其转化为 更加简洁的规范路径。

在 Unix 风格的文件系统中规则如下：

    一个点 '.' 表示当前目录本身。
    此外，两个点 '..' 表示将目录切换到上一级（指向父目录）。
    任意多个连续的斜杠（即，'//' 或 '///'）都被视为单个斜杠 '/'。
    任何其他格式的点（例如，'...' 或 '....'）均被视为有效的文件/目录名称。

返回的 简化路径 必须遵循下述格式：

    始终以斜杠 '/' 开头。
    两个目录名之间必须只有一个斜杠 '/' 。
    最后一个目录名（如果存在）不能 以 '/' 结尾。
    此外，路径仅包含从根目录到目标文件或目录的路径上的目录（即，不含 '.' 或 '..'）。

返回简化后得到的 规范路径 。

<details><summary>Click to expand</summary>

``` cpp
class Solution {
public:
    string simplifyPath(string path) {
        vector<string> str_st;
        auto split=[](const string& s,char delim)->vector<string>{
            vector<string> res;
            string cur;
            for(auto ch: s){
                if(ch==delim){
                    res.emplace_back(move(cur));
                }else{
                    cur+=ch;
                }
            }
            res.emplace_back(move(cur));
            return res;
        };
        vector<string> names=split(path,'/');
        for(auto& name: names){
            if(name==".."){
                if(str_st.size()) str_st.pop_back();
            }
            else if(!name.empty()&&name!="."){
                str_st.push_back(name);
            }
        }
        string ans;
        if(str_st.empty()){
            ans="/";
        }else{
            for(auto& name: str_st){
                ans+="/"+move(name);
            }
        }
        return ans;
    }
};
```

</details>

# 54 螺旋矩阵

给你一个 m 行 n 列的矩阵 matrix ，请按照 顺时针螺旋顺序，返回矩阵中的所有元素。

注意，dx, dy有顺序要求。

<details><summary>Click to expand</summary>

``` cpp
class Solution {
    constexpr static array<int,4> dy={1,0,-1,0};
    constexpr static array<int,4> dx={0,1,0,-1};
    vector<int> ans;
    int n,m,pos=0;
    void dfs(int x,int y,vector<vector<int>>& matrix){
        if(x<0||y<0||x>=m||y>=n||matrix[x][y]==INT_MAX) return;
        ans.emplace_back(matrix[x][y]);
        matrix[x][y]=INT_MAX;
        int nx=x+dx[pos],ny=y+dy[pos];
        if(nx<0||ny<0||nx>=m||ny>=n||matrix[nx][ny]==INT_MAX){
            pos=(pos+1)%4;
            nx=x+dx[pos];
            ny=y+dy[pos];
        }
        dfs(nx,ny,matrix);
    }
public:
    vector<int> spiralOrder(vector<vector<int>>& matrix) {
        m=matrix.size();
        n=matrix[0].size();
        dfs(0,0,matrix);
        return ans;
    }
};
```

</details>

# 164 最大间距

给定一个无序的数组 nums，返回 数组在排序之后，相邻元素之间最大的差值
。如果数组元素个数小于 2，则返回 0 。

您必须编写一个在「线性时间」内运行并使用「线性额外空间」的算法。

思路：

看示例 1，想象有一根木棍，其左端点位置为 1，右端点位置为 9，长度为
9−1=8。我们在位置 3 和 6 处各切一刀，分成 3
个小木棍。问：最长小木棍的长度是多少？ 下界

最长小木棍的长度，至少是多少？

设 n 为 nums 的长度，m 为 nums 的最小值，M 为 nums 的最大值。

m=M 时，直接返回 0；m+1=M 时，直接返回 1。下面讨论 m+1\<M
的情况，注意这意味着 n≥2。

木棍的长度为 M−m，我们要切 n−2 刀，分成 n−1
个小木棍，那么小木棍的平均长度为(M-m)/(n-1)。

由于最大值不低于平均值（见下面答疑），所以最长小木棍的长度至少为
n−1M−m。由于小木棍的长度是整数，所以最长小木棍的长度至少为
d=(M-m+n-1)/(n-1)的下取整。

怎么利用「答案至少为 d」这一性质呢？

如果把两两之差小于 d
的数，分到同一组（桶）中，那么答案一定不会是同一个桶内的两数之差，而是不同的桶的两数之差！

示例 1 的 nums=\[3,6,9,1\]，分到三个桶中：第一个桶装入 1 和
3，第二个桶装入 6，第三个桶装入
9。答案一定不会是第一个桶的两数之差，因为
3−1=2\<d=3。所以我们只需要考虑不同的桶之间的两数之差。由于题目要计算的是排序后相邻元素之间的差值，所以应当取第一个桶的最大值，和第二个桶的最小值作差；取第二个桶的最大值，和第三个桶的最小值作差；依此类推。这些差值中的最大值即为答案。对于示例
1 来说，答案为 max(6−3,9−6)=3。

一般地，从 m 开始，把元素值在 m,m+1,⋯,m+d−1
中的数分到第一个桶，把元素值在 m+d,m+d+1,⋯,m+2d−1
中的数分到第二个桶，依此类推。

<details><summary>Click to expand</summary>

``` cpp
class Solution {
public:
    int maximumGap(vector<int>& nums) {
        auto [m,M]=ranges::minmax(nums);
        if(M-m<=1) return M-m;
        int n=nums.size();
        int d=(M-m+n-2)/(n-1);//答案至少是d
        vector<pair<int,int>> buckets((M-m)/d+1,{INT_MAX,INT_MIN});
        for(int x: nums){
            auto& [mn,mx]=buckets[(x-m)/d];//这里要引用
            mn=min(mn,x);//维护桶内元素的最小值和最大值
            mx=max(mx,x);
        }
        int ans=0;
        int pre_max=INT_MAX;
        for(auto [mn,mx]: buckets){
            if(mn!=INT_MAX){//非空桶
                ans=max(ans,mn-pre_max);//桶内最小值，减去上一个非空桶的最大值
                pre_max=mx;
            }
        }
        return ans;
    }
};
```

</details>

# 910 最小差值1
给你一个整数数组 nums，和一个整数 k 。

对于每个下标 i（0 \<= i \< nums.length），将 nums\[i\] 变成 nums\[i\] +k 或 nums\[i\] - k 。

nums 的 分数 是 nums 中最大元素和最小元素的差值。

在更改每个下标对应的值之后，返回 nums 的最小 分数 。

<details><summary>Click to expand</summary>

``` cpp
class Solution {
public:
    int smallestRangeII(vector<int>& nums, int k) {
        ranges::sort(nums);
        int ans=nums.back()-nums.front();
        for(int i=1;i<nums.size();++i){
            int mx=max(nums[i-1]+k,nums.back()-k);
            int mn=min(nums[0]+k,nums[i]-k);
            ans=min(ans,mx-mn);
        }
        return ans;
    }
};
```

</details>

# 31 下一个排列
整数数组的一个 排列 就是将其所有成员以序列或线性顺序排列。

    例如，arr = [1,2,3] ，以下这些都可以视作 arr 的排列：[1,2,3]、[1,3,2]、[3,1,2]、[2,3,1] 。

整数数组的 下一个排列是指其整数的下一个字典序更大的排列。更正式地，如果数组的所有排列根据其字典顺序从小到大排列在一个容器中，那么数组的下一个排列就是在这个有序容器中排在它后面的那个排列。如果不存在下一个更大的排列，那么这个数组必须重排为字典序最小的排列（即，其元素按升序排列）。

    例如，arr = [1,2,3] 的下一个排列是 [1,3,2] 。
    类似地，arr = [2,3,1] 的下一个排列是 [3,1,2] 。
    而 arr = [3,2,1] 的下一个排列是 [1,2,3] ，因为 [3,2,1] 不存在一个字典序更大的排列。

给你一个整数数组 nums ，找出 nums 的下一个排列。

必须 原地 修改，只允许使用额外常数空间。

<details><summary>Click to expand</summary>

``` cpp
class Solution {
public:
    void nextPermutation(vector<int>& nums) {
        int i=nums.size()-2;
        while(i>=0&&nums[i]>=nums[i+1]){
            i--;
        }
        if(i>=0){
            int j=nums.size()-1;
            while(j>=0&&nums[i]>=nums[j]){
                j--;
            }
            swap(nums[i],nums[j]);
        }
        reverse(nums.begin()+i+1,nums.end());
    }
};
```

</details>

# 287 寻找重复数
给定一个包含 n + 1 个整数的数组 nums ，其数字都在 \[1, n\] 范围内（包括
1 和 n），可知至少存在一个重复的整数。

假设 nums 只有 一个重复的整数 ，返回 这个重复的数 。

你设计的解决方案必须 不修改 数组 nums 且只用常量级 O(1) 的额外空间。

思路和算法

这个方法我们来将所有数二进制展开按位考虑如何找出重复的数，如果我们能确定重复数每一位是
1 还是 0 就可以按位还原出重复的数是什么。

考虑第 i 位，我们记 nums 数组中二进制展开后第 i 位为 1 的数有 x 个，数字
\[1,n\] 这 n 个数二进制展开后第 i 位为 1 的数有 y 个，那么重复的数第 i
位为 1 当且仅当 x\>y。

<details><summary>Click to expand</summary>

``` cpp
class Solution {
public:
    int findDuplicate(vector<int>& nums) {
        int n=nums.size(),ans=0;
        int bit_max=31;
        while(!((n-1)>>bit_max)){
            bit_max-=1;
        }
        for(int bit=0;bit<=bit_max;++bit){
            int x=0,y=0;
            for(int i=0;i<n;i++){
                if(nums[i]&(1<<bit)){
                    x+=1;
                }
                if(i>=1&&(i&(1<<bit))){
                    y+=1;
                }
            }
            if(x>y){
                ans|=1<<bit;
            }
        }
        return ans;
    }
};
```

</details>

# 41 缺失的第一个正数
给你一个未排序的整数数组 nums ，请你找出其中没有出现的最小的正整数。
请你实现时间复杂度为 O(n) 并且只使用常数级别额外空间的解决方案。

<details><summary>Click to expand</summary>

``` cpp
class Solution {
public:
/*假设数组长度为 n，那么能出现在数组中的最小未出现的正整数一定在 [1, n+1] 之间：
如果数组是 [1, 2, 3, ..., n]，那最小缺失的就是 n+1
如果有缺失的数字，它一定在 [1, n] 中
所以，我们希望用 nums[i] 来“存储” i+1 是否出现过 —— 用负号做标记。
*/
    int firstMissingPositive(vector<int>& nums) {
        // 第一步：把所有非正数改为 n+1（不可能是答案）
        int n=nums.size();
        for(int& num:nums){
            if(num<=0) num=n+1;
        }
         // 第二步：将出现过的数 x 的 nums[x-1] 变成负数，表示 x 出现过
        for(int i=0;i<n;i++){
            int num=abs(nums[i]);
            if(num<=n) nums[num-1]=-abs(nums[num-1]);
        }
         // 第三步：找到第一个值为正的下标 i，那么 i+1 就是答案
        for(int i=0;i<n;i++){
            if(nums[i]>0){
                return i+1;
            }
        }
         // 如果前面都被标记了，说明 [1, n] 都出现过了，答案是 n+1
        return n+1;
    }
};
```

</details>

# 3222 求出硬币游戏的赢家

给你两个 正 整数 x 和 y ，分别表示价值为 75 和 10 的硬币的数目。

Alice 和 Bob 正在玩一个游戏。每一轮中，Alice 先进行操作，Bob
后操作。每次操作中，玩家需要拿出价值 总和 为 115
的硬币。如果一名玩家无法执行此操作，那么这名玩家 输掉 游戏。

两名玩家都采取 最优 策略，请你返回游戏的赢家。

<details><summary>Click to expand</summary>

``` cpp
class Solution {
public:
    string losingPlayer(int x, int y) {
        return min(x,y/4)%2?"Alice":"Bob";
    }
};
```

</details>

# 936 戳印序列

你想要用小写字母组成一个目标字符串 target。

开始的时候，序列由 target.length 个 \'?\'
记号组成。而你有一个小写字母印章 stamp。

在每个回合，你可以将印章放在序列上，并将序列中的每个字母替换为印章上的相应字母。你最多可以进行
10 \* target.length 个回合。

举个例子，如果初始序列为 \"?????\"，而你的印章 stamp 是
\"abc\"，那么在第一回合，你可以得到
\"abc??\"、\"?abc?\"、\"??abc\"。（请注意，印章必须完全包含在序列的边界内才能盖下去。）

如果可以印出序列，那么返回一个数组，该数组由每个回合中被印下的最左边字母的索引组成。如果不能印出序列，就返回一个空数组。

例如，如果序列是 \"ababc\"，印章是 \"abc\"，那么我们就可以返回与操作
\"?????\" -\> \"abc??\" -\> \"ababc\" 相对应的答案 \[0, 2\]；

另外，如果可以印出序列，那么需要保证可以在 10 \* target.length
个回合内完成。任何超过此数字的答案将不被接受。

<details><summary>Click to expand</summary>

``` cpp
class Solution {
public:
    vector<int> movesToStamp(string stamp, string target) {
        auto m=stamp.size();
        auto n=target.size();
        vector<int> inDegreee(n-m+1,m);
        vector<vector<int>> edges(n);
        vector<int> seen(n);
        vector<int> q;
        for(auto i=0;i<n-m+1;i++){
            for(auto j=0;j<m;j++){
                if(target[i+j]==stamp[j]){
                    inDegreee[i]-=1;
                    if(inDegreee[i]==0) q.emplace_back(i);
                }else{
                    edges[i+j].emplace_back(i);
                }
            }
        }
        vector<int> ans;
        while(!q.empty()){
            int cur=q.back();
            q.pop_back();
            ans.emplace_back(cur);
            for(size_t i=0;i<m;i++){
                if(!seen[cur+i]){
                    seen[cur+i]=true;
                    for(auto &&edge: edges[cur+i]){
                        inDegreee[edge]-=1;
                        if(inDegreee[edge]==0) q.emplace_back(edge);
                    }
                }
            }
        }
        if(ans.size()<n-m+1) return {};
        ranges::reverse(ans);
        return ans;
    }
};
```

</details>

# 60 排列序列

给出集合 \[1,2,3,\...,n\]，其所有元素共有 n! 种排列。

按大小顺序列出所有排列情况，并一一标记，当 n = 3 时, 所有排列如下：

    "123"
    "132"
    "213"
    "231"
    "312"
    "321"

给定 n 和 k，返回第 k 个排列。

<details><summary>Click to expand</summary>

``` cpp
class Solution {
public:
    string getPermutation(int n, int k) {
        vector<int> s(n);
        iota(s.begin(),s.end(),1);
        for(int i=0;i<k-1;i++){
            next_permutation(s.begin(),s.end());
        }
        string ans;
        for(auto t:s){
            ans+=to_string(t);
        }
        return ans;
    }
};
```

</details>

# 3233 统计不是特殊数字的数字数量

给你两个 正整数 l 和 r。对于任何数字 x，x 的所有正因数（除了 x 本身）被称为 x 的 真因数。

如果一个数字恰好仅有两个 真因数，则称该数字为 特殊数字。例如：

    数字 4 是 特殊数字，因为它的真因数为 1 和 2。
    数字 6 不是 特殊数字，因为它的真因数为 1、2 和 3。

返回区间 \[l, r\] 内 不是 特殊数字 的数字数量。

<details>

``` cpp
class Solution {
public:
    int nonSpecialCount(int l, int r) {
        int n=sqrt(r);
        vector<int> v(n+1);
        int res=r-l+1;
        for(int i=2;i<=n;i++){
            if(v[i]==0){
                if(i*i>=l&&i*i<=r){
                    res--;
                }
            }
            for(int j=i*i;j<=n;j+=i){
                v[j]=1;
            }
        }
        return res;
    }
};
```

</details>

# 1739 放置盒子

有一个立方体房间，其长度、宽度和高度都等于 n 个单位。请你在房间里放置 n 个盒子，每个盒子都是一个单位边长的立方体。放置规则如下：

    你可以把盒子放在地板上的任何地方。
    如果盒子 x 需要放置在盒子 y 的顶部，那么盒子 y 竖直的四个侧面都 必须 与另一个盒子或墙相邻。

给你一个整数 n ，返回接触地面的盒子的 最少 可能数量。

<details>

``` cpp
class Solution {
public:
    int minimumBoxes(int n) {
        int ans = 0, max_n = 0;
        for (int i = 1; max_n + ans + i <= n; ++i) {
            ans += i;
            max_n += ans;
        }
        for (int j = 1; max_n < n; ++j) {
            ++ans;
            max_n += j;
        }
        return ans;
    }
};
```

</details>

# UNSOLVED 407 接雨水 2

给你一个 m x n 的矩阵，其中的值均为非负整数，代表二维高度图每个单元的高度，请计算图中形状最多能接多少体积的雨水。

 <details>

思路： 哪个格子的接水量，在一开始就能确定？

    最外面一圈的格子是无法接水的。
    假设 (0,1) 的高度是最外面一圈的格子中最小的，且高度等于 5，那么和它相邻的 (1,1)，我们能知道：
        (1,1) 的水位不能超过 5，否则水会从 (0,1) 流出去。
        (1,1) 的水位一定可以等于 5，这是因为 (0,1) 的高度是最外面一圈的格子中最小的，(1,1) 的水不可能从其他地方流出去。

我们从最外面一圈的格子开始。想象成一个木桶，最外面一圈格子的高度视作木板的高度。

接着上面的讨论：

    如果 (1,1) 的高度 ≥5，那么 (0,1) 这块木板就没用了，我们去掉 (0,1) 这块木板，改用 (1,1) 这块木板。
    如果 (1,1) 的高度 <5，假设我们接的不是水，是水泥。那么把 (1,1) 的高度填充为 5，仍然可以去掉 (0,1) 这块木板，改用 (1,1) 这块（填充水泥后）高为 5 的木板水泥板。

继续，从当前木板中，找到一根最短的木板。假设 (1,1)
是当前所有木板中最短的，那么其邻居 (1,2) 和 (2,1) 的水位就是 (1,1)
的高度，因为超过 (1,1) 高度的水会流出去。然后，去掉 (1,1) 这块木板，改用
(1,2) 和 (2,1) 这两块木板。依此类推。

由于每次都要找最短的木板，所以用一个最小堆维护木板的高度。按照上述做法，不断循环，直到堆为空。

``` cpp
class Solution {
    static constexpr int dxy[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
public:
    int trapRainWater(vector<vector<int>>& heightMap) {
        int m=heightMap.size(),n=heightMap[0].size();
        priority_queue<tuple<int,int,int>,vector<tuple<int,int,int>>,greater<>> pq;
        for(int i=0;i<m;i++){
            for(int j=0;j<n;j++){
                if(i==0||i==m-1||j==0||j==n-1){
                    pq.emplace(heightMap[i][j],i,j);
                    heightMap[i][j]=-1;//标记(i,j)，表示访问过
                }
            }
        }
        int ans=0;
        while(!pq.empty()){
            auto [min_height,i,j]=pq.top();
            pq.pop();
            for(auto& [dx,dy]:dxy){
                int x=i+dx,y=j+dy;//(x,y)的邻居
                if(x>=0&&x<m&&y>=0&&y<n&&heightMap[x][y]>=0){//(x,y)没有访问过
                    //如果(x,y)的高度小于min_height,那么接水量为min_height-heightMap[x][y]
                    ans+=max(min_height-heightMap[x][y],0);
                    //给木桶新增一块高为max(heightMap[x][y],min_height)的木板
                    pq.emplace(max(min_height,heightMap[x][y]),x,y);
                    heightMap[x][y]=-1;
                }
            }
        }
        return ans;
    }
};
```

 </details>

# 408 有效单词缩写

字符串可以用 缩写 进行表示，缩写 的方法是将任意数量的 不相邻的子字符串替换为相应子串的长度。例如，字符串 \"substitution\"可以缩写为（不止这几种方法）：


    "s10n" ("s ubstitutio n")
    "sub4u4" ("sub stit u tion")
    "12" ("substitution")
    "su3i1u2on" ("su bst i t u ti on")
    "substitution" (没有替换子字符串)

下列是不合法的缩写：

    "s55n" ("s ubsti tutio n"，两处缩写相邻)
    "s010n" (缩写存在前导零)
    "s0ubstitution" (缩写是一个空字符串)

给你一个字符串单词 word 和一个缩写 abbr，判断这个缩写是否可以是给定单词的缩写。

子字符串是字符串中连续的非空字符序列。

<details>

``` cpp
class Solution {
public:
    bool validWordAbbreviation(string word, string abbr) {
        int len=abbr.size();
        int wordLen=word.size();
        int abbrLen=0,num=0;
        for(int i=0;i<len;i++){
            if(abbr[i]>='a'&&abbr[i]<='z'){
                abbrLen+=num+1;
                num=0;
                if(abbrLen>wordLen||abbr[i]!=word[abbrLen-1]){
                    return false;
                }
            }else{
                if(!num&&abbr[i]=='0'){
                    return false;
                }
                num=num*10+abbr[i]-'0';
            }
        }
        return abbrLen+num==wordLen;
    }
};
```

</details>

# 3208 交替数组

给你一个整数数组 colors 和一个整数 k，colors表示一个由红色和蓝色瓷砖组成的环，第 i 块瓷砖的颜色为 colors\[i\] ：

    colors[i] == 0 表示第 i 块瓷砖的颜色是 红色 。
    colors[i] == 1 表示第 i 块瓷砖的颜色是 蓝色 。

环中连续 k 块瓷砖的颜色如果是 交替颜色（也就是说除了第一块和最后一块瓷砖以外，中间瓷砖的颜色与它 左边 和右边 的颜色都不同），那么它被称为一个 交替 组。

请你返回 交替 组的数目。

注意 ，由于 colors 表示一个 环 ，第一块 瓷砖和 最后一块 瓷砖是相邻的。

<details>

``` cpp
class Solution {
public:
    int numberOfAlternatingGroups(vector<int>& colors, int k) {
        int n=colors.size();
        int ans=0,cnt=0;
        for(int i=0;i<n*2;i++){
            if(i>0&&colors[i%n]==colors[(i-1)%n]){
                cnt=0;
            }
            cnt++;
            ans+=i>=n&&cnt>=k;
        }
        return ans;
    }
};
```

</details>

# UNSOLVED 3209 子数组按位与值为 K 的数目

给你一个整数数组 nums 和一个整数 k ，请你返回 nums
中有多少个子数组满足：子数组中所有元素按位 AND 的结果为 k 。

<details>

``` cpp
class Solution {
public:
    long long countSubarrays(vector<int>& nums, int k) {
        long long ans=0;
        for(int i=0;i<nums.size();i++){
            int x=nums[i];
            for(int j=i-1;j>=0&&(nums[j]&x)!=nums[j];j--){
                nums[j]&=x;
            }
            ans+=upper_bound(nums.begin(),nums.begin()+i+1,k)-lower_bound(nums.begin(),nums.begin()+i+1,k);
        }
        return ans;
    }
};
```

</details>

# 782 变为棋盘

一个 n x n 的二维网络 board 仅由 0 和 1 组成 。每次移动，你能交换任意两列或是两行的位置。

返回 将这个矩阵变为  “棋盘”  所需的最小移动次数 。如果不存在可行的变换，输出 -1。

“棋盘” 是指任意一格的上下左右四个方向的值均与本身不同的矩阵。

<details>

思路：

board 要能变成棋盘，有两个必要条件：

    只有两种行：A 行是从 0101⋯ 交换得到的，也就是说，A 行中的 0 的个数和 1 的个数相差至多为 1；B 行是从 1010⋯ 交换得到的，和 A 行完全相反。
    这两种行的个数相差至多为 1。

不能变成棋盘的情况

核心思路：统计 A 行和 B 行的个数。统计的过程中，如果发现有一行既不是 A 行也不是 B 行，直接返回 −1。统计结束后，如果发现 A 行和 B 行的个数相差超过 1，也返回 −1。

可以用哈希表统计。但是，没有这个必要。

以 board 第一行（或者任意一行）为参照物。

统计 board 第一行中的 0 和 1 的个数，如果 0 和 1 的个数相差超过 1，说明这一行既不是 A 行也不是 B 行，无论如何交换，绝不可能得到棋盘，返回 −1。

对于其余行 board[i]，首先比较 board\[i\][0] 和 board\[0\][0]：

    相同：那么 board[i] 必须和 board[0] 完全相同，即对于任意 j，board[i][j]=board[0][j] 成立。若不满足，返回 −1。
    不同：那么 board[i] 必须和 board[0] 完全不同，即对于任意 j，board[i][j]=board[0][j] 成立。若不满足，返回 −1。

如果没有返回 −1，那么说明只有 A 行和 B 行。我们也只需统计 board\[i\][0]（第一列）的 0 和 1 的个数，就知道有多少个 A 行和 B 行。如果 0 和 1 的个数相差超过 1，那么返回 −1。（代码实现时，关于第一列的判断可以放在前面）

如果上述情况都没有返回 −1，那么 board 一定可以变成棋盘，方法如下。
最小交换次数

比如把 s=001110 通过交换元素，变成 t=010101。这其中 s[i]=t[i] 出现了 4 次。我们需要让 s 中的 0 在偶数下标上，1 在奇数下标上。那么把奇数上的 0 和偶数上的 1 交换，就可以满足要求，所以只需要 24​=2 次交换。一般地，有如下定理。

定理：如果有 diff 个位置与目标值不同，那么只需交换 2diff​ 次。

证明：首先 2diff​ 是交换次数的下界。下面证明可以只用 2diff​ 次交换。

不失一般性，假设要把 s 通过交换，变成 t=010101⋯。注意这意味着当 n 是奇数时，0 的个数更多。

    如果 n 是偶数，那么恰好有 2n​ 个 0，2n​ 个 1，2n​ 个偶数下标，2n​ 个奇数下标。设有 k 个 0 在奇数下标上，那么有 2n​−k 个 1 在奇数下标上，所以有 2n​−(2n​−k)=k 个 1 在偶数下标上，所以只需 k 次交换。注意这意味着 diff=2k，所以 diff 必定是偶数。
    如果 n 是奇数，0 的个数比 1 的多 1，那么有 2n−1​+1 个 0 和 2n−1​ 个奇数下标，所以必然存在一个 0 在偶数下标上（也就是在正确的位置上），去掉这个 0，变成 n 是偶数的情况，结论同上。

根据该定理，得到如下计算方法：

    如果 n 是偶数，那么可以交换成 010101⋯，也可以交换成 101010⋯，设交换成 t=010101⋯ 的 s[i]=t[i] 的个数为 diff，那么交换成 t′=101010⋯ 的 s[i]=t′[i] 的个数为 n−diff，二者取最小值，最小交换次数为 2min(diff,n−diff)​。
    如果 n 是奇数，那么 t 是唯一的，最小交换次数为 2diff​。

答案是第一行的最小交换次数，加上第一列的最小交换次数。

交换完成后，就得到了棋盘。

```cpp
class Solution {
public:
    int movesToChessboard(vector<vector<int>>& board) {
        int n=board.size();
        auto& first_row=board[0];
        vector<int> first_col(n);
        int row_cnt[2]{},col_cnt[2]{};
        for(int i=0;i<n;i++){
            row_cnt[first_row[i]]++;
            first_col[i]=board[i][0];
            col_cnt[first_col[i]]++;
        }
        if(abs(row_cnt[0]-row_cnt[1])>1||abs(col_cnt[0]-col_cnt[1])>1){
            return -1;
        }
        for(auto& row:board){
            bool same=row[0]==first_row[0];
            for(int i=0;i<n;i++){
                if((row[i]==first_row[i])!=same){
                    return -1;
                }
            }
        }
        auto min_swap=[&](vector<int>& arr,int cnt[2]){
            int x0=cnt[1]>cnt[0];
            int diff=0;
            for(int i=0;i<n;i++){
                diff+=i%2^arr[i]^x0;
            }
            return n%2?diff/2:min(diff,n-diff)/2;
        };
        return min_swap(first_row,row_cnt)+min_swap(first_col,col_cnt);
    }
};
```

</details>

# 3280 将日期转换为二进制表示

给你一个字符串 date，它的格式为 yyyy-mm-dd，表示一个公历日期。

date 可以重写为二进制表示，只需要将年、月、日分别转换为对应的二进制表示（不带前导零）并遵循 year-month-day 的格式。

返回 date 的 二进制 表示。

<details>

```cpp
class Solution {
public:
    string convertDateToBinary(string date) {
        auto bin=[](int x)->string{
            string s=bitset<32>(x).to_string();
            return s.substr(s.find('1'));
        };
        return bin(stoi(date.substr(0, 4))) + "-" +
               bin(stoi(date.substr(5, 2))) + "-" +
               bin(stoi(date.substr(8, 2)));
    }
};
```

</details>

# 1706 球会落在何处？
用一个大小为 m x n 的二维网格 grid 表示一个箱子。你有 n 颗球。箱子的顶部和底部都是开着的。

箱子中的每个单元格都有一个对角线挡板，跨过单元格的两个角，可以将球导向左侧或者右侧。

    将球导向右侧的挡板跨过左上角和右下角，在网格中用 1 表示。
    将球导向左侧的挡板跨过右上角和左下角，在网格中用 -1 表示。

在箱子每一列的顶端各放一颗球。每颗球都可能卡在箱子里或从底部掉出来。如果球恰好卡在两块挡板之间的 "V" 形图案，或者被一块挡导向到箱子的任意一侧边上，就会卡住。

返回一个大小为 n 的数组 answer ，其中 answer[i] 是球放在顶部的第 i 列后从底部掉出来的那一列对应的下标，如果球卡在盒子里，则返回 -1 。

<details>

```cpp
class Solution{
public:
    vector<int> findBall(vector<vector<int>>& grid){
        int n=grid[0].size();
        vector<int> ans(n);
        for(int i=0;i<n;i++){
            int cur_col=i;
            for(auto& row:grid){
                int d=row[cur_col];
                cur_col+=d;
                if(cur_col<0 || cur_col==n || row[cur_col]!=d){
                    cur_col=-1;
                    break;
                }
            }
            ans[i]=cur_col;
        }
        return ans;
    }
};
```

</details>

# 348 井字棋

请在 n × n 的棋盘上，实现一个判定井字棋（Tic-Tac-Toe）胜负的神器，判断每一次玩家落子后，是否有胜出的玩家。

在这个井字棋游戏中，会有 2 名玩家，他们将轮流在棋盘上放置自己的棋子。

在实现这个判定器的过程中，你可以假设以下这些规则一定成立：

      1. 每一步棋都是在棋盘内的，并且只能被放置在一个空的格子里；

      2. 一旦游戏中有一名玩家胜出的话，游戏将不能再继续；

      3. 一个玩家如果在同一行、同一列或者同一斜对角线上都放置了自己的棋子，那么他便获得胜利。

<details>

```cpp
class TicTacToe {
    vector<int> rows,cols;
    int diagonal;
    int antiDiagonal;
public:
    TicTacToe(int n):rows(n,0),cols(n,0),diagonal(0),antiDiagonal(0) {

    }

    int move(int row, int col, int player) {
        int currentPlauer=(player==1)?1:-1;
        rows[row]+=currentPlauer;
        cols[col]+=currentPlauer;
        if(row==col){
            diagonal+=currentPlauer;
        }
        if(col==(cols.size()-row-1)){
            antiDiagonal+=currentPlauer;
        }
        int n=rows.size();
        if(abs(rows[row])==n||
        abs(cols[col])==n||
        abs(diagonal)==n||
        abs(antiDiagonal)==n){
            return player;
        }
        return 0;
    }
};

/**
 * Your TicTacToe object will be instantiated and called as such:
 * TicTacToe* obj = new TicTacToe(n);
 * int param_1 = obj->move(row,col,player);
 */
```

</details>

# 1538 找出隐藏数组中出现次数最多的元素

给定一个整数数组 nums，且 nums 中的所有整数都为 0 或 1。你不能直接访问这个数组，你需要使用 API ArrayReader ，该 API 含有下列成员函数：

    int query(int a, int b, int c, int d)：其中 0 <= a < b < c < d < ArrayReader.length() 。此函数查询以这四个参数为下标的元素并返回：
        4 : 当这四个元素相同（0 或 1）时。
        2 : 当其中三个元素的值等于 0 且一个元素等于 1 时，或当其中三个元素的值等于 1 且一个元素等于 0 时。
        0 : 当其中两个元素等于 0 且两个元素等于 1 时。
    int length()：返回数组的长度。

你可以调用 query() 最多 2 * n 次，其中 n 等于 ArrayReader.length()。

返回 nums 中出现次数最多的值的任意索引，若所有的值出现次数均相同，返回 -1。

<details>

```cpp
/**
 * // This is the ArrayReader's API interface.
 * // You should not implement it, or speculate about its implementation
 * class ArrayReader {
 *   public:
 *     // Compares 4 different elements in the array
 *     // return 4 if the values of the 4 elements are the same (0 or 1).
 *     // return 2 if three elements have a value equal to 0 and one element has value equal to 1 or vice versa.
 *     // return 0 : if two element have a value equal to 0 and two elements have a value equal to 1.
 *     int query(int a, int b, int c, int d);
 *
 *     // Returns the length of the array
 *     int length();
 * };
 */

class Solution {
public:
    int guessMajority(ArrayReader &reader) {
        int cnt1=1;
        int cnt2=0;
        int n=reader.length();
        int i2=-1;
        int base=reader.query(0,1,2,4);
        if(reader.query(1,2,3,4)==base){
            ++cnt1;
        }
        else{
            ++cnt2;
            i2=0;
        }
        if(reader.query(0,2,3,4)==base){
            ++cnt1;
        }else{
            ++cnt2;
            i2=1;
        }
        if(reader.query(0,1,3,4)==base){
            ++cnt1;
        }else{
            ++cnt2;
            i2=2;
        }
        base=reader.query(0,1,2,3);
        for(int i=4;i<n;i++){
            if(reader.query(0,1,2,i)==base){
                ++cnt1;
            }else{
                ++cnt2;
                i2=i;
            }
        }
        return cnt1!=cnt2?(cnt1>cnt2?3:i2):-1;
    }
};
```

</details>

# 1183 矩阵中1的最大数量

现在有一个尺寸为 width * height 的矩阵 M，矩阵中的每个单元格的值不是 0 就是 1。

而且矩阵 M 中每个大小为 sideLength * sideLength 的 正方形 子阵中，1 的数量不得超过 maxOnes。

请你设计一个算法，计算矩阵中最多可以有多少个 1。

<details>

```cpp
class Solution {
public:
    int maximumNumberOfOnes(int width, int height, int sideLength, int maxOnes) {
        vector<int> all;
        for(int i=0;i<sideLength;i++){
            for(int j=0;j<sideLength;j++){
                int w=width/sideLength+(width%sideLength>i?1:0);
                int h=height/sideLength+(height%sideLength>j?1:0);
                all.emplace_back(w*h);
            }
        }
        ranges::sort(all,std::greater<>());
        return accumulate(all.begin(),all.begin()+maxOnes,0);
    }
};
```
</details>

# 12 整数转罗马数字

七个不同的符号代表罗马数字，其值如下：
符号	值
I	1
V	5
X	10
L	50
C	100
D	500
M	1000

罗马数字是通过添加从最高到最低的小数位值的转换而形成的。将小数位值转换为罗马数字有以下规则：

    如果该值不是以 4 或 9 开头，请选择可以从输入中减去的最大值的符号，将该符号附加到结果，减去其值，然后将其余部分转换为罗马数字。
    如果该值以 4 或 9 开头，使用 减法形式，表示从以下符号中减去一个符号，例如 4 是 5 (V) 减 1 (I): IV ，9 是 10 (X) 减 1 (I)：IX。仅使用以下减法形式：4 (IV)，9 (IX)，40 (XL)，90 (XC)，400 (CD) 和 900 (CM)。
    只有 10 的次方（I, X, C, M）最多可以连续附加 3 次以代表 10 的倍数。你不能多次附加 5 (V)，50 (L) 或 500 (D)。如果需要将符号附加4次，请使用 减法形式。

给定一个整数，将其转换为罗马数字。

<details>

```cpp
class Solution {
 public:
  string intToRoman(int num) {
    int values[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    static vector<string> reps = {"M",  "CM", "D",  "CD", "C",  "XC", "L",
                            "XL", "X",  "IX", "V",  "IV", "I"};
    string res;
    for (int i = 0; i < 13; i++) {
      while (num >= values[i]) {
        num -= values[i];
        res += reps[i];
      }
    }
    return res;
  }
};
```

</details>

# 59 螺旋矩阵Ⅱ

给你一个正整数 n ，生成一个包含 1 到 n2 所有元素，且元素按顺时针顺序螺旋排列的 n x n 正方形矩阵 matrix 。

<details>

```cpp
class Solution {
    constexpr static array<pair<int,int>,4> dxy={
        {
            {0,1},
            {1,0},
            {0,-1},
            {-1,0}
        }
    };
public:
    vector<vector<int>> generateMatrix(int n) {
        vector matrix(n,vector<int>(n));
        int x=0,y=0;
        int directionIndex=0;
        int curNum=1,endNum=n*n;
        while(curNum<=endNum){
            matrix[x][y]=curNum++;
            auto [dx,dy]=dxy[directionIndex];
            int nx=x+dx,ny=y+dy;
            if(nx<0||ny<0||nx>=n||ny>=n||matrix[nx][ny]!=0){
                directionIndex=(directionIndex+1)%4;
            }
            x=x+dxy[directionIndex].first,y=y+dxy[directionIndex].second;
        }
        return matrix;
    }
};
```

</details>

# 1275 找出井字棋的获胜者

井字棋 是由两个玩家 A 和 B 在 3 x 3 的棋盘上进行的游戏。井字棋游戏的规则如下：

    玩家轮流将棋子放在空方格 (' ') 上。
    第一个玩家 A 总是用 'X' 作为棋子，而第二个玩家 B 总是用 'O' 作为棋子。
    'X' 和 'O' 只能放在空方格中，而不能放在已经被占用的方格上。
    只要有 3 个相同的（非空）棋子排成一条直线（行、列、对角线）时，游戏结束。
    如果所有方块都放满棋子（不为空），游戏也会结束。
    游戏结束后，棋子无法再进行任何移动。

给你一个数组 moves，其中 moves[i] = [rowi, coli] 表示第 i 次移动在 grid[rowi][coli]。如果游戏存在获胜者（A 或 B），就返回该游戏的获胜者；如果游戏以平局结束，则返回 "Draw"；如果仍会有行动（游戏未结束），则返回 "Pending"。

你可以假设 moves 都 有效（遵循 井字棋 规则），网格最初是空的，A 将先行动。

<details>

```cpp
class Solution {
public:
    string tictactoe(vector<vector<int>>& moves) {
        // 用数组记录0-2行、0-2列、正对角线、副对角线是否已满3个棋子
		// count[0-2]对应0-2行、count[3-5]对应0-2列、count[6]对应正对角线、count[7]对应副对角线
        int count[8]{};
        int n=moves.size();
        for(int i=n-1;i>=0;i-=2){
            count[moves[i][0]]++;
            count[moves[i][1]+3]++;
            if(moves[i][0]==moves[i][1]){
                count[6]++;
            }
            if(moves[i][0]+moves[i][1]==2){
                count[7]++;
            }
            if(count[moves[i][0]]==3|| count[moves[i][1]+3]==3||count[6]==3||count[7]==3){
                return n%2==0?"B":"A";
            }
        }
        if(moves.size()<9){
            return "Pending";
        }
        return "Draw";
    }
};
```

</details>

# 169 多数元素

给定一个大小为 n 的数组 nums ，返回其中的多数元素。多数元素是指在数组中出现次数 大于 ⌊ n/2 ⌋ 的元素。

你可以假设数组是非空的，并且给定的数组总是存在多数元素。

<details>

```cpp
class Solution {
public:
    int majorityElement(vector<int>& nums) {
        int candidate=-1;
        int count=0;
        for(int num:nums){
            if(num==candidate){
                count++;
            }else if(--count<0){
                candidate=num;
                count=1;
            }
        }
        return candidate;
    }
};
```

</details>

# 3644 排序排列

给你一个长度为 n 的整数数组 nums，其中 nums 是范围 [0..n - 1] 内所有数字的一个 排列 。

你可以在满足条件 nums[i] AND nums[j] == k 的情况下交换下标 i 和 j 的元素，其中 AND 表示按位与操作，k 是一个非负整数。

返回可以使数组按 非递减 顺序排序的最大值 k（允许进行任意次这样的交换）。如果 nums 已经是有序的，返回 0。

排列 是数组所有元素的一种重新排列。

<details>

```c++
class Solution {
public:
    int sortPermutation(vector<int>& nums) {
        int n=nums.size();
        int ans=(1<<30)-1;
        bool flag=true;
        for(int i=0;i<n;i++){
            if(nums[i]!=i){
                ans&=i;
                flag=false;
            }
        }
        return flag?0:ans;
    }
};
```

</details>

# n个物品中选两个，使其和为m的倍数的选法数量

```c++
#include <iostream>
#include <vector>
using namespace std;

// 计算从n个物品中选两个，使其和为m的倍数的选法数量
long long countPairs(vector<int>& arr, int n, int m) {
    // 创建一个长度为m的数组，用于存储每个余数出现的次数
    vector<long long> remainderCount(m, 0);

    // 统计每个余数出现的次数
    for (int i = 0; i < n; i++) {
        // 注意处理负数的情况，确保余数为非负
        int remainder = ((arr[i] % m) + m) % m;
        remainderCount[remainder]++;
    }

    // 计算结果
    long long result = 0;

    // 处理余数为0的情况（两个余数为0的数可以配对）
    result += (remainderCount[0] * (remainderCount[0] - 1)) / 2;

    // 处理m为偶数且余数为m/2的情况
    if (m % 2 == 0) {
        result += (remainderCount[m / 2] * (remainderCount[m / 2] - 1)) / 2;
    }

    // 处理其他余数的情况
    for (int i = 1; i < (m + 1) / 2; i++) {
        result += remainderCount[i] * remainderCount[m - i];
    }

    return result;
}

int main() {
    // 示例输入
    int n, m;
    cout << "输入物品数量n: ";
    cin >> n;

    cout << "输入除数m: ";
    cin >> m;

    vector<int> arr(n);
    cout << "输入" << n << "个物品的值: ";
    for (int i = 0; i < n; i++) {
        cin >> arr[i];
    }

    // 计算结果
    long long result = countPairs(arr, n, m);
    cout << "满足条件的选法数量: " << result << endl;

    return 0;
}
```
