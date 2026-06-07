---
title: oh-my-pi的独特之处
author: deepseek-v4-pro
tags:
  - LLM
  - Agent
date: 2026-06-07
LLM: 1
---

# Oh My Pi：Memory、Cache 与 Harness 工程深度剖析

## 一、记忆工程（Memory Engineering）

记忆系统是一个 coding agent 能否在长期项目中持续有效的基础。没有记忆的 agent 每次启动就像"失忆"一样重新开始。Oh My Pi 的记忆工程在两层设计上远超 Codex CLI。

### 1.1 Oh My Pi 的 Hindsight 系统

Oh My Pi 的记忆后端称为 Hindsight，是一个**两阶段提取-合并流水线**（extraction-consolidation pipeline）：

#### 第一阶段：逐会话提取（Phase 1 — Per-Session Extraction）

```
启动 pipeline
  │
  ├─ collectThreads() → 扫描所有历史会话 JSONL 文件
  │   ├─ 过滤：跳过 maxRolloutAgeDays 之前的旧会话
  │   ├─ 跳过：minRolloutIdleHours 内仍活跃的会话
  │   └─ 过滤：跳过当前活跃会话
  │
  ├─ upsertThreads() → 将会话元数据写入 SQLite
  │
  ├─ claimStage1Jobs() → 按项目（cwd）领取待处理任务
  │   ├─ 任务队列是 SQLite 表：`memory_stage1`
  │   ├─ 用所有权限令牌（ownership token）防止多进程抢跑
  │   └─ 默认最多处理 64 个会话（maxRolloutsPerStartup）
  │
  └─ runStage1Job() → 对每个认领的会话：
      ├─ 从 JSONL 提取可持久化的回复消息
      │   └─ 只保留 user/assistant/toolResult（过滤 custom/hook）
      ├─ 调用总结模型生成 `{ summary, synopsis }`
      ├─ 输出写入 `memories/<project>/stage1/<threadId>.md`
      └─ markStage1SucceededWithOutput()
```

**关键设计决策：**

- **按项目隔离**（per-cwd）：`encodeProjectPath(cwd)` 将路径编码为目录名（如 `--home-jask-codes-oh-my-pi--`），防止交叉项目污染。此 bug 之前确实出现过。
- **所有权限令牌 + 租约**：Phase 2 合并使用了基于心跳的分布式锁，多个同时启动的进程不会重复合并
- **模型路由**：Phase 1 使用当前的 `default` 角色模型，Phase 2 合并使用 `smol` 角色（低价模型）

#### 第二阶段：跨会话合并（Phase 2 — Consolidation）

```
tryClaimGlobalPhase2Job()
  ├─ 检查是否有新的 Phase 1 输出
  ├─ 获取全球任务的排他性租约（leaseSeconds）
  │
  ├─ listStage1OutputsForGlobal() → 收集所有 Phase 1 输出
  │   └─ 按 cwd 过滤，仅合并本项目数据
  │
  ├─ buildRawMemoriesMarkdown() → 将所有逐会话提取拼接为一个 Markdown
  │
  ├─ runConsolidationModel() → 调用合并模型
  │   └─ 输出结构：
  │       MEMORY.md        ← 长格式的记忆文档
  │       memory_summary.md ← 注入系统提示的压缩摘要
  │       skills/          ← 可复用的程序性技能 playbook
  │
  ├─ redactSecrets() → 在写入磁盘前移除常见密钥模式
  ├─ applyConsolidation() → 写入磁盘
  └─ markGlobalPhase2Succeeded()
```

### 1.2 会话内记忆工具

除了后台自动流水线外，会话内也暴露了直接的工具：

| 工具 | 作用 |
|------|------|
| `retain` | Agent 在运行中主动将事实排队写入记忆库 |
| `recall` | 搜索 Hindsight 记忆库获取原始记忆 |
| `reflect` | 让 Hindsight 在记忆库上合成答案 |
| `memory://root` | 通过 `read` 读取当前项目的压缩记忆摘要 |
| `memory://root/MEMORY.md` | 读取完整的长期记忆文档 |

记忆注入时机：每次会话启动时，如果当前项目存在 `memory_summary.md`，它会被注入到系统提示的 Memory Guidance 块中。

### 1.3 Codex CLI 的记忆对比

**Codex CLI 没有记忆系统。** 这是 omp 与 Codex CLI 在 Memory 维度上最本质的差异。

| 维度 | Oh My Pi (Hindsight) | Codex CLI |
|------|---------------------|-----------|
| 持久记忆 | ✅ 两阶段流水线 | ❌ 无 |
| 跨会话提取 | ✅ 从历史 JSONL 提取 | ❌ 无 |
| 合并策略 | ✅ 项目隔离 + 版本合并 | ❌ 无 |
| 注入方式 | ✅ 会话启动系统提示 | ❌ 无 |
| 运行时记忆操作 | ✅ `retain`/`recall`/`reflect` | ❌ 无 |
| 技能生成 | ✅ 自动生成 skills/ playbook | ❌ 无 |
| 密钥过滤 | ✅ 写入前 redact 常见模式 | ❌ 无 |
| 记忆范围 | ✅ 项目级（per cwd） | ❌ 无 |

