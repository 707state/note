<!--toc:start-->
- [3243 新增道路查询后的最短距离1](#3243-新增道路查询后的最短距离1)
- [407 接雨水Ⅱ](#407-接雨水ⅱ)
- [UNSOLVED 773 滑动谜题](#unsolved-773-滑动谜题)
- [UNSOLVED 2612 最少翻转操作数](#unsolved-2612-最少翻转操作数)
<!--toc:end-->

# 3243 新增道路查询后的最短距离1

给你一个整数 n 和一个二维整数数组 queries。

有 n 个城市，编号从 0 到 n - 1。初始时，每个城市 i
都有一条单向道路通往城市 i + 1（ 0 \<= i \< n - 1）。

queries\[i\] = \[ui, vi\] 表示新建一条从城市 ui 到城市 vi
的单向道路。每次查询后，你需要找到从城市 0 到城市 n - 1
的最短路径的长度。

返回一个数组 answer，对于范围 \[0, queries.length - 1\] 中的每个
i，answer\[i\] 是处理完前 i + 1 个查询后，从城市 0 到城市 n - 1
的最短路径的长度。

<details><summary>Click to expand</summary>

``` cpp
class Solution {
public:
    vector<int> shortestDistanceAfterQueries(int n, vector<vector<int>>& queries) {
        vector<vector<int>> graph(n);
        for(int i=0;i<n-1;i++){
            graph[i].emplace_back(i+1);
        }
        vector<int> res;
        for(auto& query: queries){
            graph[query[0]].emplace_back(query[1]);
            res.emplace_back(bfs(n,graph));
        }
        return res;
    }
    int bfs(int n,vector<vector<int>>& nums){
        vector<int> dist(n,-1);
        queue<int> q;
        q.push(0);
        dist[0]=0;
        while(q.size()){
            int x=q.front();
            q.pop();
            for(auto y: nums[x]){
                if(dist[y]>=0) continue;
                q.push(y);
                dist[y]=dist[x]+1;
            }
        }
        return dist.back();
    }
};
```

</details>

# 407 接雨水Ⅱ

给你一个 m x n 的矩阵，其中的值均为非负整数，代表二维高度图每个单元的高度，请计算图中形状最多能接多少体积的雨水。

<details>

```cpp
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

# UNSOLVED 773 滑动谜题

在一个 2 x 3 的板上（board）有 5 块砖瓦，用数字 1~5 来表示, 以及一块空缺用 0 来表示。一次 移动 定义为选择 0 与一个相邻的数字（上下左右）进行交换.

最终当板 board 的结果是 [[1,2,3],[4,5,0]] 谜板被解开。

给出一个谜板的初始状态 board ，返回最少可以通过多少次移动解开谜板，如果不能解开谜板，则返回 -1 。

<details>

```cpp
class Solution {
    vector<vector<int>> neighbors = {{1, 3}, {0, 2, 4}, {1, 5},
                                     {0, 4}, {1, 3, 5}, {2, 4}};

public:
    int slidingPuzzle(vector<vector<int>>& board) {
        auto get = [&](string& status) -> vector<string> {
            vector<string> ret;
            int x = status.find('0');
            for (int y : neighbors[x]) {
                swap(status[x], status[y]);
                ret.push_back(status);
                swap(status[x], status[y]);
            }
            return ret;
        };
        string initial;
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 3; j++) {
                initial += char(board[i][j] + '0');
            }
        }
        if (initial == "123450") {
            return 0;
        }
        queue<pair<string, int>> q;
        q.emplace(initial, 0);
        unordered_set<string> seen = {initial};
        while (!q.empty()) {
            auto [status, step] = q.front();
            q.pop();
            for (auto&& next_status : get(status)) {
                if (!seen.count(next_status)) {
                    if (next_status == "123450") {
                        return step + 1;
                    }
                    q.emplace(next_status, step + 1);
                    seen.insert(move(next_status));
                }
            }
        }
        return -1;
    }
};
```

A*做法：

```cpp
struct AStar {
    // 曼哈顿距离
    static constexpr array<array<int, 6>, 6> dist = {{
        {0, 1, 2, 1, 2, 3},
        {1, 0, 1, 2, 1, 2},
        {2, 1, 0, 3, 2, 1},
        {1, 2, 3, 0, 1, 2},
        {2, 1, 2, 1, 0, 1},
        {3, 2, 1, 2, 1, 0}
    }};

    // 计算启发函数
    static int getH(const string& status) {
        int ret = 0;
        for (int i = 0; i < 6; ++i) {
            if (status[i] != '0') {
                ret += dist[i][status[i] - '1'];
            }
        }
        return ret;
    };

    AStar(const string& status, int g): status_{status}, g_{g}, h_{getH(status)} {
        f_ = g_ + h_;
    }

    bool operator< (const AStar& that) const {
        return f_ > that.f_;
    }

    string status_;
    int f_, g_, h_;
};

class Solution {
private:
    vector<vector<int>> neighbors = {{1, 3}, {0, 2, 4}, {1, 5}, {0, 4}, {1, 3, 5}, {2, 4}};;

public:
    int slidingPuzzle(vector<vector<int>>& board) {
        // 枚举 status 通过一次交换操作得到的状态
        auto get = [&](string& status) -> vector<string> {
            vector<string> ret;
            int x = status.find('0');
            for (int y: neighbors[x]) {
                swap(status[x], status[y]);
                ret.push_back(status);
                swap(status[x], status[y]);
            }
            return ret;
        };

        string initial;
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 3; ++j) {
                initial += char(board[i][j] + '0');
            }
        }
        if (initial == "123450") {
            return 0;
        }

        priority_queue<AStar> q;
        q.emplace(initial, 0);
        unordered_set<string> seen = {initial};

        while (!q.empty()) {
            AStar node = q.top();
            q.pop();
            for (auto&& next_status: get(node.status_)) {
                if (!seen.count(next_status)) {
                    if (next_status == "123450") {
                        return node.g_ + 1;
                    }
                    q.emplace(next_status, node.g_ + 1);
                    seen.insert(move(next_status));
                }
            }
        }

        return -1;
    }
};
```

</details>

# UNSOLVED 2612 最少翻转操作数

给你一个整数 n 和一个在范围 [0, n - 1] 以内的整数 p ，它们表示一个长度为 n 且下标从 0 开始的数组 arr ，数组中除了下标为 p 处是 1 以外，其他所有数都是 0 。

同时给你一个整数数组 banned ，它包含数组中的一些位置。banned 中第 i 个位置表示 arr[banned[i]] = 0 ，题目保证 banned[i] != p 。

你可以对 arr 进行 若干次 操作。一次操作中，你选择大小为 k 的一个 子数组 ，并将它 翻转 。在任何一次翻转操作后，你都需要确保 arr 中唯一的 1 不会到达任何 banned 中的位置。换句话说，arr[banned[i]] 始终 保持 0 。

请你返回一个数组 ans ，对于 [0, n - 1] 之间的任意下标 i ，ans[i] 是将 1 放到位置 i 处的 最少 翻转操作次数，如果无法放到位置 i 处，此数为 -1 。

    子数组 指的是一个数组里一段连续 非空 的元素序列。
    对于所有的 i ，ans[i] 相互之间独立计算。
    将一个数组中的元素 翻转 指的是将数组中的值变成 相反顺序 。

<details>

```cpp
class Solution {
public:
    vector<int> minReverseOperations(int n, int p, vector<int>& banned, int k) {
        unordered_set<int> ban{banned.begin(), banned.end()};
        set<int> indices[2];
        for (int i = 0; i < n; i++) {
            if (i != p && !ban.contains(i)) {
                indices[i % 2].insert(i);
            }
        }
        indices[0].insert(n); // 哨兵，下面无需判断 it != st.end()
        indices[1].insert(n);
        vector<int> ans(n, -1);
        ans[p] = 0; // 起点
        queue<int> q;
        q.push(p);
        while (!q.empty()) {
            int i = q.front(); q.pop();
            // indices[mn % 2] 中的从 mn 到 mx 的所有下标都可以从 i 翻转到
            int mn = max(i - k + 1, k - i - 1);
            int mx = min(i + k - 1, n * 2 - k - i - 1);
            auto& st = indices[mn % 2];
            for (auto it = st.lower_bound(mn); *it <= mx; it = st.erase(it)) {
                ans[*it] = ans[i] + 1; // 移动一步
               q.push(*it);
            }
        }
        return ans;
    }
};
```

</details>
