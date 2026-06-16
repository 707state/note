---
title: Mali-G610的linux内核驱动
author: GLM-5.1
tags:
  - Linux
  - GPU
date: 2026-06-18
done: 0
LLM: 1
---

# CoolPi-Kernel Panfrost GPU 驱动深度分析

## 1. 驱动概述与架构

Panfrost 是 Linux 内核主线中用于 Arm Mali GPU（Midgard、Bifrost、Valhall-JM 架构）的开源 DRM 驱动。它是一个**纯 Render-only 驱动**（不包含 KMS 显示功能），仅负责 GPU 计算/渲染命令的提交。

在 `coolpi-kernel` 中，Panfrost 驱动支持 RK3588/RK3588S 上集成的 **Mali-G610 (Valhall 架构)** GPU。

### 文件结构

```
drivers/gpu/drm/panfrost/
├── panfrost_drv.c          # DRM 驱动入口、IOCTL 实现、平台驱动注册
├── panfrost_device.c       # 设备初始化/销毁、时钟/电源/复位
├── panfrost_device.h       # 核心数据结构定义
├── panfrost_gpu.c          # GPU 硬件初始化、复位、电源控制、型号检测
├── panfrost_gpu.h
├── panfrost_job.c          # Job 提交、调度器、fence、IRQ 处理
├── panfrost_job.h
├── panfrost_mmu.c          # MMU/地址空间管理、页表映射、缺页处理
├── panfrost_mmu.h
├── panfrost_gem.c          # GEM 缓冲区管理
├── panfrost_gem.h
├── panfrost_gem_shrinker.c # GEM BO 内存回收 shrinker
├── panfrost_devfreq.c      # 动态调频 (Devfreq)
├── panfrost_devfreq.h
├── panfrost_perfcnt.c      # 性能计数器
├── panfrost_perfcnt.h
├── panfrost_dump.c         # GPU Core Dump（Job 超时时）
├── panfrost_dump.h
├── panfrost_regs.h         # GPU 寄存器定义
├── panfrost_features.h     # GPU 硬件特性表
├── panfrost_issues.h       # GPU 硬件 errata 表
├── Kconfig
├── Makefile
└── TODO
```

### 整体架构图

```
┌──────────────────────────────────────────────────┐
│             Userspace (Mesa / Panfrost)          │
│  ┌─────────┐  ┌──────────┐  ┌───────────────┐   │
│  │OpenGL ES│  │  Vulkan* │  │ OpenCL*       │   │
│  └────┬────┘  └─────┬────┘  └──────┬────────┘   │
│       │              │              │             │
│       ▼              ▼              ▼             │
│  ┌─────────────────────────────────────────┐     │
│  │         Panfrost Gallium3D Driver       │     │
│  │    (将 GL 着色器编译为 Mali ISA)         │     │
│  └────────────────┬────────────────────────┘     │
│                   │ DRM IOCTL                     │
└───────────────────┼───────────────────────────────┘
                    ▼
┌──────────────────────────────────────────────────┐
│              Kernel Panfrost Driver               │
│  ┌────────┐ ┌──────┐ ┌──────┐ ┌──────────────┐  │
│  │  DRM   │ │ GEM  │ │ MMU  │ │ DRM Scheduler│  │
│  │IOCTL   │ │ BO   │ │ Page │ │ Job Queue    │  │
│  │Handler │ │ Mgmt │ │ Table│ │ Fence        │  │
│  └───┬────┘ └──┬───┘ └──┬───┘ └──────┬───────┘  │
│      │         │        │            │           │
│      ▼         ▼        ▼            ▼           │
│  ┌─────────────────────────────────────────┐     │
│  │         Mali GPU Hardware               │     │
│  │  ┌─────────┐ ┌───────┐ ┌────────────┐  │     │
│  │  │Job Mgr  │ │ MMU   │ │Shader Core │  │     │
│  │  │(3 slots)│ │(AS)   │ │ + Tiler    │  │     │
│  │  └─────────┘ └───────┘ └────────────┘  │     │
│  └─────────────────────────────────────────┘     │
└──────────────────────────────────────────────────┘
```

---

## 2. 硬件基础：Mali GPU Job Manager 模型

Panfrost 驱动面向的 Mali GPU 采用 **Job Manager (JM)** 硬件模型，这是 Midgard/Bifrost/Valhall-JM 系列 GPU 共有的架构。理解这个模型对理解驱动至关重要。

### Job Manager

Job Manager 是 GPU 上的一个硬件单元，负责接收 CPU 提交的 Job 并分发给 Shader Core 执行。

- **3 个 Job Slot**（`NUM_JOB_SLOTS = 3`）：
  - **JS0**：Fragment（片段着色）Job
  - **JS1**：Vertex/Tiler（顶点/分块）Job
  - **JS2**：Compute-only（纯计算）Job（当前未暴露给用户态）

- 每个 Job Slot 有 2 个深度的硬件队列（`pfdev->jobs[slot][0/1]`）

- 代码位置：`panfrost_device.h:24`
  ```c
  #define NUM_JOB_SLOTS 3
  ```

### Job Chain

用户态提交的不是单个 Job，而是一个 **Job Chain**（作业链）。Job Chain 是一个链表结构的 GPU 命令序列，首地址（`jc`）由用户态提供。

代码位置：`panfrost_drm.h:52`（UAPI）
```c
struct drm_panfrost_submit {
    __u64 jc;              /* Job Chain 首地址（GPU VA） */
    __u64 in_syncs;        /* 输入同步对象数组 */
    __u32 in_sync_count;
    __u32 out_sync;
    __u64 bo_handles;      /* 引用的 BO 句柄数组 */
    __u32 bo_handle_count;
    __u32 requirements;    /* PANFROST_JD_REQ_FS 等 */
};
```

### 地址空间 (Address Space, AS)

GPU 有多个独立的地址空间，每个进程（DRM fd）拥有自己的 AS。RK3588 的 Mali-G610 支持 16 个 AS。

---

## 3. 驱动初始化流程

### 3.1 入口：platform_driver probe

代码位置：`panfrost_drv.c:558-622`

```c
static int panfrost_probe(struct platform_device *pdev)
{
    pfdev = devm_kzalloc(&pdev->dev, sizeof(*pfdev), GFP_KERNEL);
    pfdev->pdev = pdev;
    pfdev->dev = &pdev->dev;
    pfdev->comp = of_device_get_match_data(&pdev->dev);
    pfdev->coherent = device_get_dma_attr(&pdev->dev) == DEV_DMA_COHERENT;

    ddev = drm_dev_alloc(&panfrost_drm_driver, &pdev->dev);
    ddev->dev_private = pfdev;

    err = panfrost_device_init(pfdev);

    pm_runtime_set_active(pfdev->dev);
    pm_runtime_enable(pfdev->dev);
    pm_runtime_set_autosuspend_delay(pfdev->dev, 50); /* ~3 frames */

    err = drm_dev_register(ddev, 0);
    panfrost_gem_shrinker_init(ddev);
}
```

