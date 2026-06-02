---
title: 初步了解Hexagon NPU
author: jask、deepseek v4 pro
tags:
  - NPU
  - Android
date: 2026-06-01
---

# LocalDream NPU (Qualcomm Hexagon) 运行机制全解析

## 1. 架构概览

LocalDream 是一个原生 Android App，在设备端利用 Qualcomm Hexagon NPU 运行 Stable Diffusion 推理。系统由两层组成：

```
┌─────────────────────────────────────────────────────┐
│  Android App (Kotlin)                               │
│  ┌───────────────────────────────────────────────┐  │
│  │  BackendService.kt                            │  │
│  │  - 管理子进程生命周期                          │  │
│  │  - 复制 QNN 库到运行时目录                     │  │
│  │  - 设置环境变量 (LD_LIBRARY_PATH,             │  │
│  │    DSP_LIBRARY_PATH)                          │  │
│  │  - 启动 libstable_diffusion_core.so           │  │
│  └───────────────┬───────────────────────────────┘  │
│                  │ ProcessBuilder                   │
│                  │ HTTP (localhost:8081)             │
└──────────────────┼──────────────────────────────────┘
                   │
┌──────────────────┼──────────────────────────────────┐
│  Native C++ Process                                 │
│  ┌───────────────────────────────────────────────┐  │
│  │  main.cpp (3851 lines)                         │  │
│  │  - 解析 CLI 参数                              │  │
│  │  - 动态加载 QNN 库 (dlopen)                   │  │
│  │  - 创建并初始化 QNN 模型                      │  │
│  │  - 运行 Stable Diffusion 推理管线             │  │
│  │  - 启动 HTTP Server (cpp-httplib)             │  │
│  └───────────────┬───────────────────────────────┘  │
│                  │                                   │
│  ┌───────────────┼───────────────────────────────┐  │
│  │  QNN 模型层                                    │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐      │  │
│  │  │ CLIP     │ │ UNet     │ │ VAE      │      │  │
│  │  │ QNN Model│ │ QNN Model│ │ QNN Model│      │  │
│  │  └────┬─────┘ └────┬─────┘ └────┬─────┘      │  │
│  │       │             │            │             │  │
│  └───────┼─────────────┼────────────┼─────────────┘  │
│          │             │            │                 │
│  ┌───────┼─────────────┼────────────┼─────────────┐  │
│  │  QNN SDK Runtime                                 │  │
│  │  ┌──────────────────────────────────────────┐   │  │
│  │  │ libQnnHtp.so  (HTP Backend Driver)       │   │  │
│  │  │ libQnnSystem.so (System Interface)        │   │  │
│  │  │ libQnnHtpV{68..81}.so (DSP Compute)       │   │  │
│  │  │ libQnnHtpV{68..81}Skel.so (DSP Skeleton)  │   │  │
│  │  └──────────────────────────────────────────┘   │  │
│  └────────────────────┬─────────────────────────────┘  │
│                       │                                │
│  ┌────────────────────┴─────────────────────────────┐  │
│  │  Hexagon DSP / NPU Hardware                       │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

**关键设计点：**
- `libstable_diffusion_core.so` 虽然名为 `.so`，实际是一个 **PIE 可执行文件**（PIE = Position Independent Executable），被 `ProcessBuilder` 作为子进程启动
- 整个推理过程在子进程内完成，通过 HTTP (localhost:8081) 与 Android App 通信
- QNN 库通过 `dlopen()` 动态加载，与 NPU 硬件交互

---

## 2. QNN 库的加载与初始化

### 2.1 库清单

QNN (Qualcomm Neural Network) SDK 提供了运行在 Hexagon DSP 上的 AI 推理能力。LocalDream 使用两组库：

| 库文件 | 用途 | 加载方式 |
|--------|------|----------|
| `libQnnHtp.so` | HTP Backend 驱动（CPU 侧） | `dlopen(DL_NOW \| DL_GLOBAL)` |
| `libQnnSystem.so` | System 接口（二进制检视、上下文管理） | `dlopen(DL_NOW \| DL_LOCAL)` |
| `libQnnHtpV{68..81}Stub.so` | HTP Stub（架构兼容层） | 运行时自动发现 |
| `libQnnHtpV{68..81}.so` | DSP 端计算库（unsigned） | 运行时加载到 DSP |
| `libQnnHtpV{68..81}Skel.so` | DSP 端骨架库 | 运行时加载到 DSP |

这些库在构建时从 QNN SDK 复制到 `assets/qnnlibs/`，运行时被 App 复制到 `filesDir/runtime_libs/`。

### 2.2 动态加载流程

代码路径：

```
main.cpp:876  getQnnSystemFunctionPointers()
  → DynamicLoadUtil.cpp:130  dlopen("libQnnSystem.so")
  → 解析 QnnSystemInterface_getProviders 符号
  → 调用 getSystemInterfaceProviders() 获取系统 API 函数表
  → 结果存入全局变量 g_qnnSystemFuncs.qnnSystemInterface

