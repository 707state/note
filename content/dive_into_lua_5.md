---
title: luajit 和 ffi
author: jask
tags:
  - Lua
  - ProgrammingLanguages
date: 2026-05-28
series: 深入理解lua
---

# Luajit与ffi

luajit提供了非常好用的ffi功能，可以直接在`cdef`里面声明C函数然后调用。

luajit的ffi用到C的ABI，因此参数需要从Lua栈搬运到寄存器中来调用C函数。
反过来，callback（C调用Lua函数）则需要从寄存器搬运到Lua栈来执行。

整个过程在两条路径上完全对称：

```
方向一: Lua → C                         方向二: C → Lua
────────────────────                    ────────────────────

Lua Stack (TValue[])                   CPU 寄存器 (rdi/rsi/...)
        │                                       │
        ▼                                       ▼
  ccall_set_args()                     lj_vm_ffi_callback (汇编)
  类型转换 + ABI 决策                   保存寄存器值到 CTState
        │                                       │
        ▼                                       ▼
  CCallState (中转缓冲区)               CTState.cb (中转缓冲区)
  .gpr[]  .fpr[]  .stack[]              .cb.gpr[]  .cb.fpr[]  .cb.stack
        │                                       │
        ▼                                       ▼
  lj_vm_ffi_call (汇编)                lj_ccallback_enter
  搬入真实寄存器, call                  类型转换, 建 Lua 栈帧
        │                                       │
        ▼                                       ▼
  CPU 执行 C 函数                       Lua 解释器执行 Lua 函数
        │                                       │
        ▼                                       ▼
  lj_vm_ffi_call 保存返回值            lj_ccallback_leave
  真实寄存器 → CCallState               Lua 栈 → CTState.cb
        │                                       │
        ▼                                       ▼
  ccall_get_results()                  lj_vm_ffi_callback 尾部
  CCallState → Lua Stack               CTState.cb → CPU 寄存器, ret
```

核心数据结构 `CCallState` 就是两端的通用插头。

---

## 1. Foward: Lua → C 调用

### 1.1. 数据结构：CCallState — 参数中转站

```c
/* lj_ccall.h:165-188 */

#define CCALL_NUM_STACK    31
#define CCALL_SIZE_STACK    (CCALL_NUM_STACK * CTSIZE_PTR)

typedef LJ_ALIGN(CCALL_ALIGN_CALLSTATE) struct CCallState {
  void (*func)(void);                /* Pointer to called function. */
  uint32_t spadj;                    /* Stack pointer adjustment. */
  uint8_t nsp;                       /* Number of bytes on stack. */
  uint8_t retref;                    /* Return value by reference. */
#if LJ_TARGET_X64
  uint8_t ngpr;                      /* Number of arguments in GPRs. */
  uint8_t nfpr;                      /* Number of arguments in FPRs. */
#elif LJ_TARGET_X86
  uint8_t resx87;                    /* Result on x87 stack: 1:float, 2:double. */
#elif LJ_TARGET_ARM64
  void *retp;                        /* Aggregate return pointer in x8. */
#elif LJ_TARGET_PPC
  uint8_t nfpr;                      /* Number of arguments in FPRs. */
#endif
#if LJ_32
  int32_t align1;
#endif
#if CCALL_NUM_FPR
  FPRArg fpr[CCALL_NUM_FPR];         /* Arguments/results in FPRs. */
#endif
  GPRArg gpr[CCALL_NUM_GPR];         /* Arguments/results in GPRs. */
  GPRArg stack[CCALL_NUM_STACK];     /* Stack slots. */
} CCallState;
```

在不同架构上，寄存器的数量不同：

```c
/* lj_ccall.h:18-38 */

#if LJ_TARGET_X86
#define CCALL_NARG_GPR     2          /* x86 fastcall: 2 GPR */
#define CCALL_NARG_FPR     0          /* x86: FP 走 x87 栈 */
#elif LJ_ABI_WIN
#define CCALL_NARG_GPR     4          /* Win64: 4 GPR */
#define CCALL_NARG_FPR     4          /* Win64: 4 FPR */
#else
#define CCALL_NARG_GPR     6          /* SysV ABI: rdi,rsi,rdx,rcx,r8,r9 */
#define CCALL_NARG_FPR     8          /* SysV ABI: xmm0..xmm7 */
#endif

#elif LJ_TARGET_ARM64
#define CCALL_NARG_GPR     8          /* ARM64: x0..x7 */
#define CCALL_NARG_FPR     8          /* ARM64: d0..d7 */
```

