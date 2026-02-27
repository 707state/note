---
title: 在安卓上跑起来docker!
author: jask
tags:
    - Android
    - Linux
    - Container
date: 2026-02-27
---

# 起因

本来想尝试一下在安卓上使用mesa freedreno OpenGL ES驱动，但是发现kgsl backend并没有进入上游，只能用turnip驱动。好吧，那就不尝试了，但是还是想要对我手里的OnePlus 6好好折腾一番。

搜了一下发现有人成功在Android上跑起来docker，大概翻阅之后了解了怎么做，于是开始尝试。

首先向大佬致们以崇高敬意：[Morakhiyasaiyam](https://github.com/Morakhiyasaiyam/Docker-native-on-Termux-on-Android) 和 [FreddieOliveira](https://gist.github.com/FreddieOliveira/efe850df7ff3951cb62d74bd770dce27)。

# 怎么搞

安卓上欠缺一些Kernel特性，需要手动编译内核启用一些特性才行，这就需要用到check-config.sh那个脚本。这个脚本可以看到需要编译的配置项，有了这个文件加上已经root的安卓设备就可以继续。

# LineageOS
我用的是LineageOS来编译，配置好开发环境以后就去kernel/oneplus/sdm845/arch/arm64/configs/enchilada_defconfig进行修改，添加一些config之后croot再breakfast enchilada就开始编译了。

将编译产物boot.img和lineage-xxxxx-UNOFFICIAL.zip通过adb sideload和fastboot flash boot装入手机，并获取到root以后就去termux里面安装docker、iproute2、dnsutils等等，参考前面的大佬文章。

# 总结

今天没什么活（至少我没分配到），所以就用组里的构建服务器好好玩了一下，Android还是很棒的，等到手里的OnePlus pad 2 Pro新鲜劲过去了我会把这个平板也刷机，这不得比chromebook有性价比啊。
