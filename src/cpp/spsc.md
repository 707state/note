# Lock-Free Queue in C++

实现一个简单的Lock-Free queue在C++里面可以非常简单，因为完全可以用mutex来实现。

问题在于mutex这个东西依赖于原子指令，往往效率会比较一般。

## Mutex原理

mutex基于两个原子操作(memory barrier)来实现：

1. acquire(m): 当m被锁上时阻塞；
2. release(m): 释放m的锁。

这两个原子操作又是基于硬件的原子指令来实现的，比如说Test-and-Set, Compare-and-Swap, Fetch-and-Add等等，有些指令集上还可以使用load-linked/store-conditinal指令来实现(ll/sc指令可以避免ABA问题)。

## 内存屏障

这里需要讲讲acquire, release的作用。

- acquire: 获取屏障确保屏障之后的所有读取/写入操作不会重排到屏障之前。
- release: 如果屏障之后的任何写入对其他核心可见，那么屏障之前的所有写入也应该对其他核心可见——前提是其他核心在读取屏障后写入的数据之后执行了读取屏障。

例子：
线程A：写数据 → 释放屏障 → 写标志位
线程B：读标志位 → 获取屏障 → 读数据
保证：如果线程B看到了新的标志位，那么它也能看到之前写入的所有数据


