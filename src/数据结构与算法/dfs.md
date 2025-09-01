<!--toc:start-->
- [3235 判断矩形的两个角落是否可达](#3235-判断矩形的两个角落是否可达)
- [386 字典序排数](#386-字典序排数)
- [486 预测赢家](#486-预测赢家)
- [365 水壶问题](#365-水壶问题)
- [130 被围绕的区域](#130-被围绕的区域)
- [529 UNSOLVED 扫地雷](#529-unsolved-扫地雷)
- [2056 棋盘上有效移动组合的数目](#2056-棋盘上有效移动组合的数目)
- [688 骑士落在棋盘上的概率](#688-骑士落在棋盘上的概率)
- [UNSOLVED 1755 最接近目标值的子序列和](#unsolved-1755-最接近目标值的子序列和)
- [1755 最接近目标值的子序列和](#1755-最接近目标值的子序列和)
- [980 不同路径Ⅲ](#980-不同路径ⅲ)
- [2698 求一个整数的惩罚数](#2698-求一个整数的惩罚数)
- [3669 K 因数分解](#3669-k-因数分解)
<!--toc:end-->


# 3235 判断矩形的两个角落是否可达

给你两个正整数 xCorner 和 yCorner 和一个二维整数数组 circles ，其中
circles\[i\] = \[xi, yi, ri\] 表示一个圆心在 (xi, yi) 半径为 ri 的圆。

坐标平面内有一个左下角在原点，右上角在 (xCorner, yCorner)
的矩形。你需要判断是否存在一条从左下角到右上角的路径满足：路径 完全
在矩形内部，不会 触碰或者经过 任何 圆的内部和边界，同时 只
在起点和终点接触到矩形。

如果存在这样的路径，请你返回 true ，否则返回 false 。

<details><summary>Click to expand</summary>

``` cpp
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

给你一个整数 n ，按字典序返回范围 \[1, n\] 内所有整数。

你必须设计一个时间复杂度为 O(n) 且使用 O(1) 额外空间的算法。

<details><summary>Click to expand</summary>

``` cpp
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

玩家 1 和玩家 2 轮流进行自己的回合，玩家 1 先手。开始时，两个玩家的初始分值都是 0。每一回合，玩家从数组的任意一端取一个数字（即，nums\[0\] 或nums\[nums.length - 1\]），取到的数字将会从数组中移除（数组长度减 1）。玩家选中的数字将会加到他的得分上。当数组中没有剩余数字可取时，游戏结束。

如果玩家 1 能成为赢家，返回 true 。如果两个玩家得分相等，同样认为玩家 1 是游戏的赢家，也返回 true。你可以假设每个玩家的玩法都会使他的分数最大化。

<details><summary>Click to expand</summary>

``` cpp
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

有两个水壶，容量分别为 x 和 y
升。水的供应是无限的。确定是否有可能使用这两个壶准确得到 target 升。

你可以：

    装满任意一个水壶
    清空任意一个水壶
    将水从一个水壶倒入另一个水壶，直到接水壶已满，或倒水壶已空。

<details><summary>Click to expand</summary>

``` cpp
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

给你一个 m x n 的矩阵 board ，由若干字符 \'X\' 和 \'O\' 组成，捕获 所有
被围绕的区域：

    连接：一个单元格与水平或垂直方向上相邻的单元格连接。
    区域：连接所有 'O' 的单元格来形成一个区域。
    围绕：如果您可以用 'X' 单元格 连接这个区域，并且区域中没有任何单元格位于 board 边缘，则该区域被 'X' 单元格围绕。

通过将输入矩阵 board 中的所有 \'O\' 替换为 \'X\' 来 捕获被围绕的区域。

<details><summary>Click to expand</summary>

``` cpp
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

# 529 UNSOLVED 扫地雷

给你一个大小为 m x n 二维字符矩阵 board ，表示扫雷游戏的盘面，其中：

    'M' 代表一个 未挖出的 地雷，
    'E' 代表一个 未挖出的 空方块，
    'B' 代表没有相邻（上，下，左，右，和所有4个对角线）地雷的 已挖出的 空白方块，
    数字（'1' 到 '8'）表示有多少地雷与这块 已挖出的 方块相邻，
    'X' 则表示一个 已挖出的 地雷。

给你一个整数数组 click ，其中 click = \[clickr, clickc\] 表示在所有
未挖出的 方块（\'M\' 或者 \'E\'）中的下一个点击位置（clickr
是行下标，clickc 是列下标）。

根据以下规则，返回相应位置被点击后对应的盘面：

    如果一个地雷（'M'）被挖出，游戏就结束了- 把它改为 'X' 。
    如果一个 没有相邻地雷 的空方块（'E'）被挖出，修改它为（'B'），并且所有和其相邻的 未挖出 方块都应该被递归地揭露。
    如果一个 至少与一个地雷相邻 的空方块（'E'）被挖出，修改它为数字（'1' 到 '8' ），表示相邻地雷的数量。
    如果在此次点击中，若无更多方块可被揭露，则返回盘面。

<details><summary>Click to expand</summary>

``` cpp
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

# 2056 棋盘上有效移动组合的数目

有一个 8 x 8 的棋盘，它包含 n 个棋子（棋子包括车，后和象三种）。给你一个长度为 n 的字符串数组 pieces ，其中 pieces[i] 表示第 i 个棋子的类型（车，后或象）。除此以外，还给你一个长度为 n 的二维整数数组 positions ，其中 positions[i] = [ri, ci] 表示第 i 个棋子现在在棋盘上的位置为 (ri, ci) ，棋盘下标从 1 开始。

棋盘上每个棋子都可以移动 至多一次 。每个棋子的移动中，首先选择移动的 方向 ，然后选择 移动的步数 ，同时你要确保移动过程中棋子不能移到棋盘以外的地方。棋子需按照以下规则移动：

    车可以 水平或者竖直 从 (r, c) 沿着方向 (r+1, c)，(r-1, c)，(r, c+1) 或者 (r, c-1) 移动。
    后可以 水平竖直或者斜对角 从 (r, c) 沿着方向 (r+1, c)，(r-1, c)，(r, c+1)，(r, c-1)，(r+1, c+1)，(r+1, c-1)，(r-1, c+1)，(r-1, c-1) 移动。
    象可以 斜对角 从 (r, c) 沿着方向 (r+1, c+1)，(r+1, c-1)，(r-1, c+1)，(r-1, c-1) 移动。

移动组合 包含所有棋子的 移动 。每一秒，每个棋子都沿着它们选择的方向往前移动 一步 ，直到它们到达目标位置。所有棋子从时刻 0 开始移动。如果在某个时刻，两个或者更多棋子占据了同一个格子，那么这个移动组合 不有效 。

请你返回 有效 移动组合的数目。

注意：

    初始时，不会有两个棋子 在 同一个位置 。
    有可能在一个移动组合中，有棋子不移动。
    如果两个棋子 直接相邻 且两个棋子下一秒要互相占据对方的位置，可以将它们在同一秒内 交换位置 。

<details>

```cpp
class Solution {
    struct Move{
        int x0,y0;
        int dx,dy;
        int step;
    };
    constexpr static auto DIRS= array<pair<int,int>,8>{{
        {-1,0},{1,0},{0,-1},{0,1},{1,1},{-1,1},{-1,-1},{1,-1}
    }};
    unordered_map<char,vector<pair<int,int>>> PIECE_DIRS={
        {'r',{DIRS.begin(),DIRS.begin()+4}},
        {'b',{DIRS.begin()+4,DIRS.end()}},
        {'q',{DIRS.begin(),DIRS.end()}}
    };
public:
    vector<Move> generate_moves(int x0,int y0,vector<pair<int,int>>& dirs){
        const int SIZE=8;
        vector<Move> moves={{x0,y0,0,0,0}};
        for(auto [dx,dy]:dirs){
            int x=x0+dx,y=y0+dy;
            for(int step=1;x>0&&x<=SIZE&&y>0&&y<=SIZE;step++){
                moves.emplace_back(x0,y0,dx,dy,step);
                x+=dx;
                y+=dy;
            }
        }
        return moves;
    }
    bool is_valid(Move& m1,Move& m2){
        int x1=m1.x0,y1=m1.y0;
        int x2=m2.x0,y2=m2.y0;
        for(int i=0;i<max(m1.step,m2.step);++i){
            if(i<m1.step){
                x1+=m1.dx;
                y1+=m1.dy;
            }
            if(i<m2.step){
                x2+=m2.dx;
                y2+=m2.dy;
            }
            if(x1==x2 && y1==y2){
                return false;
            }
        }
        return true;
    }
    int countCombinations(vector<string>& pieces, vector<vector<int>>& positions) {
        int n=pieces.size();
        vector<vector<Move>> all_moves(n);
        for(int i=0;i<n;i++){
            all_moves[i]=generate_moves(positions[i][0], positions[i][1],PIECE_DIRS[pieces[i][0]]);
        }
        vector<Move> path(n);
        int ans=0;
        auto dfs=[&](auto&& dfs,int i)->void{
            if(i==n){
                ans++;
                return;
            }
            for(auto& move1: all_moves[i]){
                bool ok=true;
                for(int j=0;j<i;j++){
                    if(!is_valid(move1,path[j])){
                        ok=false;
                        break;
                    }
                }
                if(ok){
                    path[i]=move1;
                    dfs(dfs,i+1);
                }
            }
        };
        dfs(dfs,0);
        return ans;
    }
};
```

</details>

# 688 骑士落在棋盘上的概率

在一个 n x n 的国际象棋棋盘上，一个骑士从单元格 (row, column) 开始，并尝试进行 k 次移动。行和列是 从 0 开始 的，所以左上单元格是 (0,0) ，右下单元格是 (n - 1, n - 1) 。

象棋骑士有8种可能的走法，如下图所示。每次移动在基本方向上是两个单元格，然后在正交方向上是一个单元格。

每次骑士要移动时，它都会随机从8种可能的移动中选择一种(即使棋子会离开棋盘)，然后移动到那里。

骑士继续移动，直到它走了 k 步或离开了棋盘。

返回 骑士在棋盘停止移动后仍留在棋盘上的概率 。

<details>

```cpp
class Solution {
    static constexpr array<pair<int, int>, 8> DIRS = {{{2, 1},
                                                       {1, 2},
                                                       {-1, 2},
                                                       {-2, 1},
                                                       {-2, -1},
                                                       {-1, -2},
                                                       {1, -2},
                                                       {2, -1}}};

public:
    double knightProbability(int n, int k, int row, int column) {
        vector<vector<vector<double>>> memo(
            k + 1, vector<vector<double>>(n, vector<double>(n)));
        auto dfs = [&](auto&& dfs, int k, int i, int j) -> double {
            if (i < 0 || i >= n || j < 0 || j >= n) {
                return 0;
            }
            if (k == 0)
                return 1;
            double& res = memo[k][i][j];
            if (res) { // 之前已经算过
                return res;
            }
            for (auto& [dx, dy] : DIRS) {
                res += dfs(dfs, k - 1, i + dx, j + dy);
            }
            res /= 8;
            return res;
        };
        return dfs(dfs, k, row, column);
    }
};
```

```rust
use std::cell::RefCell;
use std::rc::Rc;
impl Solution {
    pub fn knight_probability(n: i32, k: i32, row: i32, column: i32) -> f64 {
        let dirs = [
            (2, 1),
            (1, 2),
            (-1, 2),
            (-2, 1),
            (-2, -1),
            (-1, -2),
            (1, -2),
            (2, -1),
        ];

        // 创建一个 3D 动态数组用于记忆化存储
        let mut memo = vec![vec![vec![None; n as usize]; n as usize]; (k + 1) as usize];

        // 定义递归函数
        fn dfs(
            n: i32,
            k: i32,
            i: i32,
            j: i32,
            dirs: &[(i32, i32)],
            memo: &mut Vec<Vec<Vec<Option<f64>>>>,
        ) -> f64 {
            // 边界检查
            if i < 0 || i >= n || j < 0 || j >= n {
                return 0.0;
            }
            if k == 0 {
                return 1.0;
            }
            // 检查是否已经计算过
            if let Some(res) = memo[k as usize][i as usize][j as usize] {
                return res;
            }

            // 计算当前状态的概率
            let mut res = 0.0;
            for &(dx, dy) in dirs {
                res += dfs(n, k - 1, i + dx, j + dy, dirs, memo);
            }
            res /= 8.0;

            // 存入记忆化数组
            memo[k as usize][i as usize][j as usize] = Some(res);
            res
        }

        // 调用递归函数
        dfs(n, k, row, column, &dirs, &mut memo)
    }
}
```

</details>

# UNSOLVED 1755 最接近目标值的子序列和

给你一个整数数组 nums 和一个目标值 goal 。

你需要从 nums 中选出一个子序列，使子序列元素总和最接近 goal 。也就是说，如果子序列元素和为 sum ，你需要 最小化绝对差 abs(sum - goal) 。

返回 abs(sum - goal) 可能的 最小值 。

注意，数组的子序列是通过移除原始数组中的某些元素（可能全部或无）而形成的数组。

<details>

```cpp
class Solution {
    constexpr static int N=2e6+10;
    array<int,N> q;
    int n,cnt,goal,res;
    void dfs1(vector<int>& nums,int idx,int sum){
        if(idx==(n+1)/2){
            q[cnt++]=sum;
            return;
        }
        dfs1(nums,idx+1,sum);
        dfs1(nums,idx+1,sum+nums[idx]);
    }
    void dfs2(vector<int>& nums,int idx,int sum){
        if(idx==n){
            int l=0,r=cnt-1;
            while(l<r){
                int mid=(l+r+1)>>1;
                if(q[mid]+sum<=goal) l=mid;
                else r=mid-1;
            }
            res=min(res,abs(q[r]+sum-goal));
            if(r+1<cnt){
                res=min(res,abs(q[r+1]+sum-goal));
            }
            return;
        }
        dfs2(nums,idx+1,sum);
        dfs2(nums,idx+1,sum+nums[idx]);
    }
public:
    int minAbsDifference(vector<int>& nums, int goal_) {
        n=nums.size(),cnt=0,goal=goal_,res=INT_MAX;
        dfs1(nums,0,0);
        ranges::sort(q.begin(),q.begin()+cnt);
        dfs2(nums,(n+1)/2,0);
        return res;
    }
};
```

</details>

# 1755 最接近目标值的子序列和

给你一个整数数组 nums 和一个目标值 goal 。

你需要从 nums 中选出一个子序列，使子序列元素总和最接近 goal 。也就是说，如果子序列元素和为 sum ，你需要 最小化绝对差 abs(sum - goal) 。

返回 abs(sum - goal) 可能的 最小值 。

注意，数组的子序列是通过移除原始数组中的某些元素（可能全部或无）而形成的数组。

<details>

```cpp
class Solution {
    constexpr static int N=2e6+10;
    array<int,N> q;
    int n,cnt,goal,res;
    void dfs1(vector<int>& nums,int idx,int sum){
        if(idx==(n+1)/2){
            q[cnt++]=sum;
            return;
        }
        dfs1(nums,idx+1,sum);
        dfs1(nums,idx+1,sum+nums[idx]);
    }
    void dfs2(vector<int>& nums,int idx,int sum){
        if(idx==n){
            int l=0,r=cnt-1;
            while(l<r){
                int mid=(l+r+1)>>1;
                if(q[mid]+sum<=goal) l=mid;
                else r=mid-1;
            }
            res=min(res,abs(q[r]+sum-goal));
            if(r+1<cnt){
                res=min(res,abs(q[r+1]+sum-goal));
            }
            return;
        }
        dfs2(nums,idx+1,sum);
        dfs2(nums,idx+1,sum+nums[idx]);
    }
public:
    int minAbsDifference(vector<int>& nums, int goal_) {
        n=nums.size(),cnt=0,goal=goal_,res=INT_MAX;
        dfs1(nums,0,0);
        ranges::sort(q.begin(),q.begin()+cnt);
        dfs2(nums,(n+1)/2,0);
        return res;
    }
};
```

</details>

# 980 不同路径Ⅲ

在二维网格 grid 上，有 4 种类型的方格：

    1 表示起始方格。且只有一个起始方格。
    2 表示结束方格，且只有一个结束方格。
    0 表示我们可以走过的空方格。
    -1 表示我们无法跨越的障碍。

返回在四个方向（上、下、左、右）上行走时，从起始方格到结束方格的不同路径的数目。

每一个无障碍方格都要通过一次，但是一条路径中不能重复通过同一个方格。

<details>

```cpp
class Solution {
    int dfs(int x,int y,int left,vector<vector<int>>& grid){
        if(x<0||y<0||x>=grid.size()||y>=grid[x].size()||grid[x][y]<0){
            return 0;
        }
        if(grid[x][y]==2){
            return left==0;
        }
        grid[x][y]=-1;
        int ans=dfs(x-1,y,left-1,grid)+dfs(x,y-1,left-1,grid)+dfs(x+1,y,left-1,grid)+dfs(x,y+1,left-1,grid);
        grid[x][y]=0;
        return ans;
    }
public:
    int uniquePathsIII(vector<vector<int>>& grid) {
        int left=0;
        int sx,sy;
        for(int i=0;i<grid.size();i++){
            for(int j=0;j<grid[0].size();j++){
                if(grid[i][j]==0){
                    left++;
                }else if(grid[i][j]==1){
                    sx=i;
                    sy=j;
                }
            }
        }
        return dfs(sx,sy,left+1,grid);
    }
};
```

</details>

# 2698 求一个整数的惩罚数

给你一个正整数 n ，请你返回 n 的 惩罚数 。

n 的 惩罚数 定义为所有满足以下条件 i 的数的平方和：

    1 <= i <= n
    i * i 的十进制表示的字符串可以分割成若干连续子字符串，且这些子字符串对应的整数值之和等于 i 。

<details>

```cpp
int PRE_SUM[1001];
int init=[](){
    for(int i=1;i<=1000;i++){
        string s=to_string(i*i);
        int n=s.length();
        auto dfs=[&](this auto&& dfs,int p,int sum)->bool{
            if(p==n) return sum==i;
            int x=0;
            for(int j=p;j<n;j++){
                x=x*10+s[j]-'0';
                if(dfs(j+1,sum+x)){
                    return true;
                }
            }
            return false;
        };
        PRE_SUM[i]=PRE_SUM[i-1]+(dfs(0,0)?i*i:0);
    }
    return 0;
}();
class Solution {
public:
    int punishmentNumber(int n) {
        return PRE_SUM[n];
    }
};
```

</details>

# 3669 K 因数分解

给你两个整数 n 和 k，将数字 n 恰好分割成 k 个正整数，使得这些整数的 乘积 等于 n。

返回一个分割方案，使得这些数字中 最大值 和 最小值 之间的 差值 最小化。结果可以以 任意顺序 返回。

<details>

```c++
constexpr int MAX=1e5+1;
vector<int> divisors[MAX];
int init=[]{
    for(int i=1;i<MAX;i++){
        for(int j=i;j<MAX;j+=i){
            divisors[j].emplace_back(i);
        }
    }
    return 0;
}();
class Solution {
public:
    vector<int> minDifference(int n, int k) {
        int min_diff=INT_MAX;
        vector<int> path(k),ans;
        auto dfs=[&](this auto&& dfs,int i,int n,int mn, int mx)->void{
            if(i==k-1){
                int d=max(mx,n)-min(mn,n);
                if(d<min_diff){
                    min_diff=d;
                    path[i]=n;
                    ans=path;
                }
                return;
            }
            for(auto d:divisors[n]){
                path[i]=d;
                dfs(i+1,n/d,min(mn,d),max(mx,d));
            }
        };
        dfs(0,n,INT_MAX,0);
        return ans;
    }
};
```

</details>