main.cpp:278  getQnnFunctionPointers()
  → DynamicLoadUtil.cpp:42  dlopen("libQnnHtp.so")
  → 解析 QnnInterface_getProviders 符号
  → 调用 getInterfaceProviders() 获取后端 API 函数表
  → 匹配 API 版本 (QNN_API_VERSION_MAJOR/MINOR)
  → 结果存入 funcs.qnnInterface
```

**关键源码位置：**
- `qairt/.../SampleApp/src/Utils/DynamicLoadUtil.hpp` - 函数声明
- `qairt/.../SampleApp/src/Utils/DynamicLoadUtil.cpp` - `getQnnFunctionPointers()` (line 35), `getQnnSystemFunctionPointers()` (line 123)
- `qairt/.../SampleApp/src/SampleApp.hpp` - `QnnFunctionPointers` 结构体定义 (line 34)

### 2.3 QNN API 核心对象

加载完成后，获得以下核心 API 对象：

```
qnnInterface {
    backendCreate()        // 创建后端上下文
    deviceCreate()         // 创建设备句柄
    contextCreateFromBinary()  // 从序列化 .bin 创建推理上下文
    graphRetrieve()        // 获取图句柄
    graphExecute()         // 执行推理 !
    graphFinalize()        // 最终化图
    ...
}