**影响：**
- omp 可以在同一项目上连续工作数周，增量积累技术决策、已有约束、已解决过的故障
- Codex CLI 每次启动都是"空白"状态，项目知识仅靠当前对话窗口维持

---

## 二、缓存工程（Cache Engineering）

缓存是 omp 在性能上碾压 Codex CLI 的核心原因。omp 在三个层级的缓存上做了深度工程：**文件系统扫描缓存**、**内容哈希快照存储**、**二进制 blob 全局去重**。

### 2.1 FS Scan Cache（文件系统扫描缓存）

这是 omp 最高频的缓存路径，位于 Rust 层，用 `DashMap` 实现：

```
消费方：
  glob()          ──┐
  fuzzyFind()     ──┤
  grep (cached)   ──┼──→ get_or_scan(root, hidden, gitignore, detail)
                      │
                      ├─ 缓存命中（TTL 内）→ 返回克隆条目
                      ├─ 缓存过期           → 淘汰、重新扫描、存储
                      └─ 缓存未命中         → 扫描、存储
```

**缓存键（CacheKey）维度：**

| 维度 | 含义 | 示例 |
|------|------|------|
| `root` | 规范化后根目录路径 | `/home/user/project` |
| `include_hidden` | 是否包含隐藏文件 | `false` |
| `use_gitignore` | 是否遵循 gitignore | `true` |
| `skip_node_modules` | 是否跳过 node_modules | `true` |
| `detail` | `Minimal` 或 `Full`（含 mtime+size） | `Minimal` |

任何维度变化都会创建不同的缓存分区（partition）。

**核心策略：**

| 策略 | 值 | 作用 |
|------|-----|------|
| 缓存 TTL | 1000ms（可通过 `FS_SCAN_CACHE_TTL_MS` 覆盖） | 短过期，平衡新鲜度与命中率 |
| 空结果快速重查 | 200ms（可通过 `FS_SCAN_EMPTY_RECHECK_MS` 覆盖） | 新文件被添加时避免假空 |
| 最大条目数 | 16（可通过 `FS_SCAN_CACHE_MAX_ENTRIES` 覆盖） | 按创建时间驱逐 |
| 缓存粒度 | 整个目录树的条目列表 | 不是单文件、不是最终工具结果 |

**空结果快速重查（Stale-Negative 修复）：**
当缓存命中但过滤/查询后的结果是空时，如果缓存年龄 ≥ 空重查阈值，消费者执行一次 `force_rescan(store=true)` 并重试。这防止了新文件在缓存 TTL 内的假阴。

**失效机制（Invalidation）：**
每次文件变异（write、edit、delete、rename）后，编码层调用：
```typescript
invalidateFsScanCache(path?)  // 带路径：删除根目录包含该路径的条目
                               // 无路径：清除全部缓存条目
```

**调用方（Consumer）缓存使用：**
- `glob` / `fuzzyFind` / grep cached 模式 → `cache: false` 为默认，高频 mention 候选发现启用缓存
- 工具级搜索（`search` tool）→ `cache: false`

### 2.2 Hashline Snapshot Store（内容哈希快照存储）

hashline 编辑系统的核心——它是**每次文件编辑的验证与恢复层**：

```
                                 ┌──────────────────────────┐
   read  "foo.ts" ───────────→   │  SnapshotStore.record()  │
   (lines 1-50, tag #A1B2)       │                          │
                                  │  path: "foo.ts"         │
                                  │    history:             │
                                  │      [Snapshot #A1B2]   │  ← SHA-256 的前 4 个十六进制字符
                                  │      [Snapshot #3C4D]   │
                                  │      ...                │
                                  └──────────────────────────┘
                                         │
   edit [foo.ts#A1B2]                   │
   replace 1..1:                        ▼
   +new content              ┌──────────────────────┐
                              │  Patcher.resolveTag()│
                              │                      │
                              │  tag #A1B2 valid?    │
                              │  ├─ YES → apply edit │
                              │  └─ NO  → RECOVERY   │
                              │           ├─ SnapshotStore.byHash(#A1B2)
                              │           │   → 找到历史全文
                              │           ├─ 在历史版本上重放编辑
                              │           ├─ 与当前磁盘内容 3-way merge
                              │           └─ 仅在所有锚点匹配时接受
                              └──────────────────────┘
```