对于 RK3588，设备树中 GPU 节点的 compatible 为 `"arm,mali-valhall-jm"`，匹配 `default_data`。

代码位置：`panfrost_drv.c:684-685`
```c
{ .compatible = "arm,mali-valhall-jm", .data = &default_data, },
```

### 3.2 设备初始化链

代码位置：`panfrost_device.c:199-279`

`panfrost_device_init()` 按以下顺序初始化各子系统：

```
panfrost_device_init()
  ├── panfrost_clk_init()          // 获取并使能 GPU 时钟
  ├── panfrost_devfreq_init()      // 初始化动态调频
  ├── panfrost_regulator_init()    // 获取并使能电源调节器
  ├── panfrost_reset_init()        // 获取复位控制并解复位
  ├── panfrost_pm_domain_init()    // 附加电源域
  ├── devm_platform_ioremap_resource()  // 映射 MMIO 寄存器
  ├── panfrost_gpu_init()          // ★ GPU 硬件初始化
  ├── panfrost_mmu_init()          // MMU 中断注册
  ├── panfrost_job_init()          // ★ Job 调度器初始化
  └── panfrost_perfcnt_init()      // 性能计数器初始化
```

### 3.3 GPU 硬件初始化

代码位置：`panfrost_gpu.c:399-430`

```c
int panfrost_gpu_init(struct panfrost_device *pfdev)
{
    err = panfrost_gpu_soft_reset(pfdev);     // 1. 软复位 GPU
    panfrost_gpu_init_features(pfdev);         // 2. 读取 GPU 特性寄存器
    err = dma_set_mask_and_coherent(...);      // 3. 设置 DMA 掩码
    err = devm_request_irq(..., "gpu", ...);   // 4. 注册 GPU 中断
    panfrost_gpu_power_on(pfdev);              // 5. 给 Shader/L2/Tiler 上电
}
```

**GPU 特性检测** 是关键步骤（`panfrost_gpu_init_features()`，代码位置：`panfrost_gpu.c:214-319`）：

1. 从 GPU 寄存器读取所有硬件能力：shader_present、tiler_present、l2_present、mmu_features 等
2. 根据 GPU ID 在 `gpu_models[]` 表中查找匹配的型号（如 G57 对应 ID 0x9001）
3. 确定硬件特性（`hw_features`）和已知缺陷（`hw_issues`）
4. 打印 `mali-xxx id 0x...` 信息

代码位置：`panfrost_gpu.c:185-212`（型号表）
```c
static const struct panfrost_model gpu_models[] = {
    GPU_MODEL(t600, 0x600, ...),
    ...
    GPU_MODEL(g71, 0x6000, ...),
    GPU_MODEL(g57, 0x9001, GPU_REV(g57, 0, 0)),  // Valhall
};
```

> **注意**：Mali-G610 的 GPU ID 不在此表中，但 Panfrost 对未知型号仍能工作，只是不会应用型号特定的特性/修复。

### 3.4 Job 调度器初始化

代码位置：`panfrost_job.c:776-840`

```c
int panfrost_job_init(struct panfrost_device *pfdev)
{
    // 为 3 个 Job Slot 各创建一个 drm_gpu_scheduler
    for (j = 0; j < NUM_JOB_SLOTS; j++) {
        js->queue[j].fence_context = dma_fence_context_alloc(1);
        ret = drm_sched_init(&js->queue[j].sched,
                     &panfrost_sched_ops, NULL,
                     DRM_SCHED_PRIORITY_COUNT,
                     nentries, 0,
                     msecs_to_jiffies(JOB_TIMEOUT_MS),  // 500ms 超时
                     pfdev->reset.wq, NULL, "pan_js", pfdev->dev);
    }
    panfrost_job_enable_interrupts(pfdev);
}
```

每个 Job Slot 对应一个 `drm_gpu_scheduler` 实例，超时时间为 500ms。

### 3.5 FD 打开（per-process 初始化）

代码位置：`panfrost_drv.c:473-503`

```c
static int panfrost_open(struct drm_device *dev, struct drm_file *file)
{
    panfrost_priv = kzalloc(sizeof(*panfrost_priv), GFP_KERNEL);
    panfrost_priv->mmu = panfrost_mmu_ctx_create(pfdev);  // 创建 MMU 上下文
    ret = panfrost_job_open(panfrost_priv);                 // 初始化调度实体
}
```

每个 DRM fd 打开时：
1. 创建独立的 **MMU 上下文**（含独立页表和地址空间）
2. 为 3 个 Job Slot 各创建一个 **`drm_sched_entity`**（调度实体）

代码位置：`panfrost_job.c:857-873`
```c
int panfrost_job_open(struct panfrost_file_priv *panfrost_priv)
{
    for (i = 0; i < NUM_JOB_SLOTS; i++) {
        ret = drm_sched_entity_init(&panfrost_priv->sched_entity[i],
                        DRM_SCHED_PRIORITY_NORMAL, &sched, 1, NULL);
    }
}
```

---

## 4. 用户态接口：IOCTL 与 OpenGL ES 渲染

### 4.1 IOCTL 列表

代码位置：`panfrost_drv.c:517-530`

| IOCTL | 编号 | 功能 |
|-------|------|------|
| `PANFROST_SUBMIT` | 0x00 | 提交 Job Chain 到 GPU |
| `PANFROST_WAIT_BO` | 0x01 | 等待 BO 上的 GPU 操作完成 |
| `PANFROST_CREATE_BO` | 0x02 | 创建 GEM 缓冲区对象 |
| `PANFROST_MMAP_BO` | 0x03 | 获取 BO 的 mmap 偏移 |
| `PANFROST_GET_PARAM` | 0x04 | 查询 GPU 参数 |
| `PANFROST_GET_BO_OFFSET` | 0x05 | 获取 BO 在 GPU 地址空间的偏移 |
| `PANFROST_PERFCNT_ENABLE` | 0x06 | 启/停性能计数器（unstable） |
| `PANFROST_PERFCNT_DUMP` | 0x07 | 导出性能计数器（unstable） |
| `PANFROST_MADVISE` | 0x08 | 建议内核回收/保留 BO 页面 |

驱动版本：`major=1, minor=2`（代码位置：`panfrost_drv.c:549-552`）

### 4.2 OpenGL ES 渲染完整流程

以下是 OpenGL ES 应用通过 Mesa Panfrost 驱动进行一帧渲染的完整流程：

#### 步骤 1：打开 DRM 设备

```
open("/dev/dri/renderD128")  →  触发 panfrost_open()
                               → 创建 per-fd MMU 上下文
                               → 创建 3 个 sched_entity
```

#### 步骤 2：查询 GPU 能力

```
DRM_IOCTL_PANFROST_GET_PARAM(GPU_PROD_ID)       → GPU 型号 ID
DRM_IOCTL_PANFROST_GET_PARAM(SHADER_PRESENT)     → 可用 Shader Core 位图
DRM_IOCTL_PANFROST_GET_PARAM(JS_FEATURES0..15)   → Job Slot 能力
...
```

