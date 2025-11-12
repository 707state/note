<!--toc:start-->
- [异步api发展史](#异步api发展史)
  - [远古时代](#远古时代)
  - [select时代](#select时代)
  - [poll的引入](#poll的引入)
  - [epoll: reactor模式的解决方案](#epoll-reactor模式的解决方案)
<!--toc:end-->

# 异步api发展史

## 远古时代

Unix远古时代的异步API是不存在的。

远古时代的连接是：一个进程一个连接，所以就有了[C10K问题](https://en.wikipedia.org/wiki/C10k_problem)。


进程数目是不可能无限增加的，因为进程的上下午切换的开销非常大，并且IPC机制也一个比一个慢。

这就是为什么引入了线程，换句话说，Thread这个东西一开始引入就是为了实现异步。在DOS时代不存在多任务，就会导致任务的阻塞。

在Windows时代，Thread被引入到了PC里面，也就是多任务系统。即使多任务系统在此之前就已经存在了，其在PC的大规模应用还是要等一等。

## select时代

Unix很早便有了一个叫做select的系统调用。select的作用可以让一个进程在 单线程 下监控 多个文件描述符（fd） 的状态，从而避免阻塞在单个 I/O 操作上。

问题1: fd set用一个位数组表示，也就是说监听的fd是有限的（Linux下默认为1024），这显然不够用。

问题2: 内核对于select的fd就绪是采用的线性扫描，也就是说遍历一遍fd set，开销巨大。

问题3: 受阻塞和超时限制，select 在 阻塞模式 下，调用期间 CPU 不能做其他事情。

## poll的引入

poll这个系统调用使用动态大小的数组来表示fd，解决了select的问题1。

对于pollfd数组不会修改，只会返回revents，不会修改fd集合，解决了问题2。

但是poll还是有问题，也就是线性扫描、大数组复制开销、没有事件通知模式（只能水平触发没有边沿触发）。

## epoll: reactor模式的解决方案

epoll：事件驱动 + 内核维护就绪列表 → O(1) 操作，非常适合高并发场景。

这里面内核维护就绪列表，不再线性扫描，并且不需要每一次调用时复制数组，fd在内核只需要注册一次。

来看一段样例代码。

```c++
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

constexpr int BACKLOG  = 16;
constexpr int BUFSIZE  = 1024;
constexpr int MAXEVENT = 32;

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);

    // 1. 创建监听 socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(listenfd);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);
    if (bind(listenfd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(listenfd, BACKLOG) < 0) {
        perror("listen"); exit(1);
    }

    // 2. 创建 epoll 实例
    int epfd = epoll_create1(0);
    if (epfd < 0) { perror("epoll_create1"); exit(1); }

    epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.fd = listenfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) < 0) {
        perror("epoll_ctl listenfd"); exit(1);
    }

    printf("Server listening on port %d...\n", port);

    // 3. 事件循环
    epoll_event events[MAXEVENT];
    while (true) {
        int nfds = epoll_wait(epfd, events, MAXEVENT, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (fd == listenfd) {
                // 新连接
                sockaddr_in cli{};
                socklen_t len = sizeof(cli);
                int connfd = accept(listenfd, (sockaddr*)&cli, &len);
                if (connfd >= 0) {
                    set_nonblocking(connfd);
                    epoll_event cev{};
                    cev.events  = EPOLLIN;
                    cev.data.fd = connfd;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &cev);
                    printf("New client: %s:%d\n",
                           inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));
                }
            } else if (events[i].events & EPOLLIN) {
                // 客户端可读
                char buf[BUFSIZE];
                ssize_t n = read(fd, buf, sizeof(buf));
                if (n <= 0) {
                    // 断开连接
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                    close(fd);
                    printf("Client disconnected (fd=%d)\n", fd);
                } else {
                    write(fd, buf, n);  // echo 回显
                }
            }
        }
    }

    close(listenfd);
    close(epfd);
    return 0;
}
```

这是一个TCP的echo server。