**实现细节：**
- `InMemorySnapshotStore` 使用 `lru-cache`：最多 30 个路径，每个路径保留 4 个历史版本
- 每个 `Snapshot` 存完整文件的规范化文本 + 哈希 + 时间戳
- 恢复（Recovery）采用 `fuzzFactor: 0`——这意味着编辑锚点的行内容必须**精确匹配**，不允许模糊偏移

**为什么这是缓存工程？** 快照存储本质上是一个**以文件路径为维度的内容版本缓存**，让编辑操作在锚点过时后仍能恢复，而不是直接报错。它省去了重新读取 + 重新确定编辑位置 + 重新生成编辑指令的成本。

### 2.3 Blob Store（内容寻址二进制存储）

```
                 ┌──────────────────────────────────┐
插入二进制 ───→  │  SHA-256(bytes) → hash           │
(图片 base64)   │                                   │
                 │  存储路径：<blobsDir>/<hash>       │
                 │  引用形式：blob:sha256:<hash>     │
                 └──────────────────────────────────┘
                 
                 相同内容 → 相同哈希 → 相同文件
                 ├─ 跨会话去重
                 ├─ 不绑定到任何单个会话生命周期
                 └─ 写入是幂等的（内容级）
```

这与会话局部产物存储（`artifact://`）是两个独立的系统：
- **Blob**：全局的、内容寻址的、给图片和大 JSON 用的
- **Artifact**：会话本地的、自增 ID 编号的、给工具输出和子代理输出用的

### 2.4 Codex CLI 的缓存对比

Codex CLI 没有上述任何缓存层。

| 维度 | Oh My Pi | Codex CLI |
|------|----------|-----------|
| 文件系统扫描缓存 | ✅ Rust 进程内，多维度分区，TTL 1s | ❌ 每次 fork `find`/`rg` |
| 编辑快照缓存 | ✅ LRU 4 版本/路径，3-way merge 恢复 | ❌ 无 |
| 二进制 blob 去重 | ✅ SHA-256 内容寻址 | ❌ 无 |
| 扫描失效 | ✅ Mutation 后自动 invalidate | ❌ 无 |
| 空结果快速重查 | ✅ 200ms 内检测并纠正 | ❌ 无 |
| 热路径 | ✅ 零 fork/exec | ❌ 每次操作都 fork 子进程 |
| 跨进程缓存 | ✅ 否（进程本地） | ❌ 否 |

**性能影响的量级：**
- 文件搜索：omp 在 Rust 内 grep，~毫秒级；Codex CLI fork `rg`，~数十毫秒（加上 shell 启动）
- 大项目初次扫描：omp 的 workspace walker 1 次 pass，gitignore 感知；Codex CLI 多次 shell 调用
- 重复读取：omp 的 scan cache 在 1s TTL 内直接返回；Codex CLI 每次都重新 fork

---

## 三、Harness 工程（Harness Engineering）

Harness 工程是指 Agent 的**执行框架设计**——工具粒度的选择、编辑格式的设计、流控制策略、会话压缩策略、错误恢复策略。这是 omp 区别于竞赛类 Agent 的最深层差异。

### 3.1 工具设计哲学

**统一命名空间 vs 独立命令：**

Codex CLI 和其他 Agent 倾向于为每个操作设计独立的工具类型（`bash`、`Read`、`Write`、`grep`、`find`、`curl`、...）。omp 的设计完全不同：

```
read 工具是统一的入口：
  ├─ read src/foo.ts        → 文件内容
  ├─ read src/               → 目录条目列表
  ├─ read db.sqlite:users    → SQLite 行
  ├─ read https://arxiv.org/pdf/...  → PDF 文本
  ├─ read pr://1428          → GitHub PR
  ├─ read issue://123        → GitHub Issue
  ├─ read agent://<id>/field → 子代理输出
  ├─ read skill://name       → 技能指令
  ├─ read memory://root      → 持久记忆
  ├─ read archive.tar.gz:inner/file.ts  → 压缩包内文件
  └─ read notebook.ipynb     → 笔记本（可编辑）
```

**一个接口，一个缓存层。** 这从根本上简化了 agent 的工具使用决策——"读取任何东西"都是一个工具。同时，也只需要一处处理内部 URL 解析、内容选择器和分页。

**隐藏工具 + 动态发现：**

32 个内置工具默认不全加载。通过 `--tools read,edit,bash,...` 固定活跃集合，其余通过 `search_tool_bm25` 在会话中按需激活。这意味着：

- 上下文窗口不会因几十个工具定义而膨胀
- Agent 只在需要时通过搜索发现扩展功能
- 工具可以被 `tools.discoveryMode` 保护起来

### 3.2 Hashline：编辑格式工程

这是 Harness 工程中 omp 最核心的发明。编辑格式看似是个小问题，实际上是 agent 可靠性最脆弱的环节。

