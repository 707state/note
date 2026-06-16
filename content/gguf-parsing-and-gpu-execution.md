---
title: llama.cpp与推理技术
author: GLM-5.1
tags:
  - Inference
  - GPU
  - LLM
LLM: 1
date: 2026-06-16
---

# llama.cpp 如何解析 GGUF 格式并在 GPU 上运行

## 1. 背景知识：什么是大语言模型推理

### 1.1 Transformer 极简概念

大语言模型（如 LLaMA、GPT）的核心是 **Transformer** 架构。你可以把它想象成一个"文字接龙机器"：

```
输入: "今天天气"  →  [Transformer 模型]  →  输出: "很好"
```

Transformer 内部由多层重复的"块"（Layer/Block）组成，每个块包含：
- **注意力机制（Attention）**：让模型"看到"输入中所有位置的信息
- **前馈网络（FFN）**：对信息做非线性变换
- **归一化（Normalization）**：稳定数值

这些操作本质上都是**矩阵乘法**和**逐元素运算**——这正是 GPU 擅长的。

### 1.2 为什么需要量化

一个 70B 参数的模型，如果用 FP16（每个参数 2 字节）存储，需要 ~140GB 显存。
通过**量化**（Quantization），可以用更少的位数表示权重：

| 量化类型 | 每参数位数 | 70B 模型大小 |
|---------|-----------|-------------|
| F16     | 16 bit    | ~140 GB     |
| Q8_0    | 8.5 bit   | ~74 GB      |
| Q4_0    | 4.5 bit   | ~39 GB      |
| Q2_K    | ~2.6 bit  | ~23 GB      |

GGUF 格式就是为了高效存储这些量化后的模型权重而设计的。

### 1.3 推理 vs 训练

llama.cpp 只做**推理**（Inference）：给定输入，计算输出。不做训练（不更新权重）。
推理的核心循环是：

```
for each token to generate:
    1. 将 token 转为 embedding 向量
    2. 通过 N 层 Transformer 块
    3. 输出概率分布
    4. 采样得到下一个 token
```

---

## 2. GGUF 文件格式详解

### 2.1 设计目标

GGUF（GGML Universal File format）是 llama.cpp 使用的二进制模型文件格式，设计目标：
- **自描述**：文件本身包含所有元数据（模型架构、超参数、词表等）
- **高效加载**：支持 mmap（内存映射），可以直接将文件映射到内存
- **量化友好**：原生支持各种量化格式
- **对齐**：张量数据按对齐边界存储，方便 GPU 直接读取

### 2.2 文件结构

GGUF 文件的二进制布局如下：

```
┌─────────────────────────────────────────────┐
│  Magic: "GGUF" (4 bytes)                    │  ← 文件标识
├─────────────────────────────────────────────┤
│  Version: uint32 (当前为 3)                  │  ← 格式版本
├─────────────────────────────────────────────┤
│  n_tensors: int64                           │  ← 张量总数
├─────────────────────────────────────────────┤
│  n_kv: int64                                │  ← 键值对总数
├─────────────────────────────────────────────┤
│                                             │
│  Key-Value Pairs (元数据区)                  │  ← 模型超参数、架构信息等
│  ┌─────────────────────────────────────┐    │
│  │ key (string)                        │    │
│  │ value_type (gguf_type)              │    │
│  │ value (binary)                      │    │
│  └─────────────────────────────────────┘    │
│  ... 重复 n_kv 次                           │
│                                             │
├─────────────────────────────────────────────┤
│                                             │
│  Tensor Info (张量描述区)                    │  ← 每个张量的形状、类型、偏移
│  ┌─────────────────────────────────────┐    │
│  │ name (string)                       │    │
│  │ n_dims (uint32)                     │    │
│  │ dimensions[n_dims] (int64 each)     │    │
│  │ type (ggml_type, e.g. Q4_0)        │    │
│  │ offset (uint64, 相对数据区起始)      │    │
│  └─────────────────────────────────────┘    │
│  ... 重复 n_tensors 次                      │
│                                             │
├─────────────────────────────────────────────┤
│  Padding (对齐到 alignment 边界)             │
├─────────────────────────────────────────────┤
│                                             │
│  Tensor Data (张量数据区)                    │  ← 实际的权重二进制数据
│  [tensor_0 data] [padding]                  │
│  [tensor_1 data] [padding]                  │
│  ...                                        │
│                                             │
└─────────────────────────────────────────────┘
```

