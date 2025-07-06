<!--toc:start-->
- [Go基本知识](#go基本知识)
  - [赋值](#赋值)
    - [变量的生命周期](#变量的生命周期)
    - [可赋值性](#可赋值性)
  - [声明](#声明)
- [基本数据](#基本数据)
- [复合数据类型](#复合数据类型)
  - [slice](#slice)
  - [结构体字面量](#结构体字面量)
  - [结构体嵌套](#结构体嵌套)
- [goroutine](#goroutine)
- [通道](#通道)
<!--toc:end-->

# Go基本知识

Golang原生支持Unicode字符。

只有在一行内放入多个表达式时才会需要分号，其他时候不需要分号。

命令行参数通过os包提供，os.Args就给出了命令行arguments。

go只提供for用于进行循环，并且可以用for condition的方式（类似于while）。

bufio包可以用与简便的处理输入/输出。例如：

```go
input:=bufio.NewScanner(os.Stdin)
```

map是一个使用make创建的数据结构的引用。当一个map被传递给一个函数时，函数接收到这个引用的副本，所以被调用函数中对于map数据结构中的改变对函数调用者使用的map引用也是可见的。换句话说，map使用的是引用语义。

## 赋值

var name type=expression是通用的形式，但是:=可以简化较短的局部变量的赋值。

new也可以创建变量，但是不需要引入（和生命）一个虚拟的名字，可以直接用new(Type)的形式。

以下两端代码作用是一致的：

```go
func newInt() *int{
    return new(int)
}
```

```go
func newInt() *int{
    var dummy int
    return &dummy
}
```

换句话说，go并不遵循C++/Rust的RAII风格。

### 变量的生命周期

包级别变量的生命周期是整个程序的执行时间，局部变量有动态的生命周期。

每一个包级别的变量或者当前执行函数的局部变量，可以作为追溯该变量的路径的源头，通过指针和其他方式的引用可以找到变量。如果变量的路径不存在，那么变量就会变得不可访问。

同时，栈上/堆上分配并不依赖于声明方式，而是与变量逃逸有关：

```go
var global *int
func f(){
    var x int
    x=1
    global=&x
}
func g(){
    y:=new(int)
    *y=1
}
```

这段代码中，x一定使用堆空间，因为f函数返回后还可以从global变量访问。而g函数返回后，*y就不可访问，所以编译器可以安全地在栈上分配y。

### 可赋值性

根据类型不同也具有不同的赋值规则，类似于C++的构造函数或者Rust的new。

规则很简单，就是类型必须匹配，nil可以被付给任意变量。

两个值使用==或者!=进行比较与可赋值性相关：任何比较中，第一个操作数相对于第二个操作数的类型必须是可赋值的。

## 声明

类型的声明通常出现在包级别，这里命名的类型在整个包中可见，如果名字是导出的（也就是大写），其他的包也可以访问他。

# 基本数据

Go中默认编码全都为UTF-8，是一种前缀编码。

具有这样的特性：

判断一个字符串是否为另一个的前缀而无需解码
```go
func HasPrefix(s,prefix string)bool{
    return len(s)>=len(prefix) && s[:len(s)-len(prefix)]==prefix
}
```

判断后缀
```go
func HasSuffix(s,suffix string) bool{
    return len(s)>=len(suffix) && s[len(s)-len(suffix):]==suffix
}
```

# 复合数据类型

数组和结构体都是复合数据类型。

注意，数组是具有固定长度的具有相同数据类型元素的序列，并不能扩容。

## slice

slice可以用来访问数据的部分/全部元素，这个数组被称为Slice的底层数组。

slice有三个属性：指针、长度和容量。

slice在初始化的时候并不会指定长度，并且slice无法进行比较。

检查slice是否为空使用的是len(s)==0而不是s==nil。

slice可以使用make([]T,len)的形式来创建。

或者make([]T,len,cap)。

append函数可以把元素追加到slice后面。

## 结构体字面量

```go
type Point struct {
    X,Y int
}
```

在实现方法时要注意：

- 值接收者：值和指针都能实现接口，推荐用于不会修改结构体数据的方法。
- 指针接收者：只有指针能实现接口，适用于大结构体或需要修改内部状态的方法。

## 结构体嵌套

Go允许我们定义不带名称的结构体成员，只需要指定类型即可。这种成员叫做匿名成员。这个结构体成员的类型必须是一个命名类型或者指向命名类型的指针。

# goroutine

go中每一个并发执行的活动称为goroutine。

# 通道

通道分为无缓冲通道和有缓冲的两种。

无缓冲通道上的发送操作将会阻塞，直到另一个goroutine在对应的通道上执行接收操作，这时候值传送完成，两个goroutine可以继续执行。相反，如果接受操作先执行，就阻塞接收方的goroutine，直到另一个goroutine在同一个通道上发送一个值。

# defer

defer 语句中的参数在 defer 语句执行时就被求值，而不是在延迟函数实际执行。

```go
func example() {
    i := 0
    defer fmt.Println(i) // 这里 i 的值是 0
    i++
    return
}
// 输出：0（不是 1）
```
