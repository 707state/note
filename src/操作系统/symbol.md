# 链接？

在知道链接是怎么回事之前，需要对另一个东西更熟悉！

那就是文件格式。这里的文件格式指的是ELF/Mach-O，至于Windows的COFF暂不考虑。

## 导出符号？

汇编只需要知道偏移距离就好了，动态链接器要处理的就多了。

比如说：
```asm
bl _printf
```

这里说的是跳转到\_printf函数的段，假定这里的\_printf函数说的是libc的标准库函数，assembler是怎么知道他的地址呢？

答案是不知道！

请看二进制：
```txt
0000000000000024        bl      0x24
```

这段代码是otool -tV的结果，可以看到bl的那个imm26部分居然到了自己这里来，这岂不是倒反天罡？

这说明_printf的地址是未知的，as对此无能为力，可以用nm -g看一下：
```txt
                 U _exit
0000000000000000 T _main
                 U _printf
                 U _puts
                 U _strlen
```

这里面U就说明UNDEFINED，即符号不存在于当前Object文件中。

那这里是怎么被找到的呢？还是回到Object文件来。

这里还是以[Mach-O格式为例](https://alexdremov.me/mystery-of-mach-o-object-file-builders/)。

可以看到Mach-O文件有Symbols Table和Dynamic Symbols Table这两个东西。前者是记录所有符号（不管是已定义的还是未定义的）信息，位置由LC_SYMTAB指定；后者则是专门为动态链接提供符号分布和重定位信息，Dysymtab需要告诉dyld这些符号在Symbols Table中的位置和数量。

上面的信息可以用:
```sh
otool -l | grep LC_SYMTAB或者LC_DYSYMTAB来查看
```

怎么看到更具体的动态链接表的信息呢？可以用otool -r来看。

这里我选择使用[MachOView](https://github.com/gdbinit/MachOView)这个软件来看。

### 深入解析符号规则！

先来看一段汇编：

```asm
        .text
        .globl _main

_main:
        // 调用 puts("Hello from puts!")
        adrp    x0, msg_puts@PAGE
        add     x0, x0, msg_puts@PAGEOFF
        bl      _puts

        adrp    x0, msg_strlen@PAGE
        add     x0, x0, msg_strlen@PAGEOFF
        bl      _strlen

        // 将 strlen 的返回值传给 printf
        mov     x1, x0
        adrp    x0, fmt_len@PAGE
        add     x0, x0, fmt_len@PAGEOFF
        bl      _printf

        // 最终 exit(0)
        mov     x0, #0
        bl      _exit

        .section __TEXT,__cstring
msg_puts:
        .asciz "Hello from puts!\n"

msg_strlen:
        .asciz "ABCDEFG"

fmt_len:
        .asciz "strlen returned %d\n"

```

这段汇编并不包含多少信息，值得注意的东西有：1. 这里并没有写.external标记就调用了\_printf等libc函数 2. msg\_puts@PAGE这是什么。

搞懂这两个信息就可以解读差不多理解Mach-O了（但是还有一些东西，比如说Relocations的每一项里面，Addend是什么呢）。

## 保护现场

函数调用会带来寄存器的修改，aarch64或者其他架构都是对易失性寄存器有规定的。

在aarch64中，x29被用作frame pointer，x30被用作stack pointer。