**传统的编辑方式（Codex CLI 的方式）：**
```
model 生成：用某些搜索关键词找到代码块 → 生成 patch → 应用
问题：
  - 行号漂移：文件被其他编辑改过后行号全变了
  - 搜索关键词匹配失败：旧的 patch 或工具输出中有相同字符串
  - 「string not found」的无限重试循环
  - 编辑静默破坏代码（行号偏离，patch 应用到错误位置）
```

**omp 的 hashline 方式：**
```
model 读取文件 → 获得 [path#TAG] 锚点 + 行号
model 生成编辑 → 引用该 #TAG + 仅包含要改的行
patcher 收到编辑 →
  ├─ 解析 #TAG → 在 SnapshotStore 中查找
  ├─ TAG 有效（磁盘文件内容匹配快照）→ 直接应用
  └─ TAG 无效（文件已改变）→ 恢复流程
      ├─ 从 SnapshotStore 取历史版本
      ├─ 在历史版本上重放编辑
      ├─ 生成 diff
      ├─ 在当前磁盘内容上 3-way merge
      └─ 仅在锚点行内容匹配时才接受
```

**Hashline 的 Harness 工程意义：**
- **验证先于修改**：每个编辑的锚点是文件的完整内容哈希，不是凭空的行号
- **宽误差容忍**：即使文件在 read 和 edit 之间被改过，3-way merge 仍可能成功
- **但绝非宽松**：恢复机制要求所有锚点行的内容精确匹配（`fuzzFactor: 0`）——这是为了安全，不是为了提高命中率
- **token 减少 61%**：Grok 4 Fast 的测量结果——因为模型不再需要重述未修改的内容
- **没有「string not found」的死循环**：锚点验证一步到位

### 3.3 TTSR：流规则 vs 静态提示词

时间旅行流规则是 omp 对「规则注入」这个问题的独特解法。

```
传统方式（Codex CLI）：规则是系统提示的一部分
  problem: 每个规则都占据上下文 token 预算
           规则越多，模型越迟钝
           但规则越少，指导越稀疏

omp 的 TTSR 方式：规则作为流监视器休眠，仅违反时激活
  ┌───────────────────┐
  │ 模型流输出        │
  │ ...doing normal... │
  │ Box::leak(         │  ← 匹配到正则
  │     data)          │
  └───────┬───────────┘
          │
   checkDelta(delta, matchContext)
          │
   ┌──────▼──────────┐
   │ 规则匹配！       │
   │ agent.abort()    │  ← 立即中止流
   └──────┬──────────┘
          │
   50ms 后：
   ┌──────────────────────────────┐
   │ 替换消息（contextMode: discard 时移除违规部分）
   │ 注入：<system-interrupt reason="rule_violation" rule="box-leak">
   │           Don't reach for Box::leak in production code paths
   │         </system-interrupt>
   │ agent.continue()  ← 从断点继续
   └──────────────────────────────┘
```

**多源匹配策略：**

| 匹配源 | 中断方式 | 注入位置 |
|--------|---------|---------|
| `text_delta` / `thinking_delta` | 立即中止流 | 作为 hidden custom_message 后接 agent.continue() |
| `toolcall_delta` | 不分层 | 在工具调用被调度前中止 |
| 非中断模式 + prose-source | 不分层 | 在当前回复完成后注入隐藏 custom_message |
| 非中断模式 + tool-source | 不分层 | 通过 afterToolCall hook 在 toolResult 内容前插入 `<system-reminder>` |

**重复策略：**
- `once`：每个规则在签名被标记为"已注入"后不再触发
- `after-gap`：仅在至少 N 个完整回合后才可重新触发

**注入持久化**：`ttsr_injection` 条目被写入会话 JSONL 并恢复。下次会话即使从断点恢复，注入的规则仍被视为已消耗。

### 3.4 Compaction：会话压缩工程

长会话不可避免。omp 的压缩系统不是简单截断，而是**理解型压缩**（context-full summarization）。

```
压缩边界确定：
  ├─ 找到上次压缩点
  ├─ 计算需要保持的最近 token 量（默认 20000）
  ├─ findCutPoint() → 扫描候选边界
  │   ├─ 禁止切割：toolResult 消息（会把工具调用与结果断开）
  │   ├─ 候选：user, assistant, bashExecution, hookMessage,
  │   │        branchSummary, compactionSummary
  │   └─ 从不切割 metadata：model_change, thinking_level_change
  │       这些在切割前被拉入保留区域
  │
  └─ split-turn 处理：
      如果不巧切割在用户回合中部，
      生成两段摘要：历史摘要 + 回合片段摘要

压缩触发条件：
  ├─ 手动：/compact [instructions]
  ├─ 溢出恢复：上下文溢出错误 → context promotion → 失败 → auto compact
  ├─ 不完整输出：stopReason === "length" → context promotion → 失败 → auto compact
  ├─ 阈值触发：成功回复后 token > resolveThreshold()
  └─ 空闲触发：启用 idle 后 N 秒无交互
```

