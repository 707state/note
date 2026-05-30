---
title: Codex 和 Harness Engineering
author: jask、deepseek v4 pro
tags:
  - LLM
date: 2026-06-01
---

# Codex 和 Harness Engineering

本文分析 Codex CLI（codex-rs）在**不考虑 LLM 模型本身能力**的前提下，**本地运行环境层面**做了哪些准备工作，以及这些工作如何帮助模型理解代码仓库、安全地执行操作。

## 整体架构：Harness 与 LLM 的分工

Codex 的核心理念是 **Harness 负责「感知 + 执行」，LLM 负责「理解 + 决策」**：

```
Harness（本地运行环境）               LLM（远程模型 API）
─────────────────────────────        ─────────────────────
• 环境感知（Shell、Git、路径）        • 理解用户意图
• 项目结构解析（AGENTS.md）          • 生成代码修改方案
• 权限与沙箱管理                      • 决策调用哪个工具
• 工具注册与执行                      • 接收工具结果并继续推理
• 历史管理与上下文压缩
• Hook 生命周期管理
       │                                        │
       └────── context XML ────────────►────────┘
       ◄────── tool calls ──────────────┬────────
       └────── tool outputs ────────────►────────┘
```

---

## 一、环境感知层 —— 让模型知道「我在哪」

### 1.1 Shell 检测

Codex 在启动时检测用户的 Shell 类型和路径，决定之后所有工具调用的执行参数：

`codex-rs/core/src/shell.rs`:

```rust
#[derive(Debug, PartialEq, Eq, Clone, Serialize, Deserialize)]
pub enum ShellType {
    Zsh,
    Bash,
    PowerShell,
    Sh,
    Cmd,
}

pub struct Shell {
    pub(crate) shell_type: ShellType,
    pub(crate) shell_path: PathBuf,
    pub(crate) shell_snapshot: watch::Receiver<Option<Arc<ShellSnapshot>>>,
}
```

不同 Shell 使用不同的命令行参数。例如 Bash/Zsh 用 `-lc`（login shell），PowerShell 用 `-NoProfile -Command`：

```rust
//codex-rs/core/src/shell.rs 43:59
    pub fn derive_exec_args(&self, command: &str, use_login_shell: bool) -> Vec<String> {
        match self.shell_type {
            ShellType::Zsh | ShellType::Bash | ShellType::Sh => {
                let arg = if use_login_shell { "-lc" } else { "-c" };
                vec![
                    self.shell_path.to_string_lossy().to_string(),
                    arg.to_string(),
                    command.to_string(),
                ]
            }
            ShellType::PowerShell => {
                let mut args = vec![self.shell_path.to_string_lossy().to_string()];
                if !use_login_shell {
                    args.push("-NoProfile".to_string());
                }
                args.push("-Command".to_string());
                args.push(command.to_string());
```

### 1.2 Shell 环境快照

Codex **启动用户的 login shell，执行 snapshot 脚本**，捕获完整的 Shell 环境。这是确保 LLM 发出的命令能在用户真实环境中正确执行的关键。

`codex-rs/core/src/shell_snapshot.rs`:

```rust
pub struct ShellSnapshot {
    pub path: AbsolutePathBuf,
    pub cwd: AbsolutePathBuf,
}

// 3 天保留期，每次 snapshot 前清理旧文件
const SNAPSHOT_RETENTION: Duration = Duration::from_secs(60 * 60 * 24 * 3);
// 排除不需要的环境变量
const EXCLUDED_EXPORT_VARS: &[&str] = &["PWD", "OLDPWD"];
```

**snapshot 流程**（`ShellSnapshot::try_new`）：
1. 向 Shell 发送特定 snapshot 脚本（如 `bash_snapshot_script()`、`zsh_snapshot_script()`）
2. 捕获：**所有函数定义**（`declare -f`）、**所有 alias**（`alias -p`）、**所有环境变量**（排除 `PWD`/`OLDPWD`）、**Shell 选项**（`set -o`）
3. 写入 `{codex_home}/shell_snapshots/{session_id}.{nonce}.sh`
4. 验证：用 `set -e; . {snapshot_path}` 确保 snapshot 可正确执行

`codex-rs/core/src/shell_snapshot.rs` — Bash snapshot 脚本示例：

