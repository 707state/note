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

# Lua工作机制

## Compiling

Lua的compiler和vm设计并不是解耦合的，luac把源码转换为字节码并导出的主要作用是提高导入速度。在源码中Parser体现为：

![f_parser函数的实现](images/lua/f_parser.png)

此外，lua的parser/lexer设计上也不是分离的，而是mainfunc读取第一个token之后就开始parsing，然后parsing期间继续读token。这样的设计常见于早期的编译器里面，可以节省内存。

还有一个比较有意思的点是Lua的parser/lexer并不是独立于lua mv的函数。可以看看luaY\_parser: 

![luaY\_parser这个函数](images/lua/luaY_parser.png)

可以看到inctop, top.p--这样的操作，这些是lua vm栈相关的操作。Lua可以通过load函数当作eval使用，所以需要在虚拟机中还能够调用parser，所以会这样设计（还没看完，继续看看）。

## stdlib

在实际看lua虚拟机实现之前，需要搞明白lua是怎么工作的。

Lua运行环境除开lua vm全部由模块提供stdlibs提供。具体代码在linit.c当中，通过luaL_openselectedlibs加载。

### baselib

最基础的基础库，ipairs, load, print, pcall都是baselib提供的（这里面baselib提供print有点怪，我认为这个应该是io相关的）。

从这里面的一些函数实现就可以看出来，lua是应用序，即先计算参数再传递。

![luaB_print](images/lua/luaB_print.png)

这个是非常简单的例子，实际上就是对于fprintf的封装。稍微复杂一点的例子可以看luaB\_setmetatable，这里面就有更具体的lua库如何与lua vm本身协作。

更复杂一点的例子可以参考:

![luaB_pairs](images/lua/luaB_pairs.png)

这个函数会去判断pairs函数的参数的metamethod，如果设置了\_\_pairs这个metamethod就会去使用用户定义的方法（把用户的方法入栈，然后再下一次luaD\_precall就会派发到对应的部分）。

注意到先前说的lua虚拟机都在用栈的方式描述，但是lua vm是一个寄存器虚拟机，这实际上是因为现在在看的都是和c交互的api。lua对外暴露了一个栈式接口，所以看起来是栈虚拟机。C API通过lapi.c里面定义的index2value和index2stack把C API的栈索引映射到TValue（tagged value）或者地址。

### package

package这个lib提供了path, cpath等等配置项，比如说path，如果想要lua能够打开一个自定义的lua包就必须用path来设置路径。

### coroutine

Lua提供了非对称协程，而协程的实现几乎是独立于VM本身的（全靠corolib），其中依赖于lua\_newthread相关的函数。

corolib定义了create, resume, running, status, yield等等函数，位于coroutine包里面。

协程最核心的三个函数是：resume, yield, create。create没什么好说的，就是创建协程同时分配一个sizeof(void*)的栈空间。

coroutine.resume中，第一次resume传入的参数会作为协程主函数的参数，之后每一次resume传入的参数会作为上一次yield的返回值回到协程里面。coroutine.yield会把控制权还给外面，yield里面的参数会变成外面那次Resume的返回值。

### io

lua对于stdin/stdout/stderr的处理比较有意思。

![luaopen_io](images/lua/luaopen_io.png)

lua针对标准流的处理是变成Lua的file userdata并注册到io表里的函数。

io还有一个\_\_gc方法用来在gc时自动close。

## VM

lua vm的指令非常精简，全部定义在lopcodes.h里面，但是具体的执行逻辑则定义在lvm.h:

![lvm.h指令定义](images/lua/lvm.png)

这里面的重点是luaV\_execute函数，这个函数负责执行lua字节码。

### 函数调用

Lua里面调用一个函数会形成OP\_CALL或者OP\_TAILCALL的指令，如果是C函数就会去调用precallC函数，否则就是更新CallInfo并回到startfunc位置。

#### C函数调用

![luaD_precall](images/lua/luaD_precall.png)

注释很明显写出来了Lua函数和参数是怎么传递的，需要分为C函数和Lua函数来看这段代码。

![precallC](images/lua/precallC.png)

这里面lua\_CFunction f就是指向C函数的指针，这些C函数都是需要通过lua\_pushcclosure来进入的，然后根据upvalues数量来判断是light C function还是c closure，也就是说C closure是有gc需求的。比如说pmain（lua interpreter的函数体）就是一个light C function。此外luaopen\_base、luaopen\_package等函数则是C Closure。

传参时候的n与gc有关，n代表的是upvalues的数量，而这些upvalues都会从栈顶取出，作为这个C函数的外部捕获变量。这些值会被存入新建的CClosure里，可以用lua\_upvalueindex访问。

对于C function不管是不是closure，他们都是通过函数指针来执行的（废话，不用函数指针怎么知道在内存的哪个位置），对于C closure来说，执行时需要先取出closure再取出cl->f执行。这一部分需要结合GC来看。

#### lua函数调用

