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
