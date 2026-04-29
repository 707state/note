---
title: C++小寄巧
author: jask
tags:
  - ProgrammingLanguages
date: 2026-04-27
---

# filesystem

标准库filesystem提供了很多有用的工具函数，比如说：

- `std::filesystem::relative` 用来计算两个path相对路径；
- `remove_filename()` 只拿到路径不获取filename，有点类似于`root_directory()`方法；


而且标准库还提供了一个`/`重载，可以看下面的代码：

```cpp
  auto base = boost::dll::program_location().parent_path().parent_path();
  auto images_path = (base / "dataset" / "train-images-idx3-ubyte").string();
```

比如说可以写出来这样的代码，这里boost封装了各个操作系统获取当前正在执行的文件所在的文件系统位置的接口，所以可以直接获取二进制程序位置。

怎么说呢，真不愧是Boost（大拇指）。

## 一点拓展

C++的filesystem提供的功能全面但是琐碎，难以组合。

有没有办法能够让代码使用起来更加语义化呢？

答案是：`|` pipeline operator。

比如说可以写出来这样的代码：

```cpp
template <typename T, typename Fn>
constexpr auto operator|(T &&val, Fn &&fn)
    -> decltype(std::forward<Fn>(fn)(std::forward<T>(val))) {
  return std::forward<Fn>(fn)(std::forward<T>(val));
}
// 取父目录（无参版本需要用括号调用）
inline const auto parent_path =
    [](const fs::path &p) -> fs::path { return p.parent_path(); };
// 取文件名
inline const auto filename =
    [](const fs::path &p) -> fs::path { return p.filename(); };	
...
  auto analyze = [](const fs::path &p) {
    return std::make_tuple(
        p | po::parent_path | po::to_str,     // "/home/user/data"
        p | po::filename | po::to_str,         // "photo.jpg"
        p | po::replace_ext(".png") | po::to_str // "photo.png"
    );
  };

  auto [dir, name, renamed] =
      analyze(fs::path("/home/user/project/data/photo.jpg"));
  std::cout << std::format("示例3: dir={}  name={}  renamed={}\n",
                           dir, name, renamed);
```

这样看起来起码比变量满天飞好看。
