---
title: "dp"
author: "jask"
date: "10/01/2024"
output: pdf_document
header-includes:
  - \usepackage{fontspec}
  - \usepackage{xeCJK}
  - \setmainfont{ComicShannsMono Nerd Font}
  - \setCJKmainfont{Noto Sans CJK SC}  # 替换为可用的字体
  - \setCJKmonofont{Noto Sans CJK SC}
  - \setCJKsansfont{Noto Sans CJK SC}
  - \usepackage[top=1cm, bottom=1cm, left=1cm, right=1cm]{geometry}
---

# 983 最低票价

在一个火车旅行很受欢迎的国度，你提前一年计划了一些火车旅行。在接下来的一年里，你要旅行的日子将以一个名为 days 的数组给出。每一项是一个从 1 到 365 的整数。

火车票有 三种不同的销售方式 ：

    一张 为期一天 的通行证售价为 costs[0] 美元；
    一张 为期七天 的通行证售价为 costs[1] 美元；
    一张 为期三十天 的通行证售价为 costs[2] 美元。

通行证允许数天无限制的旅行。 例如，如果我们在第 2 天获得一张 为期 7 天 的通行证，那么我们可以连着旅行 7 天：第 2 天、第 3 天、第 4 天、第 5 天、第 6 天、第 7 天和第 8 天。

返回 你想要完成在给定的列表 days 中列出的每一天的旅行所需要的最低消费 。

```c++ 
class Solution {
public:
    int mincostTickets(vector<int>& days, vector<int>& costs) {

        array<int, 366> dp; // dp[i]表示本年度第i天出游的花费
        int last = min(costs[0], min(costs[1], costs[2]));
        if (days.size() == 1) {
            return last;
        }
        dp[days[0]] = last;
        for (int i = days[0]; i <= days[1]; i++) {
            dp[i] = last;
        }
        int index = 1;
        for (int i = days[1]; i < 366; i++) {
            if (i == days[index]) { // 这一天需要去旅游
                index++;
                if (i >= 30) { // 从30天前，7天前，1天前
                    dp[i] =
                        min(dp[i - 30] + costs[2],
                            min(dp[i - 7] + costs[1], dp[i - 1] + costs[0]));
                } else if (i >= 7) {
                    dp[i] = min(
                        min(dp[i - 7] + costs[1], dp[i - 1] + costs[0]),
                        costs[2]); // 要么从七天前或者一天前或者涵盖在30天之内
                } else {
                    dp[i] = min(dp[i - 1] + costs[0], min(costs[2], costs[1]));
                }
                last = dp[i]; // 最近一天总共花了多少钱
            } else {          // 不用旅游的花费
                dp[i] = last;
            }
            if (index == days.size()) {
                return dp[i];
            }
        }
        return dp[365];
    }
};
```

# 组合总和 4 

给定一个由 不同 正整数组成的数组 nums ，和一个目标整数 target 。请从 nums 中找出并返回总和为 target 的元素组合的个数。数组中的数字可以在一次排列中出现任意次，但是顺序不同的序列被视作不同的组合。

```c++ 
class Solution {
public:
    int combinationSum4(vector<int>& nums, int target) {
        vector<int> dp(target+1,0);
        dp[0]=1;
        for(int i=1;i<=target;i++){
            for(auto& num: nums){
                if(num<=i&& dp[i - num] < INT_MAX - dp[i]){
                    dp[i]+=dp[i-num];
                }
            }
        }
        return dp[target];
    }
};
```

# 1928 规定时间内到达终点的最小花费

一个国家有 n 个城市，城市编号为 0 到 n - 1 ，题目保证 所有城市 都由双向道路 连接在一起 。道路由二维整数数组 edges 表示，其中 edges[i] = [xi, yi, timei] 表示城市 xi 和 yi 之间有一条双向道路，耗费时间为 timei 分钟。两个城市之间可能会有多条耗费时间不同的道路，但是不会有道路两头连接着同一座城市。