```rust
//codex-rs/core/src/shell_snapshot.rs 227:254
async fn capture_snapshot(shell: &Shell, cwd: &AbsolutePathBuf) -> Result<String> {
    let shell_type = shell.shell_type.clone();
    match shell_type {
        ShellType::Zsh => run_shell_script(shell, &zsh_snapshot_script(), cwd).await,
        ShellType::Bash => run_shell_script(shell, &bash_snapshot_script(), cwd).await,
        ShellType::Sh => run_shell_script(shell, &sh_snapshot_script(), cwd).await,
        ShellType::PowerShell => run_shell_script(shell, powershell_snapshot_script(), cwd).await,
        ShellType::Cmd => bail!("Shell snapshotting is not yet supported for {shell_type:?}"),
    }
}
```

后续所有 shell 命令执行前都会 **source 这个 snapshot 文件**，确保 LLM 的命令在用户真实环境中运行。

### 1.3 Git 仓库信息采集

Codex 在启动时并行执行三个 Git 命令（带 5 秒超时），获取代码仓库的版本上下文：

`codex-rs/git-utils/src/info.rs`:

```rust
#[derive(Serialize, Deserialize, Clone, Debug, JsonSchema, TS)]
pub struct GitInfo {
    pub commit_hash: Option<GitSha>,
    pub branch: Option<String>,
    pub repository_url: Option<String>,
}

const GIT_COMMAND_TIMEOUT: TokioDuration = TokioDuration::from_secs(5);
```

**并行采集**（`collect_git_info()`）：

```rust
// codex-rs/git-utils/src/info.rs 96:100
    // Run all git info collection commands in parallel
    let (commit_result, branch_result, url_result) = tokio::join!(
        run_git_command_with_timeout(&["rev-parse", "HEAD"], cwd),
        run_git_command_with_timeout(&["rev-parse", "--abbrev-ref", "HEAD"], cwd),
        run_git_command_with_timeout(&["remote", "get-url", "origin"], cwd),
```

此外，Codex 还通过**向上遍历目录查找 `.git`** 确定项目根目录（`get_git_repo_root()`），这对于后续 AGENTS.md 发现和文件权限边界判断至关重要：

```rust
//codex-rs/git-utils/src/info.rs
/// Return `true` if the project folder specified by the `Config` is inside a
/// Git repository.
///
/// The check walks up the directory hierarchy looking for a `.git` file or
/// directory ...
pub fn get_git_repo_root(base_dir: &Path) -> Option<PathBuf> {
    let base = if base_dir.is_dir() {
        base_dir
    } else {
        base_dir.parent()?
    };
    find_ancestor_git_entry(base).map(|(repo_root, _)| repo_root)
}
```

### 1.4 时间信息

Codex 采集当前日期和时区（通过 `chrono::Local` 和 `chrono::Utc`），使模型能做出时间敏感的决策（如生成带时间戳的文件名、判断时效性），这些信息被组装到 `EnvironmentContext` 中发送给模型。

---

## 二、项目理解层 —— 让模型知道「这是什么项目」

### 2.1 AGENTS.md 发现与加载

AGENTS.md 是 Codex 理解代码仓库的**核心知识注入点**。`codex-rs/core/src/agents_md.rs` 文档注释清楚说明了发现规则：

```rust
//codex-rs/core/src/agents_md.rs
//! AGENTS.md discovery and user instruction assembly.
//!
//! 1.  Determine the project root by walking upwards ...
//! 2.  Collect every `AGENTS.md` found from the project root down to the
//!     current working directory (inclusive) and concatenate their contents in
//!     that order.
//! 3.  We do **not** walk past the project root.
```

**关键常量**：

```rust
//codex-rs/core/src/agents_md.rs
pub const DEFAULT_AGENTS_MD_FILENAME: &str = "AGENTS.md";
pub const LOCAL_AGENTS_MD_FILENAME: &str = "AGENTS.override.md";

const AGENTS_MD_SEPARATOR: &str = "\n\n--- project-doc ---\n\n";
```

- `AGENTS.override.md` 优先于 `AGENTS.md`
- 支持 `project_doc_fallback_filenames` 配置 fallback 文件名
- 有 `project_doc_max_bytes` 预算限制，防止超长文档

### 2.2 与用户指令的拼接

Codex 将 AGENTS.md 内容与用户配置的 `user_instructions` 拼接在一起，作为模型的可见指令：

