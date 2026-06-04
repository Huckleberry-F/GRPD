# MCP 强制约束与环境隔离修复计划 (MCP Strict Constraint Plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 
通过在 `AGENTS.md` 注入硬性规则，彻底封死 AI 在缺乏 MCP 工具时“裸跑底层脚本”的降级漏洞。同时，修正 `.gemini/settings.json` 的 Python 环境路径，确保 IDE 能够正确拉起 MCP 服务器。

**Architecture:** 
1. **修改 `AGENTS.md`**：在文件末尾新增“架构隔离与 MCP 管线强制约束规约”，明确定义如果 MCP 工具未加载，AI 必须通过 `import server` 编写中转脚本来触发逻辑，严禁直接调用 CLI。
2. **修改 `.gemini/settings.json` (可选)**：将 `"command": "python"` 替换为项目虚拟环境下的绝对路径（如适用），避免因全局环境缺包导致的静默加载失败。

---

### Task 1: 注入 MCP 降级铁律

**Files:**
- `[MODIFY]` `AGENTS.md`

- [ ] **Step 1: 新增约束条目**
在 `## ⚠️ 终极工作流拦截机制 (Anti-Planning-Mode)` 之前，插入一段名为 `## 架构隔离与 MCP 管线强制约束规约` 的新章节。
明确规定：遇到未注入工具的极端情况，必须手写 Python 入口脚本调用 `server.py` 的顶层接口，以此保护 `work_dir` 与 SQLite 落盘机制，绝对禁止裸调底层子模块。

### Task 2: 修正 IDE 挂载配置 (需您本地确认路径)

**Files:**
- `[MODIFY]` `.gemini/settings.json`

- [ ] **Step 1: 定位并更新 Python 解释器**
将 `ansys-server`, `grpd-server`, `grpd-experiment-server`, `matlab-server` 中的 `"command": "python"` 全部修改为包含 `mcp` 依赖的有效路径。
*(由于我无法探测到您本地是否使用 `.venv` 或特定的 conda 环境路径，我们可以在修改时将它改为 `uv run python`，或者直接指定绝对路径)*。

## User Review Required
在修改 `.gemini/settings.json` 前，请告诉我您通常在本地如何启动带有 `mcp` 依赖的 Python 环境？
1. 是通过绝对路径（如 `D:\Project_C++\GRPD\.venv\Scripts\python.exe`）？
2. 还是通过 `uv` 工具（如改成 `"command": "uv", "args": ["run", "python", ...]`）？
3. 或者只修改 `AGENTS.md`，环境问题您自己手动改？