每次经过一个城市时，你需要付通行费。通行费用一个长度为 n 且下标从 0 开始的整数数组 passingFees 表示，其中 passingFees[j] 是你经过城市 j 需要支付的费用。

一开始，你在城市 0 ，你想要在 maxTime 分钟以内 （包含 maxTime 分钟）到达城市 n - 1 。旅行的 费用 为你经过的所有城市 通行费之和 （包括 起点和终点城市的通行费）。

给你 maxTime，edges 和 passingFees ，请你返回完成旅行的 最小费用 ，如果无法在 maxTime 分钟以内完成旅行，请你返回 -1 。

```c++ 
class Solution {
public:
    int minCost(int maxTime, vector<vector<int>>& edges, vector<int>& passingFees) {
        int n=passingFees.size();//城市总数
        vector<vector<int>> dp(maxTime+1,vector<int>(n,INT_MAX/2));//dp[t][i]表示t分钟到达城市i的最少通行费总和
        dp[0][0]=passingFees[0];
        for(int t=1;t<=maxTime;t++){
            for(const auto& edge: edges){
                int i=edge[0],j=edge[1],cost=edge[2];
                if(cost<=t){//因为是双向道路，所以i, j之间可以互相到达
                    dp[t][i]=min(dp[t][i],dp[t-cost][j]+passingFees[i]);
                    dp[t][j]=min(dp[t][j],dp[t-cost][i]+passingFees[j]);
                }
            }
        }
        int ans=INT_MAX/2;
        for(int i=1;i<=maxTime;i++){//找到到达n-1城市的最小值
            ans=min(ans,dp[i][n-1]);
        }
        return ans==INT_MAX/2?-1:ans;
    }
};
```

# LCP 09 最小跳跃次数

为了给刷题的同学一些奖励，力扣团队引入了一个弹簧游戏机。游戏机由 N 个特殊弹簧排成一排，编号为 0 到 N-1。初始有一个小球在编号 0 的弹簧处。若小球在编号为 i 的弹簧处，通过按动弹簧，可以选择把小球向右弹射 jump[i] 的距离，或者向左弹射到任意左侧弹簧的位置。也就是说，在编号为 i 弹簧处按动弹簧，小球可以弹向 0 到 i-1 中任意弹簧或者 i+jump[i] 的弹簧（若 i+jump[i]>=N ，则表示小球弹出了机器）。小球位于编号 0 处的弹簧时不能再向左弹。

为了获得奖励，你需要将小球弹出机器。请求出最少需要按动多少次弹簧，可以将小球从编号 0 弹簧弹出整个机器，即向右越过编号 N-1 的弹簧。


思路：

从右向左计算dp值(从后向前)，当前位置如果为i 则它如果直接跳到右边（前面）去就是dp[jump[i]+i]+1（这个值已经计算过了），计算出当前位置dp[i]之后，当前位置i可以影响 i+1到dp[j] >= dp[i]+1位置上的值（因为某个位置可以跳到左边任意位置）注意遍历到dp[j]>=dp[i]+1即可。
```c++ 
class Solution {
public:
    int minJump(vector<int>& jump) {
        int n=jump.size();
        vector<int> dp(n);
        dp[n-1]=1;
        for(int i=n-2;i>=0;i--){
            dp[i]=(i+jump[i]>=n)?1: dp[jump[i]+i]+1;
            for(int j=i+1;j<n&&dp[j]>=dp[i]+1;++j){
                dp[j]=dp[i]+1;
            }
        }
        return dp[0];
    }
};
```


# 790 多米诺和托米诺平铺

```c++ 
class Solution {
public:
    int numTilings(int n) {
        vector<long> dp(n+1,0);
        if(n==1) return 1;
        if(n==2) return 2;
        constexpr static int mod=1e9+7;
        dp[0]=1;
        dp[1]=1;
        dp[2]=2;
        for(int i=3;i<=n;i++){
           dp[i]=(dp[i-1]*2+dp[i-3])%mod;
        }
        return dp[n];
    }
};
```
