---
name: single-flow-task-execution
description: "Use when executing approved implementation plans in Antigravity or other environments where parallel coding subagents are unavailable, risky, or not desired; especially for GRPD task-by-task execution with review and verification gates."
---

# Single-Flow Task Execution

本 skill 是 GRPD 在 Antigravity 下的执行 Overlay。它保留 Superpowers 的任务拆解、评审和验证纪律，但把并行子代理模型收敛为单流执行，避免多个 coding agent 同时修改同一 CAE 代码链路。

## 核心原则

一次只推进一个任务。每个任务必须先确定 CAE 影响面，再实施、验证、复核，最后更新运行时任务表。

## 运行时任务表

默认任务表路径：

```text
docs/superpowers/task.md
```

如果文件不存在，执行时创建以下表格。该文件是运行时状态，不是长期设计文档。

```markdown
| ID | 任务 | 状态 | 证据 | 备注 |
|---|---|---|---|---|
| T1 | 示例任务 | pending |  |  |
```

状态只能使用：

- `pending`
- `in_progress`
- `blocked`
- `done`
- `cancelled`

任意时刻只能有一个任务是 `in_progress`。

## 必做流程

1. 读取已批准的计划文件，提取所有任务和验证命令。
2. 使用 `superCAE/skills/grpd-cae-router` 判断每个任务需要的 GRPD 专项 skill。
3. 初始化或更新 `docs/superpowers/task.md`，只保留任务表和必要证据。
4. 按顺序执行任务：
   - 标记当前任务为 `in_progress`。
   - 读取任务对应的专项 skill。
   - 写清本任务的 CAE 风险字段：管线层级、状态变量、SoA、OpenMP、验证。
   - 做最小范围修改。
   - 运行任务要求的验证命令。
   - 自查 spec 符合性和代码质量。
   - 验证通过后标记为 `done`，并记录命令与关键输出。
5. 所有任务完成后，运行全局验证命令。
6. 使用 `verification-before-completion` 后再声明完成。

## 单任务执行模板

每个任务开始前，先写出这一小段：

```markdown
### 当前任务：Tn - <任务名>

**Plan Source:** `docs/superpowers/plans/<plan>.md`
**Touched Pipeline Layers:** 层 1/2/3/4/5/6/7/8 中的具体层级
**Domain Skills:** `superCAE/skills/grpd-cae-router` + 具体专项 skill
**State Risk:** 是否涉及 Old/Trial/Commit、stateMode、损伤正式提交
**SoA Risk:** 是否涉及 FieldManager 字段或核心循环裸指针
**Parallel Risk:** 是否涉及 OpenMP 共享写、归约、邻域循环
**Validation:** 本任务必须运行的命令或 MCP 流程
```

## 评审门禁

任务完成前必须过两道门：

1. **Spec 符合性**
   - 是否实现了计划要求的行为。
   - 是否没有额外引入未请求的架构变化。
   - 是否保持 GRPD 的 8 层管线边界。
   - 是否遵循所选专项 skill 的检查项。

2. **代码质量**
   - 是否保持 C++17、RAII、头/源分离。
   - 是否避免核心循环中的 AoS 或不必要拷贝。
   - 是否没有明显数据竞争或状态污染。
   - 是否有最小验证证据支撑。

评审发现问题时，保持当前任务为 `in_progress` 或 `blocked`，修复并重新验证，不要跳到下一个任务。

## Antigravity 约束

- 不使用隐藏 Artifact 路径保存计划或总结。
- 不并行派发多个 coding subagent。
- 浏览器相关任务可以独立拆成一步，但不得和 C++ 修改并行推进。
- 优先使用 Antigravity 原生文件工具；在 Codex 环境中使用等价的文件读取、搜索、补丁和计划工具。
- 任何破坏性 git 或文件操作必须等待用户明确指令。

## 与 GRPD 的关系

本 skill 只管理执行节奏。遇到 CAE 内容时，必须交给 `superCAE/skills/grpd-cae-router` 选择专项 skill；遇到验证结论时，必须交给 `verification-before-completion` 约束完成声明。

## 什么时候不用

- 用户只要一次性解释、代码评审或建议，不需要执行计划。
- 任务只有一个很小的非 CAE 文档改动，并且无需任务表。
- 已有平台提供可靠的隔离工作树和子代理机制，且用户明确要求并行执行。
