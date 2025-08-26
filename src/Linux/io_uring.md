<!--toc:start-->
- [基本原理](#基本原理)
  - [基本流程](#基本流程)
<!--toc:end-->

# 基本原理

用两个共享队列：SQ是一个用户态的submission queue,CQ是内核态完成I/O之后把结果写回给用户态。

glibc没有提供io\_uring的封装，需要自行实现。

## 基本流程

1. io\_uring\_setup创建出来ring buffer。
2. 用户程序用mmap这两个环形队列。
3. 提交I/O：把请求写入SQ,然后调用io\_uring\_enter提交给内核。
4. 内核处理，结果写出CQ。
5. 用户读取CQ,这样就知道那个请求完成了。
