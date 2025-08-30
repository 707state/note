<!--toc:start-->
- [std::function的奇妙之处](#stdfunction的奇妙之处)
  - [事实](#事实)
  - [实现](#实现)
<!--toc:end-->

[原始文档](https://nihil.cc/posts/std_function/)

# std::function的奇妙之处

首先，std::function可以递归；

其次，std::function可以包装有捕获的Lambda。

## 事实

1. 函数指针不能指向带有捕获的lambda！

## 实现

```c++
template <typename Ret, typename... Args>
class Closure {
 public:
  typedef Ret (*Func)(Args...);
  Closure(Func fp) : _fp(fp) {}

  Ret operator()(Args... args) const { return _fp(std::forward<Args>(args)...); }

 private:
  Func _fp;
};
```

这样的简单实现既不能接收有捕获的lambda，也不能递归！

通过模板特化可以写出这样的代码：

```c++
#include <iostream>

template <typename T>
class Closure {
  Closure() = delete;
};

template <typename Ret, typename... Args>
class Closure<Ret(Args...)> {
 public:
  typedef Ret (*Func)(Args...);
  Closure(Func fp) : _fp(fp) {}

  Ret operator()(Args... args) const { return _fp(std::forward<Args>(args)...); }

 private:
  Func _fp;
};

int main() {
  Closure<int(int)> addOne = (Closure<int(int)>::Func)[](int x) { return x + 1; };
  std::cout << addOne(10) << std::endl;

  return 0;
}
```

问题在于：复制初始化（即使用等号初始化）对于隐式类型转换的要求比直接初始化（即使用括号初始化）更严格。

这里不允许隐式转换。

可以通过模板构造函数解决：

```c++
#include <iostream>

template <typename T>
class Closure {
  Closure() = delete;
};

template <typename Ret, typename... Args>
class Closure<Ret(Args...)> {
 public:
  typedef Ret (*Func)(Args...);

  template<typename Lambda>
  Closure(Lambda&& fp) : _fp(std::forward<Lambda>(fp)) {}

  Ret operator()(Args... args) const { return _fp(std::forward<Args>(args)...); }

 private:
  Func _fp;
};

int main() {
  Closure<int(int)> addOne = [](int x) { return x + 1; };
  std::cout << addOne(10) << std::endl;

  return 0;
}
```

这样就解决了初始化的问题。

接下来就是解决捕获的问题。

注意直接将 lambda 函数的类型作为模板类型，然后使用该模板类型来声明一个成员变量是不可行的，因为 lambda 函数想要捕捉一个环境变量，这个环境变量的类型大小必须是已知的，如果 Closure 类型依赖 lambda 函数的类型，那么它就无法在 lambda 函数的函数体中被捕捉。

可以通过：

```c++
template <typename T>
class Closure {
  Closure() = delete;
};

template <typename Ret, typename... Args>
class Closure<Ret(Args...)> {
 public:
  template <typename Lambda>
  Closure(Lambda&& fp) {
    // 我们通过 new 把 lambda 函数转移到堆上，这样我们就不需要直接持有它
    _fp = new Lambda(std::forward<Lambda>(fp));
  }

  // 禁止拷贝构造
  Closure(const Closure&) = delete;

  // 允许移动构造
  Closure(Closure &&other) {
    _fp = other._fp;
    other._fp = nullptr;
  }

  Ret operator()(Args... args) const {
    // 我们不知道 lambda 的实际类型，因此这里暂时无法实现
  }

 private:
  void* _fp;
};
```

这样的方式来存储函数指针，接下来就是保存类型Lambda函数的实际类型了。

```c++
template <typename T>
class Closure {
  Closure() = delete;
};

template <typename Ret, typename... Args>
class Closure<Ret(Args...)> {
 private:
  // 注意到该函数的类型是 Ret (*)(void*, Args...)，并不包含 Lambda 模板类型
  template <typename Lambda>
  static Ret invoke(void* fp, Args... args) {
    // 在函数体内使用 Lambda 模板类型，而不是让函数的调用者提供
    return (*reinterpret_cast<Lambda*>(fp))(std::forward<Args>(args)...);
  }

 public:
  template <typename Lambda>
  Closure(Lambda&& fp) {
    _fp = new Lambda(std::forward<Lambda>(fp));
    // 我们通过储存 invoke 函数模板特化后的指针来间接储存 lambda 的类型
    _invoke = invoke<Lambda>;
  }
  Closure(const Closure&) = delete;
  Closure(Closure&& other) {
    _fp = other._fp;
    other._fp = nullptr;
    _invoke = other._invoke;
  }

  Ret operator()(Args... args) const {
    // 最终实际调用的是模板特化后的 invoke 函数
    return _invoke(_fp, std::forward<Args>(args)...);
  }

 private:
  void* _fp;
  // _invoke 指针不依赖 lambda 函数的实际类型
  Ret (*_invoke)(void*, Args...);
};
```

当然，由于我们用到了 new 来储存 lambda 函数，因此我们也需要一个合适的方法去 delete 掉它。和 invoke 同理，我们可以实现一个 free 函数：

```c++
template <typename T>
class Closure {
  Closure() = delete;
};

template <typename Ret, typename... Args>
class Closure<Ret(Args...)> {
 private:
  template <typename Lambda>
  static Ret invoke(void* fp, Args... args) {
    return (*reinterpret_cast<Lambda*>(fp))(std::forward<Args>(args)...);
  }
  template <typename Lambda>
  static void free(void* fp) {
    delete reinterpret_cast<Lambda*>(fp);
  }

 public:
  template <typename Lambda>
  Closure(Lambda&& fp) {
    _fp = new Lambda(std::forward<Lambda>(fp));
    _invoke = invoke<Lambda>;
    _free = free<Lambda>;
  }
  Closure(const Closure&) = delete;
  Closure(Closure&& other) {
    _fp = other._fp;
    other._fp = nullptr;
    _invoke = other._invoke;
    _free = other._free;
  }
  ~Closure() {
    // 最终实际调用的是模板特化后的 free 函数
    _free(_fp);
  }

  Ret operator()(Args... args) const {
    // 最终实际调用的是模板特化后的 invoke 函数
    return _invoke(_fp, std::forward<Args>(args)...);
  }

 private:
  void* _fp;
  Ret (*_invoke)(void*, Args...);
  void (*_free)(void*);
};
```

泛型类型 Lambda 有可能是引用类型，因此需要用到remove\_reference\_t来去除饮用。

```c++
template <typename T>
class Closure {
  Closure() = delete;
};

template <typename Ret, typename... Args>
class Closure<Ret(Args...)> {
 private:
  template <typename Lambda>
  static Ret invoke(void* fp, Args... args) {
    return (*reinterpret_cast<std::remove_reference_t<Lambda>*>(fp))(
        std::forward<Args>(args)...);
  }
  template <typename Lambda>
  static void free(void* fp) {
    delete reinterpret_cast<std::remove_reference_t<Lambda>*>(fp);
  }

 public:
  template <typename Lambda>
  Closure(Lambda&& fp) {
    _fp = new std::remove_reference_t<Lambda>(std::forward<Lambda>(fp));
    _invoke = invoke<Lambda>;
    _free = free<Lambda>;
  }
  Closure(const Closure& other) = delete;
  Closure(Closure&& other) {
    _fp = other._fp;
    other._fp = nullptr;
    _invoke = other._invoke;
    _free = other._free;
  }
  ~Closure() {
    _free(_fp);
  }

  Ret operator()(Args... args) const {
    return _invoke(_fp, std::forward<Args>(args)...);
  }

 private:
  void* _fp;
  Ret (*_invoke)(void*, Args...);
  void (*_free)(void*);
};
```
