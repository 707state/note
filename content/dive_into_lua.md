---
title: 深入理解Lua（与虚拟机）
author: jask
tags:
  - Lua
  - ProgrammingLanguages
date: 2026-01-05
series: 深入理解lua
---

# 前置知识

Lua 是一个脚本语言，采用 register VM 方式，字节码运行，采用 tagged pointer 来标记所有 value 及其类型，所以 Lua 是 dynamically typed。它支持协程，使用 ANSI C 实作，强调扩展性和可移植性。

# 初步理解

## 基本类型

Lua 的基本类型为：`Nil`、`Boolean`、`Number`、`String`、`Userdata`、`Table`、`Thread`、`Function`。这里面 `Boolean` 和 `Number` 直接存储在 tagged pointer，其他的则是存储指针。

```c
#define TValuefields  Value value_; lu_byte tt_

typedef struct TValue {
  TValuefields;
} TValue;
```

对于 8 bit 对齐的机器，`Number` / `Boolean` 一次拷贝就会带来 16 bit 的开销，显得比较大。在 [Implementation Of Lua 5 这篇论文](https://www.lua.org/doc/jucs05.pdf) 里面，作者提到了 Smalltalk-80 使用指针对齐带来的 2 或 3 个 bit 总为空这一特性来实现一些其他用途，但由于 ANSI C 对于此类行为没有良好规定而没有考虑；另一种实现则是把 `Number` 也放在堆上，但这会带来运行速度的缓慢。作者给出的另一种做法是：`Integer` 直接放在 tagged pointer，而浮点数放置在堆上，但这又会带来极其复杂的数学运算实现。

## Tables

Lua 5 为 table 引入了混合的数据结构，即 `Hash Table + Array`。针对非 Integer 或者不在 array 范围中的 key，就存入哈希表，否则存入 array。

Hash Table 采用链式冲突加 Brent's variation。也就是当插入新元素发生冲突时，不一定让新元素一直探测下去，而是尝试重新安置已有元素，使整体查找成本最小化。

## Functions and Closures

Lua 引入 upvalue 来解决内层闭包引用 outer local value 的问题。任何一个 outer local value 都是通过 upvalue 间接访问的。当外层函数返回时，其关联的栈帧也要一并销毁，这个时候 upvalue 内部会有一个槽，把闭包访问的外部变量拷贝进来，同时 upvalue 的指针也会发生改变。

# Lua 工作机制

## Compiling

Lua 的 compiler 和 VM 设计并不是解耦合的。`luac` 把源码转换为字节码并导出的主要作用是提高导入速度。在源码中，parser 入口可以看 `f_parser`：

```c
static void f_parser(lua_State *L, void *ud) {
  LClosure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  const char *mode = p->mode ? p->mode : "bt";
  int c = zgetc(p->z);  /* read first character */

  if (c == LUA_SIGNATURE[0]) {
    int fixed = 0;
    if (strchr(mode, 'B') != NULL)
      fixed = 1;
    else
      checkmode(L, mode, "binary");
    cl = luaU_undump(L, p->z, p->name, fixed);
  }
  else {
    checkmode(L, mode, "text");
    cl = luaY_parser(L, p->z, &p->buff, &p->dyd, p->name, c);
  }

  lua_assert(cl->nupvalues == cl->p->sizeupvalues);
  luaF_initupvals(L, cl);
}
```

可以看到它先判断输入是不是二进制 chunk。如果是，就直接走 `luaU_undump`；否则进入 `luaY_parser` 做文本解析。

此外，Lua 的 parser / lexer 设计上也不是分离的，而是 `mainfunc` 读取第一个 token 之后就开始 parsing，然后在 parsing 期间继续读 token。这样的设计常见于早期编译器里，可以节省内存。

`luaY_parser` 也能看出 parser 和 VM 的耦合关系：

```c
LClosure *luaY_parser(lua_State *L, ZIO *z, Mbuffer *buff,
                      Dyndata *dyd, const char *name, int firstchar) {
  LexState lexstate;
  FuncState funcstate;
  LClosure *cl = luaF_newLclosure(L, 1);  /* create main closure */
  setclLvalue2s(L, L->top.p, cl);  /* anchor it (to avoid being collected) */
  luaD_inctop(L);

  lexstate.h = luaH_new(L);  /* create table for scanner */
  sethvalue2s(L, L->top.p, lexstate.h);  /* anchor it */
  luaD_inctop(L);

  funcstate.f = cl->p = luaF_newproto(L);
  luaC_objbarrier(L, cl, cl->p);
  funcstate.f->source = luaS_new(L, name);  /* create and anchor TString */
  luaC_objbarrier(L, funcstate.f, funcstate.f->source);

  lexstate.buff = buff;
  lexstate.dyd = dyd;
  dyd->actvar.n = dyd->gt.n = dyd->label.n = 0;
  luaX_setinput(L, &lexstate, z, funcstate.f->source, firstchar);
  mainfunc(&lexstate, &funcstate);

  lua_assert(!funcstate.prev && funcstate.nups == 1 && !lexstate.fs);
  lua_assert(dyd->actvar.n == 0 && dyd->gt.n == 0 && dyd->label.n == 0);
  L->top.p--;  /* remove scanner's table */
  return cl;   /* closure is on the stack, too */
}
```

可以看到 `inctop`、`top.p--` 这样的操作，这些都是和 Lua VM 栈相关的逻辑。Lua 可以通过 `load` 函数把 parser 当作一种 `eval` 机制来使用，所以需要在虚拟机内部也能调用 parser，于是就会出现这种设计。

## stdlib

在实际看 Lua 虚拟机实现之前，需要先搞明白 Lua 运行时是怎么工作的。

Lua 运行环境除开 Lua VM，本身大量能力都由标准库模块提供。具体代码在 `linit.c` 当中，通过 `luaL_openselectedlibs` 加载。

### baselib

最基础的基础库。`ipairs`、`load`、`print`、`pcall` 都是 baselib 提供的。这里面 baselib 提供 `print` 有点怪，我会更倾向把它归到 `io` 相关。

从一些函数实现就可以看出来，Lua 是应用序，也就是先计算参数再传递。比如 `luaB_print`：

```c
static int luaB_print(lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = luaL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      lua_writestring("\t", 1);  /* add a tab before it */
    lua_writestring(s, l);  /* print it */
    lua_pop(L, 1);  /* pop result */
  }
  lua_writeline();
  return 0;
}
```

这个例子非常简单，本质上就是对输出函数的封装。稍微复杂一点的例子可以看 `luaB_setmetatable`，里面会更清楚地展示 Lua 标准库如何和 Lua VM 协作。

另一个更复杂的例子是 `luaB_pairs`：

```c
static int luaB_pairs(lua_State *L) {
  luaL_checkany(L, 1);
  if (luaL_getmetafield(L, 1, "__pairs") == LUA_TNIL) {  /* no metamethod? */
    lua_pushcfunction(L, luaB_next);  /* will return generator and */
    lua_pushvalue(L, 1);              /* state */
    lua_pushnil(L);                   /* initial value */
    lua_pushnil(L);                   /* to-be-closed object */
  }
  else {
    lua_pushvalue(L, 1);  /* argument 'self' to metamethod */
    lua_callk(L, 1, 4, 0, pairscont);  /* get 4 values from metamethod */
  }
  return 4;
}
```

这个函数会去判断 `pairs` 参数的 metamethod。如果设置了 `__pairs` 这个 metamethod，就会去使用用户定义的方法，把用户方法入栈，然后在后续调用流程中派发到对应逻辑。

注意到先前说的 Lua 虚拟机经常在用“栈”的方式描述，但 Lua VM 本身其实是寄存器虚拟机。这实际上是因为这里看到的大多是和 C 交互的 API。Lua 对外暴露的是一个栈式接口，所以看起来像栈虚拟机。C API 通过 `lapi.c` 里面定义的 `index2value` 和 `index2stack` 把 C API 的栈索引映射到 `TValue` 或对应地址。

### package

`package` 这个库提供了 `path`、`cpath` 等配置项。比如说 `path`，如果想让 Lua 能够打开一个自定义的 Lua 包，就必须通过它来设置搜索路径。

### coroutine

Lua 提供了非对称协程，而协程的实现几乎是独立于 VM 本身的，主要靠 `corolib`，其中依赖于 `lua_newthread` 相关函数。

`corolib` 定义了 `create`、`resume`、`running`、`status`、`yield` 等函数，位于 `coroutine` 包里面。

协程最核心的三个函数是：`resume`、`yield`、`create`。`create` 没什么好说的，就是创建协程，同时分配一个 `sizeof(void *)` 的栈空间。

`coroutine.resume` 中，第一次 `resume` 传入的参数会作为协程主函数的参数；之后每一次 `resume` 传入的参数，会作为上一次 `yield` 的返回值回到协程里面。`coroutine.yield` 会把控制权还给外面，而 `yield` 里面的参数会变成外面那次 `resume` 的返回值。

### io

Lua 对于 `stdin` / `stdout` / `stderr` 的处理比较有意思：

```c
LUAMOD_API int luaopen_io(lua_State *L) {
  luaL_newlib(L, iolib);  /* new module */
  createmeta(L);
  /* create (and set) default files */
  createstdfile(L, stdin, IO_INPUT, "stdin");
  createstdfile(L, stdout, IO_OUTPUT, "stdout");
  createstdfile(L, stderr, NULL, "stderr");
  return 1;
}
```

Lua 针对标准流的处理方式，是把它们变成 Lua 的 `file userdata`，并注册到 `io` 表相关函数里。

`io` 还有一个 `__gc` 方法，用来在 GC 时自动 `close`。

## VM

Lua VM 的指令非常精简，全部定义在 `lopcodes.h` 里面，但是具体的执行逻辑则定义在 `lvm.h`：

```c
LUAI_FUNC int luaV_equalobj(lua_State *L, const TValue *t1, const TValue *t2);
LUAI_FUNC int luaV_lessthan(lua_State *L, const TValue *l, const TValue *r);
LUAI_FUNC int luaV_lessequal(lua_State *L, const TValue *l, const TValue *r);
LUAI_FUNC int luaV_tonumber_(const TValue *obj, lua_Number *n);
LUAI_FUNC int luaV_tointeger(const TValue *obj, lua_Integer *p, F2Imod mode);
LUAI_FUNC int luaV_tointegerns(const TValue *obj, lua_Integer *p, F2Imod mode);
LUAI_FUNC int luaV_flttointeger(lua_Number n, lua_Integer *p, F2Imod mode);
LUAI_FUNC lu_byte luaV_finishget(lua_State *L, const TValue *t, TValue *key,
                                 StkId val, lu_byte tag);
LUAI_FUNC void luaV_finishset(lua_State *L, const TValue *t, TValue *key,
                              TValue *val, int aux);
LUAI_FUNC void luaV_finishOp(lua_State *L);
LUAI_FUNC void luaV_execute(lua_State *L, CallInfo *ci);
LUAI_FUNC void luaV_concat(lua_State *L, int total);
LUAI_FUNC lua_Integer luaV_idiv(lua_State *L, lua_Integer x, lua_Integer y);
LUAI_FUNC lua_Integer luaV_mod(lua_State *L, lua_Integer x, lua_Integer y);
LUAI_FUNC lua_Number luaV_modf(lua_State *L, lua_Number x, lua_Number y);
LUAI_FUNC lua_Integer luaV_shiftl(lua_Integer x, lua_Integer y);
LUAI_FUNC void luaV_objlen(lua_State *L, StkId ra, const TValue *rb);
```

这里面的重点是 `luaV_execute`，这个函数负责执行 Lua 字节码。

### 函数调用

Lua 里面调用一个函数会形成 `OP_CALL` 或者 `OP_TAILCALL` 指令。如果是 C 函数，就会去调用 `precallC`；否则就是更新 `CallInfo` 并回到 `startfunc` 位置。

#### C 函数调用

`luaD_precall` 是整个调用路径的枢纽：

```c
CallInfo *luaD_precall(lua_State *L, StkId func, int nresults) {
  unsigned status = cast_uint(nresults + 1);
  lua_assert(status <= MAXRESULTS + 1);

 retry:
  switch (ttypetag(s2v(func))) {
    case LUA_VCCL:  /* C closure */
      precallC(L, func, status, clCvalue(s2v(func))->f);
      return NULL;

    case LUA_VLCF:  /* light C function */
      precallC(L, func, status, fvalue(s2v(func)));
      return NULL;

    case LUA_VLCL: {  /* Lua function */
      CallInfo *ci;
      Proto *p = clLvalue(s2v(func))->p;
      int narg = cast_int(L->top.p - func) - 1;  /* number of real arguments */
      int nfixparams = p->numparams;
      int fsize = p->maxstacksize;  /* frame size */
      checkstackp(L, fsize, func);
      L->ci = ci = prepCallInfo(L, func, status, func + 1 + fsize);
      ci->u.l.savedpc = p->code;  /* starting point */
      for (; narg < nfixparams; narg++)
        setnilvalue(s2v(L->top.p++));  /* complete missing arguments */
      lua_assert(ci->top.p <= L->stack_last.p);
      return ci;
    }

    default: {  /* not a function */
      checkstackp(L, 1, func);  /* space for metamethod */
      status = tryfuncTM(L, func, status);  /* try '__call' metamethod */
      goto retry;  /* try again with metamethod */
    }
  }
}
```

注释里已经很明显地写出了 Lua 函数和参数是怎么传递的，所以需要分为 C 函数和 Lua 函数两部分来看。

对于 C 函数，核心逻辑在 `precallC`：

```c
l_sinline int precallC(lua_State *L, StkId func, unsigned status,
                       lua_CFunction f) {
  int n;  /* number of returns */
  CallInfo *ci;

  checkstackp(L, LUA_MINSTACK, func);  /* ensure minimum stack size */
  L->ci = ci = prepCallInfo(L, func, status | CIST_C,
                            L->top.p + LUA_MINSTACK);
  lua_assert(ci->top.p <= L->stack_last.p);

  if (l_unlikely(L->hookmask & LUA_MASKCALL)) {
    int narg = cast_int(L->top.p - func) - 1;
    luaD_hook(L, LUA_HOOKCALL, -1, 1, narg);
  }

  lua_unlock(L);
  n = (*f)(L);  /* do the actual call */
  lua_lock(L);
  api_checknelems(L, n);
  luaD_poscall(L, ci, n);
  return n;
}
```

这里面的 `lua_CFunction f` 就是指向 C 函数的指针。这些 C 函数都需要通过 `lua_pushcclosure` 进入 Lua，然后根据 upvalues 数量来判断是 light C function 还是 C closure，也就是说 C closure 是有 GC 需求的。比如说 `pmain`（Lua interpreter 的函数体）就是一个 light C function，而 `luaopen_base`、`luaopen_package` 等函数则是 C closure。

传参时候的 `n` 与 GC 有关。`n` 代表 upvalues 的数量，而这些 upvalues 都会从栈顶取出，作为这个 C 函数的外部捕获变量。这些值会被存入新建的 `CClosure` 里，可以用 `lua_upvalueindex` 访问。

对于 C function，不管是不是 closure，它们最后都是通过函数指针来执行的。对于 C closure 来说，执行时需要先取出 closure，再取出 `cl->f` 执行。这一部分需要结合 GC 一起看。

#### Lua 函数调用

Lua 函数调用的核心是 `luaD_precall` 和 Lua VM 之间的协作。`luaD_precall` / `call` 会返回一个 `CallInfo *`，上层则进入 VM 执行循环。

`OP_CALL` 在 `luaV_execute` 中的分支大致如下：

```c
vmcase(OP_CALL) {
  StkId ra = RA(i);
  CallInfo *newci;
  int b = GETARG_B(i);
  int nresults = GETARG_C(i) - 1;

  if (b != 0)  /* fixed number of arguments? */
    L->top.p = ra + b;  /* top signals number of arguments */

  savepc(ci);  /* in case of errors */
  if ((newci = luaD_precall(L, ra, nresults)) == NULL)
    updatetrap(ci);  /* C call, nothing else to be done */
  else {  /* Lua call: run function in this same C frame */
    ci = newci;
    goto startfunc;
  }
  vmbreak;
}
```

这里 `newci` 得到了完整的 `CallInfo *` 之后，会修改当前 `ci`，然后跳转到 `startfunc` 去执行新的调用内容。

### Codegen

Lua VM 是寄存器机，这也就意味着指令的格式更像汇编风格，即 `op register1 register2` 这样。

和这个设计直接相关的是 `lcode.h` 与 `lparser.c` 里的寄存器分配。Lua 的寄存器并不是真正的硬件寄存器，而是函数栈帧里的槽位。

先看操作符枚举和基础 codegen 辅助：

```c
typedef enum BinOpr {
  /* arithmetic operators */
  OPR_ADD, OPR_SUB, OPR_MUL, OPR_MOD, OPR_POW,
  OPR_DIV, OPR_IDIV,
  /* bitwise operators */
  OPR_BAND, OPR_BOR, OPR_BXOR,
  OPR_SHL, OPR_SHR,
  /* string operator */
  OPR_CONCAT,
  /* comparison operators */
  OPR_EQ, OPR_LT, OPR_LE,
  OPR_NE, OPR_GT, OPR_GE,
  /* logical operators */
  OPR_AND, OPR_OR,
  OPR_NOBINOPR
} BinOpr;

#define foldbinop(op) ((op) <= OPR_SHR)
#define luaK_codeABC(fs,o,a,b,c) luaK_codeABCk(fs,o,a,b,c,0)

typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;
```

而真正把指令写入 `Proto->code` 的，是 `luaK_code` 这一层：

```c
int luaK_code(FuncState *fs, Instruction i) {
  Proto *f = fs->f;
  luaM_growvector(fs->ls->L, f->code, fs->pc, f->sizecode, Instruction,
                  INT_MAX, "opcodes");
  f->code[fs->pc++] = i;
  savelineinfo(fs, fs->ls->lastline);
  return fs->pc - 1;  /* index of new instruction */
}
```

#### 赋值

Lua 是允许一次性对多个变量赋值的，并且函数返回值也允许有多个。相关逻辑可以从 `restassign` 看起：

```c
static void restassign(LexState *ls, struct LHS_assign *lh, int nvars) {
  expdesc e;
  check_condition(ls, vkisvar(lh->v.k), "syntax error");
  check_readonly(ls, &lh->v);

  if (testnext(ls, ',')) {  /* restassign -> ',' suffixedexp restassign */
    struct LHS_assign nv;
    nv.prev = lh;
    suffixedexp(ls, &nv.v);
    if (vkisindexed(nv.v.k))
      check_conflict(ls, lh, &nv.v);
    enterlevel(ls);  /* control recursion depth */
    restassign(ls, &nv, nvars + 1);
    leavelevel(ls);
  }
  else {  /* restassign -> '=' explist */
    int nexps;
    checknext(ls, '=');
    nexps = explist(ls, &e);
    if (nexps != nvars)
      adjust_assign(ls, nvars, nexps, &e);
    else {
      luaK_setoneret(ls->fs, &e);  /* close last expression */
      luaK_storevar(ls->fs, &lh->v, &e);
      return;  /* avoid default */
    }
  }

  luaK_storevar(ls->fs, &lh->v, &e);  /* default assignment */
}
```

这里可以看到，对于多变量赋值会继续递归处理；否则就会直接走 `luaK_setoneret` / `storevar` 这一条路来完成赋值。

重点是 `luaK_storevar`：

```c
void luaK_storevar(FuncState *fs, expdesc *var, expdesc *ex) {
  switch (var->k) {
    case VLOCAL: {
      freeexp(fs, ex);
      exp2reg(fs, ex, var->u.var.ridx);  /* compute 'ex' into proper place */
      return;
    }

    case VUPVAL: {
      int e = luaK_exp2anyreg(fs, ex);
      luaK_codeABC(fs, OP_SETUPVAL, e, var->u.info, 0);
      break;
    }

    case VINDEXUP: {
      codeABRK(fs, OP_SETTABUP, var->u.ind.t, var->u.ind.idx, ex);
      break;
    }

    case VINDEXI: {
      codeABRK(fs, OP_SETI, var->u.ind.t, var->u.ind.idx, ex);
      break;
    }

    case VINDEXSTR: {
      codeABRK(fs, OP_SETFIELD, var->u.ind.t, var->u.ind.idx, ex);
      break;
    }
  }
}
```

#### 寄存器分配

前面的 `luaK_storevar` 使用的是已经设置好的寄存器，而实际的寄存器分配则主要由 `luaK_exp2nextreg` / `luaK_exp2anyreg` 这两个函数完成。它们会调用 `exp2reg` / `discharge2reg`，并结合 `FuncState *fs` 的 `freereg` 字段进行分配；而在 `freereg`、`freeregs`、`freeexp` 这些逻辑里，又会把 `fs->freereg` 回退来释放寄存器。

Lua 的虚拟机寄存器分配是一个非常简单的线性栈式分配器，主要逻辑如下：

- `FuncState->freereg` 表示当前可用的下一个寄存器，分配就是 `luaK_reserveregs(n)` 把 `freereg` 往上挪。
- `luaK_exp2nextreg` / `luaK_exp2anyreg` 会把表达式结果落到寄存器里，内部走 `discharge2reg`，并确保 `freereg` 足够。
- 离开作用域或表达式结束后，用 `freereg` / `freeexp` / `freeregs` 回退 `freereg`，释放临时寄存器。
- 解析阶段通过 `fs->nactvar` 管理活跃局部变量占用的寄存器区间，临时寄存器只能从 `nactvar` 以上开始使用。
- 不做跨语句的寄存器复用优化，更多依赖语法树自然结构和即时释放来减少峰值。

### GC

Lua 的 `Functions` / `Closures` 设计在 `Implementation of Lua 5` 这篇论文里有专门描述，主要围绕 Lua 如何在不引入过于复杂的 control-flow analysis（如 Bigloo Scheme compiler 那类路径）的同时，维护 upper local values。

Lua 引入了 upvalue 来实现 closure。每一个 closure 外层的 local variable，在 closure 内被访问时，都是间接经过 upvalue 完成的。upvalue 初始会指向栈偏移量，对应变量最初被分配的位置；当变量作用域结束时，它会被移入 upvalue 自身的槽里。由于访问是间接的，因此这不会影响已生成代码的结构。

为了确保多个 closure 能正确共享可变状态，一个变量最多只能有一个 upvalue，并按需复用。为了保证这一点，Lua 采用链表结构来管理 open upvalues。

具体内容还是参考论文比较好。这里先看看 Lua 的 GC 代码。

Lua GC 采用的是三色标记（`WHITE0`、`WHITE1`、`BLACK`），并保持“不允许黑指向白”的不变式。Lua 有 Incremental 和 Generational 两种 GC 模式，可以通过 `luaC_changemode` 切换。

VM 和 GC 的交互主要依靠 `luaC_newobj`、`luaC_checkGC`、`luaC_step` 这些操作。

- 每一个对象都处于 `WHITE`、`GRAY`、`BLACK` 三种状态之一。
- 未被访问的对象会被标记为 `WHITE`。
- 被访问但还没有被遍历的对象会被标记为 `GRAY`。
- 被遍历完成的对象会被标记为 `BLACK`。

举一个例子，`t = {}` 在全局环境中：

1. 创建时，新的 table 对象被分配出来，初始是白色，并挂到 GC 的 `allgc` 链表里面。
2. 本轮 GC 的标记阶段，全局表 `_G` 是根对象，会先被标记为 `GRAY`；遍历 `_G` 时会看到字段 `t` 指向的新 table，于是这个 table 也会被标记为 `GRAY`。
3. 继续遍历时，当 GC 处理到该 table，就会遍历其内容，随后把它变黑。
4. `sweep` 之后，活着的对象会重新转回白色，所以这张表在本轮结束后通常又是白的。

如果在后面一轮里面，`t = nil` 了，那这个表就不会再被标记到，于是它会保持白色，并在 `sweep` 阶段被释放。

有一个[比较详细讲述 GC 的 PPT](https://www.lua.org/wshop18/Ierusalimschy.pdf)。目前直接看代码还是比较吃力，还是得先把概念性的东西理清。
