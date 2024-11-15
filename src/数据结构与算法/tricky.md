# 295 数据流的中文数

中位数是有序整数列表中的中间值。如果列表的大小是偶数，则没有中间值，中位数是两个中间值的平均值。

    例如 arr = [2,3,4] 的中位数是 3 。
    例如 arr = [2,3] 的中位数是 (2 + 3) / 2 = 2.5 。

实现 MedianFinder 类:

    MedianFinder() 初始化 MedianFinder 对象。

    void addNum(int num) 将数据流中的整数 num 添加到数据结构中。

    double findMedian() 返回到目前为止所有元素的中位数。与实际答案相差 10-5 以内的答案将被接受。


```cpp 
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

```cpp 
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


```cpp 
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

```cpp 
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

记题目给定的序列为 a，我们规定 ai 的取值集合为 a 的「值域」。我们用桶来表示值域中的每一个数，桶中记录这些数字出现的次数。假设a={5,5,2,3,6}，那么遍历这个序列得到的桶是这样的：

index  ->  1 2 3 4 5 6 7 8 9
value  ->  0 1 1 0 2 1 0 0 0

我们可以看出它第 i−1 位的前缀和表示「有多少个数比 i 小」。那么我们可以从后往前遍历序列 a，记当前遍历到的元素为 ai，我们把 ai 对应的桶的值自增 1，把 i−1 位置的前缀和加入到答案中算贡献。为什么这么做是对的呢，因为我们在循环的过程中，我们把原序列分成了两部分，后半部部分已经遍历过（已入桶），前半部分是待遍历的（未入桶），那么我们求到的 i−1 位置的前缀和就是「已入桶」的元素中比 ai大的元素的总和，而这些元素在原序列中排在 ai 的后面，但它们本应该排在 ai 的前面，这样就形成了逆序对。

我们显然可以用数组来实现这个桶，可问题是如果 ai 中有很大的元素，比如 109，我们就要开一个大小为 109 的桶，内存中是存不下的。这个桶数组中很多位置是 0，有效位置是稀疏的，我们要想一个办法让有效的位置全聚集到一起，减少无效位置的出现，这个时候我们就需要用到一个方法——离散化。

离散化一个序列的前提是我们只关心这个序列里面元素的相对大小，而不关心绝对大小（即只关心元素在序列中的排名）；离散化的目的是让原来分布零散的值聚集到一起，减少空间浪费。那么如何获得元素排名呢，我们可以对原序列排序后去重，对于每一个 ai 通过二分查找的方式计算排名作为离散化之后的值。当然这里也可以不去重，不影响排名。

```cpp 
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

# 3200 三角形的最大高度

给你两个整数 red 和 blue，分别表示红色球和蓝色球的数量。你需要使用这些球来组成一个三角形，满足第 1 行有 1 个球，第 2 行有 2 个球，第 3 行有 3 个球，依此类推。

每一行的球必须是 相同 颜色，且相邻行的颜色必须 不同。

返回可以实现的三角形的 最大 高度。
```cpp 
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

# 71 简化路径

给你一个字符串 path ，表示指向某一文件或目录的 Unix 风格 绝对路径 （以 '/' 开头），请你将其转化为 更加简洁的规范路径。

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

```cpp 
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

# 54 螺旋矩阵

给你一个 m 行 n 列的矩阵 matrix ，请按照 顺时针螺旋顺序 ，返回矩阵中的所有元素。

注意，dx, dy有顺序要求。

```cpp 
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

# 164 最大间距

给定一个无序的数组 nums，返回 数组在排序之后，相邻元素之间最大的差值 。如果数组元素个数小于 2，则返回 0 。

您必须编写一个在「线性时间」内运行并使用「线性额外空间」的算法。

思路：

看示例 1，想象有一根木棍，其左端点位置为 1，右端点位置为 9，长度为 9−1=8。我们在位置 3 和 6 处各切一刀，分成 3 个小木棍。问：最长小木棍的长度是多少？
下界

最长小木棍的长度，至少是多少？

设 n 为 nums 的长度，m 为 nums 的最小值，M 为 nums 的最大值。

m=M 时，直接返回 0；m+1=M 时，直接返回 1。下面讨论 m+1\<M 的情况，注意这意味着 n≥2。

木棍的长度为 M−m，我们要切 n−2 刀，分成 n−1 个小木棍，那么小木棍的平均长度为(M-m)/(n-1)。

由于最大值不低于平均值（见下面答疑），所以最长小木棍的长度至少为 n−1M−m。由于小木棍的长度是整数，所以最长小木棍的长度至少为
d=(M-m+n-1)/(n-1)的下取整。

怎么利用「答案至少为 d」这一性质呢？

如果把两两之差小于 d 的数，分到同一组（桶）中，那么答案一定不会是同一个桶内的两数之差，而是不同的桶的两数之差！

示例 1 的 nums=[3,6,9,1]，分到三个桶中：第一个桶装入 1 和 3，第二个桶装入 6，第三个桶装入 9。答案一定不会是第一个桶的两数之差，因为 3−1=2\<d=3。所以我们只需要考虑不同的桶之间的两数之差。由于题目要计算的是排序后相邻元素之间的差值，所以应当取第一个桶的最大值，和第二个桶的最小值作差；取第二个桶的最大值，和第三个桶的最小值作差；依此类推。这些差值中的最大值即为答案。对于示例 1 来说，答案为 max(6−3,9−6)=3。