qnnSystemInterface {
    systemContextCreate()
    systemContextGetBinaryInfo()  // 检视 .bin 内的图信息
    ...
}
```

---

## 3. Kotlin 侧：进程管理与环境配置

**核心文件：** `app/src/main/java/io/github/xororz/localdream/service/BackendService.kt`

### 3.1 运行时环境准备

```kotlin
// BackendService.kt:155-226  prepareRuntimeDir()
fun prepareRuntimeDir() {
    val runtimeDir = File(filesDir, "runtime_libs")
    runtimeDir.mkdirs()

    // 从 assets/qnnlibs/ 复制所有 .so 到 runtimeDir
    assets.list("qnnlibs")?.forEach { fileName ->
        assets.open("qnnlibs/$fileName").use { input ->
            FileOutputStream(File(runtimeDir, fileName)).use { output ->
                input.copyTo(output)
            }
        }
    }
    // 所有 .so 设成 readable + executable
}
```

### 3.2 子进程启动

```kotlin
// BackendService.kt:228-389  startBackend()
fun startBackend(model: Model, width: Int, height: Int): Boolean {
    val nativeDir = applicationInfo.nativeLibraryDir
    val executableFile = File(nativeDir, "libstable_diffusion_core.so")

    val command = listOf(
        executableFile.absolutePath,
        "--clip",      "$modelsDir/clip.bin",       // QNN 模型文件
        "--unet",      "$modelsDir/unet.bin",
        "--vae_decoder", "$modelsDir/vae_decoder.bin",
        "--tokenizer", "$modelsDir/tokenizer.json",
        "--backend",   "$runtimeDir/libQnnHtp.so",  // QNN HTP 后端
        "--system_library", "$runtimeDir/libQnnSystem.so",
        "--port", "8081",
        "--text_embedding_size", "${model.textEmbeddingSize}",
    )

    // 环境变量配置
    val env = ProcessBuilder().environment()
    env["LD_LIBRARY_PATH"] = listOf(
        runtimeDir, "/system/lib64", "/vendor/lib64",
        "/vendor/lib64/egl", "/vendor/dsp/cdsp/lib"
    ).joinToString(":")
    env["DSP_LIBRARY_PATH"] = runtimeDir.absolutePath

    // 启动子进程
    process = ProcessBuilder(command)
        .directory(File(filesDir, "runtime_libs"))
        .redirectErrorStream(true)
        .apply { environment().putAll(env) }
        .start()
}
```

**环境变量说明：**

| 变量 | 作用 |
|------|------|
| `LD_LIBRARY_PATH` | 动态链接器搜索路径，包含 runtimeDir（QNN 库）、系统库路径 |
| `DSP_LIBRARY_PATH` | DSP 端库搜索路径，指向 runtimeDir，存放 V68-V81 的 DSP 计算库 |
| `ADSP_LIBRARY_PATH` | 备选 DSP 库路径（不显式设置，HTP Stub 自动发现） |

---

## 4. Native 入口：main.cpp 命令行解析与模型创建

**核心文件：** `app/src/main/cpp/src/main.cpp`

### 4.1 命令行参数

```bash
libstable_diffusion_core.so \
  --clip <模型路径>/clip.bin         # QNN CLIP 模型
  --unet <模型路径>/unet.bin         # QNN UNet 模型
  --vae_decoder <路径>/vae_decoder.bin # QNN VAE Decoder 模型
  --tokenizer <路径>/tokenizer.json   # HuggingFace tokenizer.json
  --backend libQnnHtp.so             # QNN HTP 后端库路径
  --system_library libQnnSystem.so   # QNN System 库路径
  --port 8081                         # HTTP 服务端口
  --text_embedding_size 768           # CLIP 嵌入维度 (SD1.5=768, SDXL=1280)
  [--cpu]                             # 可选：使用 MNN CPU 推理
  [--use_cpu_clip]                    # 可选：CLIP 用 CPU
  [--sdxl] [--lowram] [--patch ...]   # 可选：SDXL、低内存、分辨率 patch
```

### 4.2 模型创建与初始化序列

```
main() ───────────────────────────────────────────────────── main.cpp:3124
  │
  ├─ processCommandLine() ────────────────────────────────── main.cpp:540
  │   ├─ 解析 CLI 参数
  │   ├─ getQnnSystemFunctionPointers("libQnnSystem.so") ─── main.cpp:876
  │   │   └─ dlopen → 解析符号 → 存到 g_qnnSystemFuncs
  │   ├─ g_backendPathCmd = backendPath (libQnnHtp.so 路径)
  │   └─ 如果有 --patch: 用 zstd 对 unet.bin 打补丁
  │
  ├─ 加载 tokenizer ──────────────────────────────────────── main.cpp:3134
  │   └─ tokenizers::Tokenizer::FromBlobJSON(blob)
  │
  ├─ 创建 MNN 解释器 (CPU 回退 / SDXL CLIP) ────────────── main.cpp:3162
  │
  ├─ createQnnModel() × 4 ───────────────────────────────── main.cpp:272
  │   │  (clip, unet, vae_decoder, vae_encoder)
  │   ├─ getQnnFunctionPointers("libQnnHtp.so")
  │   │   └─ dlopen → 解析符号 → 版本匹配 → 填充 funcs
  │   └─ new QnnModel(funcs, backendHandle, systemFuncs)
  │
  ├─ initializeQnnApp() × 4 ─────────────────────────────── main.cpp:498
  │   对每个 QnnModel:
  │   ├─ model->initialize()          // 创建输出目录、读取输入列表
  │   ├─ model->initializeBackend()   // backendCreate()
  │   ├─ model->createDevice()        // deviceCreate()
  │   ├─ model->registerOpPackages()  // 注册 QNN 运算包
  │   ├─ model->createFromBinary()    // 从 .bin 创建 Context 和 Graph
  │   └─ model->enablePerformaceMode()// 设置 HTP 性能模式
  │
  └─ 启动 HTTP Server ───────────────────────────────────── main.cpp:3268
      ├─ GET  /health           → 健康检查
      ├─ POST /generate         → 核心推理接口 (SSE 流式)
      ├─ POST /upscale          → 超分辨率
      ├─ POST /tokenize         → token 计数
      └─ svr.listen(port)
