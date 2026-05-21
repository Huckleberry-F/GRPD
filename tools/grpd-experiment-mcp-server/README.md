# GRPD Experiment MCP Server

本 MCP server 使用 SQLite 记录 GRPD 算例实验历史，包括参数、状态、收敛指标、误差指标和结果文件路径。

启动：

```powershell
cd tools/grpd-experiment-mcp-server
python server.py
```

默认数据库：

```text
tools/grpd-experiment-mcp-server/grpd_experiments.sqlite
```
