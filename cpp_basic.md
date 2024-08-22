---
title: "CPP_BASIC"
author: "jask"
date: "2024-08-11"
output: pdf_document
header-includes:
  - \usepackage{xeCJK}
  - \setCJKmainfont{Noto Sans CJK SC}  # 替换为可用的字体
  - \setCJKmonofont{Noto Sans CJK SC}
  - \setCJKsansfont{Noto Sans CJK SC}
---

# 重读<<C++程序设计语言>>
注：由于是重读，这里大多是一些拾遗。

采用格式：章节+页码+内容
## 41章并发
### P280
"还请注意，只要你不向其他线程传递局部数据的指针，你的局部数据就不存在这里讨论的诸多问题。"

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
is_lockable 概念可以用来限制模板，使得只有那些具备 lock()、unlock() 和 try_lock() 成员函数，并且 try_lock() 返回类型可以转换为 bool 的类型才可以作为模板参数传递。这个概念通常用于多线程编程中，以确保模板只接受那些具有锁定功能的类型，比如 std::mutex 或 std::recursive_mutex。

限定类型。

## std::ignore
可用来忽略返回值，cppref解释：

1) An object such that any value can be assigned to it with no effect.
2) The type of std::ignore.

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



