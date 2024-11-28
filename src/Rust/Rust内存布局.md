-   [常用类型](#常用类型)
    -   [array](#array)
    -   [str类型](#str类型)
    -   [slice](#slice)
    -   [&str与String](#str与string)
    -   [struct](#struct)
    -   [tuple](#tuple)
    -   [closure](#closure)
    -   [union](#union)
    -   [enum](#enum)
    -   [trait](#trait)
-   [DST: Dynamically Sized
    Types](#dst-dynamically-sized-types)
-   [ZST: Zero Sized
    Type](#zst-zero-sized-type)
-   [Empty Types](#empty-types)
-   [数据对齐](#数据对齐)
    -   [repr(C)](#reprc)
    -   [repr(u) repr(i)](#repru-repri)
    -   [repr(align(x))
        repr(pack(x))](#repralignx-reprpackx)
    -   [repr(transparent)](#reprtransparent)
-   [胖指针](#胖指针)

# 常用类型

类型的布局是其大小（size），对齐方式（align）以及字段的相对偏移量。

对于Sized的数据类型，可以在编译时知道内存布局，可以通过size_of和align_of获取其size和align。

## array

数组的内存布局为系统类型元组的有序组合。

## str类型

str与\[u8\]一样表示一个u8的slice,
Rust标准库对str有个假设：符合UTF-8编码。内存布局与\[u8\]相同。

## slice

slice 是 DST 类型，是类型 T 序列的一种视图。 slice
的使用必须要通过指针，&\[T\]
是一个胖指针，保存指向数据的地址和元素个数。 slice 的内存布局与其指向的
array 部分相同。

## &str与String

         my_name: String   last_name: &str
            [––––––––––––]    [–––––––]
            +–––+––––+––––+–––+–––+–––+

stack frame │ • │ 16 │ 13 │ │ • │ 6 │
+--│--+--------+--------+------+--│--+------+ │ │ │ +------------------+
│ │ │ │ │ \[--│-------------- str ------------------\]
+--V--+------+------+------+------+------+------+--V--+------+------+------+------+------+------+------+------+
heap │ P │ a │ s │ c │ a │ l │ │ P │ r │ e │ c │ h │ t │ │ │ │
+------+------+------+------+------+------+------+------+------+------+------+------+------+------+------+------+

## struct

## tuple

元组是匿名的复合类型，有以下几种 tuple：

() (unit)

(f64, f64)

(String, i32)

(i32, String) (different type from the previous example)

(i32, f64, Vec`<String>`{=html}, Option`<bool>`{=html})

tuple 的结构和 Struct 一致，只是元素是通过 index 进行访问的。

## closure

闭包相当于一个捕获变量的结构体，实现了 FnOnce 或 FnMut 或 Fn。

``` rs

#![allow(unused)]
fn main() {
fn f<F : FnOnce() -> String> (g: F) {
    println!("{}", g());
}

let mut s = String::from("foo");
let t = String::from("bar");

f(|| {
    s += &t;
    s
});
// Prints "foobar".
}
```

生成了一个闭包类型：

``` rs

#![allow(unused)]
fn main() {
struct Closure<'a> {
    s : String,
    t : &'a String,
}

impl<'a> FnOnce<()> for Closure<'a> {
    type Output = String;
    fn call_once(self) -> String {
        self.s += &*self.t;
        self.s
    }
}
f(Closure{s: s, t: &t});
}
```

## union

union 的关键特性是 union 的所有字段共享公共存储。因此，对 union
的一个字段的写入可以覆盖其其他字段，union 的大小由其最大字段的大小决定。

每个 union
访问都只是在用于访问的字段的类型上解释存储。读取并集字段读取字段类型处的并集位。字段可能具有非零偏移量（除非使用C表示法）；在这种情况下，从字段偏移量开始的位被读取。程序员有责任确保数据在字段的类型上是有效的。否则会导致未定义的行为。比如读取整数
3，但是需要转换为 bool 类型，则会出错。

## enum

枚举项声明类型和许多变体，每个变体都独立命名，并且具有struct、tuple
struct或unit-like struct的语法。 enum
是带命名的标签联合体，因此其值消耗的内存是对应枚举类型的最大变量的内存，以及存储判别式所需的大小。

## trait

trait obj 是 DST 类型，指向 trait obj 的指针也是个胖纸针，分别指向 data
和 vtable。

# DST: Dynamically Sized Types

一般来说大多数类型，可以在编译阶段确定大小和对齐属性，Sized trait
就是保证了这种特性。非 size (?Sized）及 DST 类型。DST 类型有 slice 和
trait obj。DST 类型必须通过指针来使用。 需要注意：

1.  DST 可以作为泛型参数，但是需要注意泛型参数默认是 Sized，如果是 DST
    类型需要特别的指定为 ?Sized。

2.  trait 默认实现了 ?Sized.

3.  结构体实际上可以直接存储一个DST作为它们的最后一个成员字段，但这也使该结构体成为DST。可以参考DST
    进一步了解自定义 DST。

# ZST: Zero Sized Type

ZST 的一个最极端的例子是 Set 和 Map。已经有了类型 Map\<Key,
Value\>，那么要实现 Set\<Key, Value\>的通常做法是简单封装一个 Map\<Key,
UselessJunk\>。很多语言不得不给 UselessJunk
分配空间，还要存储、加载它，然后再什么都不做直接丢弃它。编译器很难判断出这些行为实际是不必要的。
但是在 Rust 里，我们可以直接认为 Set`<Key>`{=html} = Map\<Key,
()\>。Rust
静态地知道所有加载和存储操作都毫无用处，也不会真的分配空间。结果就是，这段范型代码直接就是
HashSet 的一种实现，不需要 HashMap 对值做什么多余的处理。

# Empty Types

空类型的一个主要应用场景是在类型层面声明不可到达性。假如，一个 API
一般需要返回一个
Result，但是在特殊情况下它是绝对不会运行失败的。这种情况下将返回值设为
Result\<T, Void\>，API 的调用者就可以信心十足地使用
unwrap，因为不可能产生一个 Void 类型的值，所以返回值不可能是一个 Err。

# 数据对齐

数据对齐对 CPU 操作及缓存都有较大的好处。Rust
中结构体的对齐属性等于它所有成员的对齐属性中最大的那个。Rust
会在必要的位置填充空白数据，以保证每一个成员都正确地对齐，同时整个类型的尺寸是对齐属性的整数倍。

## repr(C)

repr(C) 目的很简单，就是为了内存布局和 C 保持一致。需要通过 FFI
交互的类型都应该有
repr(C)。而且如果我们要在数据布局方面玩一些花活的话，比如把数据重新解析成另一种类型，repr(C)
也是很有必要的。

## repr(u) repr(i)

这两个可以指定无成员枚举的大小。包括：u8, u16, u32, u64, u128, usize,
i8, i16, i32, i64, i128, and isize.

## repr(align(x)) repr(pack(x))

align 和 packed 修饰符可分别用于提高或降低结构和联合的对齐。packed
还可能改变字段之间的填充。 align
启用了一些技巧，比如确保数组的相邻元素之间永远不会共享同一缓存线（这可能会加速某些类型的并发代码）。
pack 不能轻易使用。除非有极端的要求，否则不应使用。

## repr(transparent)

repr(transparent) 使用在只有单个 field 的 struct 或 enum 上，旨在告诉
Rust 编译器新的类型只是在 Rust 中使用，新的类型（struc 或 enum）需要被
ABI 忽略。新的类型的内存布局应该当做单个 field 处理。

# 胖指针

胖指针（fat pointer）是指向动态大小类型（DST，例如切片或 trait
对象）的指针。胖指针不仅包含指向数据的指针，还包含额外的信息，例如大小或类型信息。

胖指针是指针类型的一种特殊指针：

1.  指向静态sized类型值的常规指针；对于数组\[i32; 3\]而言，& \[i32;
    3\]就是常规指针；

2.  指向动态sized类型值的胖指针：使用的内存空间是常规指针所使用的内存空间的两倍，切片(&
    \[i32\])这种指针是胖指针(fat
    pointer)；trait对象和切片变量的类型其实是胖指针类型！
