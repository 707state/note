# 3235 判断矩形的两个角落是否可达

给你两个正整数 xCorner 和 yCorner 和一个二维整数数组 circles ，其中 circles[i] = [xi, yi, ri] 表示一个圆心在 (xi, yi) 半径为 ri 的圆。

坐标平面内有一个左下角在原点，右上角在 (xCorner, yCorner) 的矩形。你需要判断是否存在一条从左下角到右上角的路径满足：路径 完全 在矩形内部，不会 触碰或者经过 任何 圆的内部和边界，同时 只 在起点和终点接触到矩形。

如果存在这样的路径，请你返回 true ，否则返回 false 。

<details><summary>Click to expand</summary>

```cpp
class Solution {
public:
    bool in_circle(long long ox,long long oy,long long r,long long x,long long y){
        return (ox-x)*(ox-x)+(oy-y)*(oy-y)<=r*r;
    }
    bool canReachCorner(int xCorner, int yCorner, vector<vector<int>>& circles) {
        int n=circles.size();
        vector<int> vis(n);
        auto dfs=[&](auto&& dfs,int i)->bool{
            long long x1=circles[i][0],y1=circles[i][1],r1=circles[i][2];
            //圆i是否与矩形右边界/下边界相交相切
            if(y1<=yCorner&&abs(x1-xCorner)<=r1||x1<=xCorner&&y1<=r1||x1>xCorner&&in_circle(x1, y1, r1, xCorner, 0)){
                return true;
            }
            vis[i]=true;
            for(int j=0;j<n;j++){
                long long x2=circles[j][0],y2=circles[j][1],r2=circles[j][2];
                if(!vis[j]&&(x1-x2)*(x1-x2)+(y1-y2)*(y1-y2)<=(r1+r2)*(r1+r2)
                && x1*r2+x2*r1<(r1+r2)*xCorner
                && y1*r2+y2*r1<(r1+r2)*yCorner
                &&dfs(dfs,j)){
                    return true;
                }
            }
            return false;
        };
        for(int i=0;i<n;i++){
            long long x=circles[i][0],y=circles[i][1],r=circles[i][2];
            if(in_circle(x,y,r,0,0)||in_circle(x,y,r,xCorner,yCorner)||
            !vis[i] && (x<=xCorner && abs(y-yCorner)<=r ||
                y<=yCorner && x<=r || y>yCorner && in_circle(x,y,r,0,yCorner))&&dfs(dfs,i)){
                    return false;
                }
        }
        return true;
    }
};
```
</details>

# 386 字典序排数

给你一个整数 n ，按字典序返回范围 [1, n] 内所有整数。

你必须设计一个时间复杂度为 O(n) 且使用 O(1) 额外空间的算法。
<details><summary>Click to expand</summary>

```cpp 
class Solution {
public:
    vector<int> lexicalOrder(int n) {
        int number=1;
        vector<int> ans(n);
        for(int i=0;i<n;i++){
            ans[i]=number;
            if(number*10<=n){
                number*=10;
            }else{
                while(number%10==9||number+1>n){
                    number/=10;
                }
                number++;
            }
        }
        return ans;
    }
};
```
</details>
# 486 预测赢家

给你一个整数数组 nums 。玩家 1 和玩家 2 基于这个数组设计了一个游戏。

玩家 1 和玩家 2 轮流进行自己的回合，玩家 1 先手。开始时，两个玩家的初始分值都是 0 。每一回合，玩家从数组的任意一端取一个数字（即，nums[0] 或 nums[nums.length - 1]），取到的数字将会从数组中移除（数组长度减 1 ）。玩家选中的数字将会加到他的得分上。当数组中没有剩余数字可取时，游戏结束。

如果玩家 1 能成为赢家，返回 true 。如果两个玩家得分相等，同样认为玩家 1 是游戏的赢家，也返回 true 。你可以假设每个玩家的玩法都会使他的分数最大化。

<details><summary>Click to expand</summary>

```cpp
class Solution {
public:
    bool predictTheWinner(vector<int>& nums) {
        function<int(int,int,int)> dfs=[&](int start,int end,int turn){
            if(start==end){
                return nums[start]*turn;
            }
            int scoreStat=nums[start]*turn+dfs(start+1,end,-turn);
            int scoreEnd=nums[end]*turn+dfs(start,end-1,-turn);
            return max(scoreStat*turn,scoreEnd*turn)*turn;
        };
        return dfs(0,nums.size()-1,1)>=0;
    }
};
```
</details>
# 365 水壶问题

有两个水壶，容量分别为 x 和 y 升。水的供应是无限的。确定是否有可能使用这两个壶准确得到 target 升。

你可以：

    装满任意一个水壶
    清空任意一个水壶
    将水从一个水壶倒入另一个水壶，直到接水壶已满，或倒水壶已空。


<details><summary>Click to expand</summary>

