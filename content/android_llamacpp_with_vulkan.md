---
title: 在Android中用上硬件加速的llama.cpp
author: jask
tags:
    - Android
    - Vulkan
    - LLM
date: 2026-02-08
---

# 准备工作

termux的源已经把llama.cpp和ollama添加了，并且都有Vulkan和OpenCL后端的支持。这里只是写给想要试试源码构建的人。

[下载F-Droid](https://f-droid.org/zh_Hans/), 从F-Droid上搜索Termux并安装。

打开Termux，运行termux-change-repo，选择第一个，然后选择China-Mainland换源，完成后执行
```bash
pkg install clang vulkan-headers vulkan-loader-android vulkan-tools cmake shaderc git
```

接下来可以验证一下是否有了合适的Vulkan驱动。

在termux中输入
```bash
vulkaninfo --summary
```

注意看输出的内容中的deviceName或者driverName，如果是llvmpipe则需要执行

```bash
pkg uninstall vulkan-loader-generic
```

# llama.cpp

获取llama.cpp的源代码，建议打开代理并且把允许应用绕过关掉。
```bash
git clone https://github.com/ggml-org/llama.cpp --depth 1 && cd llama.cpp
```

先把~/.local/bin添加到PATH中：
```bash
# 如果是bash
echo 'EXPORT PATH="$PATH:~/.local/bin"' > ~/.bashrc #
source ~/.bashrc
# 如果是fish
cd ~/.local/bin
fish_add_path -p $(pwd)
```

然后执行开始构建项目（这一步比较慢）：

```bash
cmake -B build -DGGML_VULKAN=ON -DCMAKE_INSTALL_PREFIX=~/.local
cmake --build build --config Release
cmake --install build
```

这些执行完后就成功安装了llama.cpp，可以开始下载模型了。

# Model

下载gemma3，这里可以用现成的（需要打开代理）:
```bash
llama-cli -hf ggml-org/gemma-3-1b-it-GGUF
```

开始下载会看到一个进度条，下载完成后就会看到进入了会话中，可以进行简单的对话了。

如果觉得终端不好用，可以用llama-server：

```bash
llama-server -m .cache/llama.cpp/ggml-org_gemma-3-1b-it-GGUF_gemma-3-1b-it-Q4_K_M.gguf --port 8989
```

等待加载，然后在你的手机浏览器中输入localhost:8989就可以在网页中使用了。

# 其他

如果觉得在手机终端中使用很费劲，可以用ssh。确保电脑和手机处于同一个局域网中，在termux中执行sshd -p <port>，通过whoami查看一下用户名，ifconfig查看ip，passwd设置密码，然后在电脑上ssh -p <port> <user>@<ip>，输入密码就可以连上了。