### 2.3 关键元数据示例

KV 对中存储的典型信息：

| Key | 类型 | 含义 |
|-----|------|------|
| `general.architecture` | string | 模型架构，如 "llama" |
| `general.name` | string | 模型名称 |
| `llama.context_length` | uint32 | 最大上下文长度 |
| `llama.embedding_length` | uint32 | 嵌入维度 |
| `llama.block_count` | uint32 | Transformer 层数 |
| `llama.attention.head_count` | uint32 | 注意力头数 |
| `general.alignment` | uint32 | 数据对齐（默认 32 字节） |
| `tokenizer.ggml.model` | string | 分词器类型 |
| `tokenizer.ggml.tokens` | string[] | 词表 |

### 2.4 字符串序列化

GGUF 中的字符串格式：
```
[length: uint64] [content: char × length]  (无 null 终止符)
```

### 2.5 支持的数据类型

```c
enum gguf_type {
    GGUF_TYPE_UINT8   = 0,
    GGUF_TYPE_INT8    = 1,
    GGUF_TYPE_UINT16  = 2,
    GGUF_TYPE_INT16   = 3,
    GGUF_TYPE_UINT32  = 4,
    GGUF_TYPE_INT32   = 5,
    GGUF_TYPE_FLOAT32 = 6,
    GGUF_TYPE_BOOL    = 7,
    GGUF_TYPE_STRING  = 8,
    GGUF_TYPE_ARRAY   = 9,
    GGUF_TYPE_UINT64  = 10,
    GGUF_TYPE_INT64   = 11,
    GGUF_TYPE_FLOAT64 = 12,
};
```

---

## 3. GGUF 解析流程（源码级）

解析代码位于 `ggml/src/gguf.cpp`，核心函数是 `gguf_init_from_file_ptr`。

### 3.1 解析步骤

```
Step 1: 读取并验证 Magic ("GGUF")
Step 2: 读取版本号（当前支持 v2, v3）
Step 3: 读取 n_tensors 和 n_kv
Step 4: 循环读取 n_kv 个键值对
Step 5: 循环读取 n_tensors 个张量描述
Step 6: 对齐到数据区起始位置
Step 7: （可选）读取张量数据到内存
```

### 3.2 核心数据结构

```cpp
// 键值对
struct gguf_kv {
    std::string key;
    bool is_array;
    enum gguf_type type;
    std::vector<int8_t> data;           // 非字符串数据的原始字节
    std::vector<std::string> data_string; // 字符串数据
};

// 张量信息
struct gguf_tensor_info {
    struct ggml_tensor t;  // 包含 name, type, ne[4], nb[4]
    uint64_t offset;       // 在数据区中的偏移
};

// GGUF 上下文（解析结果）
struct gguf_context {
    uint32_t version;
    std::vector<struct gguf_kv> kv;           // 所有键值对
    std::vector<struct gguf_tensor_info> info; // 所有张量描述
    size_t alignment;  // 对齐值（默认 32）
    size_t offset;     // 数据区在文件中的起始偏移
    size_t size;       // 数据区总大小
    void * data;       // 数据区指针（如果加载了的话）
};
```

### 3.3 解析流程伪代码

```
function gguf_init_from_file_ptr(file):
    // 1. 验证 magic
    read 4 bytes → 必须是 "GGUF"

    // 2. 读取头部
    read version (uint32) → 检查是否为 2 或 3
    read n_tensors (int64)
    read n_kv (int64)

    // 3. 读取键值对
    for i in 0..n_kv:
        read key (string)
        read type (gguf_type)
        if type == ARRAY:
            read element_type
            read array_length
            read array_data
        else:
            read value

    // 4. 确定对齐值
    alignment = kv["general.alignment"] ?? 32

    // 5. 读取张量描述
    for i in 0..n_tensors:
        read name (string)
        read n_dims (uint32)
        read dimensions[n_dims] (int64 each)
        read type (ggml_type)
        read offset (uint64)
        // 计算 stride (nb[])
        nb[0] = type_size
        nb[1] = nb[0] * (ne[0] / block_size)
        nb[j] = nb[j-1] * ne[j-1]  (j >= 2)

    // 6. 对齐到数据区
    seek to ALIGN(current_pos, alignment)
    data_offset = current_pos

    // 7. 可选：加载数据
    if params.ctx != NULL:
        allocate ggml_context
        if !no_alloc:
            read entire data blob
            for each tensor_info:
                tensor.data = blob + tensor_info.offset
```

