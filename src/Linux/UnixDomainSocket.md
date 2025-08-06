<!--toc:start-->
- [基本概念](#基本概念)
- [优势](#优势)
- [常见问题](#常见问题)
  - [Unix Domain Socket 与 TCP/IP Socket 有哪些区别？](#unix-domain-socket-与-tcpip-socket-有哪些区别)
  - [在哪些场景下推荐使用 UDS？](#在哪些场景下推荐使用-uds)
  - [如何保证 UDS 的安全性？](#如何保证-uds-的安全性)
<!--toc:end-->

# 基本概念

- 用途：
Unix Domain Socket 主要用于同一台机器上的进程间通信，比使用 TCP/IP 协议栈的网络 Socket 更高效，因为它省去了网络协议的开销。

- 地址形式：
UDS 使用文件系统中的路径作为地址，比如 /tmp/my_socket。此外也支持匿名（abstract）命名空间（主要在 Linux 上）。

- 传输类型：
支持字节流（SOCK_STREAM）（类似 TCP）和数据报（SOCK_DGRAM）（类似 UDP）。

# 优势

- 性能：
由于不经过网络协议栈，传输延迟和 CPU 消耗较低。

- 安全性：
通信仅限于本机，文件系统权限可以控制访问。

- 简单性：
只需要处理本地文件路径，无需考虑网络地址和端口号。

# 常见问题

## Unix Domain Socket 与 TCP/IP Socket 有哪些区别？

- UDS 仅限本机进程间通信，TCP/IP 则可用于局域网或互联网。

- UDS 不涉及网络协议栈，因此性能更高。

- UDS 使用文件系统路径作为地址，而 TCP/IP 使用 IP 地址和端口号。

## 在哪些场景下推荐使用 UDS？

- 本机服务间通信，如数据库客户端与服务器之间。

- 日志收集、系统管理工具等对性能要求较高的场景。

## 如何保证 UDS 的安全性？

- 使用文件系统权限（chmod/chown）限制 socket 文件的访问。

- 在设计时确保只允许可信进程连接。
