---
title: WASM究竟是怎么一回事？
author: jask
tags:
  - WebAssembly
  - C
date: 2026-04-10
---

# 定义

WebAssembly 是栈虚拟机，可以在浏览器中避免 JS 这种动态语言的开销，让代码以接近原生的速度执行。

WASM 主流浏览器都支持了，而且还有 WASI 这样的标准，让 WASM 在所有操作系统上运行。

# wasm3

一个 `.wasm` 文件本质上是一段结构化的字节码，需要一个运行时来加载、验证、执行它。`wasm3` 就是这样一个运行时，用纯 C 实现，极度轻量。

## wasm3 整体架构

```txt
.wasm 文件
      │
      ▼
  [Parse]  m3_parse.c       ← 解析二进制格式，构建 M3Module
      │
      ▼
  [Compile] m3_compile.c    ← 将 Wasm 字节码编译为内部 IR（threaded code）
      │
      ▼
  [Execute] m3_exec.h       ← 执行内部 IR（解释器主循环）

  对应的核心对象层次：

  M3Environment              ← 全局环境，可托管多个运行时
    └── M3Runtime            ← 执行上下文（栈、内存、代码页）
          └── M3Module       ← 一个 .wasm 模块
                ├── M3Function[]   ← 函数列表
                ├── M3Global[]     ← 全局变量
                └── M3Memory       ← 线性内存
```

## 核心数据结构

`wasm3.h` 和 `m3_env.h` 里先定义了几类核心对象的指针类型：

```c
// wasm3.h:33-37
struct M3Environment;
typedef struct M3Environment *IM3Environment;

struct M3Runtime;
typedef struct M3Runtime *IM3Runtime;

struct M3Module;
typedef struct M3Module *IM3Module;

struct M3Function;
typedef struct M3Function *IM3Function;
```

注意命名约定：`IM3Xxx` 是指针类型。

`M3Runtime`（`m3_env.h:161`）是最核心的执行上下文：

```c
typedef struct M3Runtime {
    M3Compilation   compilation;     // 编译状态（复用以节省内存）
    IM3Environment  environment;

    M3CodePage *    pagesOpen;       // 代码页链表（有空余空间）
    M3CodePage *    pagesFull;       // 代码页链表（已满）

    IM3Module       modules;         // 已加载的模块链表

    void *          stack;           // Wasm 执行栈（slot 数组）
    u32             stackSize;
    u32             numStackSlots;

    M3Memory        memory;          // 线性内存
    M3ErrorInfo     error;
} M3Runtime;
```

`M3Module`（`m3_env.h:82`）对应一个 `.wasm` 文件：

```c
typedef struct M3Module {
    bytes_t         wasmStart;       // 指向原始 wasm 字节（必须在模块生命期内持续有效）
    bytes_t         wasmEnd;

    u32             numFunctions;
    M3Function *    functions;       // 函数数组

    u32             numGlobals;
    M3Global *      globals;

    M3MemoryInfo    memoryInfo;      // 内存声明（initPages, maxPages）
    // ...
} M3Module;
```

函数结构定义在 `m3_function.h:41`：

```c
typedef struct M3Function {
    struct M3Module *module;

    bytes_t     wasm;       // 指向原始 wasm 字节码中该函数体的起始位置
    bytes_t     wasmEnd;

    IM3FuncType funcType;   // 函数签名（参数类型 + 返回类型）

    pc_t        compiled;   // 编译后的 IR 起始地址（NULL = 尚未编译）

    u16         maxStackSlots;
    u16         numLocals;
    u16         numLocalBytes;
    void *      constants;  // 函数内的常量表
} M3Function;
```

这里有个关键设计：`compiled` 字段为 `NULL` 时表示函数还没被编译。`wasm3` 使用懒编译（lazy compilation），函数只在第一次被调用时才编译。

函数类型定义在 `m3_function.h:17`：

```c
typedef struct M3FuncType {
    struct M3FuncType *next; // 链表，Environment 中去重存储
    u16                numRets;
    u16                numArgs;
    u8                 types[]; // 先放返回类型，再放参数类型
} M3FuncType;
```

