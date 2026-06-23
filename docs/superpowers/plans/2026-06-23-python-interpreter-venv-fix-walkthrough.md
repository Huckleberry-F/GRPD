# Walkthrough - Python 解释器路径与 VS Code 虚拟环境配置总结

本文件记录了解决由于未指定虚拟环境解释器导致 Python 脚本报 “No module named 'numpy'” 依赖缺失问题的过程。

## 变更背景与问题

1. **NumPy 缺失报错**：
   在运行某些 Python 脚本或由外部加载的插件服务时，报出 `ModuleNotFoundError: No module named 'numpy'`。
2. **原因定位**：
   经排查，用户系统中的全局 Python 环境由 `uv` 工具管理，属于 `externally-managed-environment`，不允许直接修改系统级包。
   项目根目录下实际上已经存在一套包含了 NumPy 2.5.0 等所有必要依赖包的虚拟环境 `.venv`。但在此前的项目 VS Code settings 中，由于修改了 CMake 生成器导致原来的虚拟环境路径配置丢失，且原本的路径配置仅适用于 macOS，没有适配 Windows 系统。因此，在没有激活虚拟环境或者 VS Code 默认选择全局解释器时，会出现无法加载依赖包的问题。

## 解决方案与修改内容

### 1. 更新项目级 VS Code 设置
在项目根目录的 [.vscode/settings.json](file:///c:/C++/GRPD/.vscode/settings.json) 中添加 Windows 下的 Python 虚拟环境解释器和终端 PATH 环境变量：
```json
{
    "cmake.generator": "MinGW Makefiles",
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "python.defaultInterpreterPath": "${workspaceFolder}/.venv/Scripts/python.exe",
    "python.interpreterPath": "${workspaceFolder}/.venv/Scripts/python.exe",
    "terminal.integrated.env.windows": {
        "PATH": "${workspaceFolder}/.venv/Scripts;${env:PATH}",
        "VIRTUAL_ENV": "${workspaceFolder}/.venv"
    }
}
```

### 2. 更新环境配置 Reference 文档
修改 [python_environment.md](file:///c:/C++/GRPD/docs/python_environment.md)，新增关于 Windows 平台下虚拟环境激活方法、激活后检查命令以及 Windows 端的 VS Code 项目 settings 配置模板。

## 验证与测试结果

1. **虚拟环境依赖库列表验证**：
   运行 `.\.venv\Scripts\pip.exe list` 检查，显示 NumPy (2.5.0)、matplotlib (3.11.0)、pyyaml (6.0.3) 等所需依赖库在项目内部的虚拟环境 `.venv` 中全部均已正确安装并就绪。
2. **工作区设置生效验证**：
   通过更新配置，使得用户在 VS Code 中加载项目或开启新 Terminal 时，会自动激活虚拟环境并优先使用包含 NumPy 依赖的 `.venv/Scripts/python.exe` 解释器。