```

---

## 5. QNN 模型生命周期

### 5.1 类继承关系

```
QnnSampleApp (Qualcomm SDK 基类)          QnnModel.hpp:26
├── QnnSampleApp.hpp / QnnSampleApp.cpp
│   ├── 基础的 QNN API 封装
│   ├── initializeBackend()
│   ├── createDevice()
│   ├── createFromBinary()
│   ├── registerOpPackages()
│   └── executeGraphs()
│
└── QnnModel (LocalDream 派生类)          QnnModel.hpp:26
    ├── executeClipGraphs()              QnnModel.hpp:164
    ├── executeUnetGraphs()              QnnModel.hpp:237
    ├── executeUnetGraphsSDXL()          QnnModel.hpp:485
    ├── executeVaeEncoderGraphs()        QnnModel.hpp:329
    ├── executeVaeEncoderGraphsSDXL()    QnnModel.hpp:577
    ├── executeVaeDecoderGraphs()        QnnModel.hpp:414
    ├── executeVaeDecoderGraphsSDXL()    QnnModel.hpp:641
    ├── executeUpscalerGraphs()          QnnModel.hpp:701
    ├── enablePerformaceMode()           QnnModel.hpp:66
    └── createFromBuffer()               QnnModel.hpp:778
```

### 5.2 模型初始化详解

```cpp
// main.cpp:498  initializeQnnApp()
template<typename T>
bool initializeQnnApp(const std::string& name, std::unique_ptr<T>& app) {
    if (!app) return false;

    app->initialize()               // Step 1: 基础初始化
    app->initializeBackend()        // Step 2: 创建 QNN Backend
    app->isDevicePropertySupported()// Step 3: 检查设备属性
    app->createDevice()             // Step 4: 创建设备句柄
    app->initializeProfiling()      // Step 5: 初始化性能分析 (OFF)
    app->registerOpPackages()       // Step 6: 注册运算包
    app->createFromBinary()         // Step 7: 创建推理上下文和图表
    app->enablePerformaceMode()     // Step 8: 配置 HTP 性能模式
}
```

**Step 7 `createFromBinary()` 内部流程 (QnnSampleApp.cpp:324):**

```cpp
// 1. 读取模型文件到内存
std::vector<uint8_t> buffer = readFile(modelPath);

// 2. 创建 System Context（用于二进制检视）
qnnSystemInterface.systemContextCreate(&systemCtxHandle);

// 3. 获取二进制中的图信息
qnnSystemInterface.systemContextGetBinaryInfo(
    systemCtxHandle, buffer.data(), bufferSize, &binaryInfo);

// 4. 将二进制加载为推理 Context
qnnInterface.contextCreateFromBinary(
    backendHandle, deviceHandle, config,
    buffer.data(), bufferSize,
    &contextHandle, &binaryInfo);

// 5. 检索图中的所有 Graph 句柄
qnnInterface.graphRetrieve(contextHandle, graphName, &graphHandle);
```

**Step 8 `enablePerformaceMode()` (QnnModel.hpp:66):**

```cpp
// 针对 HTP (Hexagon Tensor Processor) 的极致性能配置：
HtpPerfInfrastructure* htpInfra = getHtpPerfInfra();

// 创建电源配置
htpInfra->perfInfra.createPowerConfigId(
    deviceId, coreId, &powerConfigId);