## 二进制格式

`.wasm` 文件由多个 Section（节）组成，`wasm3` 会逐节解析：

```txt
┌────────────┬──────────┬───────────────────────┐
│ Section ID │   名称   │         内容          │
├────────────┼──────────┼───────────────────────┤
│ 1          │ Type     │ 函数类型签名          │
├────────────┼──────────┼───────────────────────┤
│ 2          │ Import   │ 导入的函数/内存/全局  │
├────────────┼──────────┼───────────────────────┤
│ 3          │ Function │ 函数索引→类型索引映射 │
├────────────┼──────────┼───────────────────────┤
│ 5          │ Memory   │ 内存声明              │
├────────────┼──────────┼───────────────────────┤
│ 6          │ Global   │ 全局变量              │
├────────────┼──────────┼───────────────────────┤
│ 7          │ Export   │ 导出名称              │
├────────────┼──────────┼───────────────────────┤
│ 10         │ Code     │ 函数体字节码          │
├────────────┼──────────┼───────────────────────┤
│ 11         │ Data     │ 内存初始化数据        │
└────────────┴──────────┴───────────────────────┘
```

`Type Section` 的解析代码在 `m3_parse.c:46`：

```c
M3Result ParseSection_Type(IM3Module io_module, bytes_t i_bytes, cbytes_t i_end)
{
    u32 numTypes;
    ReadLEB_u32(&numTypes, &i_bytes, i_end);   // 读取类型数量

    for (u32 i = 0; i < numTypes; ++i) {
        i8 form;
        ReadLEB_i7(&form, &i_bytes, i_end);
        // form == -32 (0x60) 表示函数类型，这是 Wasm MVP 唯一的类型形式

        u32 numArgs;
        ReadLEB_u32(&numArgs, &i_bytes, i_end);
        // ... 读取参数类型和返回类型
    }
}
```

Wasm 用 LEB128 压缩整数（可变长度），`m3_core.h` 中的 `ReadLebUnsigned` 和 `ReadLebSigned` 负责解码。

## wasm3 的特点

### Threaded-Code

传统 switch-dispatch 解释器：

```c
while (true) {
    switch (*pc++) {
        case OP_ADD: r = a + b; break;
        case OP_SUB: r = a - b; break;
        // ...
    }
}
```

每条指令都要经过 `switch` 跳转，有分支预测开销。

`Threaded Code`（`wasm3` 的方式）则不同。编译后的代码不是字节码，而是函数指针数组。每个“指令”就是一个函数指针，执行完直接跳到下一个函数指针指向的函数，无需 `switch`。

### 关键结构

每个操作函数的参数签名大致如下：

```c
#define d_m3OpSig pc_t _pc, m3stack_t _sp, M3MemoryHeader *_mem, m3reg_t _r0, f64 _fp0
//               程序计数器   栈指针         内存基址                  整数寄存器   浮点寄存器
```

几个关键参数分别表示：

- `_pc`：当前指令流位置（函数指针数组）
- `_sp`：Wasm 栈指针（slot 数组）
- `_mem`：线性内存基址
- `_r0`：整数“寄存器”（热路径值，避免内存读写）
- `_fp0`：浮点“寄存器”

宏展开后，一个加法操作实际上是这样的函数：

```c
static inline m3ret_t vectorcall op_i32_Add_rs(
    pc_t _pc, m3stack_t _sp, M3MemoryHeader *_mem, m3reg_t _r0, f64 _fp0)
{
    i32 operand = *(i32 *)(_sp + *(i32 *)_pc++); // 从 slot 读取操作数
    _r0 = (i32)((u32)_r0 + (u32)operand);        // 执行加法，结果放寄存器

    // nextOp() 展开后会直接尾调用下一个操作函数
    return ((IM3Operation)(*_pc))(_pc + 1, _sp, _mem, _r0, _fp0);
}
```

命名约定（`m3_exec.h:16`）：

