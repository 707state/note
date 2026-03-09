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

# 开机

1. 从主板上的PMIC检测到Power键信号，这一部分内部时钟没有配置，仅有内部RAM用，一般都是厂商的私有驱动。
2. Bootload阶段，这一部分就是Uboot、Little Kernel或者ABL，去初始化DDR内存、时钟还有串口，这一步还需要把Linux Kernel镜像到内存，接下来跳转执行Kernel。代码在bootable/bootloader/以及arch/arm/boot或者arch/arm64/boot下。
3. Linux Kernel需要初始化内存管理、中断还有驱动，Power Supply驱动，Display驱动以及执行第一个用户空间进程init。这部分代码主要在kernel的drivers/power/supply/下，注册到/sys/class/power\_supply/管理POWER\_SUPPLY\_PROP\_*属性；Display驱动主要在Kernel drivers/gpu/drm或者drivers/video/fbdev/；init涉及到和用户态程序交互，主要和system/core/init/main.cpp, system/core/init/init.cpp，system/core/rootdir/init.rc有关。
4. Framework层：PowerManagerService，这是一个Android OS Service，代码主要在/frameworks/base/core/java/android/os/PowerManager.java, frameworks/base/services/core/java/com/android/server/power/PowerManagerService.java。PowerManager通过WakeLock控制屏幕状态，与之相关的HAL层在hardware/interfaces/light/这个AIDL接口和hardware/libhardware/include/hardware/lights.h里面。

代码：

| 层次 | 关键文件|
|:---:|:---:|
|Java API | `frameworks/base/core/java/android/os/PowerManager.java`|
|系统服务 |`frameworks/base/services/core/java/com/android/server/power/PowerManagerService.java` |
| Display服务| `frameworks/base/services/core/java/com/android/server/display/DisplayManager.java`|
| Init脚本 | `system/core/rootdir/init.rc` |
| 电源驱动 | `drivers/power/supply/power_supply_core.c`|
| 背光驱动 | `hardware/interfaces/light/` |

这样描述实际上太过简化了，具体的流程还是比较复杂的。

# 系统服务

Android的系统服务调用遵循一个统一的分层模式：

App应用层使用Java API，在Framework Service通过Binder IPC调用HAL暴露的接口，在HAL里面通过驱动进入Kernel Driver去修改硬件信息。

举个例子

## 屏幕亮度

亮度调节通过ContentProvider写入，在DisplayManager中通过setScreenBrightness设置内部变量，然后通过scheduleScreenUpdate去推送更新。这一过程涉及到回调函数的卸载与安装，每一次设置亮度都要用到scheduleScreenUpdate通过Handler删除已有的Runnable任务。既然有更新的推送就有更新的接收，这里面的接收方时PhotonicModulator，在setState中通知后台线程，后台线程是PhotonicModulator的run方法重写的，在这个线程里面获取到亮度之后通过DisplayBlanker把真实数据写入硬件；这是一个Interface，其具体实例在DisplayManagerService中定义，会调用LogicalDisplay、DisplayDevice、LogicalDisplayAdapter。接下来就是Binder通信的对象了，这部分并没有什么计算相关的代码，基本全部是调用链条，没有复杂的计算或者分派逻辑。

代码：

|层次|关键文件|
|:---:|:---:|
|Java API| `frameworks/base/services/core/java/com/android/server/display/`|
| SurfaceFlinger Client| `frameworks/native/libs/gui/SurfaceComposerClientcpp`|
| Binder IPC| `frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp` |
| HWCompositor| `frameworks/native/services/surfaceflinger/DisplayHardware/HWComposer.cpp` |
| Vendor HWComposer Implementation| AIDL|

## AutomaticBrightnessController

