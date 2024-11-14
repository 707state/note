# 3235 判断矩形的两个角落是否可达

给你两个正整数 xCorner 和 yCorner 和一个二维整数数组 circles ，其中 circles[i] = [xi, yi, ri] 表示一个圆心在 (xi, yi) 半径为 ri 的圆。

坐标平面内有一个左下角在原点，右上角在 (xCorner, yCorner) 的矩形。你需要判断是否存在一条从左下角到右上角的路径满足：路径 完全 在矩形内部，不会 触碰或者经过 任何 圆的内部和边界，同时 只 在起点和终点接触到矩形。

如果存在这样的路径，请你返回 true ，否则返回 false 。

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

# 386 字典序排数

给你一个整数 n ，按字典序返回范围 [1, n] 内所有整数。

你必须设计一个时间复杂度为 O(n) 且使用 O(1) 额外空间的算法。
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




