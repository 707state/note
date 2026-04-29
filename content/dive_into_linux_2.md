---
title: 从User API了解内核
author: jask
date: 2026-01-23
tags:
  - Linux
  - OS
series: Linux自我修养
---

# unshare

传统的`fork()/clone()`系统调用都会继承当前namespace的资源，父进程和子进程看到的是一样的，而`unshare()`系统调用提供了一种在不能或不想要创建新进程时在“当前进程内部”改变资源隔离关系的能力。
