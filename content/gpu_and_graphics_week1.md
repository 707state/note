---
title: 图形学学习
author: jask
done: 1
date: 2026-08-20
tags:
    - Graphics
    - Math
math: true
series: 数学、图形学
---

# 计算机图形学第一定律

_如果它看起来是对的，那就是对的_

# 坐标系

图形学的坐标系比较复杂，涉及到世界空间、直立空间和对象空间之间的变换。

$$
\mathbf{v}_{world} = 
\begin{bmatrix}
r_x & f_x & u_x \\
r_y & f_y & u_y \\
r_z & f_z & u_z
\end{bmatrix}
\cdot
\mathbf{v}_{obj}
$$

其中，$\mathbf{r} = (r_x, r_y, r_z)^T$ 为直立空间中**向右**的矢量，$\mathbf{f} = (f_x, f_y, f_z)^T$ 为**向前**的矢量，$\mathbf{u} = (u_x, u_y, u_z)^T$ 为**向上**的矢量。

如果包含平移（齐次坐标形式）：

$$
\mathbf{v}_{\text{world}} =
\begin{bmatrix}
r_x & f_x & u_x & t_x \\
r_y & f_y & u_y & t_y \\
r_z & f_z & u_z & t_z \\
0 & 0 & 0 & 1
\end{bmatrix}
\cdot
\mathbf{v}_{\text{obj}}
$$


其中，$\mathbf{t} = (t_x, t_y, t_z)^T$ 为对象空间原点在直立空间中的位置。

反向变换：

$$
\mathbf{v}_{obj} = R^T \cdot (\mathbf{v}_{world} - \mathbf{t})
$$

其中，$R = \begin{bmatrix} \mathbf{r} & \mathbf{f} & \mathbf{u} \end{bmatrix}$，且 $R^T = R^{-1}$。


# 矩阵

就不多说了，记一个差分矩阵及其逆运算，这些东西和速度场、网格形变、图像处理有关。