// 设置性能参数
setRpcControlLatency(100);       // RPC 延迟 100μs
setRpcPollingTime(9999);         // 轮询时间 9999μs
setDcvsV3Config(
    powerConfigId,
    DCVS_V3_PERFORMANCE_MODE,    // 性能模式
    SLEEP_DISABLE,               // 禁止 DSP 休眠
    0,                           // 低功耗延迟无限制
    MAX_VOLTAGE_CORNER,          // 最大电压角 (最高频率)
    DCVS_V3_TP_DISABLE,          // 禁用热保护
    0                            // 无时钟偏移
);
setRpcPollingTime(1000);         // 自适应轮询 1000μs
```

### 5.3 图执行 API 调用链

每种模型（CLIP/UNet/VAE）的执行都遵循相同的模式：

```
QnnModel::executeXxxGraphs(inputs..., outputs...)
  │
  ├─ IOTensor::setupInputAndOutputTensors()  // 1. 分配输入/输出 tensor buffer
  │
  ├─ 数据预处理（量化/格式转换）
  │   ├─ floatToTfN(): float32 → uint16 量化
  │   ├─ 或直接 memcpy: int32 输入 (token IDs)
  │   └─ prepareTensor(): 填充 tensor 描述符
  │
  ├─ qnnInterface.graphExecute()            // 2. 执行 NPU 推理 !
  │   └─ 在 Hexagon DSP 上运行
  │
  ├─ 数据后处理（反量化/格式转换）
  │   ├─ IOTensor::convertToFloat(): 反量化输出
  │   └─ 或直接 memcpy: 浮点输出
  │
  └─ 返回输出数据到调用者
```

---

## 6. Stable Diffusion 推理管线详解

**核心函数：** `main.cpp:1809 generateImage()`

整个管线分为 6 个阶段，以文生图 (txt2img) 为例：

### 阶段 1：CLIP 文本编码 (line 1888-2147)

```
用户提示词
  │
  ├─ processWeightedPrompt(): 解析带权重的 prompt
  │   └─ tokenizer->Encode(text)  →  token_ids[1, 77]
  │   └─ 处理 Textual Inversion 嵌入
  │
  ├─ QNN CLIP 推理:
  │   └─ clipApp->executeClipGraphs(input_ids, embedding)
  │       输入: int32[1, 77]  token IDs
  │       输出: float[1, 77, 768]  text embedding  (SD1.5)
  │
  └─ 负向 prompt 也走相同流程
```

### 阶段 2：调度器 & 噪声初始化 (line 2151-2195)

```cpp
// 创建调度器 (默认: DPMSolverMultistepScheduler)
scheduler = createScheduler("dpm");

// 设置推理步数 (如 20 步)
scheduler->set_timesteps(num_steps);

// 初始化随机潜在表示
latents = random_normal(1, 4, H/8, W/8);  // SD1.5: 64×64
```

### 阶段 3：UNet 去噪循环 (line 2550-2822)

这是最核心的计算密集型部分，每个 timestep 执行一次：

```
for each timestep t in [T-1, ..., 0]:

    // 1. 缩放输入潜在表示
    latent_model_input = scheduler->scale_model_input(latents, t)

    // 2. 无条件推理 (空 prompt, 即 negative)
    unetApp->executeUnetGraphs(latent_model_input, t,
                               negative_embedding,
                               noise_pred_uncond)

    // 3. 有条件推理 (用户 prompt)
    unetApp->executeUnetGraphs(latent_model_input, t,
                               text_embedding,
                               noise_pred_text)

    // 4. CFG (Classifier-Free Guidance) 混合
    noise_pred = noise_pred_uncond
                 + cfg_scale * (noise_pred_text - noise_pred_uncond)

    // 5. 调度器步进
    latents = scheduler->step(noise_pred, t, latents)
```

**QNN UNet 的数据流 (QnnModel.hpp:237 executeUnetGraphs):**

```
输入:
  - float[1, 4, 64, 64]  latent sample     → 量化到 uint16
  - int32  timestep                         → 直接传递
  - float[1, 77, 768]   text embedding      → 量化到 uint16

NPU 执行:
  qnnInterface.graphExecute(graphHandle, inputs, outputs)

输出:
  - float[1, 4, 64, 64]  noise prediction   ← 反量化