Mesa 用户态驱动据此决定使用哪个 ISA 编译着色器。

#### 步骤 3：创建和映射缓冲区

```
DRM_IOCTL_PANFROST_CREATE_BO(size, flags=0)      → 创建帧缓冲/纹理/SSBO 等 BO
DRM_IOCTL_PANFROST_MMAP_BO(handle)                → 获取 mmap 偏移
mmap(offset, ...)                                  → CPU 端映射
```

对于可执行缓冲区（着色器代码），`flags=0`（默认可执行）。
对于数据缓冲区，`flags=PANFROST_BO_NOEXEC`。
对于 growable 缓冲区（Tiler 堆），`flags=PANFROST_BO_HEAP|PANFROST_BO_NOEXEC`。

#### 步骤 4：着色器编译

Mesa Panfrost 驱动将 GLSL 着色器编译为 **Mali ISA 二进制**：
- Midgard：使用 Midgard ISA
- Bifrost/Valhall：使用 Bifrost/Valhall ISA

编译后的着色器二进制写入可执行 BO，然后构建 **Job Chain** 数据结构。

#### 步骤 5：构建 Job Chain

Job Chain 是一个 GPU 可解析的链表，包含：

```
┌─────────────────────┐
│  Job Header         │
│  ├── type           │  (VERTEX/TILER/FRAGMENT/COMPUTE)
│  ├── vertex_patch_list │
│  ├── next_job       │──→ 下一个 Job（或 NULL）
│  └── ...            │
├─────────────────────┤
│  Job Payload        │
│  ├── shader code ptr│  (GPU VA → 可执行 BO)
│  ├── texture ptrs   │  (GPU VA → 纹理 BO)
│  ├── uniform ptrs   │  (GPU VA → Uniform BO)
│  ├── ...            │
│  └── tiler hier     │  (Fragment Job 特有)
└─────────────────────┘
```

典型的 OpenGL ES 渲染帧提交 **两个 Job Chain**：

1. **Vertex+Tiler Job**（`requirements=0`，进入 JS1）：执行顶点着色器，进行 tiling（将屏幕分成 tile），生成 tile list
2. **Fragment Job**（`requirements=PANFROST_JD_REQ_FS`，进入 JS0）：根据 tile list 执行片段着色器，生成最终像素

#### 步骤 6：提交 Job Chain

代码位置：`panfrost_drv.c:241-311`

```
DRM_IOCTL_PANFROST_SUBMIT({
    jc = vertex_job_chain_gpu_va,
    in_syncs = [...],
    out_sync = syncobj_handle,
    bo_handles = [vbo, ibo, ubo, ssbo, ...],
    requirements = 0,                    // → JS1 (vertex/tiler)
})

DRM_IOCTL_PANFROST_SUBMIT({
    jc = fragment_job_chain_gpu_va,
    in_syncs = [vertex_out_sync],       // 依赖 vertex job 完成
    out_sync = frame_done_sync,
    bo_handles = [fbo, textures, ...],
    requirements = PANFROST_JD_REQ_FS,  // → JS0 (fragment)
})
```

内核处理 `SUBMIT` 的流程：

1. `panfrost_copy_in_sync()` — 解析输入同步对象为 `dma_fence`
2. `panfrost_lookup_bos()` — 将 BO handle 解析为 GEM 对象，获取对应 MMU mapping
3. `drm_sched_job_init()` — 初始化 DRM 调度器 Job
4. `panfrost_job_push()` — 将 Job 推入调度队列，获取 fence
5. 返回时，`out_sync` 同步对象被替换为 Job 的完成 fence

#### 步骤 7：Job 在内核中的执行路径

```
panfrost_ioctl_submit()
  → drm_sched_entity_push_job()
    → [DRM Scheduler 线程]
      → panfrost_job_run()           // sched_backend_ops.run_job
        → panfrost_fence_create()
        → panfrost_job_hw_submit()   // 写 GPU 寄存器提交
          → panfrost_mmu_as_get()    // 获取/分配地址空间
          → job_write(JS_HEAD_NEXT)  // 写入 Job Chain 首地址
          → job_write(JS_AFFINITY)   // 设置 Shader Core 亲和性
          → job_write(JS_CONFIG)     // MMU/优先级/flush 配置
          → job_write(JS_COMMAND_NEXT, JS_COMMAND_START)  // GO!
```

#### 步骤 8：GPU 完成与中断

GPU 完成 Job 后触发 **Job IRQ**：

代码位置：`panfrost_job.c:753-774`

```
GPU IRQ → panfrost_job_irq_handler()       (hardirq)
  → IRQ_WAKE_THREAD
  → panfrost_job_irq_handler_thread()      (threaded IRQ)
    → panfrost_job_handle_irqs()
      → panfrost_job_handle_done()         // 正常完成
        → dma_fence_signal_locked()        // 通知等待者
        → pm_runtime_put_autosuspend()     // 释放 PM 引用
```

#### 步骤 9：CPU 端等待完成

```
DRM_IOCTL_PANFROST_WAIT_BO(handle, timeout)   → 等待 BO 上所有 GPU 操作完成
或
drmSyncobjWait(syncobj, ...)                   → 等待 syncobj 中的 fence
```

### 4.3 Job Slot 路由

代码位置：`panfrost_job.c:106-126`

```c
int panfrost_job_get_slot(struct panfrost_job *job)
{
    if (job->requirements & PANFROST_JD_REQ_FS)
        return 0;  // JS0: Fragment
    return 1;      // JS1: Vertex/Tiler
    // JS2: Compute-only（未暴露）
}
```

---

## 5. GPU 内存管理（GEM + MMU）

### 5.1 GEM 缓冲区对象

Panfrost 使用 DRM 的 `drm_gem_shmem_helper` 来管理 GEM 对象，在此基础上添加了 GPU 特有逻辑。

#### 数据结构

代码位置：`panfrost_gem.h:12-41`

```c
struct panfrost_gem_object {
    struct drm_gem_shmem_object base;
    struct sg_table *sgts;        // Heap BO 的 scatter-gather 表
    struct {
        struct list_head list;     // mapping 列表
        struct mutex lock;
    } mappings;
    atomic_t gpu_usecount;        // 引用此 BO 的 Job 数
    bool noexec :1;               // 不可执行标记
    bool is_heap :1;              // Growable heap BO
};

struct panfrost_gem_mapping {
    struct list_head node;
    struct kref refcount;
    struct panfrost_gem_object *obj;
    struct drm_mm_node mmnode;    // GPU VA 空间分配节点
    struct panfrost_mmu *mmu;     // 所属 MMU 上下文
    bool active :1;               // 是否已映射到 GPU
};
```

#### 关键设计

1. **每个 BO 可以有多个 mapping**（在不同进程的地址空间中）
2. **可执行 BO 不能跨越 16MB 边界**（GPU PC 是 24-bit），因此分配时需要对齐
3. **Heap BO** 使用缺页按需映射（2MB 粒度）