```cpp
class Solution {
public:
    bool canMeasureWater(int x, int y, int target) {
        stack<pair<int,int>> stk;
        stk.emplace(0,0);
        auto hasher=[](const pair<int,int> &o){
            return hash<int>()(o.first)^hash<int>()(o.second);
        };
        unordered_set<pair<int,int>,decltype(hasher)> seen(0,hasher);
        while(stk.size()){
            if(seen.count(stk.top())){
                stk.pop();
                continue;
            }
            seen.emplace(stk.top());
            auto [remain_x,remain_y]=stk.top();
            stk.pop();
            if(remain_x==target||remain_y==target||remain_x+remain_y==target){
                return true;
            }
            stk.emplace(x,remain_y);
            stk.emplace(remain_x,y);
            stk.emplace(0,remain_y);
            stk.emplace(remain_x,0);
            stk.emplace(remain_x-min(remain_x,y-remain_y),remain_y+min(remain_x,y-remain_y));
            stk.emplace(remain_x+min(remain_y,x-remain_x),remain_y-min(remain_y,x-remain_x));
        }
        return false;
    }
};
```
</details>
# 130 被围绕的区域

给你一个 m x n 的矩阵 board ，由若干字符 'X' 和 'O' 组成，捕获 所有 被围绕的区域：

    连接：一个单元格与水平或垂直方向上相邻的单元格连接。
    区域：连接所有 'O' 的单元格来形成一个区域。
    围绕：如果您可以用 'X' 单元格 连接这个区域，并且区域中没有任何单元格位于 board 边缘，则该区域被 'X' 单元格围绕。

通过将输入矩阵 board 中的所有 'O' 替换为 'X' 来 捕获被围绕的区域。

<details><summary>Click to expand</summary>

```cpp
class Solution {
public:
    void solve(vector<vector<char>>& board) {
       int n=board.size();
       int m=board[0].size();
       if(n==0) return;
       auto dfs=[&](auto&& dfs,int x,int y){
        if(x<0||x>=n||y<0||y>=m||board[x][y]!='O'){
            return;
        }
        board[x][y]='A';
        dfs(dfs,x+1,y);
        dfs(dfs,x-1,y);
        dfs(dfs,x,y-1);
        dfs(dfs,x,y+1);
       }; 
       for(int i=0;i<n;i++){
        dfs(dfs,i,0);
        dfs(dfs,i,m-1);
       }
        for(int j=0;j<m;j++){
            dfs(dfs,0,j);
            dfs(dfs,n-1,j);
        }
        for(int i=0;i<n;i++){
            for(int j=0;j<m;j++){
                if(board[i][j]=='A'){
                    board[i][j]='O';
                }else if(board[i][j]=='O'){
                    board[i][j]='X';
                }
            }
        }
    }
};
```
</details>
# 529 扫地雷



给你一个大小为 m x n 二维字符矩阵 board ，表示扫雷游戏的盘面，其中：

    'M' 代表一个 未挖出的 地雷，
    'E' 代表一个 未挖出的 空方块，
    'B' 代表没有相邻（上，下，左，右，和所有4个对角线）地雷的 已挖出的 空白方块，
    数字（'1' 到 '8'）表示有多少地雷与这块 已挖出的 方块相邻，
    'X' 则表示一个 已挖出的 地雷。

给你一个整数数组 click ，其中 click = [clickr, clickc] 表示在所有 未挖出的 方块（'M' 或者 'E'）中的下一个点击位置（clickr 是行下标，clickc 是列下标）。

根据以下规则，返回相应位置被点击后对应的盘面：

    如果一个地雷（'M'）被挖出，游戏就结束了- 把它改为 'X' 。
    如果一个 没有相邻地雷 的空方块（'E'）被挖出，修改它为（'B'），并且所有和其相邻的 未挖出 方块都应该被递归地揭露。
    如果一个 至少与一个地雷相邻 的空方块（'E'）被挖出，修改它为数字（'1' 到 '8' ），表示相邻地雷的数量。
    如果在此次点击中，若无更多方块可被揭露，则返回盘面。

<details><summary>Click to expand</summary>

```cpp
class Solution {
    constexpr static array<int,8> dx={1,0,-1,0,1,1,-1,-1},dy={0,1,0,-1,-1,1,-1,1};
public:
    void dfs(vector<vector<char>>& board,int x,int y){
        int cnt=0;
        for(int i=0;i<8;i++){
            int tx=x+dx[i];
            int ty=y+dy[i];
            if(tx<0||tx>=board.size()||ty<0||ty>=board[0].size()){
                continue;
            }
            cnt+=board[tx][ty]=='M';
        }
        if(cnt>0){
            board[x][y]=cnt+'0';
        }else{
            board[x][y]='B';
            for(int i=0;i<8;i++){
                int tx=x+dx[i];
                int ty=y+dy[i];
                if(tx<0||tx>=board.size()||ty<0||ty>=board[0].size()||board[tx][ty]!='E'){
                    continue;
                }
                dfs(board,tx,ty);
            }
        }
    }
    vector<vector<char>> updateBoard(vector<vector<char>>& board, vector<int>& click) {
        int x=click[0];
        int y=click[1];
        if(board[x][y]=='M'){
            board[x][y]='X';
        }else{
            dfs(board,x,y);
        }
        return board;
    }
};
```
</details>
