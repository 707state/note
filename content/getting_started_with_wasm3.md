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

# 指令

wasm指令设计和通常的CPU指令差别很大，主要体现在：
1. wasm是栈机而不是寄存器机。
2. 结构化控制流，不能够跳转到任意字节。
3. 它有静态类型系统，验证在执行前完成。
4. 它用线性内存模型隔离宿主内存。
5. 它通过 import/export 与宿主交互。
6. 它支持函数表和间接调用，但仍然受类型约束。
7. 它规定了一组 trap 语义，保证错误有定义，而不是宿主进程直接越界。

以一个wat模块为例：

```wat
i32.const 1
i32.const 2
i32.add
```

它的含义是：

1. 压入 `1`
2. 压入 `2`
3. 从栈顶取两个 `i32`
4. 计算加法
5. 把结果压回栈

这说明 Wasm 虚拟机的抽象执行模型是“值栈 + 控制栈”。

## wasm3实现

Wasm3 在编译期保留 Wasm 的栈语义，但在运行期不直接按“抽象值栈”机械解释。它把栈值映射成：

- `_r0`：整数寄存器
- `_fp0`：浮点寄存器
- `_sp + offset`：slot

对应编译状态保存在 `M3Compilation` 里，关键字段是：

- `wasmStack[]`：每个 Wasm 栈值映射到哪个 slot
- `typeStack[]`：每个栈值的类型
- `slotFirstDynamicIndex`：动态 slot 起点

这些定义在 `source/m3_compile.h`，实际使用逻辑主要在 `source/m3_compile.c`。

Wasm3 保留了 Wasm 的“栈机语义”，但把它编译成了“寄存器 + slot”的执行形态。这是 Wasm3 性能设计的关键。

例如 `i32.add` 在 Wasm 层是“弹两个值、压一个值”，而在 Wasm3 中会被专门化成不同 operation：

- `op_i32_Add_rs`
- `op_i32_Add_ss`

这里：

- `r` 表示值在寄存器
- `s` 表示值在 slot

这说明 Wasm3 把同一条 Wasm 指令，按运行期数据布局进一步拆成多个更贴近真实执行的微操作。

wasm3实现的解释器跟Lua interpreter有非常大的区别，主要体现在指令执行方式上。

wasm3会在编译阶段把wasm opcode变成一串C函数指针+立即数参数，运行时靠尾调用一个接一个跳过去。

wasm3把wasm编译成一段内部的IR形式，这段IR本质上就是一个个`op_*`的C函数地址和紧跟在这些地址后面的立即数参数，比如slot offset、callee pc还有常量值。

pc 指向的不是 wasm 字节流，而是这段元代码页里的“函数指针流”。这点在 `source/m3_code.c:139` 和 `source/m3_compile.c:48` 很直接。

拨开宏调用之后可以看到调用流程：

1. RunCode
2. `op_Entry`，进入 main
3. `op_Compile`，第一次遇到对 add 的调用
4. `op_Call`，调用点被原地改写后再次执行
5. `op_Entry`，进入 add
6. `op_i32_Add_ss`，执行 add 里的 i32.add

运行到 op_Compile 时，它会：
- 先把当前调用点的前一个 operation 改写成 op_Call
- 编译目标函数
- 把当前位置的立即数从 IM3Function* 改成function->compiled
- 然后直接继续跑

可以理解为：wasm3在编译阶段就读取了wasm opcode，然后根据当前栈状态选择最具体的operation变体写入code page，运行的时候就可以直接调用这个函数，函数尾部再尾调用下一个函数。

对于一个非常简单的wat模块来说：

```wat
(module
  (func (export "add") (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.add)

  (func (export "main") (result i32)
    i32.const 20
    i32.const 22
    call 0))

```

伪代码如下：