代码位置：`panfrost_gem.c:139-142`
```c
if (!bo->noexec)
    align = size >> PAGE_SHIFT;  // 可执行缓冲区按大小对齐
else
    align = size >= SZ_2M ? SZ_2M >> PAGE_SHIFT : 0;
```

### 5.2 MMU 与地址空间

代码位置：`panfrost_device.h:125-135`

```c
struct panfrost_mmu {
    struct panfrost_device *pfdev;
    struct kref refcount;
    struct io_pgtable_cfg pgtbl_cfg;
    struct io_pgtable_ops *pgtbl_ops;   // io-pgtable 页表操作
    struct drm_mm mm;                    // GPU VA 空间分配器
    spinlock_t mm_lock;
    int as;                              // 当前分配的 Address Space 编号（-1=未分配）
    atomic_t as_count;                   // AS 引用计数
    struct list_head list;               // LRU 链表节点
};
```

#### 地址空间管理

- GPU VA 空间范围：`0x2000000`（32MB）到 `0x100000000`（4GB），代码位置：`panfrost_mmu.c:620`
  ```c
  drm_mm_init(&mmu->mm, SZ_32M >> PAGE_SHIFT, (SZ_4G - SZ_32M) >> PAGE_SHIFT);
  ```
- 页表格式：`ARM_MALI_LPAE`（Mali 专用的 64-bit LPAE 格式）
- 页面大小：4KB + 2MB 块映射

#### AS 分配与 LRU

GPU 硬件上的 Address Space 数量有限（通常 16 个），但进程数可以更多。Panfrost 使用 **LRU 淘汰**机制：

代码位置：`panfrost_mmu.c:158-222`

```c
u32 panfrost_mmu_as_get(struct panfrost_device *pfdev, struct panfrost_mmu *mmu)
{
    // 如果 MMU 上下文已有 AS，增加引用计数
    if (as >= 0) {
        atomic_inc_return(&mmu->as_count);
        list_move(&mmu->list, &pfdev->as_lru_list);  // 移到 LRU 头部
        goto out;
    }

    // 没有空闲 AS，从 LRU 尾部淘汰
    as = ffz(pfdev->as_alloc_mask);
    if (!(BIT(as) & pfdev->features.as_present)) {
        list_for_each_entry_reverse(lru_mmu, &pfdev->as_lru_list, list) {
            if (!atomic_read(&lru_mmu->as_count))
                break;
        }
        // 淘汰最久未用的 AS
        lru_mmu->as = -1;
    }

    // 分配新 AS 并加载页表到硬件
    panfrost_mmu_enable(pfdev, mmu);
}
```

#### 缺页处理（Heap BO）

Heap BO 最初不映射任何页面，当 GPU 访问未映射区域时触发 MMU Page Fault：

代码位置：`panfrost_mmu.c:439-544`

```c
static int panfrost_mmu_map_fault_addr(struct panfrost_device *pfdev, int as, u64 addr)
{
    // 1. 根据 AS 和地址找到对应的 BO mapping
    bomapping = addr_to_mapping(pfdev, as, addr);

    // 2. 分配 2MB 页面
    for (i = page_offset; i < page_offset + NUM_FAULT_PAGES; i++) {
        pages[i] = shmem_read_mapping_page(mapping, i);
    }

    // 3. 映射到 GPU 页表
    mmu_map_sg(pfdev, bomapping->mmu, addr, ...);
}
```

### 5.3 4GB 边界约束

代码位置：`panfrost_mmu.c:586-606`

可执行 BO 不能起始或结束于 4GB 边界（因为 GPU 程序计数器是 24-bit），`panfrost_drm_mm_color_adjust()` 负责在 VA 分配时避免这种对齐。

---

## 6. Job 提交与调度

### 6.1 DRM Scheduler 集成

Panfrost 使用 Linux 内核的 `drm_sched`（DRM GPU Scheduler）框架管理 Job 队列。

架构：

```
Userspace SUBMIT
  → drm_sched_entity_push_job()
    → Job 进入 entity 队列
      → drm_sched 线程取出 Job
        → panfrost_job_run()          // 提交到硬件
          → panfrost_job_hw_submit()
```

### 6.2 硬件提交

代码位置：`panfrost_job.c:187-242`

```c
static void panfrost_job_hw_submit(struct panfrost_job *job, int js)
{
    pm_runtime_get_sync(pfdev->dev);

    cfg = panfrost_mmu_as_get(pfdev, job->mmu);   // 获取 AS

    job_write(pfdev, JS_HEAD_NEXT_LO(js), lower_32_bits(jc_head));  // Job Chain 首地址
    job_write(pfdev, JS_HEAD_NEXT_HI(js), upper_32_bits(jc_head));

    panfrost_job_write_affinity(pfdev, job->requirements, js);  // Core 亲和性

    cfg |= JS_CONFIG_THREAD_PRI(8) |
           JS_CONFIG_START_FLUSH_CLEAN_INVALIDATE |
           JS_CONFIG_END_FLUSH_CLEAN_INVALIDATE;

    job_write(pfdev, JS_CONFIG_NEXT(js), cfg);

    // 写入 flush ID（用于 flush reduction 优化）
    if (panfrost_has_hw_feature(pfdev, HW_FEATURE_FLUSH_REDUCTION))
        job_write(pfdev, JS_FLUSH_ID_NEXT(js), job->flush_id);

    // GO!
    job_write(pfdev, JS_COMMAND_NEXT(js), JS_COMMAND_START);
}
```

关键配置：
- **线程优先级**：8（中等）
- **起始/结束 Cache Flush**：确保内存一致性
- **Affinity**：使用所有可用 Shader Core（`shader_present`）

### 6.3 Fence 机制

代码位置：`panfrost_job.c:48-104`

每个 Job 提交时创建一个 `panfrost_fence`（基于 `dma_fence`），用于：
1. GPU 完成时通过 `dma_fence_signal()` 通知等待者
2. 用户态通过 syncobj 跨进程传递同步状态
3. BO 的 `dma_resv` 机制确保正确的内存访问顺序

```c
struct panfrost_fence {
    struct dma_fence base;
    struct drm_device *dev;
    u64 seqno;
    int queue;  // Job Slot 编号
};
```

---

## 7. 中断处理与 GPU Reset

### 7.1 三类中断

| 中断名 | 用途 | 注册位置 |
|--------|------|---------|
| `gpu` | GPU 故障、性能计数器完成 | `panfrost_gpu.c:420` |
| `job` | Job 完成/失败 | `panfrost_job.c:796-804` |
| `mmu` | 页表翻译错误/缺页 | `panfrost_mmu.c:757-765` |

### 7.2 Job 中断处理

代码位置：`panfrost_job.c:467-566`

Job IRQ 使用 **hardirq + threaded IRQ** 模型：
- hardirq：读取中断状态，屏蔽中断，唤醒线程
- threaded IRQ：处理实际的 Job 完成/失败

