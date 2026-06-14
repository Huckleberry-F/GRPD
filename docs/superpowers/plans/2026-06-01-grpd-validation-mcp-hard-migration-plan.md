# GRPD Validation MCP 硬迁移实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 GRPD/ANSYS 对比报告能力从 ANSYS MCP 硬迁移到独立 validation MCP。

**Architecture:** 新增 `grpd-validation-mcp-server` 承担 VTK/TXT 对齐、误差和报告导出；`ansys-mcp-server` 只保留 ANSYS 求解侧工具；smoke tester 固定三段 MCP 链路。

**Tech Stack:** Python 3.12, FastMCP, PyVista, NumPy, Pandas, Matplotlib, PyTest

---

## Task 1: RED 测试

- [x] 新增 validation MCP mock 对比测试。
- [x] 新增 ANSYS 公开工具边界测试。
- [x] 新增 smoke tester 文档链路测试。

## Task 2: Validation MCP

- [x] 新建 `.agents/mcp/grpd-validation-mcp-server/`。
- [x] 添加 `server.py` facade 和 `src/` 核心实现。
- [x] 添加 `compare_grpd_vtk_with_ansys_txt` MCP tool。
- [x] 输出 Excel、PNG、JSON、ZIP artifact。

## Task 3: ANSYS MCP 清理

- [x] 从 `ansys-mcp-server/server.py` 移除 `generate_comparison_report`。
- [x] 移除 ANSYS 侧旧对比模块和根目录 `test_compare.py`。
- [x] 保持其他 ANSYS tool 签名不变。

## Task 4: Smoke Tester 与 MCP 配置

- [x] 更新 `grpd-smoke-tester` skill 和 playbook。
- [x] 新增 `grpd-smoke-test` workflow 入口。
- [x] 更新 `.codex/config.toml` 与 `.agents/settings.json.example` 注册 `grpd-validation-server`。

## Task 5: 降级规则

- [x] 修改 `AGENTS.md`，禁止持久化 Python Entry 中转脚本。
- [x] 确认 `.agents/run_thermal_smoke_test.py` 不存在。

## Task 6: 验证

- [x] 运行 validation MCP mock 对比测试。
- [x] 运行 ANSYS 公开工具边界测试。
- [x] 运行 smoke tester 文档静态测试。
- [x] 运行旧入口引用搜索，确认运行规约中不再指向 `ansys-server.generate_comparison_report`。