```rust
//codex-rs/core/src/agents_md.rs
    pub(crate) async fn user_instructions(
        &self,
        environment: Option<&Environment>,
        startup_warnings: &mut Vec<String>,
    ) -> Option<String> {
        ...
        let agents_md_docs = self.read_agents_md(fs, startup_warnings).await;

        let mut output = String::new();

        if let Some(instructions) = self.config.user_instructions.clone() {
            output.push_str(&instructions);
        }

        match agents_md_docs {
            Ok(Some(docs)) => {
                if !output.is_empty() {
                    output.push_str(AGENTS_MD_SEPARATOR);
                }
                output.push_str(&docs);
            }
            Ok(None) => {}
            Err(e) => {
                error!("error trying to find AGENTS.md docs: {e:#}");
            }
        };
```

最终拼接顺序为：`[config.user_instructions] ---project-doc--- [AGENTS.md from all levels]`

### 2.3 预算控制

`read_agents_md` 实现了逐文件读取、截断逻辑：

```rust
// 173:218:codex-rs/core/src/agents_md.rs
        let max_total = self.config.project_doc_max_bytes;
        if max_total == 0 { return Ok(None); }

        let paths = self.agents_md_paths(fs).await?;
        let mut remaining: u64 = max_total as u64;
        let mut parts: Vec<String> = Vec::new();

        for p in paths {
            if remaining == 0 { break; }
            ...
            if size > remaining {
                data.truncate(remaining as usize);
            }
```

**这对理解代码仓库的价值**：AGENTS.md 是人类写的「仓库说明书」，告诉 Agent 编码风格、架构约定、测试命令等。模型在做出修改决策前已知晓这些约束。

---

## 三、权限与沙箱层 —— 让模型知道「我能做什么」

### 3.1 环境上下文 XML

Codex 将环境信息组装为**结构化 XML** 作为 user message 发送给 LLM。

`codex-rs/core/src/context/environment_context.rs` — `EnvironmentContext::body()` 方法：

```rust
//541:589:codex-rs/core/src/context/environment_context.rs
    fn body(&self) -> String {
        let mut lines = Vec::new();
        match &self.environments {
            EnvironmentContextEnvironments::Single(environment) => {
                lines.push(format!("  <cwd>{}</cwd>", environment.cwd.to_string_lossy()));
                lines.push(format!("  <shell>{}</shell>", environment.shell));
            }
            ...
        }
        if let Some(current_date) = &self.current_date {
            lines.push(format!("  <current_date>{current_date}</current_date>"));
        }
        if let Some(timezone) = &self.timezone {
            lines.push(format!("  <timezone>{timezone}</timezone>"));
        }
        match &self.network {
            Some(network) => { lines.push(format!("  {}", network.render())); }
            None => {}
        }
        if let Some(filesystem) = &self.filesystem {
            lines.push(format!("  {}", filesystem.render()));
        }
```

实际发送给模型的 XML 示例：

```xml
<environment_context>
  <cwd>/home/user/project</cwd>
  <shell>zsh</shell>
  <current_date>2026-06-01</current_date>
  <timezone>Asia/Shanghai</timezone>
  <network enabled="true">
    <allowed>api.example.com</allowed>
  </network>
  <filesystem>
    <workspace_roots>
      <root>/home/user/project</root>
    </workspace_roots>
  </filesystem>
</environment_context>
```

### 3.2 权限指令

Codex 根据配置的 Sandbox 模式和审批策略生成指令片段。`codex-rs/core/src/context/permissions_instructions.rs` 通过模板系统组合指令：

```rust
// 1:45:codex-rs/core/src/context/permissions_instructions.rs
const APPROVAL_POLICY_NEVER: &str = include_str!("prompts/permissions/approval_policy/never.md");
const APPROVAL_POLICY_UNLESS_TRUSTED: &str =
    include_str!("prompts/permissions/approval_policy/unless_trusted.md");
const APPROVAL_POLICY_ON_FAILURE: &str =
    include_str!("prompts/permissions/approval_policy/on_failure.md");
...
const SANDBOX_MODE_DANGER_FULL_ACCESS: &str =
    include_str!("prompts/permissions/sandbox_mode/danger_full_access.md");
const SANDBOX_MODE_WORKSPACE_WRITE: &str =
    include_str!("prompts/permissions/sandbox_mode/workspace_write.md");
const SANDBOX_MODE_READ_ONLY: &str = include_str!("prompts/permissions/sandbox_mode/read_only.md");
```

