# C++ Tuple

C++开发者一定会遇到需要std::tuple的场景，但是

1. std::tuple是zero-overhead的吗？
2. std::tuple怎么实现的？

来看看吧。

## 先看一下实现

std::tuple本质上还是继承。

问题在于有多种实现方式，即0->N继承也可以从N->0的继承方式，还可以用index_sequence+多继承的方式。

### 正向单继承

```c++
#include <utility>
template<size_t Idx, typename T>
struct tuple_unit{
	tuple_unit(const T& v):value(v){}
	tuple_unit(T&& v):value(std::forward<T>(v)){}
	T value;
};
template<size_t Idx,typename ...T>
struct tuple_inner{};

template<size_t Idx,typename T>
struct tuple_inner<Idx,T>: public tuple_unit<Idx,T>{
	tuple_inner(const T& t):tuple_unit<Idx,T>(t){}
	tuple_inner(T&& t):tuple_unit<Idx,T>(std::forward<T>(t)){}
};
template<size_t Idx, typename T, typename ...Element>
struct tuple_inner<Idx,T,Element...>: public tuple_inner<Idx+1,Element...>,
	tuple_unit<Idx,T>{
	tuple_inner(const T& t, Element& ...elem): tuple_inner<Idx+1,Element...>(elem...),tuple_unit<Idx,T>(t){}
	tuple_inner(T&& t, Element&& ...elem): tuple_inner<Idx+1,Element...>(std::forward<Element>(elem)...),tuple_unit<Idx,T>(std::forward<T>(t)){}
};
template<typename ...Element>
struct tuple: public tuple_inner<0,Element...>{
	 tuple(const Element &...element):
		tuple_inner<0,Element...>(element...){}
	tuple(Element&& ...element):
		tuple_inner<0,Element...>(std::forward<Element>(element)...){}
};
template<size_t Idx,typename T>
T& get(tuple_unit<Idx,T>& t){
	return t.value;
}
```

可以看到这里面用到了单继承+子类型来实现。其核心是在tuple_inner这里的递归。get函数则是利用了子类型可以表示基类这么一个特性。

### 逆向单继承

```c++
#include <iostream>

// 基础情况：空 tuple
template<typename... Ts>
struct MyTuple {};

// 递归继承展开 tuple
template<typename Head, typename... Tail>
struct MyTuple<Head, Tail...> {
    Head head;
    MyTuple<Tail...> tail;

    MyTuple(const Head& h, const Tail&... t)
        : head(h), tail(t...) {}
};

// 获取元素
template<std::size_t Index, typename Tuple>
struct TupleElement;

// 偏特化：Index = 0
template<typename Head, typename... Tail>
struct TupleElement<0, MyTuple<Head, Tail...>> {
    using type = Head;

    static Head& get(MyTuple<Head, Tail...>& t) {
        return t.head;
    }
};

// 偏特化：Index > 0
template<std::size_t Index, typename Head, typename... Tail>
struct TupleElement<Index, MyTuple<Head, Tail...>> {
    using type = typename TupleElement<Index - 1, MyTuple<Tail...>>::type;

    static type& get(MyTuple<Head, Tail...>& t) {
        return TupleElement<Index - 1, MyTuple<Tail...>>::get(t.tail);
    }
};

// 一个 get 函数模板
template<std::size_t Index, typename... Ts>
typename TupleElement<Index, MyTuple<Ts...>>::type&
get(MyTuple<Ts...>& t) {
    return TupleElement<Index, MyTuple<Ts...>>::get(t);
}
```

原理上和正向没有区别，但是get函数没办法做到向正向那么直观。

### 多继承

一个tuple实现继承多个tuple\_leaf，什么意思呢？

就是说一个tuple通过多继承，get的时间复杂度就变成了O(1)。

```c++
#include <cstddef>
#include <utility>
#include <iostream>
#include <string>

// ---------------- tuple_leaf ----------------
template<std::size_t I, typename T>
struct tuple_leaf {
    T value;

    tuple_leaf() = default;
    tuple_leaf(const T& v) : value(v) {}
    tuple_leaf(T&& v) : value(std::move(v)) {}
};

// ---------------- tuple_impl ----------------
template<typename IndexSeq, typename... Ts>
struct tuple_impl;

template<std::size_t... Is, typename... Ts>
struct tuple_impl<std::index_sequence<Is...>, Ts...>
    : tuple_leaf<Is, Ts>...   // 多继承
{
    tuple_impl() = default;

    tuple_impl(Ts... args)
        : tuple_leaf<Is, Ts>(std::forward<Ts>(args))... {}
};

// ---------------- tuple ----------------
template<typename... Ts>
struct tuple : tuple_impl<std::index_sequence_for<Ts...>, Ts...> {
    using base = tuple_impl<std::index_sequence_for<Ts...>, Ts...>;
    using base::base; // 继承构造函数
};

// ---------------- tuple_element ----------------
template<std::size_t I, typename Tuple>
struct tuple_element;

template<std::size_t I, typename... Ts>
struct tuple_element<I, tuple<Ts...>> {
    using type = typename std::tuple_element<I, std::tuple<Ts...>>::type;
};

// ---------------- get ----------------
template<std::size_t I, typename... Ts>
decltype(auto) get(tuple<Ts...>& t) {
    using Leaf = tuple_leaf<I, typename tuple_element<I, tuple<Ts...>>::type>;
    return static_cast<Leaf&>(t).value;
}

template<std::size_t I, typename... Ts>
decltype(auto) get(const tuple<Ts...>& t) {
    using Leaf = tuple_leaf<I, typename tuple_element<I, tuple<Ts...>>::type>;
    return static_cast<const Leaf&>(t).value;
}
```
