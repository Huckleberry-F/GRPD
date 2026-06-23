# ANSYS MCP Server

本服务是 GRPD 验证链中的 ANSYS 侧 MCP。它只负责生成 APDL、调用 MAPDL、解析 ANSYS 输出文件，并将 `.txt/.db/.out/.err` 等路径返回给上层代理。

GRPD VTK 与 ANSYS TXT 的误差对比已经迁移到 `.agents/mcp/grpd-validation-mcp-server`，本服务不再公开 VTK/TXT 对比 tool。

## 目录结构

```text
ansys-mcp-server/
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
    ├── test_public_tools.py
    └── test_standard_layout.py
```

`server.py` 是 FastMCP facade，只负责注册公开 tool 并路由到 `src.service`。核心逻辑必须放在 `src/`。

## 公开 Tools

- `run_ansys_mac(...)`
- `generate_ansys_apdl_from_yaml(...)`
- `run_ansys_yaml_case(...)`
- `get_ansys_solve_status(...)`
- `get_ansys_text_results(...)`
- `list_ansys_files(...)`

## 安装

```powershell
cd D:\Project_C++\GRPD\.agents\mcp\ansys-mcp-server
python -m pip install -r requirements.txt
```

## 配置

编辑 `config.yaml`：

- `ansys_executable`: ANSYS MAPDL 可执行文件路径。
- `allowed_directories`: 允许 ANSYS MCP 访问的工作目录白名单。
- `defaults`: 默认 CPU、内存和超时设置。

## 测试

```powershell
cd D:\Project_C++\GRPD
python -m pytest .agents/mcp/ansys-mcp-server/tests -q
```

这些测试只验证导入、公开 tool 边界和目录结构，不真实启动 ANSYS。