**沙箱模式**（从文件系统策略推断）：

```rust
//139:153:codex-rs/core/src/context/permissions_instructions.rs
fn sandbox_prompt_from_policy(
    file_system_policy: &FileSystemSandboxPolicy,
    cwd: &Path,
) -> (SandboxMode, Option<Vec<WritableRoot>>) {
    if file_system_policy.has_full_disk_write_access() {
        return (SandboxMode::DangerFullAccess, None);
    }
    let writable_roots = file_system_policy.get_writable_roots_with_cwd(cwd);
    if writable_roots.is_empty() {
        (SandboxMode::ReadOnly, None)
    } else {
        (SandboxMode::WorkspaceWrite, Some(writable_roots))
    }
}
```

**三种 Sandbox 模式**：
- `danger_full_access` — 完全磁盘访问
- `workspace_write` — 仅工作区可写
- `read_only` — 只读

### 3.3 上下文片段注册系统

Codex 使用一个**上下文片段注册表**管理所有可注入的上下文类型，支持增量更新（仅在变化时发送）。

`codex-rs/core/src/context/contextual_user_message.rs`:

```rust
//46:58:codex-rs/core/src/context/contextual_user_message.rs
static CONTEXTUAL_USER_FRAGMENTS: &[&dyn FragmentRegistration] = &[
    &USER_INSTRUCTIONS_REGISTRATION,
    &ENVIRONMENT_CONTEXT_REGISTRATION,
    &ADDITIONAL_CONTEXT_REGISTRATION,
    &SKILL_INSTRUCTIONS_REGISTRATION,
    &USER_SHELL_COMMAND_REGISTRATION,
    &TURN_ABORTED_REGISTRATION,
    &SUBAGENT_NOTIFICATION_REGISTRATION,
    &INTERNAL_MODEL_CONTEXT_REGISTRATION,
    &LEGACY_UNIFIED_EXEC_PROCESS_LIMIT_WARNING_REGISTRATION,
    &LEGACY_APPLY_PATCH_EXEC_COMMAND_WARNING_REGISTRATION,
    &LEGACY_MODEL_MISMATCH_WARNING_REGISTRATION,
];
```

每种片段实现 `ContextualUserFragment` trait：

`codex-rs/core/src/context/fragment.rs`:

```rust
//41:48:codex-rs/core/src/context/fragment.rs
pub trait ContextualUserFragment {
    fn role() -> &'static str where Self: Sized;
    fn markers(&self) -> (&'static str, &'static str);
    fn body(&self) -> String;
    fn type_markers() -> (&'static str, &'static str) where Self: Sized;
```

上下文模块清单（`codex-rs/core/src/context/mod.rs`）：

```rust
//1:32:codex-rs/core/src/context/mod.rs
mod approved_command_prefix_saved;
mod apps_instructions;
mod available_plugins_instructions;
mod available_skills_instructions;
mod collaboration_mode_instructions;
mod contextual_user_message;
mod environment_context;
mod fragment;
mod fragments;
mod guardian_followup_review_reminder;
mod hook_additional_context;
mod image_generation_instructions;
mod internal_model_context;
mod permissions_instructions;
mod skill_instructions;
mod subagent_notification;
mod turn_aborted;
mod user_instructions;
mod user_shell_command;
```

---

## 四、Skill / Plugin / Connector 发现层 —— 让模型知道「我有哪些工具」

### 4.1 Skills 系统

Codex 在启动时扫描配置的 Skills 目录，解析每个 skill 的声明文件，向模型注入可用 skill 列表和调用方式。

### 4.2 Apps / Connectors

通过 MCP 连接管理器发现可用的外部服务集成，收集 `AppInfo` 并在上下文中告知模型可调用的外部工具。

`codex-rs/core/src/connectors.rs`:

```rust
// 1:44:codex-rs/core/src/connectors.rs
use codex_mcp::CODEX_APPS_MCP_SERVER_NAME;
use codex_mcp::McpConnectionManager;
...
pub use codex_app_server_protocol::AppInfo;
pub use codex_app_server_protocol::AppBranding;

const CONNECTORS_READY_TIMEOUT_ON_EMPTY_TOOLS: Duration = Duration::from_secs(30);
```

### 4.3 Plugins 系统

扫描已安装的插件，发现 `ToolSuggestDiscoverable` 类型的工具，注入 Plugin 指令片段。