### 3.4 关键设计：no_alloc 模式

llama.cpp 通常使用 `no_alloc = true` 模式：
- 只解析元数据和张量描述
- **不**立即读取张量数据到内存
- 后续由 `llama_model_loader` 按需加载到 GPU buffer

这样做的好处：
1. 可以先确定每个张量应该放在哪个设备（CPU/GPU）
2. 支持 mmap，避免不必要的内存拷贝
3. 支持分片模型（多个 GGUF 文件）

---

## 4. 模型加载：从文件到内存

### 4.1 llama_model_loader 的角色

`src/llama-model-loader.h/cpp` 是连接 GGUF 解析和 GPU 后端的桥梁：

```
GGUF 文件 → [gguf_init_from_file] → gguf_context (元数据)
                                          ↓
                                   [llama_model_loader]
                                          ↓
                              ┌────────────┴────────────┐
                              ↓                         ↓
                    CPU Buffer (部分张量)      GPU Buffer (大部分张量)
```

### 4.2 加载流程

1. **解析 GGUF 元数据**：获取模型架构、超参数
2. **确定张量分配策略**：根据用户配置（`n_gpu_layers`）决定哪些层放 GPU
3. **创建 Backend Buffer**：为每个设备分配内存
4. **加载张量数据**：
   - **mmap 模式**（默认）：将文件映射到内存，GPU 后端直接从映射地址读取
   - **直接读取模式**：从文件读到 pinned memory，异步上传到 GPU

### 4.3 权重定位

每个张量在文件中的绝对位置：
```
absolute_offset = gguf_data_offset + tensor_info.offset
```

`llama_tensor_weight` 结构记录了这个信息：
```cpp
struct llama_tensor_weight {
    uint16_t idx;   // 源文件索引（支持分片）
    size_t offs;    // 文件中的绝对偏移
    ggml_tensor * tensor;  // 对应的 ggml 张量
};
```

### 4.4 mmap 的优势

使用内存映射（mmap）加载模型：
- 操作系统按需从磁盘加载页面（lazy loading）
- 多个进程可以共享同一份物理内存
- 不需要额外的内存拷贝
- 对于 Metal 后端（统一内存架构），GPU 可以直接访问 mmap 的内存

---

## 5. 计算图（Computation Graph）

### 5.1 什么是计算图

llama.cpp 使用 **ggml** 库来构建和执行计算图。计算图是一个有向无环图（DAG）：
- **节点（Node）**：代表一个操作（如矩阵乘法、加法、softmax）
- **边（Edge）**：代表数据依赖（一个操作的输出是另一个操作的输入）

```
[embedding] → [RMSNorm] → [MatMul(Q)] ─┐
                        → [MatMul(K)] ──┤→ [Attention] → [MatMul(O)] → [Add] → ...
                        → [MatMul(V)] ──┘
```

### 5.2 构建计算图

每次推理时，llama.cpp 会根据模型架构构建计算图：

```cpp
// 简化的 Transformer 层计算图构建
for (int il = 0; il < n_layer; ++il) {
    // 1. 归一化
    cur = ggml_rms_norm(ctx, inpL);
    cur = ggml_mul(ctx, cur, model.layers[il].attn_norm);

    // 2. 注意力
    Qcur = ggml_mul_mat(ctx, model.layers[il].wq, cur);
    Kcur = ggml_mul_mat(ctx, model.layers[il].wk, cur);
    Vcur = ggml_mul_mat(ctx, model.layers[il].wv, cur);
    // ... attention computation ...
    cur = ggml_mul_mat(ctx, model.layers[il].wo, attn_out);

    // 3. 残差连接
    cur = ggml_add(ctx, cur, inpL);

    // 4. FFN
    // ...
}
```

### 5.3 计算图执行

构建好的计算图交给 **Backend Scheduler** 执行：

```
计算图 → [Backend Scheduler] → 分配到各个后端
                                    ↓
                    ┌───────────────┼───────────────┐
                    ↓               ↓               ↓
              [CPU Backend]   [Metal Backend]  [Vulkan Backend]
```

