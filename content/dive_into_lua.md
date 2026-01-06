---
title: 深入理解Lua（与虚拟机）
author: jask
tags:
  - Lua
  - ProgrammingLanguages
date: 2026-01-05
---

# 前置知识

Lua是一个脚本语言，采用register vm方式，字节码运行，采用tagged pointer来标记所有value（及其类型，所以Lua是dynamically typed），支持协程。Lua使用ANSI C实作，强调拓展性和可移植性。

# 初步理解

## 基本类型

Lua的基本类型为: Nil, Boolean, Number, String, Userdata, Table, Thread, Function。
这里面Boolean和Number直接存储在Tagged Pointer，其他的则是存储指针。

```c
#define TValuefields	Value value_; lu_byte tt_

typedef struct TValue {
  TValuefields;
} TValue;
```

对于8bit对齐的机器，Number/Boolean一次拷贝就会带来16bit的开销，显得比较大。在[Implementation Of Lua 5这篇论文](https://www.lua.org/doc/jucs05.pdf) 里面，作者提到了smalltalk80使用指针对齐带来的2或3个bit总为空这一特性来实现一些其他用途，但由于ANSI C对于此类行为没有良好规定而没有考虑；另一种实现则是把Number也放在堆上，但这会带来运行速度的缓慢！作者给出的另一种做法是：Integer直接放在Tagged Pointer，而浮点数放置在堆上，但这又会带来及其复杂的数学运算的实现。

## Tables

Lua 5为table引入了混合的数据结构—— Hash Table + Array，针对非Integer或者不在array范围中的key就存入哈希表，否则存入Array。

Hash Table采用链式冲突+Brent's variation（当插入新元素发生冲突时，不一定让新元素一直探测下去，而是尝试“重新安置”已有元素，使整体查找成本最小化。）

## Functions and Closures

Lua引入upvalue来解决内层闭包引用outer local value的问题。任何一个outer local value都是通过upvalue间接访问的，当外层函数返回时，其关联的栈帧也要一并销毁，这个时候upvalue内部会有一个槽，把闭包访问的外部变量拷贝进来，同时upvalue的指针也会发生改变。

# 源码阅读

## Compiling

Lua的compiler和vm设计并不是解耦合的，luac把源码转换为字节码并导出的主要作用是提高导入速度。在源码中Parser体现为：

![f_parser函数的实现](images/lua/f_parser.png)

此外，lua的parser/lexer设计上也不是分离的，而是mainfunc读取第一个token之后就开始parsing，然后parsing期间继续读token。这样的设计常见于早期的编译器里面，可以节省内存。

还有一个比较有意思的点是Lua的parser/lexer并不是独立于lua mv的函数。可以看看luaY\_parser: 

![luaY\_parser这个函数](images/lua/luaY_parser.png)

可以看到inctop, top.p--这样的操作，这些是lua vm栈相关的操作。Lua可以通过load函数当作eval使用，所以需要在虚拟机中还能够调用parser，所以会这样设计（还没看完，继续看看）。

## VM

Lua具体是怎么执行的呢？

请看docall函数：
