---
title: 给我的genbook构建内核
author: jask
tags:
  - Linux
  - Arm
  - Mesa
  - Graphics
date: 2026-06-20
---

# 为什么？

rockchip用的BSP Kernel基于Android Linux 6.1，在Genbook上有一个非常严重的问题是Panthor内核驱动支持不完整，没有办法运行起来Vulkan，所以需要从Linux 6.12 backport到6.1。

此外为了用上LLVM工具链，还需要对编译参数和DRM部分的宏进行修改，这部分很简单，就不赘述。

# Mali GPU

先得搞明白几个基本问题。

## 用户态驱动

指考虑开源驱动，Mali GPU的开源驱动是Mesa Panfrost (OpenGL)和Mesa PanVK (Vulkan)。

目前Mesa支持到OpenGL 3.1和Vulkan 1.4，但是缺乏非常重要的geometryShader等Vulkan 1.0的核心特性，会导致Zink无法正常使用。

## 内核模块

Mali GPU的内核模块可以说是一团乱麻。内核态有老的Panfrost驱动（和用户态驱动重名了），新的Panthor驱动，闭源的Bifrost（这个名字还是Mali GPU的架构名）...

# 排查

排查过程首先粗略判断了一下，先用`vulkaninfo`确认了同时存在GPU和llvmpipe，接著验证了llvmpipe功能正常而Mali G610失败，这就可以判断用户态的驱动本身没有出错，实际问题出在内核态，用`strace也验证了这一点。

从Mesa驱动中会发现，`DRM_IOCTL_PANTHOR_BO_SET_LABEL`、`DRM_IOCTL_PANTHOR_BO_SYNC`、`DRM_IOCTL_PANTHOR_SET_USER_MMIO_OFFSET`以及`PANTHOR_SO_QUERY_INFO`这四个IOCTL会返回`-ENOTTY`，而内核的IOCTL表并没有导出这些。

问题基本确定下来了，就是内核驱动的问题，接下来要做的就是用Agent把Linux 6.12的源代码给迁移过来，然后构建内核并测试。

# 结果

经过一番折腾，对`drivers/gpu/drm/panthor`这里做了不少修改，成功跑起来了Vulkan，然而并不能使用Zink，因为Zink依赖Vulkan 1.0中的几个核心功能，如geometryShader，但是[Mesa PanVK](https://fosdem.org/2026/schedule/event/KZUJ9X-geometry_shaders_in_panvk_with_libpoly/) 目前还没有支持geometryShader，因此这个功能只能说未来可期。不管怎么说现在至少可以用上Vulkan的基本功能和OpenGL 3.1了，如果没有LLM要做到这个事情对我可能还要非常长的时间。