```

**SDXL UNet 差异 (QnnModel.hpp:485 executeUnetGraphsSDXL):**

SDXL UNet 有 5 个输入 tensor（无量化，全 FP32）：
- `float[1, 4, H, W]` latent sample (x24)
- `float[1, 77, 2048]` prompt_embeds
- `int32` timestep
- `float[1, 6]` time_ids (原始尺寸 + 裁剪坐标 + 目标尺寸)
- `float[1, 1280]` pooled_prompt_embeds (CLIP pooled output)

### 阶段 4：VAE 解码 (line 2832-3002)

```cpp
// 缩放潜在表示
latents = (1.0 / 0.18215) * latents;  // vae_scale

// QNN VAE Decoder 推理
vaeDecoderApp->executeVaeDecoderGraphs(latents, pixels);

// 输出像素值
// pixels = (pixels + 1.0) / 2.0 * 255.0  →  clip to [0, 255]
```

**QNN VAE Decoder 数据流 (QnnModel.hpp:414):**

```
输入:
  - float[1, 4, 64, 64]  latents  → 量化到 uint16

NPU 执行:
  qnnInterface.graphExecute()

输出:
  - float[1, 3, 512, 512] pixels  ← 反量化
```

### 阶段 5：后处理 (line 3012-3120)

```
输出图像
  │
  ├─ Laplacian Blend (inpaint 场景)
  │   └─ 将生成区域与原始图像进行拉普拉斯金字塔混合
  │
  ├─ 安全检查 (NSFW)
  │   └─ safety_check() → MNN VGG 分类器
  │
  ├─ 编码为 JPEG/PNG
  │   └─ 压缩为 base64
  │
  └─ SSE 流式返回
      └─ HTTP response: data:{"output": "<base64>"}\n\n
```

### 图生图 (img2img) 额外流程

Img2img 在 txt2img 的 阶段 2 之前插入：

```
输入图像 (RGBA bytes)
  │
  ├─ 解码 + 缩放到目标尺寸
  │
  ├─ VAE 编码 (QNN):
  │   └─ vaeEncoderApp->executeVaeEncoderGraphs(pixels, mean, std)
  │       输入: float[1, 3, H, W]
  │       输出: float[1, 4, H/8, W/8] ×2 (mean + logvar)
  │
  ├─ 添加噪声:
      │   latents = scheduler->add_noise(latents, noise, t_start)
  │
  └─ 然后进入 阶段 3 正常去噪
```

---

## 7. MNN 的角色：CPU/OpenCL 回退

**MNN** 是阿里巴巴的轻量级推理框架。在 LocalDream 中用于：

### 7.1 完全 CPU 模式 (`--cpu` 标志)

当 `model.runOnCpu = true` 时：
- BackendService 使用 `.mnn` 模型文件替代 `.bin`
- 添加 `--cpu` 参数
- 所有模型 QNN → MNN CLIP/UNet/VAE 全部走 CPU (4 线程) 或 OpenCL

### 7.2 混合模式

| 组件 | QNN (NPU) | MNN (CPU) | 说明 |
|------|-----------|-----------|------|
| CLIP | clip.bin | clip.mnn | `--use_cpu_clip` 可单独回退 CLIP |
| UNet | unet.bin | unet.mnn | 核心模型，NPU 加速关键 |
| VAE Decoder | vae_decoder.bin | vae_decoder.mnn | 解码 512×512 图像 |
| VAE Encoder | vae_encoder.bin | vae_encoder.mnn | 图生图编码 |
| SDXL CLIP1/CLIP2 | 不支持 | 总是 CPU | SDXL 双 CLIP 文本编码器 |

### 7.3 为什么 QNN 有模型而 MNN 也有？

- **QNN `.bin` 文件**：经过 Qualcomm 量化工具（qnn-quantizer）转换并序列化的模型，包含量化参数（scale/offset），可以直接加载到 HTP 执行
- **MNN `.mnn` 文件**：标准 MNN 格式模型，可以是 FP32 或 FP16，用于 MNN 的 CPU/OpenCL 推理
- **SDXL CLIP**：Qualcomm 工具链可能不直接支持 SDXL 的 CLIP 模型，所以走 MNN

---

## 8. 模型文件准备：.bin 与 .mnn

### 8.1 QNN `.bin` 文件生成流程（模型开发者视角）

```
PyTorch / ONNX 模型
  │
  ├─ 导出为 ONNX 格式
  │   python export_onnx.py  →  clip.onnx, unet.onnx, vae_decoder.onnx
  │
  ├─ Qualcomm QNN 工具链转换
  │   qnn-onnx-converter:
  │     -i clip.onnx
  │     -o clip.cpp (源码模型) 或 clip.bin (序列化模型)
  │
  ├─ QNN 量化 (可选但推荐)
  │   qnn-quantizer:
  │     -i clip.cpp
  │     --input_list input_data.raw
  │     -o clip_quantized.cpp
  │
  └─ 序列化为 .bin
      qnn-context-binary-generator:
        -b clip_quantized.cpp
        -o clip.bin
