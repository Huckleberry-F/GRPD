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
            paths = glob.glob(r"C:\Program Files\MATLAB\R*\bin\matlab.exe")
            return paths[-1] if paths else None  # 取最新版本
        elif sys.platform == "darwin":
            paths = glob.glob("/Applications/MATLAB_R*.app/bin/matlab")
            return paths[-1] if paths else None
        else:
            paths = glob.glob("/usr/local/MATLAB/R*/bin/matlab")
            return paths[-1] if paths else None
    elif software_type == "ansys":
        if os.name == "nt":
            paths = glob.glob(r"C:\Program Files\ANSYS Inc\v*\ANSYS\bin\winx64\ansys*.exe")
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
        project_root = project_root.replace("\\", "/")
        
    if dir_key in config and isinstance(config[dir_key], list):
        if project_root not in config[dir_key]:
            config[dir_key].append(project_root)
    else:
        config[dir_key] = [project_root]
        
    with open(yaml_path, 'w', encoding='utf-8') as f:
        yaml.dump(config, f, allow_unicode=True, default_flow_style=False)

def main():
    print("==================================================")
    print(" GRPD MCP 服务自动配置工具 (setup_mcp.py)")
    print("==================================================")
    
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

    # 2. 检查依赖
    try:
        import mcp
        print("[+] Python 依赖包 (mcp) 检查通过。")
    except ImportError:
        print("[!] 警告: 未在当前 Python 环境中检测到 'mcp' 依赖库！")
        print("    建议在运行前先执行: pip install mcp fastmcp")
        print("    是否继续配置？ (y/n): ", end="")
        try:
            choice = input().strip().lower()
        except (KeyboardInterrupt, EOFError):
            choice = 'n'
        if choice != 'y':
            print("[-] 已取消配置。")
            return

    # 3. 读取 settings.json.example 模板
    example_path = os.path.join(script_dir, "settings.json.example")
    if not os.path.exists(example_path):
        print(f"[-] 错误: 找不到模板文件 {example_path}")
        return

    with open(example_path, "r", encoding="utf-8") as f:
        config_data = json.load(f)

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

    local_settings = {"mcpServers": updated_servers}
    local_settings_path = os.path.join(script_dir, "settings.json")
    
    with open(local_settings_path, "w", encoding="utf-8") as f:
        json.dump(local_settings, f, indent=2, ensure_ascii=False)
    print(f"[+] 已成功生成本地配置文件: {local_settings_path}")

    # 5. 生成全局 mcp_config.json
    # Antigravity 与 Antigravity IDE 都会读取 mcp_config.json。
    # 保留 cwd / timeout，确保依赖相对路径的 MCP 服务也能稳定启动。
    global_servers = {}
    for server_name, server_cfg in updated_servers.items():
        global_servers[server_name] = server_cfg.copy()
    global_mcp_config = {"mcpServers": global_servers}

    # 查找全局配置文件夹路径
    home_dir = os.path.expanduser("~")
    gemini_dir = os.path.join(home_dir, ".gemini")
    
    config_paths = [
        os.path.join(gemini_dir, "config", "mcp_config.json"),
        os.path.join(gemini_dir, "antigravity", "mcp_config.json"),
        os.path.join(gemini_dir, "antigravity-ide", "mcp_config.json"),
        os.path.join(gemini_dir, "antigravity-backup", "mcp_config.json")
    ]

    updated_any = False
    for config_path in config_paths:
        parent_dir = os.path.dirname(config_path)
        try:
            os.makedirs(parent_dir, exist_ok=True)
            with open(config_path, "w", encoding="utf-8") as f:
                json.dump(global_mcp_config, f, indent=2, ensure_ascii=False)
            print(f"[+] 已成功更新全局 IDE 配置文件: {config_path}")
            updated_any = True
        except Exception as e:
            print(f"[!] 无法写入 {config_path}: {e}")

    if not updated_any:
        # 如果未找到任何配置文件夹，则在新路径尝试创建
        default_dir = os.path.join(gemini_dir, "antigravity-ide")
        os.makedirs(default_dir, exist_ok=True)
        default_path = os.path.join(default_dir, "mcp_config.json")
        try:
            with open(default_path, "w", encoding="utf-8") as f:
                json.dump(global_mcp_config, f, indent=2, ensure_ascii=False)
            print(f"[+] 已成功创建默认的全局 IDE 配置文件: {default_path}")
        except Exception as e:
            print(f"[!] 创建全局配置文件失败: {e}")

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

    print("==================================================")
    print("[+] 配置完成！请重启您的 IDE 或重新加载 MCP 服务。")
    print("==================================================")

if __name__ == "__main__":
    main()