每个 Job Slot 有两种中断事件：
- **DONE**：Job 正常完成
- **ERR**：Job 执行出错

```c
static void panfrost_job_handle_irq(struct panfrost_device *pfdev, u32 status)
{
    // 收集所有 done/failed jobs
    // 正常完成的 job → panfrost_job_handle_done()
    // 出错的 job → panfrost_job_handle_err()
    //   - STOPPED：更新 jc head，不 signal fence（将恢复）
    //   - TERMINATED：设置 fence error = -ECANCELED
    //   - 其他 fault：设置 fence error = -EINVAL
}
```

### 7.3 GPU Reset

当 Job 超时（500ms）或检测到致命错误时，触发 GPU Reset：

代码位置：`panfrost_job.c:604-707`

```
panfrost_job_timedout()
  → panfrost_reset()
    1. drm_sched_stop() — 停止所有调度器
    2. 屏蔽 Job 中断
    3. JS_COMMAND(SOFT_STOP) — 软停止所有 Job
    4. 等待 10ms 让软停止完成
    5. panfrost_job_handle_irqs() — 处理残余中断
    6. panfrost_device_reset() — 执行完整复位
       → panfrost_gpu_soft_reset()
       → panfrost_gpu_power_on()
       → panfrost_mmu_reset()
    7. drm_sched_resubmit_jobs() — 重新提交未完成的 Job
    8. drm_sched_start() — 重启调度器
    9. 重新使能 Job 中断
```

### 7.4 GPU Core Dump

代码位置：`panfrost_dump.c`

当 Job 超时时，`panfrost_core_dump(job)` 会将 GPU 寄存器状态和引用的 BO 内容导出为二进制 core dump，供用户态工具分析。

---

## 8. 电源管理与 Devfreq

### 8.1 Runtime PM

Panfrost 使用 Linux Runtime PM 框架：

- 每次 Job 提交：`pm_runtime_get_sync()` 唤醒 GPU
- 每次 Job 完成：`pm_runtime_put_autosuspend()` 延迟 50ms 后自动挂起
- 50ms 约等于 3 帧@60fps，避免频繁唤醒/挂起

代码位置：`panfrost_drv.c:600`
```c
pm_runtime_set_autosuspend_delay(pfdev->dev, 50);
```

### 8.2 GPU 上电/下电序列

代码位置：`panfrost_gpu.c:344-397`

**上电**：
```
panfrost_gpu_power_on()
  → panfrost_gpu_init_quirks()           // 设置硬件 quirk 寄存器
  → gpu_write(L2_PWRON_LO, ...)          // 给 L2 cache 上电
  → gpu_write(SHADER_PWRON_LO, ...)      // 给 Shader Core 上电
  → gpu_write(TILER_PWRON_LO, ...)       // 给 Tiler 上电
  → 等待各单元 READY 位
```

**下电**：
```
panfrost_gpu_power_off()
  → gpu_write(SHADER_PWROFF_LO, ...)
  → gpu_write(TILER_PWROFF_LO, ...)
  → gpu_write(L2_PWROFF_LO, ...)
  → 等待各单元 PWRTRANS 位清零
```

### 8.3 Devfreq

代码位置：`panfrost_devfreq.c`

Panfrost 集成了 `devfreq` 子系统实现动态调频：
- 使用 `simple_ondemand` governor
- 在 Job 提交时标记 GPU busy，Job 完成时标记 idle
- 根据负载调整 GPU 时钟频率

---

## 9. 与 Linux 6.18.35 Panfrost 的差异

coolpi-kernel 的 Panfrost 版本为 **v1.2**，而 Linux 6.18.35 的 Panfrost 已升级到 **v1.4**。

### 9.1 UAPI 差异

| 特性 | coolpi (v1.2) | 6.18.35 (v1.4) | 代码位置（6.18） |
|------|--------------|-----------------|------------------|
| `PANFROST_JD_REQ_CYCLE_COUNT` | 无 | 有 | `panfrost_drm.h:45` |
| `SYSTEM_TIMESTAMP` 查询 | 无 | 有 | `panfrost_drm.h:178` |
| `SYSTEM_TIMESTAMP_FREQUENCY` 查询 | 无 | 有 | `panfrost_drm.h:179` |
| `SET_LABEL_BO` IOCTL | 无 | 有 | `panfrost_drm.h:24, 235-249` |
| `PANFROST_BO_LABEL_MAXLEN` | 无 | 4096 | `panfrost_gem.h` |

### 9.2 新增功能

#### 9.2.1 GPU Cycle Counter / Profiling

6.18 版本新增了 GPU profiling 支持：

代码位置：`panfrost_drv.c:610-647`（6.18）
```c
static void panfrost_gpu_show_fdinfo(struct panfrost_device *pfdev, ...)
{
    for (i = 0; i < NUM_JOB_SLOTS - 1; i++) {
        if (pfdev->profile_mode) {
            drm_printf(p, "drm-engine-%s:\t%llu ns\n", ...);
            drm_printf(p, "drm-cycles-%s:\t%llu\n", ...);
        }
        drm_printf(p, "drm-maxfreq-%s:\t%lu Hz\n", ...);
        drm_printf(p, "drm-curfreq-%s:\t%lu Hz\n", ...);
    }
}
```

- 通过 sysfs `profiling` 属性控制开关
- 在 Job 完成时记录 GPU cycle 计数和耗时
- 通过 `fdinfo` 暴露给用户态（如 `top` 命令可显示 GPU 使用率）

coolpi 版本**没有此功能**。

#### 9.2.2 BO Label（调试标签）

6.18 版本允许用户态为 BO 设置标签字符串（最长 4096 字符），用于 debugfs 显示：

代码位置：`panfrost_drv.c:501-539`（6.18）
```c
static int panfrost_ioctl_set_label_bo(...)
{
    label = strndup_user(u64_to_user_ptr(args->label), PANFROST_BO_LABEL_MAXLEN);
    panfrost_gem_set_label(obj, label);
}
```

coolpi 版本**没有此功能**。

#### 9.2.3 Timestamp 查询

6.18 版本新增 `SYSTEM_TIMESTAMP` 和 `SYSTEM_TIMESTAMP_FREQUENCY` 参数查询：

代码位置：`panfrost_drv.c:34-49, 98-110`（6.18）
```c
static int panfrost_ioctl_query_timestamp(struct panfrost_device *pfdev, u64 *arg)
{
    panfrost_cycle_counter_get(pfdev);
    *arg = panfrost_timestamp_read(pfdev);
    panfrost_cycle_counter_put(pfdev);
}

case DRM_PANFROST_PARAM_SYSTEM_TIMESTAMP_FREQUENCY:
    param->value = arch_timer_get_cntfrq();
```

coolpi 版本**没有此功能**。

#### 9.2.4 JD_REQ_CYCLE_COUNT

