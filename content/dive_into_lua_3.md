--- 
title: 深入理解Lua（与虚拟机）3
author: jask
tags:
  - Lua
  - ProgrammingLanguages
date: 2026-04-06
---

# 重写Parser

Lua的parser并不是理论上的parser，教科书版本的parser都是把lexer生成的一大串token转换为一棵树状结构，但是第一部分就讲到过，Lua的Parser和Lexer是紧耦合的，而且不存在表达式树，它同时承担了表达式分析和代码生成的工作。

举一个例子：

```lua
x = 1
...
a = x
```

先不考虑内层外层之类的，当运行到x这里时，parser不会把它加载到寄存器，而是标记为upvalue这样的标记，只有在真正需要使用时，才通过luaK\_dischargevars发出 `GETUPVAL`、`GETTABLE`、`GETFIELD` 等指令。


