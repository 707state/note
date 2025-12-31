<!--toc:start-->
- [异步api发展史](#异步api发展史)
  - [远古时代](#远古时代)
  - [select时代](#select时代)
  - [poll的引入](#poll的引入)
  - [epoll: reactor模式的解决方案](#epoll-reactor模式的解决方案)
- [libtask](#libtask)
  - [task.h](#taskh)
  - [task.c](#taskc)
  - [context.c](#contextc)
  - [qlock.c](#qlockc)
  - [Event-Driven?](#event-driven)
    - [poll](#poll)
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

# libtask

Russ Cox的一个项目，在C中实现的csp。

整体api设计非常接近go的机制（毕竟是go的作者之一）。

## task.h

主要是定义了Task, TaskList这些基本结构，还有QLock（任务队列的锁），RWLock（任务的读写锁），Rendez（任务的挂起与恢复，起条件变量的作用），Channel的基本结构。

## task.c

main函数放在这里，而调用libtask写的程序使用的是taskmain，其实是rsc认为main函数（用户写的逻辑）也只是整个调用链条的一个任务罢了（比较特殊的任务——任务的起点）。

这部分代码里面最重要的就是taskscheduler，任务调度部分。不过taskscheduler里面的调度器是很简单的，取出头部的任务进行执行，等待执行完成或者被挂起后回到taskscheduler里面。

## context.c

这里需要和asm.S这个文件一起看看。

不过这个东西的原理还是简单的，其实就是：getcontext函数用来把寄存器/PSTATE/PC这样的信息保存到一块内存上，setcontext。如果纯用汇编来写的话，可以简单地理解为保存运行时的现场，然后ldr/str恢复现场就行了。

## qlock.c

QLock的结构是：
```c
struct QLock{
    Task *owner;
    TaskList queue;
};
```

这里的owner用来指示qlock是否被持有。

## Event-Driven?

前面只是提到libtask是如何创建和管理协程的，但是具体到协程是如何与一个event notification机制协作的呢？

### poll

在libtask里面为了保证Unix平台的通用,使用了poll来做IO多路复用.

具体代码在fd.c的fdtask函数中:

```c
	for(;;){
		/* let everyone else run */
		while(taskyield() > 0)
			;
		/* we're the only one runnable - poll for i/o */
		errno = 0;
		taskstate("poll");
		if((t=sleeping.head) == nil)
			ms = -1;
		else{
			/* sleep at most 5s */
			now = nsec();
			if(now >= t->alarmtime)
				ms = 0;
			else if(now+5*1000*1000*1000LL >= t->alarmtime)
				ms = (t->alarmtime - now)/1000000;
			else
				ms = 5000;
		}
		// poll阻塞等待
		if(poll(pollfd, npollfd, ms) < 0){
			if(errno == EINTR)
				continue;
			fprint(2, "poll: %s\n", strerror(errno));
			taskexitall(0);
		}

		/* wake up the guys who deserve it */
		for(i=0; i<npollfd; i++){
		    // poll会修改这个revents字段,表示发生的事件
			while(i < npollfd && pollfd[i].revents){
			    //唤醒这个任务
				taskready(polltask[i]);
				--npollfd;
				// 用最后一个元素覆盖掉当前元素
				pollfd[i] = pollfd[npollfd];
				polltask[i] = polltask[npollfd];
			}
		}

		now = nsec();
		while((t=sleeping.head) && now >= t->alarmtime){
			deltask(&sleeping, t);
			if(!t->system && --sleepingcounted == 0)
				taskcount--;
			taskready(t);
		}
	}
```

这里就可以看明白poll是怎么在事件发生时执行相应任务的. 这里的fdtask会在第一次fdwait时被创建出来,这个任务是一个长期的任务.