**压缩策略：**
- `context-full`：保留到切割点的所有消息 + 压缩历史为一个 summary 消息
- `handoff`：生成 handoff 文档，开启全新会话，将文档注入为第一条消息
- `off`：不自动压缩

**工具输出预压缩（Pruning）：**
压缩前，可能先对工具输出做轻量级裁剪：
- 保留最新的 40,000 工具输出 token
- 至少能节约 20,000 token 才执行
- 永远不裁剪 `skill` 或 `read` 工具的输出

### 3.5 Retry：多层次的恢复链路

```
agent_end → 检查 stopReason → error

   ├─ isContextOverflow? → 不进入 retry → 走 compaction 路径
   │
   └─ isRetryableError?
       ├─ 检查 transient/rate/500/503/超时等
       ├─ 递增 #retryAttempt
       ├─ 超过 maxRetries → 放弃
       ├─ 计算延迟：baseDelay * 2^(attempt-1)
       │   ├─ usage-limit 错误 → 解析 retry-after → 标记凭据耗尽 → 切换凭据
       │   └─ 延迟为 0 → 说明凭据/模型已切换
       ├─ 如果延迟 > maxDelay → 放弃
       ├─ 移除失败 assistant 消息（会话历史保留）
       ├─ sleep(delay)
       ├─ agent.continue()
       └─ 第一次成功的非错误消息后重置 retryAttempt
```

**Fallback Chain（模型故障转移）：** 与 retry 不同——这是**角色级别的退路**：
```
primary: "default" → anthropic/claude-sonnet-4.5
fallbackChain: ["fallback-secondary", "fallback-tertiary"]
  ↓
当 primary 失败时，尝试 chain 中的下一个角色
成功则临时绑定到新模型，冷却后恢复
```

**凭据轮换：** 多个 API key 叠加，运行时会话级别的亲和性绑定，单个 key 耗尽时自动切换。

### 3.6 Agent Loop 设计

omp 的 Agent 循环有几个区别于 Codex CLI 的特征：

**事件流格式：**
```
prompt("Hello")
├─ agent_start
├─ turn_start
├─ message_start (user)
├─ message_end   (user)
├─ message_start (assistant)  ← LLM 开始响应
├─ message_update (text_delta) ← 每个 token chunk 流
├─ message_update (toolcall_delta)
├─ message_end
├─ tool_execution_start
├─ tool_execution_update (partialResult)
├─ tool_execution_end
├─ message_start (toolResult)
├─ message_end
├─ turn_end
├─ turn_start (下一个回合) ───→ 循环...
├─ agent_end
```

**Steering（操控）和 Follow-up（追加）：**
```
会话运行时：
  session.steer("Stop, do this instead")
    ├─ 检查 interruptMode:
    │   ├─ "immediate" → 当前工具完成后立即处理
    │   └─ "wait" → 当前回合完成后再处理
    └─ 处理 steering 消息 → 继续 agent

  session.followUp("Now summarize the changes")
    └─ agent 正常停止后，注入 follow-up 消息并继续
```

### 3.7 子代理系统（Task Orchestration）

omp 的子代理不是独立的进程——而是同一个进程中的隔离工作区：

```
task("优化 auth")
  ├─ 创建工作区隔离（isolation backend）
  │   ├─ APFS 克隆（macOS）
  │   ├─ btrfs reflink（Linux）
  │   ├─ zfs clone
  │   ├─ overlayfs
  │   └─ fallback: rcopy
  ├─ 启动子 agent 进程（隔离文件系统视图）
  ├─ 子代理间通过 irc 工具通信
  ├─ 父代理以 schema 验证的方式获得结构化结果
  └─ 完成 → 清理隔离工作区
```

这与 Codex CLI 的单进程模型中每个任务都是全新 bash 调用的方式有本质差异。

### 3.8 对比总结：Harness 工程维度

| Harness 维度 | Oh My Pi | Codex CLI |
|---|---|---|
| **编辑格式** | Hashline：内容哈希锚定 + 3-way merge | search/replace 或 str_replace |
| **编辑失败处理** | Snapshot 恢复 + 锚点验证 | 重试循环，string not found |
| **规则注入** | TTSR：流中按需注入 | 静态系统提示 |
| **压缩策略** | Context-full 总结 + handoff + 预压缩 | 基础截断 |
| **恢复链路** | Retry + context promotion + fallback chain + compaction | 基础重试 |
| **Agent 可控** | Steering / follow-up / interruptMode | 基本 prompt/continue |
| **子代理** | 隔离工作区 + IRC + schema 验证 | 无 |
| **工具设计** | 统一命名空间，`read` 处理所有 I/O | 各自独立的工具 |
| **编辑验证** | 哈希前置验证，禁止多字节吞噬 | 行号搜索，无验证 |
| **运行模式** | TUI + print + RPC + ACP | 仅终端交互 |

