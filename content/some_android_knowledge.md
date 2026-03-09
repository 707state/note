---
title: 关于安卓的知识
author: jask
tags:
    - Linux
    - Android
    - Arm
date: 2026-03-09
---

# 总览

Android/Linux归根结底是Linux，很多知识和GNU/Linux是共同的，但是不同点也很多：

1. GNU/Linux使用的是glibc，而android使用的是bionic libc。而且不像glibc和musl对于Linux的通用性，bionic有很多设计就是针对低功耗的移动设备的。
2. init过程区别很大。常见的Linux发行版的会使用systemd来作为init system，一些老版本发行版也可能会用sysvinit，gentoo也有openrc的构建，但大体都遵从kernel启动init后就把控制权移交给用户空间的流程。但是Android的init则是一个多段流程的启动，涉及到init进程自身多次execv自己；首先是first stage，这一阶段挂载基础文件系统、加载内核模块、挂载/system；之后通过execv的方式去启动SELinux，在SELinux中加载完整SELinux策略以及restorecon；这之后又通过execv去启动second stage，这一阶段才执行解析rc、启动服务、属性系统还有epoll。此外，systemd采用dag并行展开，而Android init只是一个三段execv，对于并行化支持非常有限，而且不涉及到dbus等服务，非常精简。
3. 内核模块的差别巨大。Android有大量的私有闭源驱动，这些驱动受制于GPL协议没有办法直接进入主线Linux，因此Android通过HAL层，让内核只通过ioctl暴露接口给用户态HAL，因此每款Soc都有自己的内核分支，包含大量的私有补丁、私有驱动。手机厂商本身又会对系统进行各种调优和改动，这时候就与上游差别很大了。Android 12开始，谷歌开始推动Generic Kernel Image，厂商驱动作为独立的ko模块存在，通过稳定的Kernel Module Interface连接。
4. 图形API和Window Manager完全不同。Linux通常用X11或者Wayland Client去和X Server或者Wayland Compositor进行通信，Compositor则通过Mesa（用户态）和DRM或者KRM去调用GPU硬件。Android上则是每个窗口对应一个Surface，Surface通过SurfaceFlinger去调用OpenGL ES或者HWC进行合成，通过Gralloc进行图形内存分配，再通过帧缓冲交换上屏。SurfaceFlinger从设计开始就是合成与显示管理统一的，而X11一直被诟病的地方就是多次拷贝。而且Android很早就引入了fence机制，避免CPU等待GPU。最大的差异就是HWC，Linux上每一个Layer的合成都需要OpenGL渲染，SurfaceFlinger则是在合成前先查询HWC，通过Overlay的方式进行叠加。
5. 进程间通信机制。这个差异非常重要，后面细细道来。