Backend Scheduler 的职责：
1. 根据张量所在的 buffer 确定使用哪个后端
2. 在需要时插入数据拷贝操作（CPU ↔ GPU）
3. 按拓扑顺序执行操作

---

## 6. GPU 后端架构总览

### 6.1 后端抽象层

ggml 定义了统一的后端接口：

```
┌─────────────────────────────────────────────────────┐
│                  ggml_backend API                     │
│  (graph_compute, tensor_set, tensor_get, alloc...)   │
├─────────────────────────────────────────────────────┤
│         │              │              │              │
│    ggml-cpu       ggml-metal      ggml-vulkan       │
│    (CPU)          (Apple GPU)     (跨平台 GPU)      │
│         │              │              │              │
├─────────────────────────────────────────────────────┤
│                  硬件层                               │
│    x86/ARM        Apple M1/M2     NVIDIA/AMD/Intel  │
└─────────────────────────────────────────────────────┘
```

### 6.2 后端接口关键方法

每个后端需要实现：

| 接口 | 功能 |
|------|------|
| `alloc_buffer` | 在设备上分配内存 |
| `set_tensor` | 将数据从 CPU 写入设备 buffer |
| `get_tensor` | 将数据从设备 buffer 读回 CPU |
| `graph_compute` | 执行整个计算图 |
| `supports_op` | 查询是否支持某个操作 |

### 6.3 Buffer 类型

```
┌──────────────────────────────────────────┐
│  Backend Buffer Type                      │
│  ├── CPU (host memory)                   │
│  ├── Metal Shared (CPU+GPU 共享)         │
│  ├── Metal Private (GPU 独占)            │
│  ├── Vulkan Device (GPU 显存)            │
│  └── Vulkan Host (pinned memory)         │
└──────────────────────────────────────────┘
```

---

## 7. Metal 后端详解（Apple GPU）

### 7.1 Metal 是什么

Metal 是 Apple 的 GPU 编程框架，类似于 NVIDIA 的 CUDA。
Apple Silicon（M1/M2/M3/M4）使用**统一内存架构（UMA）**：CPU 和 GPU 共享同一块物理内存。

### 7.2 架构概览

```
┌─────────────────────────────────────────────────────────┐
│  ggml-metal 后端                                         │
│                                                         │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │ Device      │  │ Library      │  │ Command Queue │  │
│  │ (GPU 设备)  │  │ (Shader 库)  │  │ (命令队列)    │  │
│  └─────────────┘  └──────────────┘  └───────────────┘  │
│         │                │                    │          │
│         ↓                ↓                    ↓          │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │ Buffer      │  │ Pipeline     │  │ Command Buffer│  │
│  │ (GPU 内存)  │  │ (编译后着色器)│  │ (命令缓冲)    │  │
│  └─────────────┘  └──────────────┘  └───────────────┘  │
│                                              │          │
│                                              ↓          │
│                                     ┌───────────────┐  │
│                                     │ Encoder       │  │
│                                     │ (编码计算命令) │  │
│                                     └───────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### 7.3 Metal Buffer 管理

Metal 后端有两种 buffer 类型：

**Shared Buffer（共享缓冲区）**：
- CPU 和 GPU 都可以直接访问
- 适合频繁的 CPU↔GPU 数据交换
- 在 UMA 上没有拷贝开销

**Private Buffer（私有缓冲区）**：
- 只有 GPU 可以直接访问
- GPU 访问速度更快（可以使用更优的内存布局）
- 需要通过 blit 命令拷贝数据

```cpp
// 分配 Metal buffer
ggml_metal_buffer_t ggml_metal_buffer_init(
    ggml_metal_device_t dev,
    size_t size,
    bool shared  // true = shared, false = private
);
```

### 7.4 Metal Shader（计算着色器）

Metal 的计算核心是 `.metal` 文件中的 kernel 函数。
文件位于 `ggml/src/ggml-metal/ggml-metal.metal`（约 11000+ 行）。

每个 ggml 操作对应一个或多个 Metal kernel：

```metal
// 矩阵-向量乘法 kernel 示例（简化）
kernel void kernel_mul_mv_f16_f32(
    device const  half * src0,    // 权重矩阵 (f16)
    device const float * src1,    // 输入向量 (f32)
    device       float * dst,     // 输出向量 (f32)
    // ... 维度参数 ...
    uint3 tgpig [[threadgroup_position_in_grid]],
    uint  tiisg [[thread_index_in_simdgroup]],
    uint  sgitg [[simdgroup_index_in_threadgroup]]
) {
    // 每个线程组处理输出的一行
    // 使用 SIMD group 并行计算点积
    // ...
}
```

### 7.5 Pipeline（管线）

Metal Pipeline = 编译后的 shader + 执行配置：

```cpp
struct ggml_metal_pipeline_with_params {
    ggml_metal_pipeline_t pipeline;  // 编译后的计算管线
    int nsg;   // SIMD groups per threadgroup
    int nr0;   // threadgroups 维度 0
    int nr1;   // threadgroups 维度 1
    size_t smem;  // 共享内存大小
};
```

llama.cpp 在初始化时预编译所有需要的 pipeline：
```
初始化 → 加载 .metal 源码 → 编译为 MTLLibrary → 获取各 kernel 的 Pipeline
```

### 7.6 计算图执行流程

```
graph_compute(graph):
    1. 创建 Command Buffer
    2. 创建 Compute Command Encoder
    3. for each node in graph:
        a. 选择对应的 Pipeline (kernel)
        b. 设置 buffer 参数（权重、输入、输出）
        c. 设置常量参数（维度、stride 等）
        d. dispatch threadgroups（启动 GPU 计算）
    4. 提交 Command Buffer
    5. 等待完成