---

## 四、三者关系的综合视角

Memory、Cache、Harness Engineering 三者不是独立的模块，而是**相互咬合的齿轮**：

```
                    Harness 层
                    编辑格式
                    流规则(TTSR)
                    压缩/重试
                    代理循环
                 ┌───────┼───────┐
                 ▼               ▼
           Cache 层          Memory 层
           FS scan cache     Hindsight 流水线
           Snapshot store    会话提取 + 合并
           Blob 去重         项目级隔离
                 │               │
                 └───────┬───────┘
                         ▼
                  工具/Agent 调用层
                read / task / search / ...
```

- **Cache 层**减轻了**Harness 层**的负担：FS scan cache 让 grep/glob 免于重复扫描，snapshot 让编辑免于反复判断文件状态
- **Harness 层**为**Memory 层**提供输入：compaction 的输出可以喂入 memory extraction，每个会话的上下文变成记忆系统的原始素材
- **Memory 层**指导**Harness 层**的行为：下次会话启动时的记忆摘要改变了 agent 对项目当前状态的初始理解，产生更一致的决策

Codex CLI 在这三个维度上几乎一片空白：没有记忆系统、没有缓存层（每次 fork，无缓存语义）、Harness 工程停留在编辑格式 + 简单沙箱的初级阶段。

Oh My Pi 的深层工程不只是"多了几个功能"——它从根本上改变了 agent 在一段时间内的行为模式：从一个"每次启动都要重新学习项目"的单次任务执行器，变成了一个"记住过去的教训、预测未来的冲突、缓存当前的状态"的持续工作伙伴。

---

## 五、工具调用工程（Tool Calling）

工具调用是 Agent 与外部世界交互的唯一通道。一个工具调用的端到端成本包含三层：**上下文 token**（工具定义占用的提示词空间）、**生成 token**（模型输出工具调用参数）、**往返 token**（工具结果占用后续上下文）。omp 在这三层上都做了深度的 token 节省设计。

### 5.1 工具定义管线：从 Zod 到 LLM

每个工具的参数由 Zod schema 严格定义。这是整个工具的"单一事实源"，不仅用于运行时验证，也驱动 LLM 可见的 JSON Schema 生成：

```
AgentTool.parameters (Zod schema)
  │
  ├──→ toolWireSchema()           ← 转换为 draft-2020-12 JSON Schema
  │     ├─ 去掉 $schema / 默认值 / 空 schema
  │     ├─ 重写 nullable 标量 anyOf
  │     └─ 输出通用 JSON Schema
  │
  ├──→ normalizeXXXSchema()       ← 提供商特定归一化
  │     ├─ Anthropic: 限制关键字白名单
  │     ├─ Google: 移除不支持的字段
  │     └─ OpenAI: 严格的 strict 模式（required + additionalProperties: false）
  │
  └──→ validateToolArguments()    ← LLM 返回后再验证
        ├─ 5 轮 coercion：JSON 字符串→数组、字符串→数字、...
        └─ 失败 → 抛出格式化错误返回 LLM
```

**为什么这很重要？** 不同提供商对 JSON Schema 的支持度差异极大（Google 不支持 `$comment`、OpenAI strict 要求全部 `required`、Anthropic 限制 `pattern` 关键字）。omp 在每提供商层面做了深度归一化，确保：
- 工具定义在每个模型处都是**有效且紧凑**的 JSON Schema
- 不浪费 token 发送模型不支持的字段
- 不会因为 schema 格式问题导致 400/422 错误

对比 Codex CLI：工具定义是每个提供商硬编码的 JSON 对象，没有统一的类型→schema 管线。

### 5.2 工具加载分层：不做"全量发送"

omp 有 32 个内置工具，但不是全部发送给模型。工具分三级加载：

| 加载模式 | token 中的形式 | 哪些工具 | 默认 |
|---------|---------------|---------|------|
| **essential** | 完整描述 + JSON Schema + 用法约束 | `read`, `bash`, `edit` | 仅 3 个 |
| **discoverable** | 仅 `label: \`name\`` 一行 | 其余 29 个内置工具 | 隐藏但可搜索 |
| **hidden** | 完全不发送 | `yield`, `resolve`, `goal` 等内部工具 | 永远不暴露 |

**系统提示中的 Inventory 段：**
```
# Inventory
- Read: `read`           ← essential，有完整描述
- Bash: `bash`            ← essential，有完整描述
- Edit: `edit`            ← essential，有完整描述
- Search: `search`        ← discoverable，仅一行
- Find: `find`            ← discoverable，仅一行
- Lsp: `lsp`              ← discoverable，仅一行
- ...
```

