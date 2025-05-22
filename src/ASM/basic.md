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
