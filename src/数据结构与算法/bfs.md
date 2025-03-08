<!--toc:start-->
- [3243 新增道路查询后的最短距离1](#3243-新增道路查询后的最短距离1)
- [407 接雨水Ⅱ](#407-接雨水ⅱ)
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
