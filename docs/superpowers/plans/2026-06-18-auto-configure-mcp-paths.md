# 自动配置 MCP 服务环境 (Auto Configure MCP Paths) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 `setup_mcp.py` 支持跨平台自动探测 MATLAB 和 ANSYS 等依赖软件路径，并一键回写到对应的 MCP 服务 YAML 配置文件中；若未安装则向用户提示。

**Architecture:** 
1. 扩展 `.agents/setup_mcp.py`：新增 `find_software_executable` 功能，按平台枚举常见的默认安装路径。
2. 引入 `yaml` 解析库对 `.agents/mcp/matlab-mcp-server/config.yaml` 和 `.agents/mcp/ansys-mcp-server/config.yaml` 进行就地读写。
3. 把探测到的绝对路径以及当前项目的 `project_root` 注入到服务配置的 `allowed_work_dirs` 中。如果找不到软件则保持为默认值（如直接填软件名），并给出明确的安装警告。

**Tech Stack:** Python, PyYAML, OS/Glob 模块

---

### Task 1: 扩展 `setup_mcp.py` 增加跨平台搜索与 YAML 修改逻辑

**Files:**
- Modify: `.agents/setup_mcp.py`

- [ ] **Step 1: 引入依赖并添加跨平台搜索函数**

在文件头部引入 `yaml` 和 `glob`，然后增加软件路径搜索逻辑。

```python
import os
import sys
import json
import glob
try:
    import yaml
except ImportError:
    print("[!] 警告: 未找到 yaml 库，请执行 pip install pyyaml")
    sys.exit(1)

def find_software_executable(software_type):
    """跨平台寻找软件执行路径"""
    if software_type == "matlab":
        if os.name == "nt":
            paths = glob.glob(r"C:\\Program Files\\MATLAB\\R*\\bin\\matlab.exe")
            return paths[-1] if paths else None  # 取最新版本
        elif sys.platform == "darwin":
            paths = glob.glob("/Applications/MATLAB_R*.app/bin/matlab")
            return paths[-1] if paths else None
        else:
            paths = glob.glob("/usr/local/MATLAB/R*/bin/matlab")
            return paths[-1] if paths else None
    elif software_type == "ansys":
        if os.name == "nt":
            paths = glob.glob(r"C:\\Program Files\\ANSYS Inc\\v*\\ANSYS\\bin\\winx64\\ansys*.exe")
            return paths[-1] if paths else None
        elif sys.platform == "darwin":
            # Mac 通常没有 ANSYS APDL 原生版
            return None
        else:
            paths = glob.glob("/usr/ansys_inc/v*/ansys/bin/ansys*")
            return paths[-1] if paths else None
    return None

def update_yaml_config(yaml_path, exec_key, exec_val, project_root, dir_key):
    """更新 YAML 配置文件"""
    if not os.path.exists(yaml_path):
        return
    with open(yaml_path, 'r', encoding='utf-8') as f:
        config = yaml.safe_load(f)
    if config is None:
        config = {}
        
    if exec_val:
        config[exec_key] = exec_val
    
    # 将项目根目录统一替换分隔符
    if os.name != "nt":
        project_root = project_root.replace("\\\\", "/")
        
    if dir_key in config and isinstance(config[dir_key], list):
        if project_root not in config[dir_key]:
            config[dir_key].append(project_root)
    else:
        config[dir_key] = [project_root]
        
    with open(yaml_path, 'w', encoding='utf-8') as f:
        yaml.dump(config, f, allow_unicode=True, default_flow_style=False)
```

- [ ] **Step 2: 修改 `main()` 函数，注入探测流程**

在 `main()` 中（例如在“生成全局 mcp_config.json”逻辑之后），加入针对 MATLAB 和 ANSYS 的探测。

```python
    # 6. 配置具体的 MCP 子服务 (YAML)
    print("==================================================")
    print("[*] 正在扫描并配置子服务依赖软件环境...")
    
    # MATLAB
    matlab_exe = find_software_executable("matlab")
    matlab_yaml = os.path.join(script_dir, "mcp", "matlab-mcp-server", "config.yaml")
    if matlab_exe:
        print(f"[+] 找到 MATLAB: {matlab_exe}")
        update_yaml_config(matlab_yaml, "matlab_executable", matlab_exe, project_root, "allowed_work_dirs")
    else:
        print("[-] 未能在默认路径找到 MATLAB。MATLAB MCP 服务暂不可用，如需使用请自行安装。")
        update_yaml_config(matlab_yaml, "matlab_executable", "matlab", project_root, "allowed_work_dirs")
        
    # ANSYS
    ansys_exe = find_software_executable("ansys")
    ansys_yaml = os.path.join(script_dir, "mcp", "ansys-mcp-server", "config.yaml")
    if ansys_exe:
        print(f"[+] 找到 ANSYS: {ansys_exe}")
        update_yaml_config(ansys_yaml, "ansys_executable", ansys_exe, project_root, "allowed_directories")
    else:
        if sys.platform == "darwin":
            print("[-] macOS 系统暂无原生 ANSYS APDL 支持，ANSYS MCP 需通过远程/Docker方式调用。")
        else:
            print("[-] 未能在默认路径找到 ANSYS。ANSYS MCP 服务暂不可用，如需使用请自行安装。")
        update_yaml_config(ansys_yaml, "ansys_executable", "ansys", project_root, "allowed_directories")
```

- [ ] **Step 3: 测试并运行探测逻辑**

在终端中执行：
```bash
python .agents/setup_mcp.py
```
验证其能够正常探测 MATLAB 和 ANSYS 安装情况，并将探测结果及当前项目路径覆盖写入至 `.agents/mcp/*/config.yaml` 中。

---
