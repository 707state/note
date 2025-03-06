# 一种比Mod运算更快的映射

![原文地址](https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/)

并不是说，这样的实现：

```go
func fastModN(x, n uint32) uint32 {
    return uint32((uint64(x) * uint64(n)) >> 32)
}
```

就一定比常规的Mod Hash更优，而是在n为2的k次方时，上述代码会更具有优化潜力。

对于mod性能不够时，可以考虑到SIMD指令集。
