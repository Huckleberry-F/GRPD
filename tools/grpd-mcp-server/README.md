# GRPD MCP Server

这个 MCP server 只封装 GRPD 项目自身的自动化能力：

- 构建 GRPD
- 运行指定算例
- 定位最新或指定子步的 VTK

启动方式：

```powershell
cd tools/grpd-mcp-server
python server.py
```

ANSYS APDL 生成和运行由 `tools/ansys-mcp-server` 负责；GRPD/ANSYS 对比由 `tools/path-comparison-mcp-server` 负责。
