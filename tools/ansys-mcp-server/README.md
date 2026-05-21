# ANSYS MCP Server 🚀

这是一个用于 Model Context Protocol (MCP) 的 Python 服务器端实现，专门为了让你能够通过 AI 助手（例如 Claude、Antigravity）无缝控制和分析本地的 **ANSYS MAPDL** 仿真任务。

这个项目是展示你将 **领域专业软件集成入现代 AI 工具链** 的完美示例，可以在面试中重点提及！

## ✨ 核心功能
1. **自动化调用**: 让 AI 直接触发你的 `.mac` APDL 脚本运行。
2. **状态监测**: 解析 `.out` 和 `.err` 日志，自动提取非线性迭代的收敛状态、计算时间和报错信息。
3. **数据解析**: 自动读取 `PRVAR` 等命令导出的 `.txt` 文本结果，将其转换为 JSON 结构化数据，供 AI 帮你画图或进行理论对标（例如 GRPD vs ANSYS）。
4. **APDL 生成**: 根据 GRPD `PD.yaml` 生成 ANSYS 验证宏，并保存 `.db` 数据库文件用于人工复查。
5. **安全沙箱**: 通过 `config.yaml` 限制工作目录，防止意外操作。

## ⚙️ 安装与配置

### 1. 依赖安装
推荐使用 `uv` 或标准的 `pip`：
```bash
cd tools/ansys-mcp-server
pip install -r requirements.txt
```

### 2. 修改配置
打开 `config.yaml`，根据你的实际环境修改：
- `ansys_executable`: 你的 ANSYS 执行文件路径（请确保路径正确，比如 `C:\Program Files\ANSYS Inc\vxxx\ANSYS\bin\winx64\ansysxxx.exe`）。
- `allowed_directories`: 添加你的工程路径，以防 AI 误读系统敏感目录。

## 🛠️ MCP 注册与使用

如果你在 Claude Desktop 或其他支持 MCP 的独立环境中使用，可以在配置文件中添加该服务器：

```json
{
  "mcpServers": {
    "ansys-server": {
      "command": "python",
      "args": [
        "D:\\Project_C++\\GRPD\\tools\\ansys-mcp-server\\server.py"
      ]
    }
  }
}
```
*注：由于你现在正在与我对话，我暂时没有直接添加外部 MCP server 的本地权限，但你可以将其配置给 Claude Desktop、Cursor 或 Windsurf 使用。*

## 💡 面试亮点建议
在面试中，你可以这样展示这个小项目：
> "在使用 AI 进行 C++ 近场动力学求解器开发时，我发现手动运行 ANSYS benchmark 非常繁琐。于是我用 MCP 协议基于 Python 写了一个 Ansys Server。现在，我可以直接让 AI 助手修改我的 C++ 求解器代码，然后通过 MCP 自动调用 ANSYS 宏跑出标准解，最后让 AI 自动读取两边的结果文本进行误差比对。这极大地闭环了我的研发工作流，展示了我打通不同技术栈的能力。"
