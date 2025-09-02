<!--toc:start-->
- [预备知识](#预备知识)
- [range](#range)
- [view](#view)
  - [single view](#single-view)
  - [iota view](#iota-view)
  - [istream view](#istream-view)
<!--toc:end-->

# 预备知识

C++ ranges在c++20进入标准，提供了在一个序列上进行一系列可组合的操作的能力。

concept，C++提供的一种机制用于约束模板参数。

# range

C++中range是一个contept，定义如下：

```c++
template<class T>
concept range=
requires(T& t){
ranges::begin(t);
ranges::end(t);
}
```


# view

view也是一个concept，定义如下：

```c++
template<class T>
concept view=
range<T> && movable<T> && default_constructible<T> && enable_view<T>;
```

## single view

single\_view只会生成一个元素

## iota view

生成左闭右开的区间。

## istream view
