<!--toc:start-->
- [prctl](#prctl)
  - [功能](#功能)
  - [应用场景](#应用场景)
  - [样例](#样例)
<!--toc:end-->

# prctl
Linux下的prctl系统调用提供了一种检查和修改进程或线程特定属性的机制。这是一个功能强大且多用途的接口，允许程序员精细控制进程行为。

原型:
```c
#include <sys/prctl.h>

int prctl(int option, unsigned long arg2, unsigned long arg3,
          unsigned long arg4, unsigned long arg5);
```

## 功能
1. 进程名称管理
2. 父进程终止通知
3. 安全计算模式(Secure Computing)
4. 核心转储控制
5. 子进程收割者(Subreaper)设置
6. 特权控制

## 应用场景

1. 容器和沙箱技术：容器运行时和安全沙箱广泛使用prctl控制进程能力和系统调用权限
2. 调试和监控工具：自定义进程名以便识别，控制核心转储行为
3. 守护进程和服务：使用子进程收割者功能确保不会留下孤儿进程
4. 安全敏感应用：限制进程可执行的系统调用，减小攻击面

Linux中每个线程都有自己独立的名称属性，当你调用prctl(PR_SET_NAME, ...)时，你只修改了执行该调用的线程的名称。这是Linux线程实现的一个重要特性，因为在Linux中线程实际上是由轻量级进程(LWP)实现的。



## 样例
```c++
#include <sys/prctl.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
int main() {
    // 设置进程名
    prctl(PR_SET_NAME, "restricted-app", 0, 0, 0);
    // 父进程死亡时自动终止
    prctl(PR_SET_PDEATHSIG, SIGTERM, 0, 0, 0);
    // 禁止获取新特权
    prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    printf("进程已设置受限模式\n");
    // 应用主循环...
    while(1) {
        sleep(1);
    }
    return 0;
}
```