```

LocalDream 中使用的模型就是这些 `.bin` 文件（量化 uint16 权重，float32 激活值）。

### 8.2 MNN `.mnn` 文件生成

LocalDream 内置了转换工具 (`SafeTensor2MNN.hpp`)：

```bash
# 通过 --convert 标志启动转换模式
libstable_diffusion_core.so \
  --convert --safetensor model.safetensors
  --unet <output_unet.mnn>
  --clip <output_clip.mnn>
  --vae_decoder <output_vae_decoder.mnn>
```

转换流程：
1. `SafeTensorReader` 读取 `.safetensors` 文件
2. `SDStructure.hpp` 定义模型结构（tensor name → layer 映射）
3. 使用 MNN Express API 构建 MNN 计算图
4. 将权重导入对应层
5. 输出 `.mnn` 文件

### 8.3 UNet Patch 机制

LocalDream 支持 UNet 的 `--patch` 参数，用于不同分辨率的 UNet：

```
base unet.bin (512×512)
  │
  + zstd 差分补丁 (如 768.patch)
  │
  = 运行时拼合的 unet (768×768)
```

实现 (`main.cpp:882`):
- 使用 `ZSTD_decompress_usingDict()` 以 `unet.bin` 为字典解压补丁
- 拼合后通过 `QnnModel::createFromBuffer()` 直接从内存创建推理上下文

---

## 9. 性能优化策略

### 9.1 HTP 性能配置

`QnnModel::enablePerformaceMode()` (QnnModel.hpp:66) 配置了极致性能：

- **DCVS V3 性能模式**：锁定最高电压/频率，禁止降频
- **禁止 DSP 休眠**：避免唤醒延迟
- **最大电压角**：提高供电以支撑更高频率
- **热保护禁用**：允许 NPU 持续满载运行（代价是更高功耗和发热）
- **RPC 低延迟**：100μs 控制延迟 + 9999μs 高检测周期

### 9.2 内存优化

```cpp
// QnnModel.hpp 中的量化/反量化
floatToTfN(float, uint16, step_size)     // FP32→UINT16 量化
convertToFloat(uint16, float, step_size) // UINT16→FP32 反量化
```

通过模型量化（uint16 权重），减小了内存占用和带宽需求，但激活值仍是 float32。

### 9.3 VAE Tiling

对大分辨率图像（>512px），VAE 编码/解码使用 Tiling 策略:

```
输入 (1024×1024)
  │
  ├─ 分割为 4 个重叠 tile (512×512 每个)
  │
  ├─ 逐个 tile 执行 VAE 推理
  │
  └─ blend_vae_tiles(): 加权混合重叠区域
```

避免了超大图像的显存溢出。

### 9.4 CLIP 缓存

```
clip_cache/
  ├─ <sha256_of_prompt>.pos_emb
  ├─ <sha256_of_prompt>.neg_emb
  └─ ...
