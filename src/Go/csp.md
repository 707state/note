<!--toc:start-->
- [CSP](#csp)
  - [历史脉络](#历史脉络)
    - [guarded command](#guarded-command)
    - [parbegin](#parbegin)
  - [基本要素](#基本要素)
- [Go](#go)
<!--toc:end-->

# CSP

CSP, aka communicating sequential processes，作为并发程序设计的一种方式，从一开始就是为多核环境设计的。

## 历史脉络

### guarded command

CSP的形式化表述的notation来自于guarded commands（Edsger Dikstra, 1975）。

Guarded Command 提供了“守卫 + 非确定选择”这种建模模式。

CSP 把这种模式引入并发进程的语法中，使得进程在等待事件时也可以写成“守卫 + 动作”。

Guarded Command 是 CSP 语法与语义中“选择/同步”部分的理论源头之一，但 CSP 在此基础上增加了通信通道、进程组合和完整的并发语义。

### parbegin

Dijkstra在Guarded Command中还设计了并发结构，用parbegin/parend来表示并行的命令。

CSP 的并行组合符号与思想可以看作对 Dijkstra parbegin 的继承和彻底形式化，同时用消息通信取代了共享变量的隐式交互。

## 基本要素

CSP的核心：

- 进程 (Process)
- 事件/通信 (Event/Channel) —— 同步消息传递
- 组合子：顺序 (;)、并发 (||)、外部选择 ([])、隐藏 (\)

# Go

Go继承了Limbo, Newsqueak, Alef这一路的语言，核心的并发机制就是CSP。

Go里面就是通过Channel通信而不是通过共享内存通信。

## channel

Go可以提供有缓冲区和无缓冲区的Channel，对应异步/同步的语义。

```go
ch:=make(chan int,3)
```

这样就可以创建一个缓冲区大小为3的channel。
