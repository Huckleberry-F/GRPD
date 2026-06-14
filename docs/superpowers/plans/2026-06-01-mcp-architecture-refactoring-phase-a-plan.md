# MCP Architecture Refactoring Phase A Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `ansys-mcp-server` 标准化为 `server.py facade + src/ + templates/ + tests/`，并补齐 `grpd-validation-mcp-server` 的标准目录细节。

**Architecture:** `server.py` 只保留 FastMCP tool 注册和路由；ANSYS 业务逻辑迁入 `src.service`，路径、SQLite、runner、APDL 生成和结果解析分别放入独立模块。validation MCP 保持现有核心逻辑，只补齐标准目录守卫。

**Tech Stack:** Python, FastMCP, pytest, PyYAML, SQLite, PyVista/Numpy/Matplotlib/OpenPyXL for validation tests.

---

### Task 1: 二期 A 文档

**Files:**
- Create: `docs/superpowers/specs/2026-06-01-mcp-architecture-refactoring-phase-a-design.md`
- Create: `docs/superpowers/plans/2026-06-01-mcp-architecture-refactoring-phase-a-plan.md`

- [x] **Step 1: 写设计文档**

写明本期只处理 ANSYS MCP 与 validation MCP，不重构 GRPD/MATLAB/Experiment。

- [x] **Step 2: 写实施计划**

列出测试先行、迁移模块、瘦身 facade、补齐 validation 标准目录和验证命令。

### Task 2: 标准目录 RED 测试

**Files:**
- Create: `.agents/mcp/ansys-mcp-server/tests/test_standard_layout.py`
- Create: `.agents/mcp/grpd-validation-mcp-server/tests/test_standard_layout.py`
- Modify: `.agents/mcp/ansys-mcp-server/tests/conftest.py`

- [x] **Step 1: 写 ANSYS 标准目录测试**

断言 `src/runner.py`、`src/generator.py`、`src/result_parser.py`、`src/paths.py`、`src/history.py`、`src/service.py` 存在，根目录旧核心模块不存在。

- [x] **Step 2: 写 validation 标准目录测试**

断言 `tests/__init__.py` 与 `templates/README.md` 存在，且 validation 不要求 runner/generator。

- [x] **Step 3: 运行 RED**

Run:

```powershell
python -m pytest .agents/mcp/ansys-mcp-server/tests/test_standard_layout.py -q
python -m pytest .agents/mcp/grpd-validation-mcp-server/tests/test_standard_layout.py -q
```

Expected: FAIL，原因是 ANSYS `src/` 与 validation 标准细节尚未补齐。

### Task 3: ANSYS MCP 迁移到 src

**Files:**
- Create: `.agents/mcp/ansys-mcp-server/src/__init__.py`
- Create: `.agents/mcp/ansys-mcp-server/src/paths.py`
- Create: `.agents/mcp/ansys-mcp-server/src/runner.py`
- Create: `.agents/mcp/ansys-mcp-server/src/generator.py`
- Create: `.agents/mcp/ansys-mcp-server/src/result_parser.py`
- Create: `.agents/mcp/ansys-mcp-server/src/history.py`
- Create: `.agents/mcp/ansys-mcp-server/src/service.py`
- Delete: `.agents/mcp/ansys-mcp-server/ansys_runner.py`
- Delete: `.agents/mcp/ansys-mcp-server/apdl_generator.py`
- Delete: `.agents/mcp/ansys-mcp-server/result_parser.py`

- [x] **Step 1: 迁移 runner/generator/parser**

把根目录核心模块移动到 `src/`，并改名为 `runner.py`、`generator.py`、`result_parser.py`。

- [x] **Step 2: 提取 paths/history/service**

把 `get_next_work_dir`、配置读取、DB 路径、SQLite 写入和 tool 背后的编排逻辑移出 `server.py`。

- [x] **Step 3: 删除根目录旧核心模块**

不保留兼容层，防止 AI 或测试继续导入旧模块。

### Task 4: server.py facade 化

**Files:**
- Modify: `.agents/mcp/ansys-mcp-server/server.py`

- [x] **Step 1: 保留公开 tool 签名**

`run_ansys_mac`、`generate_ansys_apdl_from_yaml`、`run_ansys_yaml_case`、`get_ansys_solve_status`、`get_ansys_text_results`、`list_ansys_files` 签名不变。

- [x] **Step 2: tool 内只路由到 src.service**

每个 tool 只调用对应 `_service_*` 函数并返回结果。

### Task 5: validation MCP 标准细节

**Files:**
- Create: `.agents/mcp/grpd-validation-mcp-server/tests/__init__.py`
- Create: `.agents/mcp/grpd-validation-mcp-server/templates/README.md`
- Modify: `.agents/mcp/grpd-validation-mcp-server/README.md`
- Create: `.agents/mcp/ansys-mcp-server/templates/README.md`
- Modify: `.agents/mcp/ansys-mcp-server/README.md`

- [x] **Step 1: 补齐测试包和 templates 目录**

validation MCP 与 ANSYS MCP 都保留 `templates/README.md`，说明当前是否使用静态模板。

- [x] **Step 2: 更新 README**

README 明确标准目录、测试命令和职责边界。

### Task 6: 验证

**Files:**
- Test: `.agents/mcp/ansys-mcp-server/tests/*`
- Test: `.agents/mcp/grpd-validation-mcp-server/tests/*`
- Test: `.agents/tests/test_smoke_tester_links.py`

- [x] **Step 1: 运行目标测试**

```powershell
python -m pytest .agents/mcp/ansys-mcp-server/tests -q
python -m pytest .agents/mcp/grpd-validation-mcp-server/tests -q
python -m pytest .agents/tests/test_smoke_tester_links.py -q
```

- [x] **Step 2: 静态检查旧导入**

```powershell
rg --hidden -n "from ansys_runner|from apdl_generator|from result_parser|import ansys_runner|import apdl_generator" .agents/mcp/ansys-mcp-server .agents/mcp/grpd-validation-mcp-server -g '!**/__pycache__/**'
```

Expected: no matches.

- [x] **Step 3: py_compile**

```powershell
python -m py_compile .agents/mcp/ansys-mcp-server/server.py .agents/mcp/ansys-mcp-server/src/*.py .agents/mcp/grpd-validation-mcp-server/server.py .agents/mcp/grpd-validation-mcp-server/src/*.py
```

Expected: exit 0.
