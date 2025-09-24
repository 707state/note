<!--toc:start-->
- [并非Linux](#并非linux)
  - [基本概念](#基本概念)
  - [用途](#用途)
    - [监听stdin](#监听stdin)
    - [监听port](#监听port)
<!--toc:end-->

# 并非Linux

需要用到定时器的概念，但是我并不清楚应该怎么写一个定时器，先用kqueue来尝试一下。

## 基本概念

kqueue的基本API只有三个，如下：

```c
// 事件
struct kevent;
// 创建kqueue
int kqueue();
//第 2 个参数是要注册或修改的事件列表。第 4 个参数是返回的已触发事件列表。
int kevent();
//用宏来初始化 struct kevent，指定要监听的对象
#define EV_SET
```

## 用途

kqueue不止可以监听network/file，也可以用来处理内置定时器、信号、进程退出监控。

### 监听stdin

```c
#include <iostream>
#include <sys/event.h>
#include <unistd.h>
#define PROJECT_NAME "learn_kqueue"
#define exit_if(r,...) if(r) {printf(__VA_ARGS__); printf("error no: %d error msg %s\n", errno, strerror(errno)); exit(1);}
int main(int argc, char **argv) {
    int kq=kqueue();
    exit_if(kq==-1,"Failed to create kqueue");
    // 设置需要监听的事件
    struct kevent change;
    // 监听标准输入
    EV_SET(&change, STDIN_FILENO, EVFILT_READ,EV_ADD|EV_ENABLE,0,0,nullptr);
    // 事件循环
    while(true){
        // 设置需要接受发生的事件
        struct kevent event;
        int nev=kevent(kq,&change,1,&event,1,NULL);
        exit_if(nev==-1,"Kevent received -1");
        if(nev>0){
            if(event.filter==EVFILT_READ){
                char buf[256];
                ssize_t n=read(STDIN_FILENO,buf,sizeof(buf)-1);
                if(n>0){
                    buf[n]='\0';
                    buf[strcspn(buf,"\n")]='\0';
                    printf("You typed %s\n",buf);
                    if(strcmp(buf,"quit")==0){
                        break;
                    }
                }
            }
        }
    }

}
```

### 监听port

对于kqueue而言，监听一个端口其实包含了两种操作。

```c++
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sys/event.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
constexpr static int BACKLOG=16;
constexpr static int BUFSIZE=1024;
static int set_nonblocking(int fd){
  int flags=fcntl(fd,F_GETFL,0);
  return fcntl(fd,F_SETFL,flags| O_NONBLOCK);
}
int main(int argc, const char* argv[]){
  if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);

    // 创建监听套接字
    int listenfd=socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd<0) {
      perror("socket");
      exit(1);
    }
    set_nonblocking(listenfd);
    int opt=1;
    setsockopt(listenfd,SOL_SOCKET, SO_REUSEADDR,&opt,sizeof(opt));
    struct sockaddr_in addr{};
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(port);
    if(bind(listenfd, (struct sockaddr*)&addr,sizeof(addr))<0){
      perror("bind");
      exit(1);
    }
    if(listen(listenfd,BACKLOG)<0){
      perror("listen");
      exit(1);
    }
    // 创建kqueue
    int kq=kqueue();
    if(kq<0){
      perror("kqueue");
      exit(1);
    }
    struct kevent ev_set;
    EV_SET(&ev_set,listenfd,EVFILT_READ,EV_ADD,0,0,nullptr);
    if(kevent(kq,&ev_set,1,nullptr,0,nullptr)==-1){
      perror("kevent register");
      exit(1);
    }
    printf("Server listening on port %d...\n",port);

    // 事件循环
    struct kevent evlist[32];
    while(true){
      int nev=kevent(kq,nullptr,0,evlist,32,nullptr);
      if(nev<0){
        if(errno==EINTR)continue;
        perror("kevent wait"); break;
      }
      for(int i=0;i<nev;i++){
        int fd=(int)evlist[i].ident;
        if(fd==listenfd){
          // 新的连接
          struct sockaddr_in cli;
          socklen_t len=sizeof(cli);
          int connfd=accept(listenfd, (struct sockaddr*)&cli,&len);
          if(connfd>=0){
            set_nonblocking(connfd);
            EV_SET(&ev_set,connfd,EVFILT_READ,EV_ADD,0,0,nullptr);
            kevent(kq,&ev_set,1,nullptr,0,nullptr);
            printf("New client: %s:%d\n",
          inet_ntoa(cli.sin_addr),ntohs(cli.sin_port));
          }
        }else if(evlist[i].filter==EVFILT_READ){
          // 客户端可读
          char buf[BUFSIZE];
          ssize_t n=read(fd,buf,sizeof(buf));
          if(n<=0){
            // 关闭连接
            close(fd);
            EV_SET(&ev_set,fd,EVFILT_READ,EV_DELETE,0,0,nullptr);
            kevent(kq,&ev_set,1,nullptr,0,nullptr);
            printf("Client disconnected! (fd=%d)\n",fd);
          }else{
            write(fd,buf,n);
          }
        }
      }
    }
}
```


这段代码里面就非常好的展示了kqueue的基本工作理念。

一个kevent记录中最重要的字段：

ident: 事件的主体，通常就是一个文件描述符。
filter: 事件类型过滤器，比如哦EVFILT\_READ, EVFILT\_WRITE, EVFILT\_TIMER等。
flag: 状态标志，比如EV\_ADD, EV\_DELETE, EV\_EOF这些。
udata: 可以存储上下文数据。

向 kqueue 注册一个“监听某个 fd 的某类事件”的规则后，当该事件发生时，kevent() 返回的 ident 就是这个 fd，filter 告诉你是哪种事件。

在我们的例子里面，kqueue启动时只做了一个事情，就是：

```c
EV_SET(&ev_set, listenfd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
kevent(kq, &ev_set, 1, nullptr, 0, nullptr);
```

监听listenfd这个文件描述符的可读事件，这样每当listenfd上有一个新的连接时就会可读。

后面每一次accept时，又会对connfd进行注册。相当于是把connfd这个连接对应的连接加入到kqueue里面进行监听，每当其上有可读事件（发送过来了消息）时，就会触发EVFILT\_READ。

也就是说，每一次唤醒时，其监听的fd只可能是listenfd或者每一个connection的fd，而对于非listenfd的情况，其代表的就是每一个客户端是可读的。
