<!--toc:start-->
- [所谓协程](#所谓协程)
  - [Why Asio？](#why-asio)
  - [协程](#协程)
  - [timer？](#timer)
    - [怎么用？](#怎么用)
  - [awaitable operators](#awaitable-operators)
  - [任务](#任务)
  - [read/write](#readwrite)
- [signal_set？](#signalset)
<!--toc:end-->

# 所谓协程

从定义上来看，协程就是协作时多任务，换句话说就和操作系统的抢占式调度相反，协程可以自行交出CPU的所有权给别的协程。

## Why Asio？

协程往往在用户态实现，这里涉及到一个异步模型的问题。

Reactor和Proactor两种模型，其实就是回调函数和完成队列两者设计带来的差异。

Asio在协程上的设计显然是远远强于回调函数的。

## 协程

协程的最重要特点就是自行交出CPU的所有权。

举一个例子：

```c++
#include <asio.hpp>
#include <chrono>
#include <iostream>

#include "asio/detached.hpp"
#include "asio/io_context.hpp"
#include "asio/steady_timer.hpp"
#include "asio/this_coro.hpp"
#include "asio/use_awaitable.hpp"
bool is_running_=true;
asio::awaitable<void> async_main()
{
  while (is_running_) {
    std::cout << "This is on async main function!!!\n";
    std::cout << "I need to see watchdog!\n";
  }
  co_return;
}
asio::awaitable<void> watch_dog(asio::steady_timer& timer)
{
  timer.expires_after(std::chrono::milliseconds(400));
  co_await timer.async_wait(asio::use_awaitable);
  is_running_=false;
  co_return;
}
int main()
{
  asio::io_context io_context;
  asio::steady_timer timer(io_context);
  asio::co_spawn(io_context, async_main(), asio::detached);
  asio::co_spawn(io_context,watch_dog(timer),asio::detached);
  io_context.run();
}
```

这里看起来是有一个co\_spawn的，那么你会认为这个程序会在两个在同一个io\_context的协程上来回切换？

可以跑一下试试，然后会发现这个程序会在async\_main里面一直运行。

为什么没有执行watch\_dog呢？

请思考一下协程的定义，也就是主动交出所有权这里，async\_main并没有交出所有权，那么他就是会占有这个CPU不断运行，除非你主动通过co\_await来交出所有权，这样就能调度到别的协程上。

改进版本：

```c++
#include <asio.hpp>
#include <chrono>
#include <iostream>

#include "asio/detached.hpp"
#include "asio/io_context.hpp"
#include "asio/steady_timer.hpp"
#include "asio/this_coro.hpp"
#include "asio/use_awaitable.hpp"
bool is_running_=true;
asio::awaitable<void> async_main()
{
  while (is_running_) {
    std::cout << "This is on async main function!!!\n";
    std::cout << "I need to see watchdog!\n";
    asio::steady_timer timer(co_await asio::this_coro::executor);
    timer.expires_after(std::chrono::milliseconds(100));
    co_await timer.async_wait(asio::use_awaitable);
  }
  co_return;
}
asio::awaitable<void> watch_dog(asio::steady_timer& timer)
{
  timer.expires_after(std::chrono::milliseconds(400));
  co_await timer.async_wait(asio::use_awaitable);
  is_running_=false;
  co_return;
}
int main()
{
  asio::io_context io_context;
  asio::steady_timer timer(io_context);
  asio::co_spawn(io_context, async_main(), asio::detached);
  asio::co_spawn(io_context,watch_dog(timer),asio::detached);
  io_context.run();
}
```

在async\_main上加了一个新的定时器，这样就能每100毫秒挂起，io\_context调度到下面，而下面又因为定时器没有超时而挂起，执行权又被交给async\_main，直到再一次调度到watch\_dog且timer超时，is\_running被设置为false结束运行。

这样你就能观察到打印四次就退出了。

## timer？

Asio定时器的设计是非常有意思的，如何用好Timer非常值得思考。

### 怎么用？

这里有一个事情就是说Asio Timer的超时操作是会被覆盖的，这是因为：

1. steady_timer 不是 CPU 时钟，而是一个 可等待的异步事件源。
2. 它注册到 io_context 里，告诉事件循环：“请在未来某个时间点触发我”。
3. 当时间到，它就会在事件队列里生成一个“已完成”事件，唤醒所有等待它的操作。

换句话说，Timer最好被当作“一次性、专门为某个等待事件准备的定时器”，而不是一个全局用来多处协程“轮流等待”的资源。

最保守的用法应该是每一个协程里面有一个定时器，这样的做法应该是最稳妥的。

## awaitable operators

刚才的代码中用到了全局变量，而全局变量永远不是最好的实践，就是说，怎么才能干掉这个全局变量呢？

```c++
#include <asio.hpp>
#include <chrono>
#include <iostream>
#include <random>
#include "asio/experimental/awaitable_operators.hpp"
#include "asio/io_context.hpp"
#include "asio/steady_timer.hpp"
#include "asio/this_coro.hpp"
static auto generate_random_delay(int low, int high)
{
  static std::random_device device;
  static std::mt19937 gen(device());
  std::uniform_int_distribution dist(low, high);
  return std::chrono::milliseconds(dist(gen));
}
asio::awaitable<void> main_loop(asio::steady_timer &timer)
{
  while (true) {
    std::cout << "This is on async main function!!!\n";
    std::cout << "I need to see watchdog!\n";
    timer.expires_after(std::chrono::milliseconds(100));
    co_await timer.async_wait(asio::use_awaitable);
  }
  co_return;
}
asio::awaitable<void> async_main()
{
  auto executor = co_await asio::this_coro::executor;
  // 创建两个计时器
  asio::steady_timer main_timer(executor);
  asio::steady_timer watchdog_timer(executor);
  // 设置watchdog计时器
  watchdog_timer.expires_after(generate_random_delay(180, 400));
  using namespace asio::experimental::awaitable_operators;
  co_await (main_loop(main_timer) ||
            watchdog_timer.async_wait(asio::use_awaitable));
  std::cout << "Watchdog timer expired, shutting down...\n";
}
int main()
{
  asio::io_context io_context;
  asio::co_spawn(io_context, async_main(), asio::detached);
  io_context.run();
  return 0;
}
```

在这个demo里面，我们成功地把is\_running这个全局变量干掉了，通过asio提供的awaitable operator做到了组合的效果，但是，如果我们希望对不同的结果进行处理，怎么办？

这里可以利用返回值：

<details>

```c++
#include <asio.hpp>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <variant>

#include "asio/detached.hpp"
#include "asio/experimental/awaitable_operators.hpp"
#include "asio/io_context.hpp"
#include "asio/steady_timer.hpp"
#include "asio/this_coro.hpp"
#include "asio/use_awaitable.hpp"

asio::awaitable<void> handle_normal_completion(const asio::any_io_executor& executor);
asio::awaitable<void> handle_timeout(const asio::any_io_executor& executor);

static auto generate_random_delay(int low, int high)
{
  static std::random_device device;
  static std::mt19937 gen(device());
  std::uniform_int_distribution<int> dist(low, high);
  return std::chrono::milliseconds(dist(gen));
}

// 返回一个值以便我们知道循环是否正常完成
asio::awaitable<bool> main_loop(asio::steady_timer& timer)
{
  for (int i = 0; i < 4; ++i) {
    std::cout << "This is on async main function!!! Task " << i + 1 << "/4\n";
    std::cout << "I need to see watchdog!\n";

    timer.expires_after(std::chrono::milliseconds(50));
    co_await timer.async_wait(asio::use_awaitable);
  }
  // 如果所有任务都完成了，返回true
  co_return true;
}

// 添加一个watchdog函数，返回bool
asio::awaitable<bool> watchdog(asio::steady_timer& timer)
{
  co_await timer.async_wait(asio::use_awaitable);
  // 返回false表示这是超时结果
  co_return false;
}

static asio::awaitable<void> async_main()
{
  using namespace asio::experimental::awaitable_operators;
  auto executor = co_await asio::this_coro::executor;

  // 创建两个计时器
  asio::steady_timer main_timer(executor);
  asio::steady_timer watchdog_timer(executor);

  // 设置watchdog计时器
  watchdog_timer.expires_after(generate_random_delay(180, 450));

  // 使用awaitable operators并获取结果
  // 现在两个分支都返回bool
  auto result = co_await (main_loop(main_timer) || watchdog(watchdog_timer));

  // 处理结果
  if (result.index() == 0) {
    // 第一个分支完成 - main_loop正常结束
    bool success = std::get<0>(result);
    if (success) {
      std::cout << "Main loop completed successfully without timeout!\n";
      // 在这里添加正常完成的处理逻辑
      co_await handle_normal_completion(executor);
    }
    else {
      std::cout << "Main loop failed but didn't timeout\n";
      // 处理main_loop内部失败的情况
    }
  }
  else {
    // 第二个分支完成 - 发生了超时
    std::cout << "Watchdog timer expired, operation timed out!\n";
    // 在这里添加超时处理逻辑
    co_await handle_timeout(executor);
  }
}

// 处理正常完成的函数
asio::awaitable<void> handle_normal_completion(const asio::any_io_executor& executor)
{
  std::cout << "Performing normal completion actions...\n";
  // 例如：保存状态，发送成功通知等
  asio::steady_timer timer(executor);
  timer.expires_after(std::chrono::milliseconds(50));
  co_await timer.async_wait(asio::use_awaitable);
  std::cout << "Normal completion handling finished\n";
}

// 处理超时的函数
asio::awaitable<void> handle_timeout(const asio::any_io_executor& executor)
{
  std::cout << "Performing timeout recovery actions...\n";
  // 例如：记录错误，重置状态，通知管理员等
  asio::steady_timer timer(executor);
  timer.expires_after(std::chrono::milliseconds(50));
  co_await timer.async_wait(asio::use_awaitable);
  std::cout << "Timeout handling finished\n";
}

int main()
{
  asio::io_context io_context;
  asio::co_spawn(io_context, async_main(), asio::detached);
  io_context.run();
  return 0;
}
```

</details>

这里可以通过std::variant来获取对应结果。这里需要注意，void返回的事monostate，建议再多封装一次。

## 任务

从asio来看，一个任务就是被co\_spawn调用的函数和其整个调用链。

一个任务包含许多co_await带来的上下文交换，在无栈协程中并不一定涉及到栈指针交换，因为无栈协程可以用编译期状态机转换，而如果是有栈协程就可以采用栈指针的交换。

## read/write

Asio有一个非常重要的设计，就是async\_read\_some和async\_read之间的区别。

async\_read\_some不保证读取指定的全部数据，只保证至少读取一个字节（如果有数据可用）。他是一个流式地输入接收。


一般会直接映射到系统调用。

可以用于处理协议解析，比如说处理协议里面长度可变部分数据的接收。

# signal_set？

Asio提供了一个处理系统信号的能力，它允许程序以异步的方式响应操作系统发送的各种信号，aka一个信号的handler。

1. 创建 signal_set 对象：首先需要创建一个 signal_set 实例，并指定要监听的信号。
2. 注册回调函数：通过 async_wait() 方法注册一个回调函数，当指定的信号发生时，这个回调函数会被调用。
3. 处理信号：在回调函数中实现对信号的处理逻辑。
4. 重新注册：如果需要继续监听信号，需要在回调函数中再次调用 async_wait()。

```c++
asio::awaitable<void> async_main(asio::io_context& io_context)
{
  asio::signal_set signal(io_context, SIGINT, SIGTERM);
  //等待注册的信号（由操作系统）发送
  auto sig_num = co_await signal.async_wait(asio::use_awaitable);
  //这之后就说明signal已经被触发，获得到了sig_num，也就是信号的值
  ...
}
```
