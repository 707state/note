# HTTP1.1没有Keep-Alive!

http1.1使用的是persistent-connection的机制来显式Keep-Alive。默认开启，在事务处理结束之后将连接关闭，应用程序必须在报文中显式地添加Connection: close的首部。