```

### 7.7 Metal 的特殊优化

1. **Fusion（算子融合）**：将多个连续操作合并为一个 kernel，减少内存读写
2. **Concurrency（并发编码）**：多个独立操作可以并行编码
3. **Flash Attention**：专门优化的注意力 kernel，减少内存带宽需求
4. **SIMD Group Matrix（矩阵加速）**：利用 Apple GPU 的硬件矩阵乘法单元

### 7.8 统一内存的优势

在 Apple Silicon 上：
```
传统 GPU (NVIDIA):
  CPU Memory ──[PCIe 拷贝]──→ GPU Memory
  (慢，需要显式管理)

Apple Silicon (UMA):
  ┌─────────────────────────────┐
  │     统一内存 (Unified)       │
  │   CPU 和 GPU 直接共享访问    │
  └─────────────────────────────┘
  (零拷贝，mmap 的文件 GPU 可直接读取)
```

这意味着：
- 模型权重可以通过 mmap 直接映射，GPU 无需额外拷贝
- 加载速度极快（几乎是瞬时的）
- 内存效率高（不需要两份数据）

---

## 8. Vulkan 后端详解（跨平台 GPU）

### 8.1 Vulkan 是什么

Vulkan 是 Khronos Group 的跨平台 GPU API，支持：
- NVIDIA GPU（Windows/Linux）
- AMD GPU（Windows/Linux）
- Intel GPU（Windows/Linux）
- Apple GPU（通过 MoltenVK）
- 移动端 GPU（Android）

### 8.2 架构概览

```
┌──────────────────────────────────────────────────────────────┐
│  ggml-vulkan 后端                                             │
│                                                              │
│  ┌──────────────┐  ┌───────────────┐  ┌──────────────────┐  │
│  │ VkInstance   │  │ VkDevice      │  │ VkQueue          │  │
│  │ (Vulkan实例) │  │ (逻辑设备)    │  │ (命令队列)       │  │
│  └──────────────┘  └───────────────┘  └──────────────────┘  │
│                           │                     │            │
│                           ↓                     ↓            │
│  ┌──────────────┐  ┌───────────────┐  ┌──────────────────┐  │
│  │ VkBuffer     │  │ VkPipeline    │  │ VkCommandBuffer  │  │
│  │ (GPU 内存)   │  │ (计算管线)    │  │ (命令缓冲)       │  │
│  └──────────────┘  └───────────────┘  └──────────────────┘  │
│                           │                                  │
│                           ↓                                  │
│                    ┌───────────────┐                          │
│                    │ SPIR-V Shader │                          │
│                    │ (编译后着色器) │                          │
│                    └───────────────┘                          │
└──────────────────────────────────────────────────────────────┘
```

### 8.3 Vulkan Shader（GLSL → SPIR-V）

Vulkan 使用 SPIR-V 字节码格式的 shader。llama.cpp 的 shader 源码用 GLSL 编写：

文件位于 `ggml/src/ggml-vulkan/vulkan-shaders/`，包含 150+ 个 `.comp` 文件。

```glsl
// mul_mat_vec.comp 示例（简化）
#version 450

