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
