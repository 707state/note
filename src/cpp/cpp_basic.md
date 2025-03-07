<!--toc:start-->
- [重读\<\<C++程序设计语言\>\>](#重读c程序设计语言)
  - [41 章并发](#41-章并发)
    - [P280](#p280)
- [语言标准](#语言标准)
  - [concept](#concept)
  - [std::ref](#stdref)
  - [std::cref](#stdcref)
  - [std::ignore](#stdignore)
  - [std::move_only_function::move_only_function](#stdmoveonlyfunctionmoveonlyfunction)
  - [std::atomic](#stdatomic)
    - [基本操作](#基本操作)
    - [内存顺序](#内存顺序)
  - [std::chrono](#stdchrono)
  - [std::recursive_mutex](#stdrecursivemutex)
  - [std::chrono::high_resolution_clock](#stdchronohighresolutionclock)
  - [cxxabi abi::\_\_cxa_demangle()](#cxxabi-abicxademangle)
  - [std::bitset](#stdbitset)
  - [std::common_type](#stdcommontype)
- [一些库](#一些库)
  - [测试](#测试)
    - [doctest](#doctest)
    - [gtest](#gtest)
  - [rpc](#rpc)
    - [json-rpc-cxx](#json-rpc-cxx)
  - [neither](#neither)
- [什么是多态？](#什么是多态)
  - [Ad-hoc Polymorphism 特设多态](#ad-hoc-polymorphism-特设多态)
  - [Parametric Polymorphism 参数多态](#parametric-polymorphism-参数多态)
  - [Subtype Polymorphism 子类型多态](#subtype-polymorphism-子类型多态)
  - [Row Polymorphism（行多态）](#row-polymorphism行多态)
- [语言标准](#语言标准)
  - [for co_await](#for-coawait)
  - [字符串字面量](#字符串字面量)
  - [指针与 const](#指针与-const)
  - [右值引用](#右值引用)
  - [static](#static)
  - [类型双关](#类型双关)
  - [Dependent Names](#dependent-names)
    - [Binding Rule](#binding-rule)
    - [查找规则 (Lookup Rule)](#查找规则-lookup-rule)
    - [Two Phase Name Lookup](#two-phase-name-lookup)
    - [为什么需要两阶段查找？](#为什么需要两阶段查找)
  - [extern template (C++ 11)](#extern-template-c-11)
  - [ODR 与翻译单元](#odr-与翻译单元)
  - [虚函数一定是运行期才绑定的吗？](#虚函数一定是运行期才绑定的吗)
    - [final](#final)
  - [虚函数究竟在哪些地方变慢了？](#虚函数究竟在哪些地方变慢了)
  - [构造函数默认使用浅拷贝](#构造函数默认使用浅拷贝)
- [杂项](#杂项)
  - [forward declaration](#forward-declaration)
  - [std::swap VS xor](#stdswap-vs-xor)
  - [cbrt](#cbrt)
  - [sendto](#sendto)
  - [__builtin_ffs](#builtinffs)
<!--toc:end-->


# 重读\<\<C++程序设计语言\>\>

注：由于是重读，这里大多是一些拾遗。

采用格式：章节+页码+内容

## 41 章并发

### P280

\"还请注意，只要你不向其他线程传递局部数据的指针，你的局部数据就不存在这里讨论的诸多问题。\"

# 语言标准

## concept

例子：

```cpp
template <typename Lock>
concept is_lockable=requires(Lock &&lock){
  lock.lock();
  lock.unlock();
  {lock.try_lock()}->std::convertible_to<bool>;
};
```

is_lockable 概念可以用来限制模板，使得只有那些具备 lock()、unlock() 和
try_lock() 成员函数，并且 try_lock() 返回类型可以转换为 bool
的类型才可以作为模板参数传递。这个概念通常用于多线程编程中，以确保模板只接受那些具有锁定功能的类型，比如
std::mutex 或 std::recursive_mutex。


## std::ref

std::ref 相当于告诉编译器这里要使用引用而不是按值传递，比如 boost::asio::context 就需要按引用。

std::ref 的作用是显式地告诉编译器传递引用，而不是尝试通过值传递。

asio::io_context的拷贝构造函数被删除，不能被拷贝，只能通过引用或指针传递。

通过 std::ref，你成功避免了编译器试图调用已删除的拷贝构造函数。

## std::cref

用于生成一个 std::reference_wrapper`<const T>` 对象，类似于
std::ref，但它为常量对象创建引用。

std::cref 允许你创建对 const 类型对象的引用包装器。

这个包装器可以用于那些期望传递对象而实际上需要传递常量引用的场景。

## std::ignore

可用来忽略返回值，cppref 解释：

1.  An object such that any value can be assigned to it with no effect.
2.  The type of std::ignore.

```cpp
#include <iostream>
#include <set>
#include <string>
#include <tuple>

[[nodiscard]] int dontIgnoreMe()
{
    return 42;
}

int main()
{
    std::ignore = dontIgnoreMe();

    std::set<std::string> set_of_str;
    if (bool inserted{false};
        std::tie(std::ignore, inserted) = set_of_str.insert("Test"),
        inserted)
        std::cout << "Value was inserted successfully.\n";
}
```

## std::move_only_function::move_only_function

## std::atomic

C++标准库中提供的原子类型模板，允许你对某些基本数据类型进行原子操作，确保在多线程环境中对这些变量的操作不会产生竞争条件（race
condition）。std::atomic 提供了一系列原子操作，避免了对锁（如
std::mutex）的依赖，从而提高了并发性能。

### 基本操作

- load()：读取变量的值。
- store()：存储一个新值。
- exchange()：交换变量值并返回旧值。
- fetch_add() 和 fetch_sub()：原子加减操作。
- compare_exchange_strong() 和 compare_exchange_weak()：比较并交换值。

> compare_exchange_strong 和 compare_exchange_weak
> 用于原子的比较并交换操作。它们的用途是当且仅当原子的当前值与给定的预期值相等时，才更新为新值。否则，不做更新。

### 内存顺序

std::atomic
操作可以指定内存顺序，以控制操作的内存可见性。这些顺序包括：\
std::memory_order_relaxed：无同步或顺序保证。\
std::memory_order_acquire：确保此操作之前的所有读操作都不会被重排序。\
std::memory_order_release：确保此操作之后的所有写操作都不会被重排序。\
std::memory_order_acq_rel：结合了 acquire 和 release 的保证。\
std::memory_order_seq_cst：最强的内存顺序，提供全局顺序保证。

## std::chrono

获取当前时间点：system_clock::now() 返回当前的时间点，类型为
std::chrono::system_clock::time_point。

计算时间差：time_since_epoch()
返回自系统纪元以来到当前时间点的持续时间，类型为
system_clock::duration，表示的时长单位可能是秒、毫秒、微秒等。

时间单位转换：duration_cast`<microseconds>`{=html}(d) 将时间差 d
转换为以微秒为单位的持续时间。duration_cast
是一个模板函数，用于将时间持续时间转换为另一种时间单位。

获取微秒数：.count() 返回以 microseconds 表示的数值。

## std::recursive_mutex

允许同一线程多次对互斥量加锁而不会导致死锁。它的主要用途是在递归函数或涉及到多次调用同一资源锁定的场景中。

典型用途：

递归函数中需要加锁的场景：
如果一个递归函数需要对某个共享资源进行加锁，而递归调用过程中又需要再次加锁，这时使用普通的
std::mutex 会导致死锁，而使用 std::recursive_mutex 则可以避免这种情况。

对象方法互相调用的场景：
如果类的多个方法之间相互调用，并且这些方法都需要对某些共享资源进行加锁，使用
std::recursive_mutex
可以避免同一线程在调用链中多次锁定同一互斥量时出现死锁。

## std::chrono::high_resolution_clock

std::chrono::high_resolution_clock 是 C++ 标准库中的一个时钟类型，定义在
`<chrono>`
头文件中。它用于测量时间点，并提供高分辨率的时间精度，通常用于精确的时间测量，比如计算代码执行时间、性能分析等。

特点：

高分辨率：它通常提供比 system_clock
更高的精度，可以用于测量短时间间隔，如纳秒或微秒级的时间。

非系统时钟：high_resolution_clock
不是一个与系统时间同步的时钟，因此它通常不用于显示当前日期和时间，而是用于测量时间间隔。

实现细节：在不同的系统上，high_resolution_clock
可能映射到不同的实现。例如：

在某些实现中，它可能是
steady_clock，这意味着它不会受到系统时间调整的影响（如系统时间更新或闰秒）。

在其他实现中，它可能是 system_clock 的高精度版本。

用途：high_resolution_clock
常用于需要高精度的性能分析和时间测量。例如，测量一段代码执行的时间、计算函数的延迟等。

## cxxabi abi::\_\_cxa_demangle()

abi::\_\_cxa_demangle() 是一个 GCC 和 Clang 提供的函数，用于将 C++
编译器修饰过的名称（mangled name）还原为可读的、未修饰的函数名。

如果 abi::\_\_cxa_demangle() 成功，还原后的名称会存储在返回的指针 v 中。

如果解析成功，函数将这个可读的函数名返回；否则返回原始字符串。

## std::bitset

bitset 存储二进制数位。

bitset 就像一个 bool 类型的数组一样，但是有空间优化------bitset 中的一个元素一般只占 1
bit，相当于一个 char 元素所占空间的八分之一。

bitset 中的每个元素都能单独被访问，例如对于一个叫做 foo 的 bitset，表达式 foo\[3\]访问了它的第 4 个元素，就像数组一样。

bitset 有一个特性：整数类型和布尔数组都能转化成 bitset。

bitset 的大小在编译时就需要确定。如果你想要不确定长度的 bitset，请使用（奇葩的）vector`<bool>`。

## std::common_type

确定多个类型之间的公共类型。也就是给定类型之间转换结果最合适的类型。

规则：

1. 如果所有类型 T1, T2, ..., Tn 是同一种类型，那么公共类型就是该类型。

2. 如果可以找到一个类型 T，使得所有 T1, T2, ..., Tn 都可以隐式转换为 T，那么公共类型是 T。

3. 如果前两条规则都失败，并且用户没有为类型定义隐式转换或者重载操作符，std::common_type 将导致编译错误。

# 一些库

## 测试

### doctest

single-header, 开箱即用

### gtest

功能丰富

## rpc

### json-rpc-cxx

学习 ing, 功能相对简单，依赖简单。

## neither

一个基于 Either Type 的函数式库，设计思路有点意思。

# 什么是多态？

不针对 C++，而是一种类型论的概念。

## Ad-hoc Polymorphism 特设多态

特设多态通过函数重载和运算符重载来实现，同一函数或运算符在不同的类型上有不同的行为。

C++中的运算符重载/函数重载，Haskell，都有这个机制。

## Parametric Polymorphism 参数多态

参数多态允许函数或数据类型对任何类型的参数进行操作，而不依赖于具体的类型。在参数多态中，类型是参数化的，常用于泛型编程。

经典：C++的模板特殊化这样的类型多态（type polymorphism）表面上类似于参数多态并同时引入了特设多态。

## Subtype Polymorphism 子类型多态

子类型多态，也称为包含多态，是指对象的某种子类型可以替代父类型使用。这通常涉及继承和接口，允许基于父类的接口调用子类的实现。

有点像里氏替换原则。

### C++虚函数原理

c++作为面向对象的语言，主要有三大特性：继承、封装、多态。关于多态，简而言之就是用父类型的指针指向其子类的实例，然后通过父类的指针调用实际子类的成员函数。这种技术可以让父类的指针有“多种形态”，这是一种泛型技术。所谓泛型技术，说白了就是试图使用不变的代码来实现可变的算法。比如：模板技术，RTTI技术，虚函数技术，要么是试图做到在编译时绑定，要么试图做到运行时绑定。因此C++的多态分为静态多态（编译时多态）和动态多态（运行时多态）两大类。静态多态通过重载、模板来实现；动态多态就是通过本文的主角虚函数来体现的。

#### 虚函数的内存分布

对C++ 了解的人都应该知道虚函数（Virtual Function）是通过一张虚函数表（Virtual Table）来实现的。简称为V-Table。在这个表中，主是要一个类的虚函数的地址表，这张表解决了继承、覆盖的问题，保证其容真实反应实际的函数。这样，在有虚函数的类的实例中这个表被分配在了这个实例的内存中，所以，当我们用父类的指针来操作一个子类的时候，这张虚函数表就显得由为重要了，它就像一个地图一样，指明了实际所应该调用的函数。因此有必要知道虚函数在内存中的分布。

这样的一个类：

```cpp
class A {
  public:
    virtual void v_a(){}
    virtual ~A(){}
    int64_t _m_a;
};

int main(){
    A* a = new A();
    return 0;
}

```

如以上代码所示，在C++中定义一个对象 A，那么在内存中的分布大概是如下图这个样子。

- 首先在主函数的栈帧上有一个 A 类型的指针指向堆里面分配好的对象 A 实例。
- 对象 A 实例的头部是一个 vtable 指针，紧接着是 A 对象按照声明顺序排列的成员变量。（当我们创建一个对象时，便可以通过实例对象的地址，得到该实例的虚函数表，从而获取其函数指针。）
- vtable 指针指向的是代码段中的 A 类型的虚函数表中的第一个虚函数起始地址。
- 虚函数表的结构其实是有一个头部的，叫做 vtable_prefix ，紧接着是按照声明顺序排列的虚函数。
- 注意到这里有两个虚析构函数，因为对象有两种构造方式，栈构造和堆构造，所以对应的，对象会有两种析构方式，其中堆上对象的析构和栈上对象的析构不同之处在于，栈内存的析构不需要执行 delete 函数，会自动被回收。
- typeinfo 存储着 A 的类基础信息，包括父类与类名称，C++关键字 typeid 返回的就是这个对象。
- typeinfo 也是一个类，对于没有父类的 A 来说，当前 tinfo 是 class_type_info 类型的，从虚函数指针指向的vtable 起始位置可以看出。

![C++内存分布](../image/cpp_memory.png)

Virtual table（虚表）只实现了虚拟函数的一半机制,如果只有这些是没有用的。只有用某种方法指出每个对象对应的 vtbl 时,它们才能使用。这是 virtual table pointer 的工作,它来建立这种联系。

每个声明了虚函数的对象都带有它,它是一个看不见的数据成员,指向对应类的virtual table。这个看不见的数据成员也称为 vptr,被编译器加在对象里,位置只有才编译器知道。

#### 虚函数实现原理

##### 调用过程

调用一个虚函数时，首先通过对象内存中的vptr找到虚函数表vtbl，接着通过vtbl找到对应虚函数的实现区域并进行调用。其中被执行的代码必须和调用函数的对象的动态类型相一致。编译器需要做的就是如何高效的实现提供这种特性。不同编译器实现细节也不相同。大多数编译器通过虚表vtbl（virtual table）和虚表指针vptr（virtual table pointer）来实现的。 当一个类声明了虚函数或者继承了虚函数，这个类就会有自己的vtbl。vtbl核心就是一个函数指针数组，有的编译器用的是链表，不过方法都是差不多。vtbl数组中的每一个元素对应一个函数指针指向该类的一个虚函数，同时该类的每一个对象都会包含一个vptr，vptr指向该vtbl的地址。 在有继承关系时(子类相对于其直接父类)

- 一般继承时，子类的虚函数表中先将父类虚函数放在前，再放自己的虚函数指针。
- 如果子类覆盖了父类的虚函数，将被放到了虚表中原来父类虚函数的位置。
- 在多继承的情况下，每个父类都有自己的虚表，子类的成员函数被放到了第一个父类的表中。，也就是说当类在多重继承中时，其实例对象的内存结构并不只记录一个虚函数表指针。基类中有几个存在虚函数，则子类就会保存几个虚函数表指针

例如：

```cpp
class A{
private:
    uint64_t a;
public:
    virtual void A_a(){std::cout << __func__;}
};
class C{
private:
    uint64_t c;
public:
    virtual void C_a(){std::cout << __func__;}
};

class D:public A,public C{
private:
    uint64_t d;
public:
    virtual void D_a(){std::cout << __func__;}
};

```

![上述代码的内存分布](../image/cpp_inheritance.png)

##### 调用性能从前面虚函数的调用过程可知。当调用虚函数时过程如下（引自More Effective C++）:

- 通过对象的 vptr 找到类的 vtbl。

    这是一个简单的操作,因为编译器知道在对象内 哪里能找到 vptr(毕竟是由编译器放置的它们)。因此这个代价只是一个偏移调整(以得到 vptr)和一个指针的间接寻址(以得到 vtbl)。

- 找到对应 vtbl 内的指向被调用函数的指针。
    这也是很简单的, 因为编译器为每个虚函数在 vtbl 内分配了一个唯一的索引。这步的代价只是在 vtbl 数组内的一个偏移。

- 调用第二步找到的的指针所指向的函数。
  1. 在单继承的情况下

    调用虚函数所需的代价基本上和非虚函数效率一样，在大多数计算机上它多执行了很少的一些指令，所以有很多人一概而论说虚函数性能不行是不太科学的。

  2. 在多继承的情况

    由于会根据多个父类生成多个vptr，在对象里为寻找 vptr 而进行的偏移量计算会变得复杂一些，但这些并不是虚函数的性能瓶颈。虚函数运行时所需的代价主要是虚函数不能是内联函数。这也是非常好理解的，是因为内联函数是指在编译期间用被调用的函数体本身来代替函数调用的指令，但是虚函数的“虚”是指“直到运行时才能知道要调用的是哪一个函数。”但虚函数的运行时多态特性就是要在运行时才知道具体调用哪个虚函数，所以没法在编译时进行内联函数展开。当然如果通过对象直接调用虚函数它是可以被内联，但是大多数虚函数是通过对象的指针或引用被调用的，这种调用不能被内联。 因为这种调用是标准的调用方式，所以虚函数实际上不能被内联。

##### 空间占用

在上面的虚函数实现原理部分，可以看到为了实现运行时多态机制，编译器会给每一个包含虚函数或继承了虚函数的类自动建立一个虚函数表，所以虚函数的一个代价就是会增加类的体积。在虚函数接口较少的类中这个代价并不明显，虚函数表vtbl的体积相当于几个函数指针的体积，如果你有大量的类或者在每个类中有大量的虚函数,你会发现 vtbl 会占用大量的地址空间。但这并不是最主要的代价，主要的代价是发生在类的继承过程中，在上面的分析中，可以看到，当子类继承父类的虚函数时，子类会有自己的vtbl，如果子类只覆盖父类的一两个虚函数接口，子类vtbl的其余部分内容会与父类重复。如果存在大量的子类继承，且重写父类的虚函数接口只占总数的一小部分的情况下，会造成大量地址空间浪费。在一些GUI库上这种大量子类继承自同一父类且只覆盖其中一两个虚函数的情况是经常有的，这样就导致UI库的占用内存明显变大。 由于虚函数指针vptr的存在，虚函数也会增加该类的每个对象的体积。在单继承或没有继承的情况下，类的每个对象会多一个vptr指针的体积，也就是4个字节；在多继承的情况下，类的每个对象会多N个（N＝包含虚函数的父类个数）vptr的体积，也就是4N个字节。当一个类的对象体积较大时，这个代价不是很明显，但当一个类的对象很轻量的时候，如成员变量只有4个字节，那么再加上4（或4N）个字节的vptr，对象的体积相当于翻了1（或N）倍，这个代价是非常大的。

#### 虚函数的应用注意事项

- 内联函数 (inline)

    虚函数用于实现运行时的多态，或者称为晚绑定或动态绑定。而内联函数用于提高效率。内联函数的原理是，在编译期间，对调用内联函数的地方的代码替换成函数代码。内联函数对于程序中需要频繁使用和调用的小函数非常有用。默认地，类中定义的所有函数，除了虚函数之外，会隐式地或自动地当成内联函数(注意：内联只是对于编译器的一个建议，编译器可以自己决定是否进行内联).

    无论何时，使用基类指针或引用来调用虚函数，它都不能为内联函数(因为调用发生在运行时)。但是，无论何时，使用类的对象(不是指针或引用)来调用时，可以当做是内联，因为编译器在编译时确切知道对象是哪个类的。

- 静态成员函数 (static)

    static成员不属于任何类对象或类实例，所以即使给此函数加上virutal也是没有任何意义的。此外静态与非静态成员函数之间有一个主要的区别，那就是静态成员函数没有this指针，从而导致两者调用方式不同。虚函数依靠vptr和vtable来处理。vptr是一个指针，在类的构造函数中创建生成，并且只能用this指针来访问它，因为它是类的一个成员，并且vptr指向保存虚函数地址的vtable。虚函数的调用关系：this -> vptr -> vtable ->virtual function，对于静态成员函数，它没有this指针，所以无法访问vptr. 这就是为何static函数不能为virtual。

- 构造函数 (constructor)

    虚函数基于虚表vtable（内存空间），构造函数 (constructor) 如果是virtual的，调用时也需要根据vtable寻找，但是constructor是virtual的情况下是找不到的，因为constructor自己本身都不存在了，创建不到class的实例，没有实例class的成员（除了public static/protected static for friend class/functions，其余无论是否virtual）都不能被访问了。此外构造函数不仅不能是虚函数。而且在构造函数中调用虚函数，实际执行的是父类的对应函数，因为自己还没有构造好,多态是被disable的。

- 析构函数 (deconstructor)

    对于可能作为基类的类的析构函数要求就是virtual的。因为如果不是virtual的，派生类析构的时候调用的是基类的析构函数，而基类的析构函数只要对基类部分进行析构，从而可能导致派生类部分出现内存泄漏问题。

-纯虚函数

    析构函数可以是纯虚的，但纯虚析构函数必须有定义体，因为析构函数的调用是在子类中隐含的。


## Row Polymorphism（行多态）

Row Polymorphism 是一种较为特殊的多态形式，通常用于记录类型（record
types）或类似结构中。它允许在具有不完全相同字段的记录类型之间进行操作。不同于参数多态，Row
Polymorphism
能够处理部分类型，即它允许记录类型的子集作为参数，而不要求记录类型完全匹配。

Row Polymorphism 允许函数作用于多个具有相同字段的记录类型，即使它们有额外的字段。

# 语言标准

## for co_await

已经被移除标准：

The wording for co_await statement makes assumptions of what future
asynchronous generator interface will be. Remove it for now as not to
constraint the design space for asynchronous generators.

## 字符串字面量

对于一个字符串字面量，其类型为 char\[\]，长度编译期可知。

字符串字面量是静态分配的，因此函数返回字符串字面量是很安全的行为。

C++提供了原始字符串，也就是 R\'str\'。

## 指针与 const

constexpr 提供的是（尽可能）编译期求值，只是或确保在编译期求值。

const 提供的是当前作用域内值不发生改变，主要任务是规定接口的不可修改性。

使用 const会改变一种类型，所谓的改变并不是改变了常量的分配方式，而是限制了他的使用方式。

c++允许通过显示类型转换的方式显式地除掉对于指针指向常量的限制。

注意，const 变量的存储是一个 implementation detail.

GCC Compiler 会把 read-only 变量，常数和跳转表放置在.text 段中。

在局部 const 变量的情况下，存储在 stack segment （栈区）的写保护区。

对于全局初始化 const 变量，存储在 data segment 部分。

对于全局未初始化 const 变量，存放在 BSS segment（存放未初始化的全局/静态变量）。

## 右值引用

C++之所以设计了几种不同形式的引用，是为了支持对象的不同用法：

1.非 const 左值引用所引用的对象可以由用户写入内容。

2.const 左值引用所引用的对象从用户的角度来看是不可修改的。

3.右值引用对应一个临时对象，用户可以修改这个对象，并且认定这个对象以后不会被用到了。

## static

静态局部变量只在第一次调用时初始化，后续的函数调用会保留其值，而不会再次初始化。因此，这里涉及到函数的执行顺序和静态变量的特性。

## 类型双关

在 C++ 中，通过 union 实现类型双关（type punning）来将一种枚举类型转换为另一种，是一种不安全的做法，因为在 C++ 中，这种写法可能导致未定义行为。为实现安全的类型转换，推荐使用 static_cast 或其他显式转换方式。

## Dependent Names

在一个模板中，一些类型可能会依赖于模板参数，例如这样：

```cpp
template<typename T>
struct X : B<T> // “B<T>” is dependent on T
{
    typename T::A* pa; // “T::A” is dependent on T
                       // (see below for the meaning of this use of “typename”)

    void f(B<T>* pb)
    {
        static int i = B<T>::i; // “B<T>::i” is dependent on T
        pb->j++; // “pb->j” is dependent on T
    }
};
```

### Binding Rule

非依赖/限定的名称会在模板定义时就被查找和绑定，如下：
```cpp
#include <iostream>

void g(double) { std::cout << "g(double)\n"; }

template<class T>
struct S
{
    void f() const
    {
        g(1); // “g” is a non-dependent name, bound now
    }
};

void g(int) { std::cout << "g(int)\n"; }

int main()
{
    g(1);  // calls g(int)

    S<int> s;
    s.f(); // calls g(double)
}
```
如果非依赖名称的含义在定义上下文和模板特化的实例化点之间发生变化，则程序格式不正确，无需进行诊断。以下情况下可能会出现这种情况：

1. 非依赖名称中使用的类型在定义时不完整，但在实例化时完整。
2. 在模板定义中查找名称时发现了使用声明，但在实例化中相应范围的查找未找到任何声明，因为使用声明是包扩展，而相应的包为空。
3. 实例化使用在定义时未定义的默认参数或默认模板参数。
4. 实例化点处的常量表达式使用整型或无作用域枚举类型的 const 对象的值、constexpr 对象的值、引用的值或 constexpr 函数的定义(自 C++11 起)，并且该对象/引用/函数(自 C++11 起)在定义点未定义。
5. 模板在实例化时使用非依赖类模板特化或变量模板特化（自 C++14 起），并且它使用的这个模板要么是从定义时未定义的部分特化实例化，要么命名在定义时未声明的显式特化。

依赖名称的绑定被推迟，直到查找发生为止。

### 查找规则 (Lookup Rule)

模板中使用的依赖名称的查找被推迟，直到模板参数已知，此时:
1. 非 ADL 查找检查模板定义上下文中可见的具有外部链接的函数声明
2. DL检查具有外部链接的函数声明，这些声明在模板定义上下文或模板实例化上下文中可见（换句话说，在模板定义后添加新的函数声明不会使其可见，除非通过 ADL）。

此规则的目的是帮助防止违反模板实例的ODR ：

```cpp
// an external library
namespace E
{
    template<typename T>
    void writeObject(const T& t)
    {
        std::cout << "Value = " << t << '\n';
    }
}

// translation unit 1:
// Programmer 1 wants to allow E::writeObject to work with vector<int>
namespace P1
{
    std::ostream& operator<<(std::ostream& os, const std::vector<int>& v)
    {
        for (int n : v)
            os << n << ' ';
        return os;
    }

    void doSomething()
    {
        std::vector<int> v;
        E::writeObject(v); // Error: will not find P1::operator<<
    }
}

// translation unit 2:
// Programmer 2 wants to allow E::writeObject to work with vector<int>
namespace P2
{
    std::ostream& operator<<(std::ostream& os, const std::vector<int>& v)
    {
        for (int n : v)
            os << n << ':';
        return os << "[]";
    }

    void doSomethingElse()
    {
        std::vector<int> v;
        E::writeObject(v); // Error: will not find P2::operator<<
    }
}
```

在上面的例子中，如果operator<<允许从实例化上下文中进行非 ADL 查找，则实例化E :: writeObject < vector <int> >​会有两个不同的定义：一个使用P1 ::操作符<<和一个使用P2 ::操作符<<. 链接器可能无法检测到此类 ODR 违规，从而导致在两种情况下都使用其中一个。

如果嵌套类派生自其封闭类模板，则基类可以是当前实例。属于依赖类型但不是当前实例的基类是依赖基类:
```cpp
template<class T>
struct A
{
    typedef int M;

    struct B
    {
        typedef void M;

        struct C;
    };
};

template<class T>
struct A<T>::B::C : A<T>
{
    M m; // OK, A<T>::M
};
```

如果名称满足以下条件，则将其归类为当前实例的成员：

    在当前实例或其非依赖基中通过非限定查找找到的非限定名称。
    限定名称，如果限定符（ 左侧的名称::）命名当前实例，并且查找在当前实例或其非依赖基中找到该名称
    类成员访问表达式中使用的名称（是在x. y或者xp- > y​），其中对象表达式（十或者*经验值）是当前实例，并且查找在当前实例或其非依赖基中找到名称
```cpp
template<class T>
class A
{
    static const int i = 5;

    int n1[i];       // i refers to a member of the current instantiation
    int n2[A::i];    // A::i refers to a member of the current instantiation
    int n3[A<T>::i]; // A<T>::i refers to a member of the current instantiation

    int f();
};

template<class T>
int A<T>::f()
{
    return i; // i refers to a member of the current instantiation
}
```

当前实例的成员可能既是依赖的，又是非依赖的。

如果在实例化点和定义点之间查找当前实例化的成员时得到不同的结果，则该查找是不明确的。但请注意，当使用成员名称时，它不会自动转换为类成员访问表达式，只有显式成员访问表达式才能指示当前实例化的成员：

```cpp
struct A { int m; };
struct B { int m; };

template<typename T>
struct C : A, T
{
    int f() { return this->m; } // finds A::m in the template definition context
    int g() { return m; }       // finds A::m in the template definition context
};

template int C<B>::f(); // error: finds both A::m and B::m
 //在 f() 中使用 this->m 时，编译器会在模板定义上下文（即模板未实例化时）查找 m。此时，编译器只看到 A::m，所以它不会报错。但在模板实例化时（C<B>::f()），this->m 需要确定具体的 m 是来自 A 还是 B，结果发现 A::m 和 B::m 都存在，产生二义性（Ambiguity），因此报错。
template int C<B>::g(); // OK: transformation to class member access syntax
                        // does not occur in the template definition context
//在 g() 中直接使用 m，编译器会先进行 非限定名称（Unqualified Name）查找，并且它在 模板定义上下文 会把 m 当作类成员的可能候选，等到模板实例化时才解析具体的 m。在实例化时，m 会被编译器转换为 this->m，但这时它的语法规则已经不同了，不会再查找两个不同的 m，因此编译成功。
```
### Two Phase Name Lookup

这个机制的目的是区分模板定义阶段和模板实例化阶段，以便正确解析名称。C++ 编译器在解析模板时，会分为 两个阶段 来查找名称：第一阶段（模板定义阶段）和 第二阶段（模板实例化阶段）。

1. 第一阶段：模板定义阶段
在模板定义阶段，编译器只根据模板的定义来解析名称。在此阶段，编译器不知道模板参数的类型，因此它只能查找模板本身的成员或依赖于模板参数的名称，但不会依赖于实例化的类型。

在模板定义阶段，编译器会查看模板本身的内容，并对非依赖名称（non-dependent names）进行查找。非依赖名称指的是模板定义中直接可以解析的名称，这些名称不依赖于模板参数（类型或非类型参数）。

    非依赖名称的查找：例如，int x; 中的 x，编译器可以直接在模板定义中查找。
    依赖名称的查找：例如，模板中使用了 T::m，由于编译器无法在模板定义阶段知道 T 的具体类型，它只能在后续实例化时进行查找。


2. 第二阶段：模板实例化阶段
在模板实例化阶段，模板会根据特定类型的参数进行实例化。这时，编译器将根据实例化后的类型查找名称并解析相关内容。

当模板被实例化时，编译器会根据实例化的类型或非类型参数来查找和解析名称。这时，模板的依赖名称才会被解析。

依赖名称（Dependent Name）是指在模板中，名称的解析依赖于模板参数的类型或非类型参数。只有当模板实例化时，编译器才能知道模板参数的具体类型，并解析这些名称。
关键点：

    模板定义阶段，依赖名称无法完全解析，因为模板参数的类型尚未确定。
    在模板实例化阶段，编译器会根据实例化的类型（或者非类型参数）来解析依赖名称。

### 为什么需要两阶段查找？
C++ 的名称查找是基于 作用域规则 的。在模板定义时，模板参数的类型尚未确定，所以不能立即解析依赖名称。两阶段查找机制通过区分模板定义和实例化阶段，允许编译器正确地解析模板中的依赖名称。

## extern template (C++ 11)

同一个模板，在不同的翻译单元中，会被重复实例化，这就会导致 C++编译出来的体积膨胀（符号变多），如果使用 extern template xxx 的形式，就可以控制模板实例化的发生位置，使得模板只在一个翻译单元中实例化，而其他地方只声明它，避免重复实例化。

在一个 lib 里面，一个模板只定义一次，其他地方只需要声明就可以了。

## ODR 与翻译单元

在 C++中， 一个翻译单元由一个实现文件及其直接或间接包含的所有标头组成。 实现文件通常具有文件扩展名 .cpp 或 .cxx。 头文件通常具有扩展名 .h 或 .hpp。 每个翻译单元由编译器独立编译。 编译完成后，链接器会将编译后的翻译单元合并到单个程序中。

static 这个关键字是一个局部的 definition，只在自己的这个.cpp 里有用。这样也不会造成重定义。

在 C++17 的 inline 变量出现前，要想在多个 cpp 中用同一个变量，我们需要 extern 声明变量+只有一个定义。

由于 inline 现在的作用其实和内联没什么关系了，他是为了让多个翻译单元可以共用一个变量，c++17 后对 inline 的解释是“允许重复定义”。而内敛与否完全是编译器的优化行为了。

如果有多个编译单元拒绝了内联，就会生成多份函数/变量定义，为了在链接时不报错，由 inline 修饰的函数会生成弱符号，所以 inline 帮助我们在多个翻译单元中可以重复定义，帮助我们突破了 ODR 规则。

inline 要起作用（指内联）,必须要与函数定义放在一起，而不是函数的声明

虚函数其实最主要的性能开销在于它阻碍了编译器内联函数和各种函数级别的优化，导致性能开销较大，在普通函数中log(10)会被优化掉，它就只会被计算一次，而如果使用虚函数，log(10)不会被编译器优化，它就会被计算多次。如果代码中使用了更多的虚函数，编译器能优化的代码就越少，性能就越低。

## 虚函数一定是运行期才绑定的吗？

虚函数运行期绑定的性质只有在指针或者引用下能用，通过值调用的虚函数是编译器静态绑定，是没有运行期绑定的性质的。

在使用限定名字查找时，即使是通过指针或者引用，虚函数也不表现多态性。

### final

(since C++11) final 对虚函数的多态性具有向下阻断作用。经 final 修饰的虚函数或经 final 修饰的类的所有虚函数，自该级起，不再具有多态性。

## 虚函数究竟在哪些地方变慢了？

1. 函数调用多一层
2. 编译器难以优化
3. Cache Miss
4. 分支预测失败

## 构造函数默认使用浅拷贝

如下代码：
```cpp
struct Test {
  int* i;
};

int main() {
  Test t1;
  t1.i = new int(1);
  Test t2 = t1;
  *t2.i = 3;

  std::cout << *t1.i; // 3
}
```

如果是被定义好的构造函数，就要看构造函数使用的是哪一个。

# 杂项

## forward declaration

为什么 c/c++需要我们在调用前声明呢？

比如说:

```c++
void foo();
void test(){
foo();
}
void foo(){
xxx
}
```

这是历史问题，c/c++编译器被设计为 single-pass,当编译器需要链接符号时必须知道这个符号链接的对象是谁。

对于 C#, java 这样的 two pass compiler 来说，就不需要前向声明（但是仍然有两个包互相依赖的问题）

## std::swap VS xor

由于现代处理器上的 Tomasulo\'s
algorithm 算法的实现，std::swap 的性能往往更佳。

该算法与之前同样用于实现指令流水线动态调度的计分板不同在于它使用了寄存器重命名机制。指令之间具有数据相关性（例如后条指令的源寄存器恰好是前条指令要写入的目标寄存器），进行动态调度时必须避免三类冒险：写后读（Read-after-Write,RAW）、写后写（Write-after-Write, WAW）、读后写（Write-after-Read, WAR）。第一种冒险也被称为真数据相关（true data dependence），而后两种冒险则并没有那么致命，它们可以由寄存器重命名来予以解决。 托马苏洛算法使用了一个共享数据总线（common data bus,CDB）将已计算出的值广播给所有需要这个值作为指令源操作数的保留站。该算法尽可能降低了使用计分板技术导致的流水线停顿，从而改善了并行计算的效率。

在指令的发射（issue）阶段，如果操作数和保留站都准备就绪，那么指令就可以直接发射并执行。如果操作数未就绪，则进入保留站的指令会跟踪即将产生这个所需操作数的那个功能单元。如果连可用的保留站功能单元都已经不够用，那么该指令必须被停顿。为了化解读后写（WAR）和写后写（WAW）冲突，需要在该阶段进行指令的寄存器重命名。从指令队列中取出下一条指令，如果其所用到的操作数目前位于寄存器中，那么如果与指令匹配的功能单元（这类处理器通常具有多个功能单元以发挥指令级并行的优势）当前可用，则发射该指令；否则，由于没有可用的功能单元，指令被停顿，直到保留站或缓存可用。尽管执行时可能并未按照指令代码的先后顺序，但是它们在发射过程还是按照原先的顺序。这是为了确保指令顺序执行时的一些现象，例如处理器异常，能够以顺序执行时的同样顺序出现。下一个阶段为执行阶段。在该阶段，指令对应的操作被执行。执行前需要保证所有操作数可用，同时写后读（RAW）冲突已经被化解。系统通过计算有效地址来避免存储区的冲突，从而保证程序的正确性。最后的阶段为写结果阶段，算术逻辑单元（ALU）的计算结果被写回到寄存器，以及任何正在等待该结果的保留站中，如果是存储（store）指令，则写回到存储器中。

但是注意，这并不是绝对优势的。这里还要考虑指令重排带来的优化与影响（函数先后位置）。所以，编写简单的代码更重要

## cbrt

cube root,开立方根。

## sendto

sys/socket.h 中的函数，用来将消息发送到 dest_addr。被用于实现 ping。

## __builtin_ffs

__builtin_ffs返回输入数的二进制表示的最低非零位的下标