layout(local_size_x = 256) in;

layout(binding = 0) readonly buffer A { float data_a[]; };  // 权重
layout(binding = 1) readonly buffer B { float data_b[]; };  // 输入
layout(binding = 2) writeonly buffer D { float data_d[]; }; // 输出

layout(push_constant) uniform Parameters {
    uint ne00, ne01, ne02;  // 维度信息
    // ...
};

void main() {
    // 每个 workgroup 计算输出的一行
    uint row = gl_WorkGroupID.x;
    float sum = 0.0;
    for (uint i = gl_LocalInvocationID.x; i < ne00; i += gl_WorkGroupSize.x) {
        sum += data_a[row * ne00 + i] * data_b[i];
    }
    // subgroup reduction...
    data_d[row] = sum;
}
```

编译流程：
```
GLSL 源码 (.comp) → [glslc/glslangValidator] → SPIR-V 字节码 → 嵌入到 C++ 代码中
```

### 8.4 Pipeline 结构

```cpp
struct vk_pipeline_struct {
    std::string name;
    vk::ShaderModule shader_module;     // SPIR-V 模块
    vk::PipelineLayout layout;          // 参数布局
    vk::Pipeline pipeline;              // 编译后的管线
    uint32_t push_constant_size;        // push constant 大小
    uint32_t parameter_count;           // 参数数量
    std::array<uint32_t, 3> wg_denoms;  // workgroup 大小除数
    uint32_t align;                     // 对齐要求
};
```

### 8.5 设备架构检测

Vulkan 后端会检测 GPU 架构以选择最优的 shader 变体：

```cpp
enum vk_device_architecture {
    OTHER,
    AMD_GCN,
    AMD_RDNA1,
    AMD_RDNA2,
    AMD_RDNA3,
    INTEL_XE2,
    NVIDIA_PRE_TURING,
    NVIDIA_TURING,
};
```

不同架构使用不同的优化策略：
- **NVIDIA Turing+**：使用 Cooperative Matrix（硬件矩阵乘法）
- **AMD RDNA**：调整 subgroup size（wave32 vs wave64）
- **Intel Xe2**：使用 SIMD16 优化

### 8.6 内存管理

Vulkan 的内存模型与 Metal 不同——CPU 和 GPU 内存是分离的：

```
┌─────────────┐                    ┌─────────────┐
│  CPU Memory │ ──[PCIe Bus]──→    │  GPU Memory │
│  (Host)     │ ←──[PCIe Bus]──   │  (Device)   │
└─────────────┘                    └─────────────┘
```

数据传输流程：
```
1. 分配 Host-visible buffer (staging)
2. 将数据写入 staging buffer
3. 提交 copy command: staging → device buffer
4. GPU 从 device buffer 读取数据进行计算
```

### 8.7 计算图执行流程

```
graph_compute(graph):
    1. 遍历计算图节点
    2. 对每个节点:
        a. 选择对应的 Pipeline
        b. 绑定 descriptor sets（buffer 绑定）
        c. 设置 push constants（维度参数）
        d. 记录 dispatch 命令到 Command Buffer
    3. 批量提交 Command Buffer 到 Queue
    4. 使用 Fence 等待完成
