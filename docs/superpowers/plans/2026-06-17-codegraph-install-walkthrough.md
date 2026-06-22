# CodeGraph 安装与 MCP 接入 Walkthrough

## 目标

在本机为 GRPD 仓库安装 CodeGraph 1.0.1，并同时接入 Codex 与 Antigravity 的 MCP 配置，使两个工具都能通过本地 `.codegraph/` 索引查询仓库代码结构。

## 实施记录

- 已将配置备份到 `/Users/hanbozhang/.codegraph-install-backups/20260617-171103`。
- 已通过官方自包含安装器安装 CodeGraph 1.0.1：
  - 安装目录：`/Users/hanbozhang/.codegraph/versions/v1.0.1`
  - PATH 链接：`/opt/homebrew/bin/codegraph`
- 已执行 `codegraph telemetry off` 关闭匿名遥测。
- 已执行 `codegraph install --target=codex,antigravity --location=global --yes`：
  - Codex 配置：`/Users/hanbozhang/.codex/config.toml`
  - Codex 全局说明：`/Users/hanbozhang/.codex/AGENTS.md`
  - Antigravity 统一 MCP 配置：`/Users/hanbozhang/.gemini/config/mcp_config.json`
- 已在仓库 `.gitignore` 中加入 `.codegraph/`，防止本地索引进入 Git。
- 已在 `/Users/hanbozhang/C++/GRPD` 执行 `codegraph init` 生成本地索引。

## 验证记录

- `codegraph version` 输出 `1.0.1`。
- `command -v codegraph` 输出 `/opt/homebrew/bin/codegraph`。
- `codegraph telemetry` 显示 `Telemetry: disabled`。
- `codegraph status /Users/hanbozhang/C++/GRPD` 显示索引已更新：
  - Files: 1,966
  - Nodes: 48,883
  - Edges: 105,876
  - Backend: `node:sqlite`
  - Journal: `wal`
- `codegraph query MatrixFreeImplicitIntegrator --limit 5` 能返回 `MatrixFreeImplicitIntegrator` 类、方法、头文件和源文件。
- `codegraph files --filter Src --max-depth 3` 能输出 `Src` 目录结构。
- `/Users/hanbozhang/.codex/config.toml` 已包含 `[mcp_servers.codegraph]`，命令为 `codegraph`，参数为 `serve --mcp`。
- `/Users/hanbozhang/.gemini/config/mcp_config.json` 已包含 `mcpServers.codegraph`，命令为 `/opt/homebrew/bin/codegraph`，参数为 `serve --mcp --path /Users/hanbozhang/C++/GRPD`，并保留原有 GRPD MCP 服务器。
- `git check-ignore -v .codegraph/codegraph.db` 确认 `.codegraph/` 由 `.gitignore` 忽略。
- 针对 Antigravity 显示“未索引”的问题，已确认其启动的 `codegraph serve --mcp` 进程工作目录为 `/`，不是 GRPD 根目录；显式加入 `--path /Users/hanbozhang/C++/GRPD` 后，本地 MCP 握手返回完整可用说明，`tools/list` 正常列出 `codegraph_search`、`codegraph_callers`、`codegraph_node`、`codegraph_explore`。

## 后续事项

重启 Codex 与 Antigravity 后，MCP 配置才会被两个应用重新加载。若 Antigravity 重启后仍未显示 CodeGraph MCP，再将同一 `codegraph` 条目同步到 `/Users/hanbozhang/.gemini/antigravity-ide/mcp_config.json` 作为兼容补救。