---

## 五、Hook 系统 —— 让外部工具介入 Codex 生命周期

Codex 提供 Hook 机制，允许外部程序在关键生命周期节点介入：

`codex-rs/core/src/hook_runtime.rs`:

```rust
//49:57:codex-rs/core/src/hook_runtime.rs
pub(crate) struct HookRuntimeOutcome {
    pub should_stop: bool,
    pub additional_contexts: Vec<String>,
}

pub(crate) enum PreToolUseHookResult {
    Continue { updated_input: Option<Value> },
    Blocked(String),
}
```

| Hook 事件 | 触发时机 | 作用 |
|-----------|---------|------|
| `SessionStart` | Codex 启动时 | 注入启动上下文 |
| `UserPromptSubmit` | 用户发送消息前 | 修改用户输入、注入额外提示 |
| `PreToolUse` | 工具执行前 | 修改工具输入、阻止执行 |
| `PostToolUse` | 工具执行后 | 根据执行结果注入反馈 |
| `Stop` | Codex 停止时 | 执行清理 |

Hook 输出的 `additional_contexts` 会被注入到模型的 user message 中：

```rust
//59:80:codex-rs/core/src/hook_runtime.rs
struct ContextInjectingHookOutcome {
    hook_events: Vec<HookCompletedEvent>,
    outcome: HookRuntimeOutcome,
}

impl From<SessionStartOutcome> for ContextInjectingHookOutcome {
    fn from(value: SessionStartOutcome) -> Self {
        let SessionStartOutcome {
            hook_events,
            should_stop,
            stop_reason: _,
            additional_contexts,
        } = value;
        Self {
            hook_events,
            outcome: HookRuntimeOutcome { should_stop, additional_contexts },
        }
    }
}
```

---

## 六、Tool 执行层 —— 让模型能「动手」

### 6.1 本地核心工具

`codex-rs/core/src/tools/registry.rs` 定义了 `CoreToolRuntime` trait：

```rust
//44:58:codex-rs/core/src/tools/registry.rs
/// Typed runtime contract for locally executed tools.
pub(crate) trait CoreToolRuntime: ToolExecutor<ToolInvocation> {
    fn search_info(&self) -> Option<ToolSearchInfo> { None }

    fn matches_kind(&self, payload: &ToolPayload) -> bool {
        matches!(payload, ToolPayload::Function { .. } | ToolPayload::ToolSearch { .. })
    }
```

向 LLM 暴露的本地核心工具包括：
- `shell_command` — 执行 Shell 命令（受 Sandbox/审批策略控制）
- `apply_patch` — 修改代码文件（受读写权限控制）
- `read_file` / `search_file` — 文件读写操作
- `use_skill` — 调用 Skill
- `update_plan` — 更新任务计划
- MCP 工具 — 通过 MCP 协议连接的外部工具

### 6.2 Tool 执行生命周期

每个工具调用经历完整生命周期：
1. **权限检查** — Guardian 系统判断是否需要审批
2. **PreToolUse Hook** — 外部程序可介入
3. **Sandbox 封装** — `bwrap` (Linux) / `sandbox-exec` (macOS)
4. **实际执行** — Shell 环境快照 + 超时保护
5. **PostToolUse Hook** — 外部程序可处理结果
6. **结果回传** — 格式化输出并发送给 LLM

---

## 七、Turn 执行引擎 —— 完整的请求/响应循环

`codex-rs/core/src/session/turn.rs` — `run_turn()` 函数：

```rust
//119:139:codex-rs/core/src/session/turn.rs
/// Takes initial turn input and runs a loop where, at each sampling request,
/// the model replies with either:
/// - requested function calls
/// - an assistant message
///
/// While it is possible for the model to return multiple of these items in a
/// single sampling request, in practice, we generally one item per sampling request:
///
/// - If the model requests a function call, we execute it and send the output
///   back to the model in the next sampling request.
/// - If the model sends only an assistant message, we record it in the
///   conversation history and consider the turn complete.
```

**完整 Turn 流程**：

