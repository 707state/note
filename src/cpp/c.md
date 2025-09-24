<!--toc:start-->
- [C语言并不是C++的一个子集！](#c语言并不是c的一个子集)
  - [标准库](#标准库)
    - [string.h](#stringh)
- [quirky stuff](#quirky-stuff)
  - [const](#const)
    - [常指针？指针常量？](#常指针指针常量)
<!--toc:end-->

# C语言并不是C++的一个子集！

## 标准库

C实际上是有一个规定的标准库的！

### string.h

strcspn: 用来计算字符串中“第一段不含某些字符的长度”。其参数为：

```c
size_t strcspn(const char *s, const char *reject);
```

返回值为：从 s 开头起，连续不包含任何 reject 中字符的那一段子串的长度。

# quirky stuff

## const

const作为一个修饰符，其语义是比较灵活的。

这里深入了解一下const究竟是怎么个事。

### 常指针？指针常量？

首先，C/C++中，一个具名的变量的表达式类型都是lvalue，换句话说，其在内存中一定有位置。

举一个例子：

```c
const int * p;
// 或者
int const* p;
```

这句话说的是：指向const int的指针p。

这里const修饰的是值而非地址，也就是说，对p dereference不能重新赋值。

```c
*p=3;//这句话就不合法，因为const修饰的内存是不可写的
//但是这里允许修改p指向的地址，即：
p=&b;// 这里b是另一个int类型的变量，这样做是合法的。
```


而另一种情况

```c
int a=1,b=2;
int * const p=&a;
```

这句话则是说的，p指向的地址不可变，即：

```c
*p=3;//这里合法，因为a的内存可写。
p=&b;//不合法，因为p指向的内容不可变
```

这两种情况都可以理解为const修饰的变量不可变，只不过一个是对于指针类型的变量，一个是对于非指针类型的变量，关键就在于dereference的意义。
