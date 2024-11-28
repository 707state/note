-   [无标准库编程](#无标准库编程)
    -   [core::slice](#coreslice)
        -   [from_raw_parts](#from_raw_parts)

# 无标准库编程

## core::slice

### from_raw_parts

与标准库的函数用法一直，接受一个指针和长度构成一个slice,
len代表元素个数而不是字节数,相当于变成一组序列。
