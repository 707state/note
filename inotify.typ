= inotify
== 基本概念
inotify是Linux核心子系统之一，做为文件系统的附加功能，它可监控文件系统并将异动通知应用程序
inotify相较之下使用较少的file descriptor，亦允许select()与poll()接口，优于dnotify使用的信号系统。这也使得inotify与既有以select()或poll()为基础之函数库(如：Glib)集成更加便利
== 运作方式
```c
int inotify_add_watch(int fd, const char* pathname, int mask)
```
建立一个inotify的实体并回传一个file descriptor，此文件描述符可供读取文件事件。随后，可透过read()接收事件，为了避免不断轮询文件，read()默认将采用同步I/O的模式，直到事件发生后才会返回。
```c
int inotify_add_watch(int fd, const char* pathname, int mask)
```
透过路径名称(pathname)并选定掩码(mask)以监控inode。inotify_add_watch()会回传一个监控器（watch descriptor），它代表pathname指向的inode(不同的pathname有可能指向相同的inode)
```c
int inotify_rm_watch(int fd, int wd)
```
取消对某个路径之监控。

如同之前所描述的，当文件系统异动时，核心将会依据程序设置的条件，触发相应的事件

事件的结构如下:
#table(
  columns:2,
  table.header(
    [*字段名称i*],[*描述*]
  ),
 [wd],[监控子],
 [mask],[事件掩码],
 [cookie],[用来辨别IN_MOVED_FROM与IN_MOVED_TO事件],
 [len],[name字段长度],
 [name],[触发该事件的文件名称]
)
inotify无法监控软链接型的子目录。