lua函数调用的核心则是luaD\_precall和lua vm之间的协作，luaD\_precall/call会返回一个CallInfo\*，上层会进入VM执行循环。

![luaV_execute](images/lua/luaV_execute_OP_CALL.png)

这里newci得到了完整的CallInfo\*之后修改当前ci，跳转到startfunc处去执行ci的内容。

### Codegen

Lua vm是寄存器机，这也就意味着指令的格式就类似于汇编的格式，即：op register1 register2这样。

![Operator of Lua VM](images/lua/bin_un_opr.png)

与之相关联的则是lcode.h和lparser.c当中的寄存器分配。Lua的寄存器并不是寄存器，而是函数栈帧里的槽位。

先看指令生成，指令生成使用的是lcode.h中定义的编译期的代码生成接口，像是luaK\_codeABC/ABx会生成Instruction并把Instruction放入Proto->code。

![luaK_code](images/lua/luaK_code.png)

#### 赋值

luxa是允许一次性对多个变量赋值的，并且函数返回值也允许有多个。

![restassign](images/lua/restassign.png)

这里可以看到对于多变量赋值就会变成递归调用，否则就会直接走luaK\_setoneret/storevar这一条路子去赋值。

重点是luaK\_storevar函数。

![luaK_storevar](images/lua/luaK_storevar.png)

#### 寄存器分配

前面的luaK\_storevar使用的是已经设置好的寄存器，而实际的寄存器分配则是luaK\_exp2nextreg/luaK\_exp2anyreg这两个函数。这两个函数会调用exp2reg->discharge2reg结合FuncState \*fs的freereg字段进行分配；而在freereg/freeregs/freeexp里面会把fs->freereg回退来释放寄存器。

Lua的虚拟机寄存器分配是一个非常简单的线性栈湿分配器，主要逻辑：

- **单调增长的寄存器栈顶**：`FuncState->freereg` 表示当前可用的“下一个寄存器”，分配就是 `luaK_reserveregs(n)` 把 `freereg` 往上挪。
- **表达式求值即分配**：`luaK_exp2nextreg` / `luaK_exp2anyreg` 会把表达式结果落到寄存器里，内部走 `discharge2reg`，并确保 `freereg` 足够。
- **作用域回收**：离开作用域或表达式结束后用 `freereg` / `freeexp` / `freeregs` 回退 `freereg`，释放临时寄存器。
- **局部变量占用固定寄存器**：解析阶段通过 `fs->nactvar` 管理“活跃局部变量”的寄存器区间，临时寄存器只能从 `nactvar` 以上开始用。
- **无全局重排/合并**：不做跨语句的寄存器复用优化，靠语法树的自然结构与即时释放减少峰值。

### GC

Lua的Functions/Closures设计在Lua 5 implementation这篇论文里面有专门的描述，主要围绕着Lua如何避免引入过于复杂的control-flow analysis(Bigloo scheme compiler)的同时能够维护upper local values。

Lua引入了upvalue来实现Closure，每一个Closure外层的local variable在Closure内被访问都是间接经过upvalue来做到的。upvalue初始会指向栈偏移量（这个偏移量所在的变量就是其原本被分配的位置），当变量的作用域结束时，就会移入到upvalue内的槽，对于访问变量的代码来说，因为upvalue是间接的，所以不会影响代码。

为确保在多个Closure中正确共享可变状态，一个变量最多只能有一个upvalue并按需使用，为了保证这一点，lua采用链表结构来管理open upvalues。

具体内容还是参考论文，这里看看Lua的GC的代码。

首先Lua GC采用的是三色标记(WHITE0, WHITE1, BLACK)，保持“不允许黑指向白”的不变式。Lua有Incremental和Generational两种GC模式，可以通过luaC\_changemode切换。

VM和GC进行交互主要是依靠luaC\_newobj、luaC\_checkGC、luaC\_step这些操作。

- 每一个对象都处于WHITE、GRAY、BLACK三种状态的一个。
- 未被访问的对象会被标记为WHITE。
- 被访问但没有被遍历的对象会被标记为GRAY。
- 被遍历的对象会被标记为BLACK。


举一个例子，t={}在全局环境中：

1. 创建时，新的table对象被分配出来，初始是白色并挂到GC的allgc链表里面。
2. 本轮GC的标记阶段，全局表\_G是根对象，会被标记为GRAY；遍历\_G会看到字段t指向的新table，这个table会被标记为GRAY（已发现，没有遍历）。
3. 继续遍历，当GC处理到该table，会遍历其内容，随后变黑。
4. sweep后，活着的对象重新转回白色，所以这张表在本轮结束后通常又是白的。

如果在后面一轮里面，t=nil了，那这个表就不会被标记到它，他就会保持白色并在sweep阶段被释放。

有一个[比较详细讲述GC的PPT](https://www.lua.org/wshop18/Ierusalimschy.pdf) 目前看代码还是比较吃力，还是得先把概念性的东西理清。