6.18 版本的 `SUBMIT` IOCTL 允许 `requirements` 包含 `PANFROST_JD_REQ_CYCLE_COUNT`：

代码位置：`panfrost_drv.c:29`（6.18）
```c
#define JOB_REQUIREMENTS (PANFROST_JD_REQ_FS | PANFROST_JD_REQ_CYCLE_COUNT)
```

coolpi 版本只允许 `PANFROST_JD_REQ_FS`。

#### 9.2.5 更多 SoC 支持与 PM Quirk

6.18 版本支持更多 SoC 的 PM 特性：

| SoC | 新增 PM 特性 | 代码位置（6.18） |
|-----|-------------|-----------------|
| Allwinner H616 | `GPU_PM_RT`（Runtime suspend + reset） | `panfrost_drv.c:858-863` |
| MediaTek MT8183B | `GPU_PM_CLK_DIS | GPU_PM_VREG_OFF` | `panfrost_drv.c:889-895` |
| MediaTek MT8186 | `GPU_PM_CLK_DIS | GPU_PM_VREG_OFF` | `panfrost_drv.c:897-903` |
| MediaTek MT8188 | `GPU_QUIRK_FORCE_AARCH64_PGTABLE` | `panfrost_drv.c:905-912` |
| MediaTek MT8192 | `GPU_QUIRK_FORCE_AARCH64_PGTABLE` | `panfrost_drv.c:914-921` |
| MediaTek MT8370 | `GPU_QUIRK_FORCE_AARCH64_PGTABLE` | `panfrost_drv.c:923-930` |

coolpi 版本只支持 `default_data`、`amlogic_data` 和 `mediatek_mt8183_data`。

#### 9.2.6 Debugfs GEM 列表

6.18 版本新增 debugfs 接口显示所有 GEM BO 及其标签、创建者信息：

代码位置：`panfrost_drv.c:666-693`（6.18）

#### 9.2.7 Suspend/Resume IRQ 处理

6.18 版本在挂起期间屏蔽 IRQ，防止在 GPU 已断电后收到中断：

coolpi 版本**没有此功能**。

#### 9.2.8 API 变化

| API | coolpi | 6.18 | 说明 |
|-----|--------|------|------|
| `drm_sched_job_add_dependency()` | 使用 | 不再使用 | 6.18 改用 `drm_sched_job_add_syncobj_dependency()` |
| `drm_gem_shmem_madvise()` | 使用 | 使用 `_locked` 变体 | 6.18: `drm_gem_shmem_madvise_locked()` |
| `drm_sched_init()` | 旧参数 | `drm_sched_init_args` 结构体 | 参数封装变化 |
| `drm_sched_job_init()` | 4 参数 | 5 参数（多 `client_id`） | 6.18 新增 `client_id` 用于 fdinfo |
| `pm_ptr()` 宏 | 不使用 | 使用 | 6.18 使用 `pm_ptr()` 替代 `#ifdef CONFIG_PM` |
| `DEFINE_DRM_GEM_FOPS` | 使用 | 不使用 | 6.18 手动定义 fops 以加入 `.show_fdinfo` |

### 9.3 差异汇总

| 方面 | coolpi (v1.2) | 6.18 (v1.4) |
|------|--------------|--------------|
| IOCTL 数量 | 9 | 10（+SET_LABEL_BO） |
| GET_PARAM 参数 | 27 | 29（+TIMESTAMP, TIMESTAMP_FREQ） |
| SUBMIT requirements | `JD_REQ_FS` | `JD_REQ_FS \| JD_REQ_CYCLE_COUNT` |
| GPU Profiling | 无 | 有（sysfs + fdinfo） |
| BO Label | 无 | 有（debugfs） |
| Suspend IRQ | 无 | 有 |
| SoC PM Quirk | 3 种 | 8+ 种 |
| 驱动版本 | 1.2 | 1.4 |

---

## 10. Panfrost vs Panthor：架构对比

Panthor 是 Mali GPU 的**下一代**开源 DRM 驱动，面向 **CSF (Command Stream Frontend)** 架构的 GPU（Mali-G610 等 Valhall CSF 及更新的 GPU）。

### 10.1 核心架构差异

| 方面 | Panfrost (JM) | Panthor (CSF) |
|------|---------------|---------------|
| 目标 GPU 架构 | Job Manager (Midgard/Bifrost/Valhall-JM) | CSF (Valhall-CSF, Diminish) |
| 命令提交方式 | Job Chain（GPU 解析链表） | Command Stream Ring（GPU 读取环形缓冲区） |
| 固件 | 无 | 有（MCU 固件 `arm-mali-csf.bin`） |
| 调度 | 内核 `drm_sched` + 3 个 Job Slot | 内核 `drm_sched` + MCU 固件调度 |
| 地址空间数量 | 硬件 AS（通常 16） | 虚拟化（CSF 固件管理） |
| GPU 型号支持 | T604 → G57 (Valhall-JM) | G610 (Valhall-CSF) → 更新 |
| 文件数量 | 23 | 22（6.18 版本） |
| 代码量 | ~150 KB | ~340 KB |

### 10.2 详细架构对比

#### 10.2.1 命令提交模型

**Panfrost (JM)**：
```
用户态构建 Job Chain（链表）→ 内核写入 JS_HEAD 寄存器 → GPU JM 硬件解析链表
```

**Panthor (CSF)**：
```
用户态写入 Command Stream Ring → 内核通知 MCU 固件 → MCU 固件读取 Ring 并调度
```

Panthor 的 CSF 模型更接近现代 GPU（如 AMDGPU 的 amdgpu_ring），由固件管理命令队列，内核不再直接操作 Job Slot 寄存器。

#### 10.2.2 固件管理

代码位置：`panthor_fw.c`（6.18，39.8 KB）

Panthor 需要加载 **MCU 固件**（`arm-mali-csf.bin`），固件负责：
- Command Stream 调度
- GPU 资源管理
- Doorbell 中断处理
- Group 优先级调度

Panfrost **不需要任何固件**，所有调度由内核和硬件 Job Manager 完成。

#### 10.2.3 调度模型

**Panfrost**：
- 3 个硬件 Job Slot，每个 2 深度队列
- `drm_sched` 直接管理每个 Slot
- 简单的 slot → type 映射

**Panthor**：
- 用户态创建 **Group**（包含多个 Queue）
- 每个 Queue 对应一个 Command Stream Ring
- MCU 固件在 Group 之间做优先级调度
- 支持实时优先级（`PANTHOR_GROUP_PRIORITY_REALTIME`）
- 内核 `drm_sched` 管理 Group 级别的调度

代码位置：`panthor_sched.c`（6.18，114.8 KB）

#### 10.2.4 UAPI 差异