```
用户输入
   │
   ▼
┌─ pre-sampling 上下文压缩 ─────────────────┐
│  • 检查 token 预算                         │
│  • 执行自动 compaction                     │
└───────────────────────────────────────────┘
   │
   ▼
┌─ record_context_updates ──────────────────┐
│  • 记录环境上下文变更（增量 diff）          │
└───────────────────────────────────────────┘
   │
   ▼
┌─ build_skills_and_plugins ────────────────┐
│  • 注入 Skill/Plugin 指令                  │
│  • 合并 Connector 工具声明                 │
└───────────────────────────────────────────┘
   │
   ▼
┌─ Hook 执行 ───────────────────────────────┐
│  • run_pending_session_start_hooks()      │
│  • run_hooks_and_record_inputs()          │
└───────────────────────────────────────────┘
   │
   ▼
┌─ 请求构建 ────────────────────────────────┐
│  • 从历史中提取 ResponseItem 列表           │
│  • 构建 tools JSON                        │
│  • 设置 model、reasoning 参数              │
└───────────────────────────────────────────┘
   │
   ▼
┌─ API 调用（WebSocket 优先，SSE 回退）─────┐
│  • 发送 ResponsesApiRequest               │
│  • 流式接收 ResponseEvent                  │
└───────────────────────────────────────────┘
   │
   ▼
   ├── Assistant Message → 发射事件 → 继续循环
   │
   └── Tool Call → 执行 → 发送结果 → 继续循环
                                    │
                                    ▼
                           检查 token 是否超限
                           是 → auto_compact → 继续
                           否 → 直接继续
```

**关键代码引用**：

```rust
// 140:254:codex-rs/core/src/session/turn.rs
    // Pre-turn compaction
    if let Err(err) = run_pre_sampling_compact(&sess, &turn_context, &mut client_session).await { ... }

    // Context updates
    sess.record_context_updates_and_set_reference_context_item(turn_context.as_ref()).await;

    // Skills & plugins injection
    let (injection_items, explicitly_enabled_connectors) =
        build_skills_and_plugins(&sess, turn_context.as_ref(), &input, &cancellation_token).await?;

    // Session start hooks
    if run_pending_session_start_hooks(&sess, &turn_context).await { return None; }

    // Input hooks
    if run_hooks_and_record_inputs(&sess, &turn_context, &input).await { return None; }

    // Main sampling loop
    loop {
        let sampling_request_input: Vec<ResponseItem> = {
            sess.clone_history().await.for_prompt(&turn_context.model_info.input_modalities)
        };
        match run_sampling_request(...).await { ... }
    }
```

### API 交互层

`codex-rs/core/src/client.rs` — `ModelClient` 和 `ModelClientSession` 管理 API 通信的生命周期：

```rust
//1:17:codex-rs/core/src/client.rs
//! `ModelClient` is intended to live for the lifetime of a Codex session and holds the stable
//! configuration and state needed to talk to a provider (auth, provider selection, conversation id,
//! and transport fallback state).
//!
//! A [`ModelClientSession`] is created per turn and is used to stream one or more Responses API
//! requests during that turn. It caches a Responses WebSocket connection (opened lazily) and stores
//! per-turn state such as the `x-codex-turn-state` token used for sticky routing.
//!
//! WebSocket prewarm is a v2-only `response.create` with `generate=false`; it waits for completion
//! so the next request can reuse the same connection and `previous_response_id`.
```

`codex-rs/codex-api/src/provider.rs` — `Provider` 封装 API 端点配置：

```rust
//42:50:codex-rs/codex-api/src/provider.rs
pub struct Provider {
    pub name: String,
    pub base_url: String,
    pub query_params: Option<HashMap<String, String>>,
    pub headers: HeaderMap,
    pub retry: RetryConfig,
    pub stream_idle_timeout: Duration,
}
```

支持 HTTP → WebSocket 自动升级：

```rust
//88:103:codex-rs/codex-api/src/provider.rs
    pub fn websocket_url_for_path(&self, path: &str) -> Result<Url, url::ParseError> {
        let mut url = Url::parse(&self.url_for_path(path))?;
        let scheme = match url.scheme() {
            "http" => "ws",
            "https" => "wss",
            "ws" | "wss" => return Ok(url),
            _ => return Ok(url),
        };
        let _ = url.set_scheme(scheme);
        Ok(url)
    }
```

支持 Azure 端点自动检测（`is_azure_responses_provider`）：

```rust
//106:127:codex-rs/codex-api/src/provider.rs
pub fn is_azure_responses_provider(name: &str, base_url: Option<&str>) -> bool {
    if name.eq_ignore_ascii_case("azure") { true }
    else if let Some(base_url) = base_url {
        matches_azure_responses_base_url(base_url)
    } else { false }
}
```