```

### 8.8 Vulkan 的特殊优化

1. **算子融合（Fusion）**：
   - 多个逐元素操作合并为一个 dispatch
   - TopK + MoE 融合
   - Norm + Scale 融合

2. **图优化（Graph Optimization）**：
   - 重排节点以提高并行度
   - 减少不必要的同步

3. **Flash Attention**：
   - 支持 Cooperative Matrix（coopmat）加速
   - 支持 Split-K 策略处理长序列

4. **异步提交**：
   - 将计算分批提交，减少 GPU 空闲时间
   - 使用 fence 进行粗粒度同步

5. **Shader 变体**：
   - 根据矩阵大小选择不同的 kernel（large/medium/small）
   - 对齐 vs 非对齐版本
   - f16 累加 vs f32 累加

---

## 9. 端到端推理流程总结

完整的推理流程如下：

```
┌─────────────────────────────────────────────────────────────────┐
│  1. 加载模型                                                     │
│     GGUF 文件 → 解析元数据 → 确定架构/超参数                      │
│              → 分配 GPU Buffer → 加载权重到 GPU                   │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│  2. 用户输入                                                     │
│     "Hello, world" → Tokenizer → [token_ids: 15043, 29892, ...]  │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│  3. 构建计算图                                                   │
│     根据模型架构，为当前 batch 构建 ggml_cgraph                    │
│     (embedding → N × transformer_block → lm_head)                │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│  4. 执行计算图                                                   │
│     Backend Scheduler:                                           │
│       - 将图分割为子图（按设备）                                   │
│       - 插入必要的数据拷贝                                        │
│       - 调用各后端的 graph_compute                                │
│                                                                 │
│     Metal/Vulkan Backend:                                        │
│       - 遍历节点，编码 GPU 命令                                   │
│       - 提交到 GPU 执行                                          │
│       - 等待完成                                                 │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│  5. 采样输出                                                     │
│     logits → softmax → temperature/top_p/top_k → 选择 token      │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│  6. 重复 3-5 直到生成结束                                         │
│     (每次生成一个 token，将其加入上下文继续生成)                    │
└─────────────────────────────────────────────────────────────────┘
```

### 关键性能瓶颈

| 阶段 | 瓶颈 | 优化手段 |
|------|------|---------|
| 模型加载 | 磁盘 I/O | mmap, 异步加载 |
| Prefill（首次处理输入） | 计算密集 | 矩阵乘法优化, Flash Attention |
| Decode（逐 token 生成） | 内存带宽 | 量化减少数据量, KV Cache |

---

## 10. 附录：关键数据结构速查

### ggml_tensor

```cpp
struct ggml_tensor {
    enum ggml_type type;       // 数据类型 (F32, F16, Q4_0, ...)
    int64_t ne[GGML_MAX_DIMS]; // 每个维度的元素数
    size_t  nb[GGML_MAX_DIMS]; // 每个维度的字节步长
    enum ggml_op op;           // 操作类型 (MUL_MAT, ADD, ...)
    struct ggml_tensor * src[GGML_MAX_SRC]; // 输入张量
    void * data;               // 数据指针
    char name[GGML_MAX_NAME];  // 张量名称
    struct ggml_backend_buffer * buffer; // 所在的 buffer
};
```

### ggml_cgraph（计算图）

```cpp
struct ggml_cgraph {
    int n_nodes;                    // 节点数
    struct ggml_tensor ** nodes;    // 节点数组（拓扑排序）
    // ...
};
```

### 量化格式示例：Q4_0

```
Q4_0: 每 32 个元素为一个 block
┌──────────────────────────────────────┐
│  scale (float16, 2 bytes)            │  ← 缩放因子
│  quants[16] (uint8, 16 bytes)        │  ← 32 个 4-bit 量化值（每字节存 2 个）
└──────────────────────────────────────┘
总计: 18 bytes / 32 elements = 4.5 bits/element

反量化: value[i] = quants[i] * scale
```

### Metal vs Vulkan 对比

| 特性 | Metal | Vulkan |
|------|-------|--------|
| 平台 | macOS/iOS only | 跨平台 |
| 内存模型 | 统一内存 (UMA) | 分离内存 (需要拷贝) |
| Shader 语言 | Metal Shading Language | GLSL → SPIR-V |
| 矩阵加速 | SIMD Group Matrix | Cooperative Matrix |
| 加载速度 | 极快 (mmap 直接用) | 需要上传到 GPU |
| 适用场景 | Apple 设备 | NVIDIA/AMD/Intel GPU |

---

## 参考文件

| 文件路径 | 内容 |
|---------|------|
| `ggml/include/gguf.h` | GGUF 格式定义和 API |
| `ggml/src/gguf.cpp` | GGUF 解析实现 |
| `src/llama-model-loader.h/cpp` | 模型加载器 |
| `ggml/include/ggml-backend.h` | 后端抽象接口 |
| `ggml/src/ggml-metal/ggml-metal.cpp` | Metal 后端实现 |
| `ggml/src/ggml-metal/ggml-metal.metal` | Metal shader 源码 |
| `ggml/src/ggml-vulkan/ggml-vulkan.cpp` | Vulkan 后端实现 |
| `ggml/src/ggml-vulkan/vulkan-shaders/` | Vulkan shader 源码 |
| `src/llama-context.cpp` | 推理上下文和图执行 |
| `src/llama-graph.h/cpp` | 计算图构建 |
