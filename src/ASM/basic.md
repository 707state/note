<!--toc:start-->
- [基本格式](#基本格式)
- [adrp](#adrp)
- [内存与寄存器](#内存与寄存器)
  - [索引模式](#索引模式)
- [aarch64基本知识](#aarch64基本知识)
  - [A64](#a64)
    - [A64汇编语言的结构](#a64汇编语言的结构)
    - [mov指令](#mov指令)
    - [PC-Relative Addressing](#pc-relative-addressing)
  - [System Registers](#system-registers)
- [编码](#编码)
- [栈指针](#栈指针)
  - [push/pop](#pushpop)
    - [Prologue and Epilogue](#prologue-and-epilogue)
<!--toc:end-->

# 基本格式

ARM汇编基本格式：
label: opcode operands

- 1号系统调用是exit，4为write，使用方法是设置对应的寄存器后通过自陷的方式svc 0x80来转为内核态。

W和X是两种寄存器，W是64位寄存器的低32位部分。

# adrp

计算页面基地址，它将目标地址的页面基地址（4KB对齐）加载到目标寄存器中。

可以与add或者ldr指令一起使用。

# 内存与寄存器

MOV系列的指令。
STR开头的，也就是store xxx，比如说STRB就是store byte。

```asm
strb w6,[x1]
```
就是把w6寄存器（x6寄存器的低32位值）的值写入到x1寄存器存储的地址。

## 索引模式

索引寻址可以用于一些更特殊的代码模式：

```c++
int *a;
int b = *(++a);
int c = *(a++);
```

对于第二行b的赋值而言，我们需要将a的值加4（int类型宽度为4）后赋值给a，然后取值赋值
对于第三行c的赋值而言，我们需要将a取值赋值，然后将a的值加4（int类型宽度为4）后赋值给a

这两种模式可以分别用：

```c++
ldr    w1, [x0, #4]!
ldr    w1, [x0], #4
```

第一种写法被称为前索引寻址，将x0的值加4赋值给x0后，将对应内存取值赋值给w1
第二种写法被称为后索引寻址，将x0对应的内存取值赋值给w1后，将x0自身值加4赋值给自身

# aarch64基本知识

- AArch64提供了21个64bit的通用寄存器，其中X30被用于procedure link寄存器。
- 64bit的Program Counter, Stack Pointers还有Exception Link Registers。
- 32个128bit的寄存器用于SIMD向量/标量浮点支持。
- 一个单独的指令集：A64。
- ArmV8 Exception Model，包括：EL0-EL3四个异常等级。
- 64bit的虚地址。
- 一系列维护PE的Process State(PSTATE)元素。

## A64

A64指令集是一套定长指令集，长度为32bit。

A64指令集的编码格式：

- 5bit的通用寄存器，选取0-30之一的寄存器，31号寄存器是特殊的。
- Advanced SIMD和浮点寄存器以及SVE寄存器，使用5bit来获取32个寄存器之一

### A64汇编语言的结构

一条instruction的第31位（从0开始计数）是sf(size field)位。值为1表示这是一条用了64位寄存器的指令，为0表示用的是32位寄存器。

w表示一个寄存器有32bit的字，x则是64bit表示一个双字。

一个汇编指令可以用在不同的结构中。

```asm
ADD w0, w1, w2 //表示32位的寄存器相加
ADD x0, x1, x2 //表示64位的寄存器相加
ADD x0, x1, w2, sxtw //表示64位拓展寄存器
ADD x0, x1, #42 //64位立即数
```

> sve assembler指的是汇编格式：operation code, destination register, predicate register, input register。


### mov指令

第21-22位是hw位，用来表示偏移量，这是因为ARM64的MOV指令只能直接编码16位立即数，但我们需要操作64位的数值。

对于这样的一条指令：
```asm
mov x0, #0x1234
```

这条指令的立即数大小就是16位的，因为hw为00，但是如果是这样：

```asm
mov x0, #0x12340000
```
这时候hw就是01，以此类推。

MOV（实际上是MOVZ）指令的前5个bit为Rd字段，用来表示的是目标寄存器。

### PC-Relative Addressing

A64指令集支持position-independent code和data addressing。

## System Registers

系统寄存器提供了控制和系统信息的特性。

其命名方式位: register\_name.bit\_field\_name。

# 编码

AArch64有一类指令又非常奇葩的编码规则，这里说的就是bitmask immediate。

这玩意用了一些非常抽象的编码规则，具体如下：
[and指令imm部分](https://developer.arm.com/documentation/dui0802/b/A64-General-Instructions/AND--immediate-)

> Is the bitmask immediate. Such an immediate is a 32-bit or 64-bit pattern viewed as a vector of identical elements of size e = 2, 4, 8, 16, 32, or 64 bits. Each element contains the same sub-pattern: a single run of 1 to e-1 non-zero bits, rotated by 0 to e-1 bits. This mechanism can generate 5,334 unique 64-bit patterns (as 2,667 pairs of pattern and their bitwise inverse). Because the all-zeros and all-ones values cannot be described in this way, the assembler generates an error message.

理解起来非常抽象，可以这么说：imm是一个 32 位或 64 位的模式，由多个相同的“子块”（element）组成；每个子块是连续的 1（长度从 1 到 e-1），可以旋转一定的位数。

# 栈指针

aarch64是16字节对齐的，并且没有x86那样的负偏移寻址形式，对于汇编语句：

这是x86的写法
```asm
mov [rsp-8],0x4
```

到aarch64可以使用
```asm
sub sp,sp,#0x1f // 16字节对齐
mov X0,#4
str X0,[sp,#8] // 将x0寄存器的值写入到栈帧偏移8处
```

而且由于aarch64支持pre-indexed addressing和post-indexed addressing，这里也可以写为
```asm
mov X0,#4
str X0,[sp,#-16]
```

但是注意，x86的写法里面并不会修改rsp（栈指针）的值，而上面两个aarch64写法却会改变，所以还可以
```asm
mov X0,#4
sub X1,sp, #8
str x0,[x1]
```

## push/pop

AArch64没有push/pop指令，而是提供了stp/ldp的组合。

举个例子：

x86的写法
```asm
myfunction:
    push rbp
    mov rbp, rsp
    sub rsp, N         ; allocate stack space for locals

    ... function body ...

    mov rsp, rbp
    pop rbp
    ret

```

而在aarch64的写法：
```asm
myfunction:
    stp x29, x30, [sp, #-16]!   // push frame pointer (x29) & link register (x30)
    mov x29, sp                 // set new frame pointer
    sub sp, sp, #N              // allocate N bytes for locals (16-byte aligned)

    ... function body ...

    add sp, sp, #N              // deallocate locals
    ldp x29, x30, [sp], #16     // pop x29, x30
    ret
```

这里，x29寄存器是帧指针寄存器，x30是返回地址保存位置(aka Link Register)。

### Store/Load
AArch64的Store/Load机制很扯，比如说indexing-mode，str/ldr有Pre-index, Post-index以及Unsigned offset三种addressing mode，但是唯独没有不修改(no write back)有符号的版本。

以及在汇编写法里面也是用:

```asm
str x0, [x29,#-8]
```

这样的写法，这真的是坑人。

实际上只store/load而不修改寄存器的指令是stur/ldur。

### Prologue and Epilogue

这里有一个会踩坑的点，就是说：
```asm
mov X29, sp
```
这个指令是MOV TO/FROM SP，是一个add imm指令的alias，跟MOV imm, MOV register都没有关系。

## 跳转！

学了一段时间的汇编，我对于jmp或者说跳转有一些想法。

汇编器是在assembly文件给定的前提下进行翻译的，但是对于LuaJIT/Node这种具有JIT Compiler的解释器，生成二进制肯定不会利用汇编器再生成二进制，所以这里面是怎么进行bl/jmp的操作的呢？

### b
AArch64指令里面的branch指令，有一个26位的立即数，这个立即数表示的是相对跳转距离（PC-Relative）。

其编码规则可用python代码：

```python
def calc_b_imm26(pc_current, target):
    """
    计算 AArch64 B 指令的 imm26
    pc_current: 当前指令地址（字节）
    target: 目标 label 地址（字节）
    """
    pc = pc_current
    offset_bytes = target - pc if target > pc_current else pc - target
    # 偏移单位是 4 字节
    imm26 = offset_bytes // 4
    if target < pc_current:
        imm26_bin = 0x10000000 - imm26
    else:
        imm26_bin = imm26
    print(f"PC: 0x{pc_current:X}, Target: 0x{target:X}")
    print(f"Offset bytes: {offset_bytes} -> imm26: {imm26}")
    print(f"imm26 26-bit binary (hex) = 0x{imm26_bin:X}")
    return imm26, imm26_bin
```

对于b.cond，也是一样的思路。

b.cond系列指令里面的imm是19位，这个19位指令就会变成0x100000来作为基准。

有了这些东西，有什么用呢？

答案就是：现在就可以在编译器里面绕过Assembly直接生成二进制来进行跳转了，也就是调用其他Procesure！

## PCS

Arm官方对于过程调用是有限制的，可以搜索关键字Procedure Call Standard。

1. 参数传递通过X0-X7寄存器，更多的参数则通过栈来传递（C++中，X0寄存器被用来传递this指针）。
2. PCS规定了那些寄存器是可以修改的，而哪些则是必须保存并恢复的。