一般地，从 m 开始，把元素值在 m,m+1,⋯,m+d−1 中的数分到第一个桶，把元素值在 m+d,m+d+1,⋯,m+2d−1 中的数分到第二个桶，依此类推。


```cpp
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

# 910 最小差值1 

给你一个整数数组 nums，和一个整数 k 。

对于每个下标 i（0 <= i < nums.length），将 nums[i] 变成 nums[i] + k 或 nums[i] - k 。

nums 的 分数 是 nums 中最大元素和最小元素的差值。

在更改每个下标对应的值之后，返回 nums 的最小 分数 。

```cpp 
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

# 31 下一个排列

整数数组的一个 排列  就是将其所有成员以序列或线性顺序排列。

    例如，arr = [1,2,3] ，以下这些都可以视作 arr 的排列：[1,2,3]、[1,3,2]、[3,1,2]、[2,3,1] 。

整数数组的 下一个排列 是指其整数的下一个字典序更大的排列。更正式地，如果数组的所有排列根据其字典顺序从小到大排列在一个容器中，那么数组的 下一个排列 就是在这个有序容器中排在它后面的那个排列。如果不存在下一个更大的排列，那么这个数组必须重排为字典序最小的排列（即，其元素按升序排列）。

    例如，arr = [1,2,3] 的下一个排列是 [1,3,2] 。
    类似地，arr = [2,3,1] 的下一个排列是 [3,1,2] 。
    而 arr = [3,2,1] 的下一个排列是 [1,2,3] ，因为 [3,2,1] 不存在一个字典序更大的排列。

给你一个整数数组 nums ，找出 nums 的下一个排列。

必须 原地 修改，只允许使用额外常数空间。

```cpp
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

# 287 寻找重复数

给定一个包含 n + 1 个整数的数组 nums ，其数字都在 [1, n] 范围内（包括 1 和 n），可知至少存在一个重复的整数。

假设 nums 只有 一个重复的整数 ，返回 这个重复的数 。

你设计的解决方案必须 不修改 数组 nums 且只用常量级 O(1) 的额外空间。

思路和算法

这个方法我们来将所有数二进制展开按位考虑如何找出重复的数，如果我们能确定重复数每一位是 1 还是 0 就可以按位还原出重复的数是什么。

考虑第 i 位，我们记 nums 数组中二进制展开后第 i 位为 1 的数有 x 个，数字 [1,n] 这 n 个数二进制展开后第 i 位为 1 的数有 y 个，那么重复的数第 i 位为 1 当且仅当 x>y。


```cpp
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

# 41 缺失的第一个正数

给你一个未排序的整数数组 nums ，请你找出其中没有出现的最小的正整数。
请你实现时间复杂度为 O(n) 并且只使用常数级别额外空间的解决方案。 

```cpp
class Solution {
public:
    int firstMissingPositive(vector<int>& nums) {
        int n=nums.size();
        for(int& num:nums){
            if(num<=0) num=n+1;
        }   
        for(int i=0;i<n;i++){
            int num=abs(nums[i]);
            if(num<=n) nums[num-1]=-abs(nums[num-1]);
        }
        for(int i=0;i<n;i++){
            if(nums[i]>0){
                return i+1;
            }
        }
        return n+1;
    }
};
```
# 3222 求出硬币游戏的赢家

给你两个 正 整数 x 和 y ，分别表示价值为 75 和 10 的硬币的数目。

Alice 和 Bob 正在玩一个游戏。每一轮中，Alice 先进行操作，Bob 后操作。每次操作中，玩家需要拿出价值 总和 为 115 的硬币。如果一名玩家无法执行此操作，那么这名玩家 输掉 游戏。

两名玩家都采取 最优 策略，请你返回游戏的赢家。

```cpp
class Solution {
public:
    string losingPlayer(int x, int y) {
        return min(x,y/4)%2?"Alice":"Bob";
    }
};
```
# 936 戳印序列

你想要用小写字母组成一个目标字符串 target。 

开始的时候，序列由 target.length 个 '?' 记号组成。而你有一个小写字母印章 stamp。

在每个回合，你可以将印章放在序列上，并将序列中的每个字母替换为印章上的相应字母。你最多可以进行 10 * target.length  个回合。

举个例子，如果初始序列为 "?????"，而你的印章 stamp 是 "abc"，那么在第一回合，你可以得到 "abc??"、"?abc?"、"??abc"。（请注意，印章必须完全包含在序列的边界内才能盖下去。）

如果可以印出序列，那么返回一个数组，该数组由每个回合中被印下的最左边字母的索引组成。如果不能印出序列，就返回一个空数组。

例如，如果序列是 "ababc"，印章是 "abc"，那么我们就可以返回与操作 "?????" -> "abc??" -> "ababc" 相对应的答案 [0, 2]；

另外，如果可以印出序列，那么需要保证可以在 10 * target.length 个回合内完成。任何超过此数字的答案将不被接受。

```cpp 
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



