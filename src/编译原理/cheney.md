# Cheney 垃圾回收算法

Cheney是一个"stop-and-copy"的tracing gc垃圾回收算法。

Cheney算法会把heap划分成两部分，第一部分是from，第二部分是to。

在一个时刻只有一个部分是处于使用中的，GC阶段通过把活跃的对象从一个半空间(from space)拷贝到另一个(to space)。

拷贝完成后，to space就办成了新的堆。这时候整个旧堆都会被抛弃掉。