```

对相同 prompt 的编码结果进行缓存，避免重复执行 CLIP 推理（CLIP 每个 prompt 只需执行一次）。

### 9.5 LoRA 适配器

通过 `SafeTensor2MNN.hpp` 的 `--lora` 支持：

```bash
--lora lora_weights.safetensors --lora_scale 0.7
```

LoRA 权重在模型转换时融入基础模型，避免了运行时的额外计算开销。

---

## 10. 总结：一次文生图的完整调用链路

```
┌─────────────────────────────────────────────────────────────────┐
│ 用户点击 "Generate"                                              │
└────────────────────────────┬────────────────────────────────────┘
                             │ HTTP POST /generate {"prompt": "a cat"}
┌────────────────────────────┼────────────────────────────────────┐
│ BackendService.kt          │ (Kotlin 主进程)                     │
│   modelListScreen.kt       │                                     │
│   → OkHttp POST → localhost:8081/generate                       │
└────────────────────────────┼────────────────────────────────────┘
                             │
┌────────────────────────────┼────────────────────────────────────┐
│ main.cpp                   │ (Native 子进程)                     │
│   generateImage()          │ main.cpp:1809                       │
│                            │                                     │
│   Phase 1: CLIP 文本编码   │                                     │
│   tokenizer->Encode()      │ "a cat" → [49406, 320, 539, ...]  │
│   clipApp->executeClip()   │ QNN: token_ids → [1,77,768] embed │
│                            │                                     │
│   Phase 2: UNet 去噪循环 ×20                                     │
│   for t in [999, ..., 0]:                                        │
│     unetApp->executeUnet() │ QNN: latent(4×64×64)+embed+timestep│
│                            │      → noise_pred(4×64×64)          │
│     scheduler->step()      │ 从噪声中逐步还原图像潜在表示         │
│                            │                                     │
│   Phase 3: VAE 解码                                               │
│   vaeDecoderApp->execute() │ QNN: latent(4×64×64)               │
│                            │      → pixels(3×512×512)            │
│                            │                                     │
│   Phase 4: 后处理                                                  │
│   stbi_write_jpg()         │ JPEG 编码                           │
│   base64_encode()          │ → base64 字符串                     │
│   SSE response:            │ "data:{"output":"/9j/4AAQ..."}"    │
└────────────────────────────┼────────────────────────────────────┘
                             │
┌────────────────────────────┼────────────────────────────────────┐
│ BackendService.kt          │ 收到 SSE 流                          │
│   → 解码 base64            │                                     │
│   → 显示在 Image Composable│                                     │
└─────────────────────────────────────────────────────────────────┘
```

**关键数据流总结：**

| 步骤 | 输入 | 模型 | 输出 | 硬件 |
|------|------|------|------|------|
| Tokenize | "a cat" | tokenizers-cpp | int32[1, 77] | CPU |
| CLIP | token IDs | QNN / MNN | float[1, 77, 768] | NPU / CPU |
| UNet ×N | latent + embed | QNN / MNN | float[1, 4, 64, 64] | NPU / CPU |
| VAE Decode | latent | QNN / MNN | float[1, 3, 512, 512] | NPU / CPU |
| JPEG Encode | pixels | stb_image | JPEG bytes | CPU |
| HTTP | SSE bytes | cpp-httplib | network | CPU |

**核心文件索引：**

| 文件 | 核心内容 | 行数 |
|------|----------|------|
| `BackendService.kt:228-389` | 启动子进程 `libstable_diffusion_core.so` | 161 |
| `main.cpp:1809` | `generateImage()` 推理管线 | 1400+ |
| `main.cpp:540` | CLI 参数解析 + QNN 模型创建 | 413 |
| `QnnModel.hpp:66` | HTP 性能模式配置 | 50 |
| `QnnModel.hpp:164` | `executeClipGraphs()` | 73 |
| `QnnModel.hpp:237` | `executeUnetGraphs()` | 248 |
| `QnnModel.hpp:414` | `executeVaeDecoderGraphs()` | 71 |
| `DynamicLoadUtil.cpp:35` | `getQnnFunctionPointers()` dlopen HTP | 86 |
| `QnnSampleApp.cpp:324` | `createFromBinary()` 加载.bin | 120+ |
| `CMakeLists.txt` | 构建配置、QNN lib 集成 | 218 |
