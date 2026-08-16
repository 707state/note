---
title: 初识ESP32S3
author: jask
date: 2026-08-16
tags:
  - ESP32
  - Embedded
series: Embedded
---

# ESP-IDF工具链和项目组织方式

以`ESP32-S3-ePaper-1.54/02_Example/ESP-IDF/V2/10_LVGL_V9_Test`这个项目为例，这个项目依赖了FreeRTOS、LVGL以及开发板的驱动，其中FreeRTOS是ESP-IDF工具链自带的，LVGL通过IDF Component Manager拉取，Board Driver这块比较复杂一点，包含四个组件，每个组件都会有一个`idf_component_register`：`board_power_bsp`、`epaper_driver_bsp`、`ui_bsp`以及`user_app`这四个模块。

main是一个特殊的模块，esp-idf工具链会自动把所有组件链接到main中而不需要在cmake里面写REQUIRES。

在esp-idf项目中，组件有四个来源：

1. `idf_components`: 这是有idf工具链自带的，在安装路径的components/目录下。
2. `project_managed_components`: 这是由IDF Component Manager拉取的。
3. `project_extra_components`: 这是由`EXTRA_COMPONENT_DIRS`指定的。
4. `project_components`: 这是放在工程的components和main目录下的。

这是基本的idf项目的构成方式，接下来看一下esp工具链。

esp工具链基本可以视为一系列松散的脚本组合。esp32的那些riscv/xtensa gcc编译器、components的各种内置组件、tools包含的gdb/trace工具等等。由于MCU本身的限制，跑完整的Linux基本别想了，能够运行FreeRTOS和Zephyr，运气好也可以运行NuttX，不过可以运行什么RTOS都是次要的，这些RTOS能够提供上下文切换、互斥还有调度能力就够了。

我主要想要在一块epaper的开发板上做一些图形学基本概念的实践，搞清楚一些最基本的概念之后再说复杂的事情。下一次就看一下FreeRTOS和bsp是怎么回事吧。
