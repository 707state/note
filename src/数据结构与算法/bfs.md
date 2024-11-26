# 3243 新增道路查询后的最短距离 1 

给你一个整数 n 和一个二维整数数组 queries。

有 n 个城市，编号从 0 到 n - 1。初始时，每个城市 i 都有一条单向道路通往城市 i + 1（ 0 <= i < n - 1）。

queries[i] = [ui, vi] 表示新建一条从城市 ui 到城市 vi 的单向道路。每次查询后，你需要找到从城市 0 到城市 n - 1 的最短路径的长度。

返回一个数组 answer，对于范围 [0, queries.length - 1] 中的每个 i，answer[i] 是处理完前 i + 1 个查询后，从城市 0 到城市 n - 1 的最短路径的长度。

<details><summary>Click to expand</summary>

```cpp
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
