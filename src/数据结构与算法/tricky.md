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


