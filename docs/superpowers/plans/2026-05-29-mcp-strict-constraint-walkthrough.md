# MCP 强制约束与环境隔离修复执行总结 (Walkthrough)

**Goal:** 
通过在 `AGENTS.md` 注入硬性规则，彻底封死 AI 在缺乏 MCP 工具时“裸跑底层脚本”的降级漏洞。同时，修正 `.gemini/settings.json` 的 Python 环境路径，确保 IDE 能够正确拉起 MCP 服务器。

## Changes Made
- **修改了 `AGENTS.md`**：在 “终极工作流拦截机制” 之前成功注入了《架构隔离与 MCP 管线强制约束规约》。明确了降级铁律：遇到 MCP 丢失的情况，AI 必须手写 Python 中转脚本调用 `server.py`，绝对禁止裸调底层子模块。
- **修改了 `.gemini/settings.json`**：将 `ansys-server`、`grpd-server`、`grpd-experiment-server` 和 `matlab-server` 的启动指令从原先的 `"python"` 全部替换为了绝对路径 `"C:\\Users\\zhanghanbo\\AppData\\Local\\Microsoft\\WindowsApps\\python.exe"`，以解决因全局解释器寻址导致的挂载失败问题。

## Validation Results
- 所有规约文本和 JSON 配置项均已精准落盘。
- **下一步操作建议**：由于 `.gemini/settings.json` 的配置变更需要宿主 IDE 重新读取才能生效，请在 VS Code 或您的编辑器中执行一次 **`Developer: Reload Window`**（重载窗口），以便系统拉起这些 MCP 服务器并向 AI 暴露出原生工具。