**关键差异：`repeatToolDescriptions` 开关。** 当此值为 `true` 时，discoverable 工具也从一行变为完整 `<tool>` 块。默认是 `false`——也就是说，29 个工具仅以每行 ~20 token 的形式存在。

这比 Codex CLI 的"所有工具全部完整发送"策略节省了大量上下文 token。Codex CLI 内置 tools 大约 6-8 个，但全部以完整 JSON 定义出现在每回合的请求中。

### 5.3 `read` 工具：Token 节省的终极武器

`read` 是 omp 最重要、也是 token 节省最多的工具。它把"读取任何东西"统一为一个操作：

```
read("src/foo.ts")               → 结构化摘要（签名保留，体省略）
read("src/foo.ts:50-200")        → 精确行范围
read("src/foo.ts:5-16,40-80")    → 多区间合并
read("db.sqlite:users")          → SQLite 表 + 示例行
read("pr://1428")                → GitHub PR
read("https://arxiv.org/pdf/...") → PDF → Markdown
read("archive.zip:inner/file.ts:10-20") → 压缩包内指定范围
read("notebook.ipynb")           → Jupyter Notebook 可编辑视图
```

**Token 节省的核心机制：**

#### a) 结构化摘要（Structural Summary）

当 `read` 不加选择器时，对可解析代码返回的是**结构化摘要**，而非全文：

```
# 原始代码（300 行）：
function alpha(): void { ... 30 行体 ... }
class Beta { ... 50 行体 ... }
function gamma(x: number): string { ... 20 行体 ... }
export { alpha, Beta, gamma }

# read 返回的结构化摘要（~15 行）：
function alpha(): void { .. }
class Beta { .. }
function gamma(x: number): string { .. }
export { alpha, Beta, gamma }

[285 lines elided; re-read needed ranges, e.g. src/foo.ts:2-5,30-80,130-150]
```

这是通过 Rust 的 tree-sitter 实现的（`crates/pi-ast/src/summary.rs`）。流程：
1. tree-sitter 解析代码 → AST
2. 识别可省略节点：函数体、类体、方法体、块注释（最少 4 行）、分组注释（最少 6 行）
3. 按行构建省略区间 → 输出签名 + 折叠标记（`..` / `…`）
4. 底部生成精确的**多区间选择器**，模型可以直接复制粘贴来恢复需要的区间

**关键设计决策**：
- 摘要显示类型签名、导出和顶层注释——模型理解代码结构所需的一切
- 体省略为 `..`（合并括号对）或 `…`（独立的非括号体）
- `..` 和 `…` **不携带内容**——模型不能"猜"省略了什么，必须用区间选择器显式读出
- 底部生成的多区间选择器让模型一次性读取需要的多个不连续区间

**token 节省量级**：常规 TypeScript 文件，摘要输出仅原文的 5-15%。一个 500 行的文件，模型可能只需要 40 行来理解其结构，再对特定区域发出精确的区间读取。

#### b) 区间选择器（Selector Grammar）

```
:50-200      精确行范围
:50+150      从 50 行开始的 150 行
:5-16,40-80  多区间合并（一次调用读多个不连续区间）
:raw         纯文本，无行号/哈希
:conflicts   Git 合并冲突索引
```

模型可以**精确读取需要的区间，而不必读取整个文件**。这从根本上改变了 token 使用模式：不再"读全文件 → 从全文件中找——"而是"看摘要 → 精确定位 → 区间读取"。

#### c) 内部 URL 行选择器

```
read("pr://1428:50-100")               ← PR diff 指定行
read("agent://TaskId/findings.0.path:1-10")  ← 子代理输出指定行
```

所有内部 URL 也都支持行选择器。模型可以在不加载全部内容的情况下精确引用子代理输出、PR、或文档的特定部分。

#### d) 目录读取也是部分的

```bash
read("src/") → 时间排序、深度限制、只显示最近修改的条目
```

不返回整个目录树，而是有深度和条目数限制的部分视图。

### 5.4 编辑工具：Hashline 的 Token 节省

这是工具调用层面 token 节省的最大创新：

```
Codex CLI（传统方式）：
  模型必须生成旧内容 + 新内容的完整替换 patch
  例：replace 50 行文件中的 1 行 → 还需描述其余 49 行的上下文
  Grok 4 Fast: 输出 token 巨大，大部分用于重述不需修改的代码

omp（Hashline 方式）：
  模型只需：文件 #TAG + 行号 + 新内容
  例：[foo.ts#A1B2]
      replace 23..23:
      +const newConfig = load();
  此 3 行就是一个完整的编辑
```

**实测数据**（来自 README）：Grok 4 Fast 使用 Hashline 后**输出 token 减少 61%**。Grok Code Fast 1 的编辑通过率从 6.7% → 68.3%。MiniMax 的通过率提升 2.1 倍。

