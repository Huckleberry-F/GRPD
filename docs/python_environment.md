# GRPD Python 环境配置说明

本文记录当前 Mac + Antigravity + GRPD 项目的 Python 配置方式。

## 当前目标

GRPD 项目使用项目内虚拟环境：

```text
/Users/hanbozhang/C++/GRPD/.venv/bin/python
```

本机终端默认优先使用 Homebrew 安装的 Python 3.12.13：

```text
/opt/homebrew/opt/python@3.12/libexec/bin/python
```

系统自带的 Python 3.9 没有删除，只是降低了查找优先级。

## 本机 PATH 配置

已在下面两个文件里加入 Homebrew Python 3.12 的路径：

```text
~/.zshrc
~/.zprofile
```

内容为：

```bash
export PATH="/opt/homebrew/opt/python@3.12/libexec/bin:/opt/homebrew/bin:$PATH"
```

这个路径很重要，因为 Homebrew 的 `python@3.12` 中，不带版本号的命令 `python`、`python3`、`pip`、`pip3` 位于：

```text
/opt/homebrew/opt/python@3.12/libexec/bin
```

验证方式：

```bash
which python
python --version
which python3
python3 --version
which pip
pip --version
```

期望看到 Python 3.12.13。

## GRPD 项目虚拟环境

GRPD 的 Python 包不再安装到系统全局环境，而是安装到项目内 `.venv`：

```text
/Users/hanbozhang/C++/GRPD/.venv
```

创建方式：

```bash
cd ~/C++/GRPD
/opt/homebrew/bin/python3.12 -m venv --copies .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

这里使用了 `--copies`，原因是普通虚拟环境里的 `python` 可能是符号链接，Antigravity 有时会把它解析回 Homebrew 的 Cellar 路径并报错。

激活虚拟环境：

- **macOS / Linux**:
  ```bash
  cd ~/C++/GRPD
  source .venv/bin/activate
  ```
- **Windows (PowerShell)**:
  ```powershell
  cd c:\C++\GRPD
  .\.venv\Scripts\Activate.ps1
  ```

激活后检查：

- **macOS / Linux**:
  ```bash
  which python
  python --version
  ```
- **Windows (PowerShell)**:
  ```powershell
  Get-Command python
  python --version
  ```

期望输出包含当前项目 `.venv` 虚拟环境下的 python 路径。

退出虚拟环境：

```bash
deactivate
```

## Python 包安装规则

以后 GRPD 项目需要的 Python 包，都应该在 `.venv` 激活后安装：

```bash
cd ~/C++/GRPD
source .venv/bin/activate
python -m pip install <package-name>
```

安装后如果需要记录依赖，更新：

```bash
python -m pip freeze > requirements-lock.txt
```

当前项目的主要依赖写在：

```text
requirements.txt
```

其中包括：

```text
numpy
pyyaml
open3d
pyvista
pymeshfix
matplotlib
pandas
scipy
pydantic
```

## Antigravity 配置

GRPD 工作区设置文件：

```text
.vscode/settings.json
```

当前设置指向项目虚拟环境。针对不同操作系统，配置方式如下：

- **macOS (Homebrew + osx 终端环境变量)**:
  ```json
  {
      "cmake.buildDirectory": "${workspaceFolder}/build",
      "python.defaultInterpreterPath": "${workspaceFolder}/.venv/bin/python",
      "python.interpreterPath": "${workspaceFolder}/.venv/bin/python",
      "terminal.integrated.env.osx": {
          "PATH": "${workspaceFolder}/.venv/bin:/opt/homebrew/opt/python@3.12/libexec/bin:/opt/homebrew/bin:${env:PATH}",
          "VIRTUAL_ENV": "${workspaceFolder}/.venv"
      }
  }
  ```

- **Windows (MinGW + windows 终端环境变量)**:
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

如果 Antigravity 让你选择 Python 解释器，选择项目内虚拟环境 `.venv` 下的解释器，不要选择全局系统解释器。

如果 Antigravity 仍然使用旧解释器，执行：

```text
Ctrl + Shift + P (Windows) 或 Cmd + Shift + P (macOS)
Developer: Reload Window
```

必要时完全退出 Antigravity 后重新打开 GRPD 项目。

## 已清理的旧环境

之前 GRPD 相关包曾安装到全局 Python 环境中，已经清理：

```text
Homebrew Python 3.12 全局环境
系统 Python 3.9 的用户级 site-packages
```

Homebrew Python 3.12 全局环境现在应只保留基础工具：

```text
pip
setuptools
wheel
```

检查命令：

```bash
/opt/homebrew/bin/python3.12 -m pip list
```

系统 Python 3.9 用户级包检查：

```bash
/usr/bin/python3 -m pip list --user
```

如果这里再次出现 `open3d`、`scipy`、`numpy` 等 GRPD 包，说明又装到了错误位置。

## 验证命令

验证 GRPD 项目虚拟环境：

```bash
cd ~/C++/GRPD
.venv/bin/python -c "import sys; print(sys.executable); print(sys.version)"
.venv/bin/python -c "import numpy, yaml, open3d, pyvista, pymeshfix, matplotlib, pandas, scipy, pydantic; print('GRPD .venv imports OK')"
```

期望输出包含：

```text
/Users/hanbozhang/C++/GRPD/.venv/bin/python
Python 3.12.13
GRPD .venv imports OK
```

## C++ 和 CMake 说明

这个 Python 环境只影响 Python 脚本和 Antigravity 的 Python 解释器选择。

GRPD 的 C++ 编译仍然使用 macOS 的 C++ 工具链：

```bash
clang++ --version
cmake --version
ninja --version
```
