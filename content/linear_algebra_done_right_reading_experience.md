---
title: 重学线性代数
author: jask
tags:
    - Math
date: 2026-04-29
---

记录我重学线性代数

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


# 附录