```c
run main:
    op_Entry(main)

    prepare_args_for_call(add)

    first time:
        op_Compile(add)
        // 编译 add，并把当前调用点改写成 op_Call

    op_Call(add_compiled_pc)

    op_Entry(add)
    op_i32_Add_ss()
    op_Return()

    back to main
    op_Return()

```

## 强类型

wasm每条指令都有非常严格的输入输出类型，比如说：

- `i32.add` 只能消费两个 `i32`
- `f64.sqrt` 只能消费一个 `f64`
- `br_if` 的条件必须是 `i32`
- `call_indirect` 必须匹配目标函数签名

这一点wasm3实现也是考虑了的：

Wasm3 的编译器在 `CompileBlockStatements()` 中顺序读取 opcode，然后通过：

- `GetOpInfo()`
- `Compile_Operator()`
- `Compile_Convert()`
- `PopType()`
- `GetStackTypeFromTop()`
- `ResolveBlockResults()`

持续维护类型栈并做约束检查。

## 类型约束

类型约束最明显体现在：

- `local.get/set/tee`
- `global.get/set`
- 所有数值算术和比较指令
- 转换与重解释指令
- `call_indirect`
- `return`
- `block` / `loop` / `if`

其中转换类指令包括：

- `i32.trunc_{s,u}/f32`
- `i32.trunc_{s,u}/f64`
- `i64.trunc_{s,u}/f32`
- `i64.trunc_{s,u}/f64`
- `f32.convert_{s,u}/i32`
- `f32.convert_{s,u}/i64`
- `f64.convert_{s,u}/i32`
- `f64.convert_{s,u}/i64`
- `reinterpret`
- `extend`
- `trunc_sat`

这些在 Wasm 虚拟机里体现的是“显式类型转换”，在 Wasm3 里主要由 `Compile_Convert()` 选择具体 operation 落地。

## 结构化控制流

Wasm 的控制流不是“跳到任意地址”，而是围绕结构化 block 定义：

- `block`
- `loop`
- `if`
- `else`
- `end`
- `br`
- `br_if`
- `br_table`
- `return`

`br 0`、`br 1` 这样的分支跳转目标不是字节偏移，而是“外层第几个控制块”。这和汇编式 VM 完全不同。

Wasm3 在编译阶段显式维护 block 栈。核心逻辑在：

- `CompileBlock()`
- `Compile_LoopOrBlock()`
- `Compile_If()`
- `Compile_Branch()`
- `Compile_BranchTable()`
- `PatchBranches()`
- `ResolveBlockResults()`
- `PushBlockResults()`

`M3CompilationScope` 记录每个控制块的：

- 类型
- 深度
- 起始 `pc`
- patch 列表
- block 参数/结果在栈上的布局

这正对应 Wasm 虚拟机中的“控制栈”。

Wasm3 不需要像传统字节码 VM 那样维护一个大 `switch` 和一个裸 `pc` 偏移表来模拟任意跳转。它把结构化控制流编译成专门的 operation：

- `op_Loop`
- `op_If_r` / `op_If_s`
- `op_Branch`
- `op_BranchIf_r` / `op_BranchIf_s`
- `op_BranchTable`
- `op_ContinueLoop`
- `op_ContinueLoopIf`
- `op_Return`

特别是 loop 的实现很能体现 Wasm 虚拟机的结构化特征。

`op_ContinueLoop` 并不是简单改写 `_pc` 然后继续，而是返回 loop 标识，让 C 调用栈先展开，再由 `op_Loop` 判断是否继续迭代。这样做有两个作用：

- 让 Wasm 的结构化循环不会无限堆积宿主调用栈
- 把“继续当前 loop”语义和“跳到任意地址”明确区分开

实际上 Wasm3 不是拿 C 代码强行模拟汇编跳转，而是把 Wasm 的结构化控制流翻译成自己的执行机制。

## 线性内存

Wasm 的内存模型不是“任意对象堆”，而是一段按页增长的线性字节数组。Wasm 指令里的地址只是偏移量，不是宿主原生指针。

这意味着：