| Panfrost IOCTL | Panthor 等价 |
|----------------|-------------|
| `SUBMIT` | `GROUP_SUBMIT`（提交到 Group 的 Queue） |
| `CREATE_BO` | `BO_CREATE` |
| `MMAP_BO` | `BO_MMAP_OFFSET` |
| `GET_PARAM` | `DEV_QUERY`（更结构化的查询） |
| `WAIT_BO` | 无直接等价（使用 syncobj） |
| `MADVISE` | `BO_MADVISE` |
| 无 | `GROUP_CREATE` / `GROUP_DESTROY` |
| 无 | `QUEUE_CREATE` / `QUEUE_DESTROY` |
| 无 | `TILER_HEAP_CREATE` / `TILER_HEAP_DESTROY` |
| 无 | `VM_CREATE` / `VM_DESTROY` |
| 无 | `VM_BIND` / `VM_UNBIND` |
| 无 | `BO_SET_LABEL` |
| 无 | `SET_USER_MMIO_OFFSET` |

Panthor 的 UAPI 更复杂（13 个 IOCTL vs 9 个），反映了 CSF 架构需要更多的内核/用户态协作。

#### 10.2.5 内存管理

**Panfrost**：
- MMU 页表由内核的 `io-pgtable` 框架管理
- GPU VA 空间使用 `drm_mm` 分配器
- AS（Address Space）硬件数量有限，需要 LRU 淘汰
- 页表映射在 `BO open` 时完成

**Panthor**：
- 引入 **VM（Virtual Memory）** 对象
- 使用 `drm_gpuvm` 框架（更现代的 GPU VM 管理）
- 支持 `VM_BIND`/`VM_UNBIND` 操作（显式页表管理）
- AS 由 MCU 固件管理，不再需要内核 LRU
- 延迟映射（lazy mapping）

代码位置：`panthor_mmu.c`（6.18，76.8 KB）

#### 10.2.6 对 RK3588 的意义

RK3588 的 Mali-G610 实际上是 **Valhall-CSF** 架构（不是 Valhall-JM），这意味着：

1. **Panfrost 驱动不能正确驱动 G610**——Panfrost 是 JM 模型驱动，但 G610 硬件是 CSF 模型
2. **Panthor 驱动才是 G610 的正确驱动**
3. coolpi-kernel 中当前存在的 Panfrost 驱动对 G610 可能只能提供**有限功能**（如查询参数），无法真正提交渲染任务
4. 要在 RK3588 上获得完整的 GPU 支持，需要 **Panthor 驱动 + MCU 固件**

---

## 11. 总结

### coolpi-kernel Panfrost 驱动现状

1. **版本较旧**：v1.2，相比 6.18 的 v1.4 缺少 profiling、BO label、timestamp 查询等功能
2. **架构不匹配**：G610 是 CSF 架构，Panfrost 是 JM 架构驱动，存在根本性的不兼容
3. **代码几乎未被修改**：除了 `DRM_GPU_SCHED_STAT_NOMINAL` → `DRM_GPU_SCHED_STAT_RESET` 的枚举重命名，驱动源码与上游一致

### 渲染流程总结

```
OpenGL ES App
  → Mesa Panfrost Gallium3D (编译着色器、构建 Job Chain)
    → DRM_IOCTL_PANFROST_CREATE_BO (分配 GPU 缓冲区)
    → DRM_IOCTL_PANFROST_SUBMIT (提交 Job Chain)
      → 内核 drm_sched 排队
      → panfrost_job_hw_submit() 写入 GPU 寄存器
      → GPU Job Manager 硬件执行
        → Vertex Shader → Tiler → Fragment Shader
      → Job 完成中断 → dma_fence signal
    → DRM_IOCTL_PANFROST_WAIT_BO 或 syncobj_wait 等待完成
  → 帧缓冲区包含最终渲染结果
```

### 对 G610 的实际意义

由于架构不匹配，Panfrost 在 RK3588 上的 G610 上的实际状态是：
- 驱动可以成功 probe 和初始化
- GPU 参数查询可以正常工作
- 但 Job 提交可能无法正常工作，因为 G610 不支持 JM 硬件接口
- 需要 Panthor 驱动才能在 G610 上实现完整的 GPU 加速

---


---

## 12 固件依赖与加载逻辑

### Panfrost 驱动：无需固件

Panfrost 驱动 **不依赖任何固件文件**。在整个 `drivers/gpu/drm/panfrost/` 目录中没有任何 `request_firmware()` 调用。

这是因为 Panfrost 针对的 JM（Job Manager）架构 GPU 自带硬件调度器：

- **Job 提交**：内核直接写 `JS_HEAD_NEXT_LO/HI` 寄存器，GPU 硬件 Job Manager 自动执行

  ```c
  // coolpi-kernel/drivers/gpu/drm/panfrost/panfrost_job.c
  // 直接写寄存器提交 Job，无需固件
  job_write(pfdev, JS_HEAD_NEXT_LO(js), lower_32_bits(jc));
  job_write(pfdev, JS_HEAD_NEXT_HI(js), upper_32_bits(jc));
  ```

- **Shader Core 电源管理**：直接操作电源寄存器

  ```c
  // coolpi-kernel/drivers/gpu/drm/panfrost/panfrost_gpu.c:360-363
  gpu_write(pfdev, SHADER_PWRON_LO,
            pfdev->features.shader_present & core_mask);
  ret = readl_relaxed_poll_timeout(pfdev->iomem + SHADER_READY_LO,
            val, val == (pfdev->features.shader_present & core_mask),
  ```

- **MMU 页表**：内核使用 `io-pgtable` (ARM_MALI_LPAE 格式) 自行构建，不需要固件参与

### Panthor 驱动：强依赖 MCU 固件

Panthor 驱动针对的 CSF（Command Stream Frontend）架构 GPU **必须加载 MCU 固件**才能工作。

#### 固件名称与路径拼接

```c
// coolpi-kernel/drivers/gpu/drm/panthor/panthor_fw.c:28
#define CSF_FW_NAME "mali_csffw.bin"

// coolpi-kernel/drivers/gpu/drm/panthor/panthor_fw.c:748-753
snprintf(fw_path, sizeof(fw_path), "arm/mali/arch%d.%d/%s",
         (u32)GPU_ARCH_MAJOR(ptdev->gpu_info.gpu_id),
         (u32)GPU_ARCH_MINOR(ptdev->gpu_info.gpu_id),
         CSF_FW_NAME);

ret = request_firmware(&fw, fw_path, ptdev->base.dev);
```

固件路径格式为：`arm/mali/arch<ARCH_MAJOR>.<ARCH_MINOR>/mali_csffw.bin`

#### GPU ID 解码

```c
// coolpi-kernel/drivers/gpu/drm/panthor/panthor_regs.h:14-17
#define   GPU_ARCH_MAJOR(x)    ((x) >> 28)                      // [31:28]
#define   GPU_ARCH_MINOR(x)    (((x) & GENMASK(27, 24)) >> 24)  // [27:24]
#define   GPU_ARCH_REV(x)      (((x) & GENMASK(23, 20)) >> 20)  // [23:20]
#define   GPU_PROD_MAJOR(x)    (((x) & GENMASK(19, 16)) >> 16)  // [19:16]
```

