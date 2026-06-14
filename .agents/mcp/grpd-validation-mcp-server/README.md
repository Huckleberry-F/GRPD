# GRPD Validation MCP Server

本服务负责跨求解器验证后处理：读取 GRPD VTK 与 ANSYS TXT，沿指定三维采样线进行点云过滤、投影对齐、插值、误差统计，并导出 Excel、PNG、JSON 与 ZIP 报告。

## 职责边界

- 不运行 GRPD。
- 不运行 ANSYS。
- 不写入实验历史数据库。
- 实验记录由 `grpd-experiment-mcp-server` 负责。

## 目录结构

```text
grpd-validation-mcp-server/
├── requirements.txt
├── README.md
├── server.py
├── src/
│   ├── __init__.py
│   ├── service.py
│   ├── comparison.py
│   ├── paths.py
│   └── result_parser.py
├── templates/
│   └── README.md
└── tests/
    ├── __init__.py
    ├── conftest.py
    ├── test_compare_grpd_ansys.py
    └── test_standard_layout.py
```

`server.py` 是 FastMCP facade，只负责注册公开 tool 并路由到 `src.service`。validation MCP 不需要 `runner.py` 或 `generator.py`，因为它不拉起外部求解器，也不生成求解脚本。

## MCP Tool

```text
compare_grpd_vtk_with_ansys_txt(...)
```

## 测试

```powershell
cd D:\Project_C++\GRPD
python -m pytest .agents/mcp/grpd-validation-mcp-server/tests -q
```

测试使用临时 mock VTK/TXT，不真实启动 GRPD 或 ANSYS。