- 内存访问必须显式经过 load/store
- 越界必须 trap
- 内存增长通过 `memory.grow`
- 查询页数通过 `memory.size`

Wasm3 的线性内存主要在：

- `runtime->memory`
- `ResizeMemory()` in `source/m3_env.c`
- `m3MemInfo()` / `m3MemData()` 宏

执行 load/store 时，`source/m3_exec.h` 中的 `d_m3Load` / `d_m3Store` 宏会统一做：

- 取地址操作数
- 加上 Wasm immediate offset
- 检查 `operand + size <= _mem->length`
- 用 `memcpy` 支持非对齐访问
- 处理小端字节序

这和真实 Wasm VM 的要求完全一致：Wasm 程序永远只看到线性内存，不直接拿到宿主裸地址。

## trap

Wasm 对很多错误都定义了明确的 trap 行为，例如：

- `unreachable`
- 除零
- 整数溢出
- 非法浮点转整数
- 越界内存访问
- table 越界
- 间接调用签名不匹配
- 调用空 table 元素

这和许多本地 ISA 或 C 代码不同。Wasm VM 要求这些错误都在虚拟机层有确定语义。

Wasm3 在执行层大量使用 `newTrap(...)` 返回错误，例如：

- `m3Err_trapUnreachable`
- `m3Err_trapOutOfBoundsMemoryAccess`
- `m3Err_trapDivisionByZero`
- `m3Err_trapIntegerOverflow`
- `m3Err_trapIntegerConversion`
- `m3Err_trapIndirectCallTypeMismatch`
- `m3Err_trapTableElementIsNull`
- `m3Err_trapTableIndexOutOfRange`
- `m3Err_trapStackOverflow`

这些 trap 在 `source/m3_exec.h`、`source/m3_math_utils.h`、`source/wasm3.h` 中都能找到。

例如：

- `op_Unreachable` 直接返回 `m3Err_trapUnreachable`
- load/store 越界会返回 `m3Err_trapOutOfBoundsMemoryAccess`
- 除法和取模的 trap 逻辑在 `m3_math_utils.h`
- `op_CallIndirect` 会处理 table 越界、空元素和签名不匹配

## wasm3执行

Wasm3 不采用传统的：

- 取 opcode
- `switch(opcode)`
- 解码操作数
- 执行
- 回到循环头

而是把 Wasm 编译成 code page，里面存放：

- operation 函数指针
- operation 立即数

执行起点是 `RunCode()`，真正的函数入口是 `op_Entry`。之后每个 operation 通过：

- `nextOpImpl()`
- `jumpOpImpl()`

直接尾调用下一个 operation。

所以 Wasm3 的执行模型可以概括为：

1. 解析 Wasm 模块
2. 编译 Wasm 指令到 M3 operation 序列
3. 用 `_pc` + `_sp` + `_mem` + `_r0` + `_fp0` 作为 VM 寄存器集
4. 由 operation 链接起来持续推进执行

# 异常处理

其实我已经成功跑起来Lua wasm32了，但使用的方案是`wasm_bindgen`向JS抛出异常然后使用一个内建的JS函数捕获异常再交给wasm处理。这个方案明显丑陋不堪，但是也算能够运行。

但是我想要知道更合适的方案，比如说能否直接编写wat模块参与到Rust的编译过程中？那就得看看喽！

```wat
(module
  (type $exn_sig (func (param i32)))
  (tag $exn (type $exn_sig))

  (func $leaf
    i32.const 99
    throw $exn)

  (func $middle
    call $leaf)

  (func (export "main") (result i32)
    (block $done (result i32)
      (try_table (result i32) (catch $exn 0)
        call $middle
        i32.const 0))))
```

这个样子的代码其实没办法用wasm3跑了，wasm3并没有实现WASM Exception Handling相关的标准，但是可以用wasmtime通过`-W exceptions=y`来运行。

那么能否使用Rust和wat模块一起工作呢？

## Rust it!