#### Mali-G610 的 GPU ID

Mali-G610 的 product_id 为 `GPU_PROD_ID_MAKE(10, 7)`（arch_major=10, prod_major=7）：

```c
// coolpi-kernel/drivers/gpu/drm/panthor/panthor_hw.c:8-9,27-28
#define GPU_PROD_ID_MAKE(arch_major, prod_major) \
    (((arch_major) << 24) | (prod_major))

case GPU_PROD_ID_MAKE(10, 7):
    return "Mali-G610";
```

因此 G610 的 `GPU_ARCH_MAJOR=10`，但 `GPU_ARCH_MINOR` 取决于芯片具体 revision（不同批次的 RK3588 可能不同）。

#### MODULE_FIRMWARE 声明

```c
// coolpi-kernel/drivers/gpu/drm/panthor/panthor_fw.c:1405-1410
MODULE_FIRMWARE("arm/mali/arch10.8/mali_csffw.bin");
MODULE_FIRMWARE("arm/mali/arch10.10/mali_csffw.bin");
MODULE_FIRMWARE("arm/mali/arch10.12/mali_csffw.bin");
MODULE_FIRMWARE("arm/mali/arch11.8/mali_csffw.bin");
MODULE_FIRMWARE("arm/mali/arch12.8/mali_csffw.bin");
MODULE_FIRMWARE("arm/mali/arch13.8/mali_csffw.bin");
```

RK3588 Mali-G610 属于 arch 10.x 系列，可能的固件路径为：

| GPU Arch | 固件路径 | 适配 GPU |
|----------|---------|---------|
| arch10.8 | `arm/mali/arch10.8/mali_csffw.bin` | Mali-G710 / G610 (早期) |
| arch10.10 | `arm/mali/arch10.10/mali_csffw.bin` | Mali-G610 / G510 |
| arch10.12 | `arm/mali/arch10.12/mali_csffw.bin` | Mali-G610 / G510 / G310 (后期) |

具体需要哪个，由运行时读取的 `GPU_ID` 寄存器决定。

#### 固件加载流程

```
panthor_probe()
  → panthor_device_init()
    → panthor_fw_init()
      → panthor_fw_load()                           // panthor_fw.c:740
        → 读取 GPU_ID 寄存器 → 拼接固件路径
        → request_firmware(&fw, fw_path, dev)       // 请求内核固件子系统加载
        → 解析固件二进制头部 (magic: 0x3F3F3F3F)
        → panthor_fw_binary_iter_read()             // 逐段解析
        → 加载固件段到 MCU 区域
      → panthor_fw_boot()                            // 启动 MCU
        → 写 MCU_CONTROL 寄存器启动固件
        → 等待固件就绪 (GLB_REQ == GLB_ACK)
      → panthor_fw_init_global_iface()               // 初始化全局接口
```

#### 固件的作用

MCU 固件在 CSF 架构中承担以下职责：

| 功能 | 说明 |
|------|------|
| Command Stream 调度 | CSF 没有 JM 硬件，由 MCU 固件运行调度算法 |
| Group/Queue 管理 | 固件负责创建/销毁 CSG 和 CS |
| Doorbell 中断处理 | 管理用户态提交的 Command Queue 通知 |
| 同步原语管理 | 管理同步对象和信号量 |
| 电源管理协助 | 协助 GPU 供电状态转换 |
| Profiling 数据采集 | 收集 GPU 性能计数器数据 |

#### 如何获取固件

```bash
# 方法1：从 linux-firmware 官方仓库获取
git clone https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git
# 固件位于：
#   linux-firmware/arm/mali/arch10.8/mali_csffw.bin
#   linux-firmware/arm/mali/arch10.10/mali_csffw.bin
#   linux-firmware/arm/mali/arch10.12/mali_csffw.bin

# 方法2：安装发行版包（Debian/Ubuntu）
sudo apt install linux-firmware
# 固件安装到 /lib/firmware/arm/mali/arch10.x/mali_csffw.bin

# 方法3：从 ARM 官方开发者网站下载
# https://developer.arm.com/downloads/-/mali-drivers-and-user-space-components
```

#### 如何部署到设备

`request_firmware()` 在以下路径搜索固件（按优先级）：

| 搜索路径 | 说明 |
|----------|------|
| `/lib/firmware/` | 标准固件目录（最常用） |
| `/lib/firmware/<kernel-version>/` | 内核版本特定目录 |
| 自定义路径 | 通过 `firmware_class.path` 内核参数指定 |

```bash
# 1. 将固件复制到设备
adb push mali_csffw.bin /lib/firmware/arm/mali/arch10.8/

# 如果不确定 arch minor，把三个都放上去
adb push mali_csffw.bin /lib/firmware/arm/mali/arch10.8/
adb push mali_csffw.bin /lib/firmware/arm/mali/arch10.10/
adb push mali_csffw.bin /lib/firmware/arm/mali/arch10.12/

# 2. 重新加载驱动
sudo modprobe -r panthor
sudo modprobe panthor

# 3. 查看内核日志确认固件加载和 GPU 型号
dmesg | grep -i "mali\|panthor\|firmware\|gpu_id"
# 成功时输出类似：
#   panthor gpu: Mali-G610 id 0xa007 major 0xa minor 0x0 status 0x0
# 失败时输出：
#   panthor gpu: Failed to load firmware image 'mali_csffw.bin'
```

#### 如何确定具体需要的固件版本

启动后查看内核日志中的 `gpu_id`：

```bash
dmesg | grep "id 0x"
# 输出示例：Mali-G610 id 0xa007 major 0xa minor 0x0 status 0x0
```

`id 0xa007` 中的高 4 位 `0xa` = arch major 10，bits [27:24] = arch minor。假设 arch minor = 8，则固件路径为 `/lib/firmware/arm/mali/arch10.8/mali_csffw.bin`。

### 两个驱动的固件对比

| 特性 | Panfrost (JM) | Panthor (CSF) |
|------|--------------|---------------|
| 固件依赖 | **无** | MCU 固件（`mali_csffw.bin`，必须） |
| 调度方式 | 硬件 Job Manager | MCU 固件调度 |
| Job 提交 | 写 `JS_HEAD` 寄存器 | 通过 Ring Buffer + Doorbell → MCU 固件处理 |
| MMU | 内核直接管理页表 | 固件参与部分管理 |
| 电源管理 | 直接操作电源寄存器 | 固件协助状态转换 |
| 适用 GPU | Mali-T6xx ~ Mali-G57 | Mali-G610+ (Valhall CSF) |

> **注意**：coolpi-kernel 中同时存在 ARM 私有 Mali 驱动（`drivers/gpu/arm/bifrost/`），它也有自己的固件加载逻辑，固件名称可能不同。两个驱动**不能同时绑定同一个 GPU**，需要通过设备树 `compatible` 字段选择使用哪个驱动。


*基于 coolpi-kernel (Linux 6.1.75) 和 linux-6.18.35 源代码分析*