**为什么减少这么多？**
1. 模型不需要重述任何未修改的代码
2. 锚点是 4 个十六进制字符的哈希，不是完整文件路径
3. 只有改变的行进入编辑体，保留的行完全不出现在编辑指令中
4. 不存在 "search pattern not found" 的无限重试循环——每个 token 都是有效操作

### 5.5 TTSR：Token 懒惰注入

多数 Agent 的规则以静态系统提示的形式在每回合占据上下文 token。如果 rules 有 2000 token，模型在 100 回合中的每回合都为此付出上下文成本。

omp 的 TTSR 改变了这个模型：

```
规则存储成本：0 token（完全不在提示词中）
规则匹配成本：~0 token（在 Rust 层，检查每个 delta 的正则匹配）
规则激活成本：仅在违规时注入 one-shot 消息
  <system-interrupt>...</system-interrupt>
  后续回合中规则标记为已注入，不再有成本

节省 = 上下文窗口中的规则 token × 未被违反的回合数
```

这比"永远在线"的静态规则系统节省了巨大的累积 token。

### 5.6 与 Codex CLI 的工具调用对比

#### a) 工具定义量

| 维度 | Oh My Pi | Codex CLI |
|------|----------|-----------|
| 内置工具数 | 32 个 | ~6-8 个 |
| LLM 可见 | 默认 3 个（essential）+ 29 个一行 | 全部完整 JSON |
| 上下文占用 | ~200 token（essential）+ ~600 token（discoverable 一行） | ~2000+ token（全部完整 JSON Schema） |
| 动态发现 | `search_tool_bm25`（BM25 语义搜索） | 无 |
| 工具隐藏 | 支持 loadMode + hidden 层 | 不支持 |

#### b) 读取机制

| 维度 | Oh My Pi | Codex CLI |
|------|----------|-----------|
| 文件读取 | 摘要优先 → 区间选择器 | 全文件读取 |
| 单工具多源 | `read` 处理文件 / URL / DB / 压缩包 / PR / Issue | 需要不同工具（`Read`, `WebFetch`, `Bash` 等） |
| 代码摘要 | tree-sitter 结构化，仅签名可见 | 无 |
| 区间读取 | 多区间合并 + raw/text 模式 | 无 |
| 目录读取 | 深度限制 + 按时间排序 | 无（需 `Bash` ls） |

#### c) 编辑机制

| 维度 | Oh My Pi | Codex CLI |
|------|----------|-----------|
| 格式 | Hashline（哈希锚定） | search/replace 或 str_replace |
| 模型输出 | 仅改动的行 | 旧+新内容 |
| 输出 token | 基准 -61%（Grok 4 Fast） | 基准 |
| 失败恢复 | Snapshot 3-way merge | 重试循环 |

#### d) 规则注入

| 维度 | Oh My Pi | Codex CLI |
|------|----------|-----------|
| 注入方式 | TTSR：流中按需 | 静态系统提示 |
| token 成本 | 0（休眠）+ one-shot（违规） | 每回合固定 |
| 多项目规则 | 支持 glob 范围限定 | 不支持 |

### 5.7 量化 Token 节省估算

| 机制 | 节省方式 | 估算节省（长会话） |
|------|---------|-----------------|
| 工具分层加载 | 29 个 discoverable 工具仅一行 vs 完整 JSON | ~1800 token/回合 |
| 结构化摘要 | 代码文件仅返回签名，体省略 | ~85-95% 文件读取 token |
| 区间选择器 | 模型一次读入非连续区间 | 减少 50-80% 后续 read 调用 |
| Hashline 编辑 | 模型仅输出改动的行 | -61% 编辑输出 token |
| TTSR 规则 | 规则在流中按需注入 | 累积节省：规则 token × 未违规回合数 |
| `read` 多协议 | 10+ 数据源共用一个工具 | 减少 ~3-5 个独立工具定义 |
| Compaction | 旧上下文压缩为 summary | 长会话持续稳定 |

### 5.8 总结：Token 节省不是偶然

omp 的 token 节省不是"用更短的参数名"或"压缩描述文本"这类浅层优化。它是一种**架构级设计**：

- **读**：看摘要 → 定位 → 区间读（三层渐进，从不读完）
- **写**：哈希锚定 → 仅改动的行 → 0 重试（一次成功，不浪费）
- **规则**：休眠 → 流中匹配 → one-shot 注入（不违反就不花钱）
- **工具**：essential 先用 → discoverable 再搜 → hidden 不暴露（动态收费）

Codex CLI 在工具调用上的设计是最小可行方案——几个工具、完整定义、全文件读写、搜索替换。它正确，但低效。omp 在工具的深处节约了那些"看起来不重要但实际上占用了绝大部分上下文"的 token。