### 1.2. 第一步：ccall_set_args — Lua栈 → CCallState

入口函数是 `lj_ccall_func`：

```c
/* lj_ccall.c:1184-1225 */

int lj_ccall_func(lua_State *L, GCcdata *cd)
{
  CTState *cts = ctype_cts(L);
  CType *ct = ctype_raw(cts, cd->ctypeid);
  CTSize sz = CTSIZE_PTR;
  if (ctype_isptr(ct->info)) {
    sz = ct->size;
    ct = ctype_rawchild(cts, ct);
  }
  if (ctype_isfunc(ct->info)) {
    CTypeID id = ctype_typeid(cts, ct);
    CCallState cc;
    int gcsteps, ret;
    cc.func = (void (*)(void))cdata_getptr(cdataptr(cd), sz);

    // ★ 遍历参数
    gcsteps = ccall_set_args(L, cts, ct, &cc);

    // ★ 汇编层执行 call
    lj_vm_ffi_call(&cc);

    // ★ 取回返回值
    gcsteps += ccall_get_results(L, cts, ct, &cc, &ret);
    return ret;
  }
}
```

`ccall_set_args` 逐参数做三件事：**识别类型 → 决定放寄存器还是栈 → 类型转换**。

```c
/* lj_ccall.c:937-1060 (精简) */

static int ccall_set_args(lua_State *L, CTState *cts, CType *ct, CCallState *cc)
{
  TValue *o, *top = L->top;
  CType *d;
  MSize maxgpr, ngpr = 0, nsp = 0;
  MSize nfpr = 0;

  maxgpr = CCALL_NARG_GPR;    // SysV x64: 6

  /* 遍历 Lua 栈 L->base+1 到 L->top */
  for (o = L->base+1, narg = 1; o < top; o++, narg++) {

    /* 取 C 类型声明 */
    d = ctype_raw(cts, did);
    sz = d->size;

    /* 判断放 GPR / FPR / Stack */
    if (ctype_isnum(d->info)) {
      if (d->info & CTF_FP) isfp = 1;   // 浮点 → FPR
    }
    n = (sz + CTSIZE_PTR-1) / CTSIZE_PTR;

    // ★ 架构相关的寄存器分配宏
    CCALL_HANDLE_REGARG

    /* 寄存器放不下的走栈 */
    dp = ((uint8_t *)cc->stack) + nsp;
    nsp += n * CTSIZE_PTR;

  done:
    // ★ 类型转换: Lua TValue → C 原始值
    lj_cconv_ct_tv(cts, d, (uint8_t *)dp, o, CCF_ARG(narg));

    /* 整数扩展: narrow int → 32bit */
    if (ctype_isinteger_or_bool(d->info) && d->size < 4) {
      if (d->info & CTF_UNSIGNED)
        *(uint32_t *)dp = d->size == 1 ? (uint32_t)*(uint8_t *)dp
                                       : (uint32_t)*(uint16_t *)dp;
      else
        *(int32_t *)dp = d->size == 1 ? (int32_t)*(int8_t *)dp
                                      : (int32_t)*(int16_t *)dp;
    }
  }
  cc->nsp = nsp;
  cc->spadj = ...;  // 栈对齐计算
}
```

`CCALL_HANDLE_REGARG` 是平台相关的宏。x86-64 SysV ABI 版本：

```c
/* lj_ccall.c:186-203 (x64 SysV) */

#define CCALL_HANDLE_REGARG \
  if (isfp) {  /* Try to pass argument in FPRs. */ \
    int n2 = ctype_isvector(d->info) ? 1 : n; \
    if (nfpr + n2 <= CCALL_NARG_FPR) { \
      dp = &cc->fpr[nfpr]; \
      nfpr += n2; \
      goto done; \
    } \
  } else {  /* Try to pass argument in GPRs. */ \
    /* Note that reordering is explicitly allowed in the x64 ABI. */ \
    if (n <= 2 && ngpr + n <= maxgpr) { \
      dp = &cc->gpr[ngpr]; \
      ngpr += n; \
      goto done; \
    } \
  }
```

