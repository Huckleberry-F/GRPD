# MCP Setup Cross-Platform Compatibility Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `.agents/setup_mcp.py` 优化为跨平台兼容版本，使其能自动处理 Windows 反斜杠与 macOS/Linux 正斜杠的差异，并智能优先探测和选择项目本地 `.venv` 虚拟环境中的 Python 解释器。

**Architecture:** 
1. 在检测 Python 解释器时，优先寻找项目根目录下 `.venv` 目录中对应的 Python 解释器路径（区分 Windows 和 macOS/Linux 的差异）。
2. 在处理 `settings.json.example` 中的 `args` 和 `cwd` 路径时，若运行在非 Windows 系统上，自动将反斜杠 `\` 替换为正斜杠 `/`，然后使用 `os.path.normpath` 转换为符合当前平台的标准路径。

**Tech Stack:** Python 3.x, Standard OS Library

---

### Task 1: 优化 `.agents/setup_mcp.py` 以适配跨平台路径与虚拟环境

**Files:**
- Modify: `.agents/setup_mcp.py`

- [ ] **Step 1: 修改 Python 解释器探测逻辑与路径分隔符替换逻辑**

修改 `.agents/setup_mcp.py` 中关于 Python 解释器的自动探测，以及在处理 `args` 和 `cwd` 时替换 `<GRPD_PROJECT_ROOT_PATH>` 的相关逻辑。

代码改动片段：
```python
<<<<
    # 1. 自动检测项目根目录与 Python 解释器
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(script_dir, ".."))
    python_exe = sys.executable

    print(f"[+] 检测到项目根目录: {project_root}")
    print(f"[+] 检测到当前 Python 解释器: {python_exe}")
====
    # 1. 自动检测项目根目录与 Python 解释器
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(script_dir, ".."))
    
    # 优先探测项目内的虚拟环境 Python 解释器，以确保其包含 mcp 等所有已安装的第三方依赖
    if os.name == "nt":
        venv_python = os.path.join(project_root, ".venv", "Scripts", "python.exe")
    else:
        venv_python = os.path.join(project_root, ".venv", "bin", "python")
        
    if os.path.exists(venv_python):
        python_exe = venv_python
        print(f"[+] 优先使用项目虚拟环境 Python 解释器: {python_exe}")
    else:
        python_exe = sys.executable
        print(f"[+] 未检测到项目虚拟环境，使用当前 Python 解释器: {python_exe}")

    print(f"[+] 检测到项目根目录: {project_root}")
>>>>
```

同时修改生成配置中的路径替换：
```python
<<<<
    # 4. 生成本地 .agents/settings.json
    mcp_servers = config_data.get("mcpServers", {})
    updated_servers = {}
    
    for server_name, server_cfg in mcp_servers.items():
        # 复制配置并替换占位符
        new_cfg = server_cfg.copy()
        new_cfg["command"] = python_exe
        new_cfg["args"] = [arg.replace("<GRPD_PROJECT_ROOT_PATH>", project_root) for arg in server_cfg.get("args", [])]
        new_cfg["cwd"] = server_cfg.get("cwd", "").replace("<GRPD_PROJECT_ROOT_PATH>", project_root)
        updated_servers[server_name] = new_cfg
====
    # 4. 生成本地 .agents/settings.json
    mcp_servers = config_data.get("mcpServers", {})
    updated_servers = {}
    
    for server_name, server_cfg in mcp_servers.items():
        # 复制配置并替换占位符
        new_cfg = server_cfg.copy()
        new_cfg["command"] = python_exe
        
        # 统一处理参数中的路径分隔符
        new_args = []
        for arg in server_cfg.get("args", []):
            replaced = arg.replace("<GRPD_PROJECT_ROOT_PATH>", project_root)
            # 在非 Windows 系统下，统一将 Windows 的 "\\" 替换为 "/"
            if os.name != "nt":
                replaced = replaced.replace("\\", "/")
            new_args.append(os.path.normpath(replaced))
        new_cfg["args"] = new_args
        
        # 统一处理 cwd 路径中的分隔符
        raw_cwd = server_cfg.get("cwd", "").replace("<GRPD_PROJECT_ROOT_PATH>", project_root)
        if os.name != "nt":
            raw_cwd = raw_cwd.replace("\\", "/")
        new_cfg["cwd"] = os.path.normpath(raw_cwd)
        
        updated_servers[server_name] = new_cfg
>>>>
```

### Task 2: 运行测试并验证生成的配置文件

**Files:**
- Test: `.agents/settings.json`
- Test: `/Users/hanbozhang/.gemini/antigravity-ide/mcp_config.json`

- [ ] **Step 1: 运行 `setup_mcp.py` 脚本**

在终端运行以下配置脚本：
运行：`/Users/hanbozhang/C++/GRPD/.venv/bin/python .agents/setup_mcp.py`

- [ ] **Step 2: 验证生成的本地 `.agents/settings.json` 是否符合格式**

检查 `.agents/settings.json` 内容：
1. `command` 字段是否正确指向虚拟环境中的 python。
2. `args` 和 `cwd` 中的路径在 Mac 上是否全部使用 `/`，且路径正确拼合，无反斜杠。

- [ ] **Step 3: 验证生成的全局 `mcp_config.json` 是否符合格式**

检查 `/Users/hanbozhang/.gemini/antigravity-ide/mcp_config.json` 内容：
1. `args` 和 `cwd` 在 Mac 上使用 `/` 分隔。
2. 5 个 MCP 服务全部正确覆盖并且没有多余语法错误。