- `_rs`：第一个操作数在寄存器（r），第二个在栈槽（s）
- `_sr`：第一个在栈槽，第二个在寄存器
- `_ss`：两个都在栈槽
- `_r`：只有一个操作数，在寄存器

### 尾递归优化

```c
#define nextOpDirect() M3_MUSTTAIL return nextOpImpl()
// M3_MUSTTAIL 在 clang 上展开为 [[clang::musttail]]
// 这强制编译器使用尾调用优化，使函数间跳转变成 jmp 而不是 call/ret
```

这是 `wasm3` 的精髓：整个执行过程是一条尾调用链，没有真正的函数调用栈增长。

### 执行

编译后的 IR（函数指针序列）存放在 `CodePage` 中：

```c
typedef struct M3CodePage {
    M3CodePageHeader info;
    code_t           code[1]; // 弹性数组，存放函数指针序列
} M3CodePage;
```

`code_t` 就是 `void *`（`m3_core.h:128`），实际存放的是操作函数的指针。

当一个 `CodePage` 写满时，编译器会自动申请新页，并在旧页末尾写一个 `op_Branch` 指令跳转过去（`m3_compile.c:41`）：

```c
EmitWord(o->page, op_Branch);        // 写入跳转指令
EmitWord(o->page, GetPagePC(page));  // 写入目标地址
```

`CompileFunction` 会遍历函数的 Wasm 字节码，为每条指令选择对应的操作函数并写入 `CodePage`。

编译状态结构（`m3_compile.h:72`）如下：

```c
typedef struct {
    IM3Runtime  runtime;
    IM3Module   module;
    bytes_t     wasm;       // 当前解析位置
    bytes_t     wasmEnd;

    u16         stackIndex;                          // 当前栈高度
    u16         wasmStack[d_m3MaxFunctionStackHeight]; // 追踪每个栈位置用的 slot 索引
    u8          typeStack[...];                     // 追踪类型（用于验证）

    m3slot_t    constants[d_m3MaxConstantTableSize]; // 常量表
} M3Compilation;
```

编译时还有一个关键决策：操作数在寄存器还是栈槽？

```c
typedef struct M3OpInfo {
    i8          stackOffset;    // 此操作对栈高度的影响
    u8          type;           // 结果类型
    IM3Operation operations[4]; // [0]=top 在寄存器, [1]=top 在栈, [2]=两个都在栈, [3]=特殊
    M3Compiler  compiler;       // 自定义编译函数（可选）
} M3OpInfo;
```

编译器会根据当前哪个值在寄存器里，选择最合适的操作变体，以减少内存访问。

宿主函数绑定示例：

```c
// 定义一个宿主函数
m3ApiRawFunction(metering_usegas)
{
    m3ApiGetArg(int32_t, gas) // 从栈上取参数
    current_gas -= gas;
    if (current_gas < 0) {
        m3ApiTrap("[trap] Out of gas");
    }
    m3ApiSuccess();
}

// 注册到模块
m3_LinkRawFunction(module, "metering", "usegas", "v(i)", &metering_usegas);
// 签名：void(int32)
```

宿主函数通过 `_sp`（栈指针）和 `_mem`（内存基址）访问 Wasm 的数据。

大致流程如下：

```c
// 1. 创建全局环境
IM3Environment env = m3_NewEnvironment();

// 2. 创建运行时（指定栈大小）
IM3Runtime runtime = m3_NewRuntime(env, 64 * 1024, NULL);

// 3. 解析 .wasm 文件
IM3Module module;
m3_ParseModule(env, &module, wasm_bytes, wasm_size);

// 4. 加载到运行时（转移所有权）
m3_LoadModule(runtime, module);

// 5. 链接宿主函数（WASI、libc 等）
m3_LinkWASI(module);

// 6. 查找要调用的函数
IM3Function func;
m3_FindFunction(&func, runtime, "_start");

// 7. 调用（懒编译在这里触发）
m3_CallArgv(func, 0, NULL);
```

## wasip1

wasm3实现了wasip1标准，提供了三个后端，分别是：simple、metawasi和uvwasi，其中uvwasi是默认的cli后端，能力最完整。