**决策算法**：浮点参数优先尝试 FPR（xmm0-xmm7），放不下走内存栈。整数/指针参数优先尝试 GPR（rdi, rsi, rdx, rcx, r8, r9），放不下走内存栈。struct 参数需要先用 `call_classify_struct` 分类，决定走寄存器还是栈。

### 1.3. 类型转换：lj_cconv_ct_tv — TValue → C 值

这是 FFI 类型系统的核心，根据 Lua 值的运行时类型决定怎么转换：

```c
/* lj_cconv.c:544-640 (精简) */

void lj_cconv_ct_tv(CTState *cts, CType *d, uint8_t *dp, TValue *o, CTInfo flags)
{
  CTypeID sid = CTID_P_VOID;
  uint8_t *sp;

  if (LJ_LIKELY(tvisint(o))) {
    sp = (uint8_t *)&o->i;
    sid = CTID_INT32;                // Lua 整数 → C int
  } else if (LJ_LIKELY(tvisnum(o))) {
    sp = (uint8_t *)&o->n;
    sid = CTID_DOUBLE;               // Lua 浮点 → C double
  } else if (tviscdata(o)) {
    sp = cdataptr(cdataV(o));        // cdata → 直接取原始 C 数据指针
    sid = cdataV(o)->ctypeid;
  } else if (tvisstr(o)) {
    sp = (uint8_t *)strdata(str);    // Lua 字符串 → const char*
    sid = CTID_A_CCHAR;
  } else if (tvisbool(o)) {
    tmpbool = boolV(o); sp = &tmpbool; sid = CTID_BOOL;
  } else if (tvisnil(o)) {
    tmpptr = (void *)0;              // nil → NULL 指针
  } else if (tvisfunc(o)) {
    p = lj_ccallback_new(cts, d, funcV(o));  // Lua 函数 → callback stub
    *(void **)dp = p; return;
  }
  // ...
  lj_cconv_ct_ct(cts, d, s, dp, sp, flags);  // C → C 类型转换
}
```

**`lj_cconv_ct_ct`** 完成了最终的类型适配（int → long, float → double, 符号扩展, 大端/小端字节序等）。

### 1.4. 第二步：lj_vm_ffi_call — CCallState → CPU 寄存器 + 执行

汇编写的。x86-64 版本（精简）：

```asm
/* vm_x86.dasc:3391-3491 */

|->vm_ffi_call@4:
  // ★ 1. 整数参数：CCallState.gpr[] → CPU 寄存器
  mov CARG1, CCSTATE->gpr[0]       ; rdi = 参数 1
  mov CARG2, CCSTATE->gpr[1]       ; rsi = 参数 2
  mov CARG3, CCSTATE->gpr[2]       ; rdx = 参数 3
  mov CARG4, CCSTATE->gpr[3]       ; rcx = 参数 4
  mov CARG5, CCSTATE->gpr[4]       ; r8  = 参数 5
  mov CARG6, CCSTATE->gpr[5]       ; r9  = 参数 6

  // ★ 2. 浮点参数：CCallState.fpr[] → xmm0..xmm7
  movaps xmm0, CCSTATE->fpr[0]
  movaps xmm1, CCSTATE->fpr[1]
  movaps xmm2, CCSTATE->fpr[2]
  movaps xmm3, CCSTATE->fpr[3]
  movaps xmm4, CCSTATE->fpr[4]
  movaps xmm5, CCSTATE->fpr[5]
  movaps xmm6, CCSTATE->fpr[6]
  movaps xmm7, CCSTATE->fpr[7]

  // ★ 3. 栈参数：CCallState.stack[] 拷贝到 rsp
  movzx ecx, byte CCSTATE->nsp
  sub ecx, 8
1:
  mov rax, [CCSTATE+rcx+offsetof(CCallState, stack)]
  mov [rsp+rcx], rax
  sub ecx, 8
  jns <1

  // ★ 4. 调用！
  call aword CCSTATE->func

  // ★ 5. 保存返回值
  mov CCSTATE->gpr[0], rax         ; 整数返回
  mov CCSTATE->gpr[1], rdx         ; 扩展整数返回（128位等）
  movaps CCSTATE->fpr[0], xmm0     ; 浮点返回
```

