<!--toc:start-->
- [饿汉式](#饿汉式)
- [线程安全的饿汉式](#线程安全的饿汉式)
- [双重检查锁定（DCLP）单例](#双重检查锁定dclp单例)
- [Meyers单例（C++11 静态局部变量）](#meyers单例c11-静态局部变量)
- [使用 std::call_once 实现的单例](#使用-stdcallonce-实现的单例)
- [使用模板的通用单例](#使用模板的通用单例)
- [使用智能指针的单例](#使用智能指针的单例)
- [使用 C++20 概念(Concepts)的单例模式](#使用-c20-概念concepts的单例模式)
- [使用 C++20 协程(Coroutines)实现的懒加载单例](#使用-c20-协程coroutines实现的懒加载单例)
- [使用 C++20 原子共享指针的线程安全单例](#使用-c20-原子共享指针的线程安全单例)
<!--toc:end-->

# 饿汉式

```c++
class Singleton {
private:
    // 私有构造函数
    Singleton() {}

    // 禁用拷贝和赋值
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    // 静态实例指针
    static Singleton* instance;

public:
    // 获取实例的静态方法
    static Singleton* getInstance() {
        if (instance == nullptr) {
            instance = new Singleton();
        }
        return instance;
    }

    // 释放资源
    static void releaseInstance() {
        if (instance != nullptr) {
            delete instance;
            instance = nullptr;
        }
    }
};

// 静态成员初始化
Singleton* Singleton::instance = nullptr;
```

# 线程安全的饿汉式

```c++
#include <mutex>

class ThreadSafeSingleton {
private:
    ThreadSafeSingleton() {}
    ThreadSafeSingleton(const ThreadSafeSingleton&) = delete;
    ThreadSafeSingleton& operator=(const ThreadSafeSingleton&) = delete;

    static ThreadSafeSingleton* instance;
    static std::mutex mutex;

public:
    static ThreadSafeSingleton* getInstance() {
        std::lock_guard<std::mutex> lock(mutex);
        if (instance == nullptr) {
            instance = new ThreadSafeSingleton();
        }
        return instance;
    }
};

ThreadSafeSingleton* ThreadSafeSingleton::instance = nullptr;
std::mutex ThreadSafeSingleton::mutex;
```

# 双重检查锁定（DCLP）单例

```c++
#include <mutex>
#include <atomic>

class DCLPSingleton {
private:
    DCLPSingleton() {}
    DCLPSingleton(const DCLPSingleton&) = delete;
    DCLPSingleton& operator=(const DCLPSingleton&) = delete;

    static std::atomic<DCLPSingleton*> instance;
    static std::mutex mutex;

public:
    static DCLPSingleton* getInstance() {
        DCLPSingleton* tmp = instance.load(std::memory_order_acquire);
        if (tmp == nullptr) {
            std::lock_guard<std::mutex> lock(mutex);
            tmp = instance.load(std::memory_order_relaxed);
            if (tmp == nullptr) {
                tmp = new DCLPSingleton();
                instance.store(tmp, std::memory_order_release);
            }
        }
        return tmp;
    }
};

std::atomic<DCLPSingleton*> DCLPSingleton::instance{nullptr};
std::mutex DCLPSingleton::mutex;
```

# Meyers单例（C++11 静态局部变量）

```c++
class MeyersSingleton {
private:
    MeyersSingleton() {}
    MeyersSingleton(const MeyersSingleton&) = delete;
    MeyersSingleton& operator=(const MeyersSingleton&) = delete;

public:
    static MeyersSingleton& getInstance() {
        // C++11 保证静态局部变量的初始化是线程安全的
        static MeyersSingleton instance;
        return instance;
    }
};
```

# 使用 std::call_once 实现的单例

```c++
#include <mutex>

class CallOnceSingleton {
private:
    CallOnceSingleton() {}
    CallOnceSingleton(const CallOnceSingleton&) = delete;
    CallOnceSingleton& operator=(const CallOnceSingleton&) = delete;

    static CallOnceSingleton* instance;
    static std::once_flag initInstanceFlag;

public:
    static CallOnceSingleton* getInstance() {
        std::call_once(initInstanceFlag, []() {
            instance = new CallOnceSingleton();
        });
        return instance;
    }
};

CallOnceSingleton* CallOnceSingleton::instance = nullptr;
std::once_flag CallOnceSingleton::initInstanceFlag;
```

# 使用模板的通用单例

```c++
template<typename T>
class Singleton {
private:
    Singleton() = delete;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

public:
    static T& getInstance() {
        static T instance;
        return instance;
    }
};

// 使用方式
class MyClass {};
// 获取 MyClass 的单例
MyClass& myClassInstance = Singleton<MyClass>::getInstance();
```

# 使用智能指针的单例

```c++
#include <memory>

class SmartPtrSingleton {
private:
    SmartPtrSingleton() {}
    SmartPtrSingleton(const SmartPtrSingleton&) = delete;
    SmartPtrSingleton& operator=(const SmartPtrSingleton&) = delete;

    static std::shared_ptr<SmartPtrSingleton> instance;
    static std::mutex mutex;

public:
    static std::shared_ptr<SmartPtrSingleton> getInstance() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!instance) {
            instance = std::shared_ptr<SmartPtrSingleton>(new SmartPtrSingleton());
        }
        return instance;
    }
};

std::shared_ptr<SmartPtrSingleton> SmartPtrSingleton::instance = nullptr;
std::mutex SmartPtrSingleton::mutex;
```

# 使用 C++20 概念(Concepts)的单例模式

```c++
#include <concepts>
#include <memory>

// 定义一个概念，表示可以作为单例的类型
template<typename T>
concept Singletonable = std::is_default_constructible_v<T> &&
                        !std::is_copy_constructible_v<T> &&
                        !std::is_move_constructible_v<T>;

template<Singletonable T>
class Singleton {
private:
    Singleton() = delete;

public:
    static T& getInstance() {
        static T instance;
        return instance;
    }
};

// 使用示例
class MyService {
public:
    MyService() = default;
    MyService(const MyService&) = delete;
    MyService(MyService&&) = delete;

    void doSomething() { /* ... */ }
};

// 获取单例
auto& service = Singleton<MyService>::getInstance();
```

# 使用 C++20 协程(Coroutines)实现的懒加载单例

```c++
#include <coroutine>
#include <memory>
#include <optional>

template<typename T>
class LazyInit {
public:
    struct promise_type {
        std::optional<T> value;

        LazyInit get_return_object() {
            return LazyInit{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void unhandled_exception() { throw; }
        void return_value(T&& val) { value = std::move(val); }
    };

    bool hasValue() const { return handle && handle.promise().value.has_value(); }
    T& value() { handle.resume(); return *handle.promise().value; }

    LazyInit(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~LazyInit() { if (handle) handle.destroy(); }

private:
    std::coroutine_handle<promise_type> handle;
};

template<typename T>
class CoroutineSingleton {
private:
    static inline LazyInit<T> lazy_instance = []() -> LazyInit<T> {
        co_return T{};
    }();

    CoroutineSingleton() = delete;

public:
    static T& getInstance() {
        return lazy_instance.value();
    }
};
```

# 使用 C++20 原子共享指针的线程安全单例

```c++
#include <memory>
#include <atomic>

template<typename T>
class AtomicSingleton {
private:
    static inline std::atomic<std::shared_ptr<T>> instance{nullptr};

    AtomicSingleton() = delete;

public:
    static std::shared_ptr<T> getInstance() {
        auto current = instance.load(std::memory_order_acquire);
        if (!current) {
            auto new_instance = std::make_shared<T>();
            // C++20 提供了原子共享指针操作
            if (!instance.compare_exchange_strong(current, new_instance,
                                                 std::memory_order_acq_rel)) {
                // 另一个线程已经创建了实例
                return current;
            }
            return new_instance;
        }
        return current;
    }
};
```
