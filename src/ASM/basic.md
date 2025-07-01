# 基本格式

ARM汇编基本格式：
label: opcode operands

- 1号系统调用是exit，4为write，使用方法是设置对应的寄存器后通过自陷的方式svc 0x80来转为内核态。

# 内存与寄存器

MOV系列的指令。
