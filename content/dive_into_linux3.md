---
title: Linux构建系统
author: jask
tags:
  - Linux
date: 2026-06-30
series: Linux自我修养
---

# Linux的构建工具

最近在尝试在coolpi genbook的Linux 6.1内核上启用Rust，但无奈这个内核版本的Rust非常不成熟而且Rust版本太低，根本不能正常编译。在进行迁移之前，还是先了解一下Linux内核的代码组织方式和构建方式。

## Kconfig

通常的内核构建流程：

`make xxx_defconfig`->扫描整个目录树的Kconfig文件与`def_config`文件->生成`.config`文件->执行make根据`.config`编译。

以 [coolpi_kernel](https://github.com/coolpi-george/coolpi-kernel/tree/linux-6.1-stan-rkr3.1)为例，在`build-kernel.sh`中，执行了`make coolpi_linux_defconfig`。这会触发`scripts/kconfig/Makefile`里面的规则：

```make
%_defconfig: $(obj)/conf
	$(Q)$< $(silent) --defconfig=arch/$(SRCARCH)/configs/$@ $(Kconfig)
```

完整展开就相当于:

```bash
scripts/kconfig/conf --defconfig=arch/arm64/configs/coolpi_linux_defconfig Kconfig
```

这里的conf是一个通过`scripts/kconfig/conf.c`编译出来的程序，作用是：

1. 读入顶层 `Kconfig`，递归解析所有 `source` 指令（`init/Kconfig` → … → `drivers/Kconfig` → `drivers/block/Kconfig` 等），构建完整配置树
2. 读入 `coolpi_linux_defconfig` 中的显式设置（约 800 行），作为初始值
3. 对于 defconfig 中没有显式设置的选项，使用 Kconfig 中定义的 `default` 值或由 `select`/`depends on` 推导
4. 输出最终的 `.config

## Kbuild

经过上面的过程就已经得到了`.config`文件，但是这个文件并不直接参与Kbuild。

首先，Makefile通过:

```makefile
ifdef need-config
include include/config/auto.conf
endif
```

加载进来，从而对Kconfig进行展开。

```txt
# drivers/block/Makefile 第35行
obj-$(CONFIG_ZRAM) += zram/

# 展开后：
obj-y += zram/
```

接下来生成`include/generated/autoconf.h`，这是给C代码用的，通过条件编译的方式提供等价的`.config`配置项。

只要`.config`文件比`auto.conf`新，就会自动触发syncconfig。

## Make

现在`CONFIG_ZRAM`已经在`auto.conf`中，那么Makefile里面就会展开成`obj-y`的形式，在`scrips/Makefile.lib`中进一步处理：

```makefile
ifdef need-builtin
obj-y		:= $(patsubst %/, %/built-in.a, $(obj-y))
else
obj-y		:= $(filter-out %/, $(obj-y))
endif
```

这之后zram模块根据这个模块里面的Makefile脚本被编译为`built-in.a`，最后被链接进内核或者变为独立的ko。

## Image

make编译出来vmlinux之后，并不能直接参与到使用，而是会经过objcopy的脱壳。

```makefile
OBJCOPYFLAGS_Image :=-O binary -R .note -R .note.gnu.build-id -R .comment -S
```

这一步把vmlinux变为可以直接加载到内存并跳转执行的裸二进制代码，从而满足U-boot的需要。

## Rust

在BSP Kernel中，Rust的支持受限于版本，还非常不成熟，但是足以用于了解其工作方式。

在`arch/<arch>/Kconfig`中会判断是否启用了`HAVE_RUST`，如果启用了就会在`generate_rust_target`这个任务中通过bindgen工具对C头文件进行解析，自动生成Rust的`extern "C"`声明，从而在不侵入C代码的情况下生成Rust的C binding。
