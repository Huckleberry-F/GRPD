# MCP 架构标准化二期 A 设计

## 目标

二期 A 只处理 `ansys-mcp-server` 与 `grpd-validation-mcp-server`。目标是把 ANSYS MCP 从扁平脚本目录迁移为标准 MCP 服务结构，并补齐 validation MCP 的标准目录细节。

本期不改公开 MCP tool 的函数名、参数和返回语义；不合并 validation 与 experiment；不重构 GRPD、MATLAB、Experiment MCP。

## 服务边界

- `ansys-mcp-server` 只负责 ANSYS 侧能力：APDL 生成、MAPDL 执行、`.out/.txt/.db/.err` 路径返回和 ANSYS 文本/日志解析。
- `grpd-validation-mcp-server` 只负责 GRPD VTK 与 ANSYS TXT 的对齐、插值、误差统计和报告导出。
- `grpd-experiment-mcp-server` 继续只记录其他 MCP 产出的 metric 与 artifact。

## ANSYS MCP 目标结构

```text
.agents/mcp/ansys-mcp-server/
├── requirements.txt
├── README.md
├── server.py
├── config.yaml
├── src/
│   ├── __init__.py
│   ├── paths.py
│   ├── runner.py
│   ├── generator.py
│   ├── result_parser.py
│   ├── history.py
│   └── service.py
├── templates/
│   └── README.md
└── tests/
    ├── __init__.py
    ├── conftest.py
    └── test_*.py
```

`server.py` 是 FastMCP facade，只注册 tool 并把调用转发到 `src.service`。具体流程、路径、SQLite、runner 初始化、APDL 生成和结果解析全部留在 `src/`。

## Validation MCP 目标结构

```text
.agents/mcp/grpd-validation-mcp-server/
├── requirements.txt
├── README.md
├── server.py
├── src/
│   ├── __init__.py
│   ├── comparison.py
│   ├── paths.py
│   └── result_parser.py
├── templates/
│   └── README.md
└── tests/
    ├── __init__.py
    ├── conftest.py
    └── test_compare_grpd_ansys.py
```

validation MCP 不需要 `runner.py` 或 `generator.py`，因为它不拉起外部求解器，也不生成求解脚本。

## 兼容策略

本期不保留根目录兼容模块：

- 删除 `ansys_runner.py`
- 删除 `apdl_generator.py`
- 删除 `result_parser.py`

内部导入统一改为 `src.runner`、`src.generator`、`src.result_parser`。公开 tool 签名不变。

## 测试策略

- 新增 ANSYS 标准目录测试，确认 `src/` 模块存在、根目录旧核心模块消失、`server.py` 仍暴露原有 ANSYS tools。
- 保留一期 ANSYS tool 边界测试，继续确认 `generate_comparison_report` 不存在。
- 保留 validation mock VTK/TXT 对比测试。
- 新增 validation 标准目录测试，确认 `tests/__init__.py` 与 `templates/README.md` 存在。
- 每个 MCP 服务作为独立测试单元运行 pytest。由于服务目录名包含连字符，多个 `tests/__init__.py` 在同一 pytest 进程中会形成同名包冲突，因此本期验证命令按服务拆开执行。
- 不在单元测试中真实启动 GRPD、ANSYS 或 MATLAB。

## 二期剩余范围

GRPD MCP、Experiment MCP 与 MATLAB MCP 的标准化留到二期 B/C。MATLAB MCP 风险最高，应单独设计 `src/service.py`、`src/history.py`、`src/compiler.py` 与现有 runner/generator/parser 的职责边界。
