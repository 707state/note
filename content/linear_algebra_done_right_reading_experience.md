---
title: 重学线性代数
author: jask
tags:
    - Math
date: 2026-04-29
math: true
---

记录我重学线性代数的过程。

# 数学概念

## functions

if $S$ is a set, then $F^S$ denotes the set of functions from $S$ to $F$.

## Subspaces

在一个向量空间里面，一定有$$ {0} \subseteq U \subseteq V $$；另一方面，V 自己当然也是 V 的子空间，而且是最大的，因为没有任何 V 的子集能比 V 更大。

对于 $R^2$，每一个子空间都是过原点的直线。

### Direct Sums

如果$V_1$、$V_2$、...、$V_m$都是$V$的Subspace，那么 $$ V_1 + V_2 + V_3 + ... + V_m $$ 表示所有形如 $$ v_1 + ... + v_m $$ 的向量组成的集合。

记号是：$$ V_1 \oplus V_2 \oplus \cdots \oplus V_m $$

举一个简单例子：$R^2$ 这个Vector Space中，令 $U$为X轴上的点的集合，$W$为Y轴上的点的集合，那么就有 $$ R^2 = U \oplus W $$

也就是说，Direct Sum就是子空间的和并且分解方式唯一。重点就在于没一个向量只能有一种拆解开的写法。

一个简单的验证两个subspaces的和是一个direct sum的方法：$$ U \cap W = {0} $$

## polynomial

即多项式，定义：如果$F$是一个域，那么多项式p是一个函数：

$$
p: F \rightarrow F
$$

满足存在有限个系数 $a_0$、...、$a_m$含于 $F$，使得：

$$
p(z) = a_0 + a_1 \cdot z + a_2 \cdot z^2 + \cdots + a_m \cdot  z^m
$$

## injective

单射：

$$

T: V \rightarrow W

$$

is called injective if 

$$ 

Tu = Tv => u = v

$$

## surjective

满射。

单射就是 $V$ 一对一映射到 $W$ 中，但是可能会有 $W$ 中的元素不能够被覆盖。

满射就是 $V$ 向 $W$ 能够完全覆盖 $W$ 的元素，但是 $V$ 可能有些元素不能够被映射到 $W$ 中。


单射且满射即为双射。

## Isomorphic Vector Space

向量空间同构。

同构映射即可逆的线性映射，有限维向量空间只要维数相同，本质上就相同。

# lean4

证明复数加法：

```lean4
@[ext]
structure MyComplex where
  re : ℝ
  im : ℝ

def add (a b : MyComplex) : MyComplex :=
  {re:= a.re+b.re, im := a.im+b.im}
def mul (a b : MyComplex) : MyComplex :=
  {re:= a.re*b.re-a.im*b.im, im := a.re*b.im + a.im*b.re}

theorem add_complex (a b : MyComplex) : add a b = add b a := by
  cases a; cases b
  ext
  · simp [add, add_comm]
  · simp [add, add_comm]

theorem mul_complex (a b : MyComplex) : mul a b = mul b a := by
  cases a; cases b
  ext
  · simp [mul]; ring
  · simp [mul]; ring
```


证明 $F^n$ 上的associativity

```lean4
`F^n` 在 Lean 4 中表示为函数类型 `Fin n → F`。
加法定义为逐点加法：`(x + y) i = x i + y i`。
-/

-- 基础版本：直接用 funext 展开到逐点，再调用域 F 上的 add_comm
theorem vec_add_comm_direct {F : Type*} [AddCommMonoid F] (n : ℕ)
    (x y : Fin n → F) : x + y = y + x := by
  funext i
  exact add_comm (x i) (y i)

-- 归纳法版本：对 n 做结构归纳，显式体现归纳步骤
theorem vec_add_comm {F : Type*} [AddCommMonoid F] (n : ℕ)
    (x y : Fin n → F) : x + y = y + x := by
  induction n with
  -- 基础步骤：n = 0，Fin 0 是空类型，两个函数在其上相等是平凡的
  | zero =>
    funext i
    exact Fin.elim0 i
  -- 归纳步骤：n = k + 1
  -- 注意：x, y 的类型随 n 变化，归纳假设 ih 针对 Fin k → F 的向量
  -- 对任意下标 i : Fin (k+1)，逐点展开后调用 F 上的 add_comm 即可
  | succ k _ih =>
    funext i
    -- Pi 类型的加法逐点定义：(x + y) i = x i + y i
    change x i + y i = y i + x i
    exact add_comm (x i) (y i)
```

# 附录
