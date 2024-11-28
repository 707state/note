-   [使用Scheduler进行线程调度](#使用scheduler进行线程调度)
    -   [Scheduler类型](#scheduler类型)
        -   [computation](#computation)
        -   [newThread](#newthread)
        -   [io](#io)
        -   [immediate](#immediate)
        -   [trampoline](#trampoline)
        -   [from](#from)

# 使用Scheduler进行线程调度

## Scheduler类型

### computation

适用于CPU相关的任务，不适合会造成阻塞的任务。

### newThread

每一次都会新建一个线程，适合工作时间长并且总数少的任务。

### io

io Scheduler可以被回收利用，内部会维持一个线程池，当io
Scheduler用于处理任务时，首先会在线程池中寻找空闲线程，否则就创建一个新的线程，并放入池中。

io Scheduler一般用于IO密集型任务。

### immediate

立即执行的任务，用Inner表示用immediate Scheduler来执行的人物。

### trampoline

等待当前线程上之前的任务都执行结束之后再开始执行的任务。

### from

一个工厂方法，可以自己定义Scheduler。
