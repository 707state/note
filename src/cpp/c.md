<!--toc:start-->
- [C语言并不是C++的一个子集！](#c语言并不是c的一个子集)
  - [标准库](#标准库)
    - [string.h](#stringh)
- [quirky stuff](#quirky-stuff)
  - [const](#const)
    - [常指针？指针常量？](#常指针指针常量)
- [macro](#macro)
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

strchr: 在目标字符串中找到某一个字符，返回这个字符起始位置的指针。

```c
const char *str = "Try not. Do, or do not. There is no try.";
char target = 'T';
const char* result = str;
while((result = strchr(result, target)) != NULL){
    printf("Found '%c' starting at '%s'\n", target, result);
    ++result; // Increment result, otherwise we'll find target at the same location
}
```

### time.h

C标准库提供了一个clock函数，这是一个不精确的函数，精度非常差，不适合用于高精度的场景。

如果需要高精度的时间，需要使用操作系统的api。对于Linux/Unix-like的系统，可以用gettimeofday这个函数；对于Windows可以用QueryPerformanceCounter和QueryPerformanceFrequency这两个windows提供的api来获取。

如果想要在用户空间实现一个调度器，gettimeofday并不能依赖，Linux提供了clock\_gettime这组api。


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

# macro

宏是一个非常有意思的东西。

我们都知道C的宏本质上只是字符串替换，但其实是要更多一点东西的。

```c
// Prints the given object.
static void print(Obj *obj) {
  switch (obj->type) {
  case TCELL:
    printf("(");
    for (;;) {
      print(obj->car);
      if (obj->cdr == Nil)
        break;
      if (obj->cdr->type != TCELL) {
        printf(" . ");
        print(obj->cdr);
        break;
      }
      printf(" ");
      obj = obj->cdr;
    }
    printf(")");
    return;

#define CASE(type, ...)                                                        \
  case type:                                                                   \
    printf(__VA_ARGS__);                                                       \
    return
    CASE(TINT, "%d", obj->value);
    CASE(TSYMBOL, "%s", obj->name);
    CASE(TRATIONAL, "%d/%d", obj->numerator, obj->denominator);
    CASE(TPRIMITIVE, "<primitive>");
    CASE(TFUNCTION, "<function>");
    CASE(TMACRO, "<macro>");
    CASE(TMOVED, "<moved>");
    CASE(TTRUE, "t");
    CASE(TNIL, "()");
#undef CASE
  default:
    error("Bug: print: Unknown tag type: %d", obj->type);
  }
}
```

这里面的CASE这个宏就很取巧地处理了代码的复杂度。