以 `ffi.C.abs(-42)` 为例：

```
Lua 栈:                  CCallState:              CPU:
┌────────────────┐      ┌──────────────────┐    ┌──────────┐
│ -42 (TValue)   │      │ gpr[0]: -42      │    │ rdi = -42│
│   .i = -42     │ ───► │ ngpr: 1          │ ──►│ call abs │
│   .it = INT    │      │ func: &abs        │    │ rax = 42 │
└────────────────┘      └──────────────────┘    └──────────┘
                                ▲
                                │  lj_ccall_func
                                │  ccall_set_args
```

### 1.5. 第三步：ccall_get_results — CCallState → Lua Stack

```c
/* lj_ccall.c:1143-1182 */

static int ccall_get_results(lua_State *L, CTState *cts, CType *ct,
                             CCallState *cc, int *ret)
{
  CType *ctr = ctype_rawchild(cts, ct);
  uint8_t *sp = (uint8_t *)&cc->gpr[0];     // 默认从 gpr[0] 取

  if (ctype_isvoid(ctr->info)) {
    *ret = 0; return 0;                      // void: 无返回值
  }
  *ret = 1;

  if (ctype_isstruct(ctr->info)) {
    // struct 已经在栈上分配了 cdata，直接填充
    CCALL_HANDLE_STRUCTRET2
    return 1;
  }

  // 浮点返回从 fpr 取
  if (ctype_isfp(ctr->info) || ctype_isvector(ctr->info))
    sp = (uint8_t *)&cc->fpr[0];

  // ★ 反向转换: C 值 → Lua TValue
  return lj_cconv_tv_ct(cts, ctr, 0, L->top-1, sp);
}
```

---

## 2. Backward: C → Lua 回调

### 2.1. Stub 机制：动态生成可执行代码

当 Lua 函数被 `ffi.cast` 转为 C 函数指针时，`lj_ccallback_new` 为其分配一个 slot 号，并返回一个 stub 机器码入口地址。

```c
/* lj_ccallback.c:820-831 */

void *lj_ccallback_new(CTState *cts, CType *ct, GCfunc *fn)
{
  ct = callback_checkfunc(cts, ct);       // 验证类型兼容
  if (ct) {
    MSize slot = callback_slot_new(cts, ct);  // 分配 slot
    GCtab *t = cts->miscmap;
    setfuncV(cts->L, lj_tab_setint(cts->L, t, (int32_t)slot), fn);
    return callback_slot2ptr(cts, slot);      // ★ 返回 stub 的函数指针
  }
}
```

```c
/* lj_ccallback.c:108-111 */

static void *callback_slot2ptr(CTState *cts, MSize slot)
{
  return (uint8_t *)cts->cb.mcode + CALLBACK_SLOT2OFS(slot);
}
```

每个 slot 的 stub 机器码（x86-64）：

```c
/* lj_ccallback.c:139-159 (精简) */

// 每个 slot 的 stub（~4-6 字节机器码）:
*p++ = XI_MOVrib | RID_EAX; *p++ = (uint8_t)slot;  // mov al, slot_number
*p++ = XI_JMPs; *p++ = offset;                     // jmp group_entry
```

也就是说 slot 0 编译后等价于：

```asm
mov al, 0          ; slot 号
jmp group_entry    ; 跳到公共入口 → push rbp; mov rbp, &g; jmp lj_vm_ffi_callback
```

这个 stub 在 MCode 区域内，具有 `RX` 权限，C 代码可以把它当普通函数指针用。

### 2.2. lj_vm_ffi_callback — CPU 寄存器 → CTState

当 C 代码通过函数指针调用过来时，先经过 stub，然后进 `lj_vm_ffi_callback`：

