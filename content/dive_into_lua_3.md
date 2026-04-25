---
title: 深入理解Lua（与虚拟机）3
author: jask
tags:
  - Lua
  - ProgrammingLanguages
  - WebAssembly
date: 2026-04-13
---

# 异常处理

经过几天的摸索，搞出来一套可用的方案：

- `luaD_throw (WASM)`           → `wasm_bindgen::throw_str("__lua__N")` 抛出 JS 字符串异常
- `luaD_throwbaselevel (WASM)`  → `wasm_bindgen::throw_str("__luabase__N")`
- `luaD_rawrunprotected (WASM)` → 用 `js_sys::Function` 构造 JS try/catch 包装器，捕获异常并解析错误码，无需 WASM EH 特性

现在解决了异常处理，但是还是有bug，栈大小还有问题。

# 栈

运行一两个指令还可以，一旦多运行了几个指令就会在浏览器Console中看到崩溃信息，显示的是与栈有关的崩溃。

原因还是先前的异常处理，使用`wasm_bindgen::throw_str`就意味着`luaD_throw`到`luaD_rawrunprotected`之间的JS try/catch边界之间，任何在Rust 堆上分配的对象（String、Box、Vec 等）都会永久泄漏。多次调用后，WASM 线性内存会耗尽，从而导致堆损坏。

而且GC遍历线程栈时边界计算有错，Lua 的调用栈由多个 CallInfo 帧组成，每个帧有自己的 ci->top（该帧可以使用的寄存器上界）。L->top 只表示当前执行点的栈顶，但调用帧中 ci->top 可能比 L->top 更高（帧为局部变量预留了空间）。

总之就是现在可以在浏览器中使用上功能还算完整的Lua（砍掉了load部分的功能）。

# 然后呢？

准备开坑WGPU这个Rust crate了，玩一下WebGPU之类的东西，今年我要做一个能在浏览器中运行的DAW！