---

## 八、上下文压缩 —— 管理有限的 Token 预算

当对话历史过长时，Codex 执行自动上下文压缩（auto_compact），通过 `auto_compact_token_limit_scope` 配置 token 阈值。压缩后的摘要作为 initial context 注入到后续请求中。

---

## 九、Sub-Agent（子 Agent）框架

Codex 支持 spawn 子 Agent 来处理复杂任务：

`codex-rs/core/src/codex_delegate.rs`:

```rust
//1:37:codex-rs/core/src/codex_delegate.rs
use crate::guardian::routes_approval_to_guardian;
use crate::guardian::spawn_approval_request_review;
...
/// Start an interactive sub-Codex thread and return IO channels.
```

子 Agent 继承父 Agent 的部分配置（权限、环境、Shell 快照、执行策略），通过独立的事件通道异步通信，审批请求路由回父 Agent 处理。

---

## 总结：Harness 为模型提供了什么？

```
            ┌─────────────────────────────────────────────┐
            │              Codex Harness                  │
            │                                             │
            │  ┌──────────┐  ┌──────────┐  ┌───────────┐ │
            │  │ 环境感知  │  │ 项目理解  │  │ 权限管理   │ │
            │  │ Shell/Git │  │ AGENTS.md│  │ Sandbox/  │ │
            │  │ 快照/时间  │  │ 根检测   │  │ Approval  │ │
            │  └────┬─────┘  └────┬─────┘  └─────┬─────┘ │
            │       │             │               │       │
            │       └─────────┬───┴───────┬───────┘       │
            │                 │           │                │
            │          context XML fragments               │
            │                 │                           │
            │  ┌──────────┐   │   ┌────────────────────┐  │
            │  │ Tools    │◄──┼──►│  LLM (模型 API)    │  │
            │  │ Shell/   │   │   │  理解上下文        │  │
            │  │ Patch/   │   │   │  生成代码方案      │  │
            │  │ MCP      │   │   │  调用工具          │  │
            │  └────┬─────┘   │   └────────────────────┘  │
            │       │         │                           │
            │  ┌────┴─────┐   │                           │
            │  │ Hooks    │───┘                           │
            │  │ Pre/Post │                               │
            │  └──────────┘                               │
            └─────────────────────────────────────────────┘
```

**本质**：Codex 的 Harness 把一个「盲人 LLM」变成有眼睛、有手、有记忆、知道自己在哪里的 Agent。所有的本地准备工作，最终都转化为结构化的 XML/文本上下文片段，让模型在做出任何决定之前，已经知晓代码仓库的全貌、本地环境的限制、以及自己拥有的能力边界。

| 本地准备 | 关键文件 | 为模型提供的信息 | 帮助解决的问题 |
|----------|---------|-----------------|---------------|
| **Shell 检测** | `core/src/shell.rs` | Shell 类型和路径 | 命令能在正确 Shell 中执行 |
| **环境快照** | `core/src/shell_snapshot.rs` | PATH、别名、函数、环境变量 | 命令在用户真实环境中运行 |
| **Git 信息** | `git-utils/src/info.rs` | commit hash、分支、远程 URL | 理解版本上下文 |
| **AGENTS.md** | `core/src/agents_md.rs` | 编码规范、架构约定 | 遵循项目规范 |
| **环境上下文** | `core/src/context/environment_context.rs` | CWD、日期、时区、权限 | 精确知道运行环境 |
| **权限指令** | `core/src/context/permissions_instructions.rs` | Sandbox 模式、审批策略 | 知道操作边界 |
| **片段注册** | `core/src/context/contextual_user_message.rs` | 增量上下文更新 | 避免冗余传输 |
| **Hook 系统** | `core/src/hook_runtime.rs` | 生命周期拦截点 | 外部工具介入 |
| **Tool 注册** | `core/src/tools/registry.rs` | 可用工具列表 | 知道能调用哪些工具 |
| **Turn 引擎** | `core/src/session/turn.rs` | 请求构建、循环执行 | 正确编排多轮交互 |
| **API 交互** | `core/src/client.rs`, `codex-api/src/provider.rs` | WebSocket/SSE 传输 | 可靠通信 |
| **子 Agent** | `core/src/codex_delegate.rs` | 审批路由、结果回传 | 处理复杂多步任务 |