```asm
/* vm_x86.dasc:3295-3350 (x64 精简) */

|->vm_ffi_callback:
  // ★ 1. 保存所有 C 参数寄存器到 CTState
  mov CTSTATE->cb.gpr[0], CARG1      ; rdi → cb.gpr[0]
  mov CTSTATE->cb.gpr[1], CARG2      ; rsi → cb.gpr[1]
  mov CTSTATE->cb.gpr[2], CARG3      ; rdx → cb.gpr[2]
  mov CTSTATE->cb.gpr[3], CARG4      ; rcx → cb.gpr[3]
  mov CTSTATE->cb.gpr[4], CARG5      ; r8  → cb.gpr[4]
  mov CTSTATE->cb.gpr[5], CARG6      ; r9  → cb.gpr[5]
  movsd CTSTATE->cb.fpr[0], xmm0     ; xmm0 → cb.fpr[0]
  movsd CTSTATE->cb.fpr[1], xmm1
  movsd CTSTATE->cb.fpr[2], xmm2
  movsd CTSTATE->cb.fpr[3], xmm3
  movsd CTSTATE->cb.fpr[4], xmm4
  movsd CTSTATE->cb.fpr[5], xmm5
  movsd CTSTATE->cb.fpr[6], xmm6
  movsd CTSTATE->cb.fpr[7], xmm7
  mov CTSTATE->cb.stack, rsp         ; 栈参数指针

  mov CTSTATE->cb.slot, eax          ; slot 号

  // ★ 2. 调用 lj_ccallback_enter 建 Lua 栈帧
  call lj_ccallback_enter

  // ★ 3. 切换到解释器执行 Lua 函数
  mov BASE, L->base
  ins_callt                          ; jump to Lua interpreter
```

与 forward 方向的 `lj_vm_ffi_call` 完全对称：forward 是 `CCallState → 寄存器 → call`，callback 是 `stub → 寄存器 → CTState → enter Lua`。

### 2.3. 返回值：lj_ccallback_leave → CTState → 寄存器 → ret

Lua 函数执行完毕后，返回到 `cont_ffi_callback`：

```asm
/* vm_x86.dasc:3353-3388 */

|->cont_ffi_callback:
  mov L->base, BASE
  mov L->top, RB                     ; 保存 Lua 栈顶
  mov FCARG1, CTSTATE
  mov FCARG2, RC                     ; RC = 返回值位置
  call lj_ccallback_leave            ; Lua TValue → CTState.cb.gpr[]

  // ★ 从 CTState 搬回 CPU 寄存器
  mov rax, CTSTATE->cb.gpr[0]        ; gpr[0] → rax
  movsd xmm0, CTSTATE->cb.fpr[0]     ; fpr[0] → xmm0
  jmp ->vm_leave_unw                 ; 恢复寄存器，ret
```

---

## 3. 数据流总结

```
┌─────────────────┐        ┌──────────────────┐        ┌─────────────────┐
│   Lua Stack     │        │   CCallState /    │        │   CPU Registers │
│   (TValue[])    │◄──────►│   CTState.cb      │◄──────►│   (rdi/xmm...)  │
└─────────────────┘        └──────────────────┘        └─────────────────┘
      │                            │                          │
      │    lj_cconv_ct_tv          │    lj_vm_ffi_call/       │
      │    (类型转换)               │    lj_vm_ffi_callback     │
      │    +                        │    (汇编搬运)            │
      │    CCALL_HANDLE_REGARG      │                          │
      │    (ABI 放寄存器还是栈)      │                          │
      ▼                            ▼                          ▼
  "Lua 值"                    "C 原始值"                "CPU 指令可
   int/num/str/                int64_t/double/           直接操作的值"
   cdata/ptr/nil               void*/struct
```

**关键设计决策**：

1. **`CCallState` 是最低共同分母**。它既不是 Lua 数据结构也不是 CPU 寄存器，而是两者之间的线性缓冲区。Lua 侧把 TValue 转成原始 C 字节写入，CPU 侧直接 `mov` 到寄存器。

2. **平台差异在 macro 中消化**。`CCALL_HANDLE_REGARG`、`CCALL_HANDLE_STRUCTARG` 等在不同架构上有不同实现，但外围的遍历循环完全共享。

3. **反向路径完全对称**。`CCallState.gpr[0] → CPU 寄存器 → call` 和 `stub → CPU 寄存器 → CTState.cb.gpr[0] → Lua`，一个正着走，一个反着走，核心结构一摸一样。
