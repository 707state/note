# SystemTap

一个Dynamic Tracing工具，可以用于在系统运行时进行检查。

## 原理

1. 将stp脚本转换为解析树
2. 使用细化步骤中关于当前运行的内核的符号信息解析符号
3. 转换流程将解析树转换成C源代码并使用解析后的信息和tapset脚本
4. stap的最后步骤是构造使用本地内核模块构建进程的内核模块
5. 有了可用的内核模块之后，stap完成了自己的任务，用staprun和stapio负责模块的安装/卸载。

## 探测点

stap使用动态跟踪探测，所以第一步就是在语言中定义探测点。

```stp
probe event {statement}
```

这里的event分为两种：

1. 同步事件：发生在进程执行某一条确定的命令时的事件。
2. 异步事件：不是执行到指定的指令或代码位置，这一类包括定时器，计时器等。

## 同步事件

探测点由：


- 前缀部分，定义所在的模块：可以是内核，还可以是内核模块，还可以是用户进程，还可以是systemtap在tapset中预定义的探测点。
- 中间部分，定义所在的函数：函数可以通过函数名指定，也可以根据文件名:行号指定。
- 后缀部分，定义调用时机：可以在函数调用时触发，也可以在函数返回时触发。

### 所在的函数

所在的函数，可以通过两种方式指定：

    function(PATTERN)
    statement(PATTERN)

PATTERN由三部分组成：

    函数名（必填）。
    @文件名：选填。
    如果存在“@文件名”的情况下，还可以选填行号。

在这里，函数名这部分可以使用通配符（wildcarded）来定义文件的名字以及函数的名字，比如：

```stp
# 所有内核中以sys_前缀开头的函数
kernel.function("sys_*)

# nginx用户进程中名为ngx_http_process_*的函数
process("/home/admin/nginx/bin/nginx").function("ngx_http_process_*")
```

使用statement可以很方便定位到具体的某一行代码执行前后，变量的变化情况，比如下面这个最简单的C代码：

```c
#include <stdio.h>

int main(int argc, char *argv[])
{
	int a;

	a = 1;
	printf("a:%d\n", a);
	a = 2;
	printf("a:%d\n", a);
	return 0;
}
```

### 调用时机

有了以上两个要素，已经可以在具体的函数、文件行中定义探测点了，但是有时候针对某一个具体函数，想在不同的时机定义探测点，比如函数被调用和调用返回的时候，那么可以在后面以后缀的方式定义出来：

```stp
probe kernel.function("*@net/socket.c").call {
  printf ("%s -> %s\n", thread_indent(1), probefunc())
}
probe kernel.function("*@net/socket.c").return {
  printf ("%s <- %s\n", thread_indent(-1), probefunc())
}
```

## 异步事件

常见的异步事件是begin、end、never、timers。

    begin、end分别在脚本开始执行以及结束执行的时候被调用。
    timers用于定义定时器探测点，常见的格式timer.s(1)来定义每秒触发的探测点。
    never定义的探测点不会被调用到，很多时候加这个探测点只是为了检查一些语法错误。

```stp
// sudo stap begin.stp -T 2
// 输出：
// probe begin
// in timer
// probe end

probe begin {
  printf("probe begin\n")
}

probe end {
  printf("probe end\n")
}

probe timer.s(1) {
  printf("in timer\n")
}

probe never {
  printf("never do this\n")
}
```

# 变量

## 目标变量

目标变量指的是当前代码位置可见的变量。

可以使用-L命令行参数，拿到一个探测点的位置及相关的变量。

## 全局变量

如果不是在当前代码位置的变量，此时可以通过这种格式拿到：

```stp
@var("varname@src/file.c")
```

## 打印结构体内容

有一些变量，本身是一个结构体，如果想打印其成员信息，但是又不知道结构体的成员分布的情况，可以首先使用`$变量名$`，比如：

```bash
sudo stap -e 'probe kernel.function("vfs_read").return {printf("%s\n", $file$); exit(); }'
```

然后就会打印出结构体的成员信息。

而如果需要打印某个成员的信息，就可以使用->操作符，注意在systemtap中，无论是指针还是引用都使用->来查看成员。

## 类型转换

当指针为void*指针时，如果知道它的确切类型，可以通过类型转换来输出其信息：

```stp
@cast(p, "type_name"[, "module"])->member
```
