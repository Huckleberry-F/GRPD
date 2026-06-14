# 自动配置与通用化环境部署实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现 GRPD 项目的 VS Code 配置通用化，并开发一键式 Windows 开发环境自动化部署脚本，确保在换电脑时能够自动配置 Python、CMake、TDM-GCC 编译链以及 MCP 服务端口，彻底解决 CMake 生成器无法识别的问题。

**Architecture:** 
1. **通用化配置**：移除非通用的绝对路径，使用变量占位符或自适应逻辑。
2. **自动化部署脚本 (`setup_env.ps1` / `setup_env.cmd`)**：利用 `winget` 自动补充安装 CMake、TDM-GCC、Python 等必备软件；获取当前系统的 Python 绝对路径及项目物理路径，动态写入 `.vscode/settings.json` 及 `.agents/settings.json` 中。
3. **整合编译脚本 (`setup_build.cmd`)**：整合环境检测，实现真正的“一键双击配置并编译”。

**Tech Stack:** PowerShell, Windows Batch Script, VS Code Workspace Settings, JSON Parsing, CMake, winget

---

### Task 1: 优化并通用化 VS Code 与 MCP 配置文件模板

**Files:**
- Modify: `.vscode/settings.json` 移除非通用的用户名和 Python 绝对路径。
- New: `.agents/settings.template.json` [NEW] 提供包含占位符的 MCP 服务通用模板。

- [ ] **Step 1: 修改 `.vscode/settings.json`**
  
  修改 [.vscode/settings.json](file:///d:/C++pro/GRPD/.vscode/settings.json)，移除对 `zhanghanbo` 用户的硬编码，将其设置为通用的 `"python"` 并在需要时由脚本自动重写。同时加入 CMake Tools 的通用生成器设置。
  
  ```json
  {
      "terminal.integrated.wordWrap": "on",
      "extensions.verifySignature": false,
      "python.defaultInterpreterPath": "python",
      "cmake.generator": "MinGW Makefiles",
      "cmake.configureSettings": {
          "CMAKE_MAKE_PROGRAM": "mingw32-make.exe"
      }
  }
  ```

- [ ] **Step 2: 创建 MCP 服务模板 `.agents/settings.template.json`**
  
  新建通用模板 [.agents/settings.template.json](file:///d:/C++pro/GRPD/.agents/settings.template.json)，其中 Python 路径使用 `{{PYTHON_PATH}}` 占位，项目根目录使用 `{{PROJECT_ROOT}}` 占位。
  
  ```json
  {
    "mcpServers": {
      "ansys-server": {
        "command": "{{PYTHON_PATH}}",
        "args": [
          "{{PROJECT_ROOT}}\\.agents\\mcp\\ansys-mcp-server\\server.py"
        ],
        "cwd": "{{PROJECT_ROOT}}\\.agents\\mcp\\ansys-mcp-server",
        "timeout": 3600000
      },
      "grpd-server": {
        "command": "{{PYTHON_PATH}}",
        "args": [
          "{{PROJECT_ROOT}}\\.agents\\mcp\\grpd-mcp-server\\server.py"
        ],
        "cwd": "{{PROJECT_ROOT}}\\.agents\\mcp\\grpd-mcp-server",
        "timeout": 3600000
      },
      "grpd-experiment-server": {
        "command": "{{PYTHON_PATH}}",
        "args": [
          "{{PROJECT_ROOT}}\\.agents\\mcp\\grpd-experiment-mcp-server\\server.py"
        ],
        "cwd": "{{PROJECT_ROOT}}\\.agents\\mcp\\grpd-experiment-mcp-server",
        "timeout": 3600000
      },
      "grpd-validation-server": {
        "command": "{{PYTHON_PATH}}",
        "args": [
          "{{PROJECT_ROOT}}\\.agents\\mcp\\grpd-validation-mcp-server\\server.py"
        ],
        "cwd": "{{PROJECT_ROOT}}\\.agents\\mcp\\grpd-validation-mcp-server",
        "timeout": 3600000
      },
      "matlab-server": {
        "command": "{{PYTHON_PATH}}",
        "args": [
          "{{PROJECT_ROOT}}\\.agents\\mcp\\matlab-mcp-server\\server.py"
        ],
        "cwd": "{{PROJECT_ROOT}}\\.agents\\mcp\\matlab-mcp-server",
        "timeout": 3600000
      }
    }
  }
  ```

---

### Task 2: 编写一键自动化部署脚本 `setup_env.ps1` 和入口 `setup_env.cmd`

**Files:**
- New: `setup_env.ps1` [NEW] 提供环境自检、一键 winget 安装以及动态 JSON 渲染功能。
- New: `setup_env.cmd` [NEW] 包装 PowerShell 脚本，提供双击一键执行的友好入口。

- [ ] **Step 1: 创建 `setup_env.ps1` 核心自动化脚本**
  
  新建 [setup_env.ps1](file:///d:/C++pro/GRPD/setup_env.ps1)，利用 PowerShell 进行依赖扫描、Winget 自动静默安装，并自动重写配置：
  
  ```powershell
  # Windows GRPD 环境一键初始化脚本 (PowerShell)
  # 解决换电脑后 Python/CMake/GCC 路径及 VS Code/MCP 配置不通用的问题
  
  $Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
  $ProjectRoot = Get-Item . | Select-Object -ExpandProperty FullName
  $VscodeSettings = Join-Path $ProjectRoot ".vscode\settings.json"
  $McpSettings = Join-Path $ProjectRoot ".agents\settings.json"
  $McpTemplate = Join-Path $ProjectRoot ".agents\settings.template.json"
  
  Write-Host "====================================================" -ForegroundColor Cyan
  Write-Host " GRPD Windows 环境自动配置与依赖安装脚本" -ForegroundColor Cyan
  Write-Host "====================================================" -ForegroundColor Cyan
  Write-Host "项目根目录: $ProjectRoot`n"
  
  # 1. 检测与安装必备 C++ 编译器与构建工具 (MSYS2/TDM-GCC, CMake)
  function Check-And-Install {
      param (
          [string]$Name,
          [string]$Command,
          [string]$WingetId
      )
      
      $installed = Get-Command $Command -ErrorAction SilentlyContinue
      if ($installed) {
          Write-Host "[√] 已检测到 $Name, 路径: $($installed.Source)" -ForegroundColor Green
      } else {
          Write-Host "[!] 未检测到 $Name。正在通过 winget 自动安装..." -ForegroundColor Yellow
          winget install -e --id $WingetId --accept-source-agreements --accept-package-agreements
          if ($LASTEXITCODE -ne 0) {
              Write-Warning "通过 winget 安装 $Name 失败，请稍后手动安装并将其加入 PATH 环境变量。"
          } else {
              Write-Host "[√] $Name 安装请求已提交！请确保重启终端/IDE 以更新 PATH 环境变量。" -ForegroundColor Green
          }
      }
  }
  
  # 检查 CMake
  Check-And-Install -Name "CMake" -Command "cmake" -WingetId "Kitware.CMake"
  
  # 检查 GCC (TDM-GCC)
  Check-And-Install -Name "TDM-GCC 编译器" -Command "gcc" -WingetId "J-Meubank.TDM-GCC"
  
  # 检查 Python
  Check-And-Install -Name "Python 3" -Command "python" -WingetId "Python.Python.3.12"
  
  # 2. 物理获取当前系统下实际的 Python 路径
  $PythonCmd = Get-Command python -ErrorAction SilentlyContinue
  if ($PythonCmd) {
      $PythonRealPath = $PythonCmd.Source
  } else {
      # 降级尝试默认安装路径
      $PythonRealPath = "C:\Users\$env:USERNAME\AppData\Local\Programs\Python\Python312\python.exe"
  }
  Write-Host "当前 Python 解释器实际路径: $PythonRealPath" -ForegroundColor Cyan
  
  # 3. 安装 Python 核心三方库
  Write-Host "`n安装 Python 计算与后处理依赖包..." -ForegroundColor Cyan
  python -m pip install --upgrade pip
  python -m pip install open3d numpy pyyaml pydantic
  
  # 4. 自适应更新 .vscode/settings.json
  Write-Host "`n更新 .vscode/settings.json 配置..." -ForegroundColor Cyan
  if (Test-Path $VscodeSettings) {
      $jsonContent = Get-Content $VscodeSettings -Raw | ConvertFrom-Json
      $jsonContent.python.defaultInterpreterPath = $PythonRealPath
      $jsonContent | ConvertTo-Json -Depth 10 | Set-Content -Path $VscodeSettings -Encoding utf8
      Write-Host "[√] 成功将 .vscode/settings.json 中的 Python 路径重写为当前系统的实际路径。" -ForegroundColor Green
  } else {
      Write-Warning ".vscode/settings.json 不存在，请先创建它。"
  }
  
  # 5. 自适应渲染 .agents/settings.json 模板
  Write-Host "`n更新 .agents/settings.json (MCP 注册表配置)..." -ForegroundColor Cyan
  if (Test-Path $McpTemplate) {
      # 读取模板，替换占位符
      $templateContent = Get-Content $McpTemplate -Raw
      # 将反斜杠进行双反斜杠转义，防止破坏 JSON 语法
      $EscapedPythonPath = $PythonRealPath -replace '\\', '\\\\'
      $EscapedProjectRoot = $ProjectRoot -replace '\\', '\\\\'
      
      $renderedContent = $templateContent -replace '\{\{PYTHON_PATH\}\}', $EscapedPythonPath
      $renderedContent = $renderedContent -replace '\{\{PROJECT_ROOT\}\}', $EscapedProjectRoot
      
      # 物理写入 settings.json
      [IO.File]::WriteAllText($McpSettings, $renderedContent, $Utf8NoBom)
      Write-Host "[√] 成功从模板渲染并重写了 .agents/settings.json！" -ForegroundColor Green
  } else {
      Write-Warning "未找到 .agents/settings.template.json 模板文件，无法重写 MCP 配置。"
  }
  
  Write-Host "`n====================================================" -ForegroundColor Green
  Write-Host " 一键环境配置完成！请重启 VS Code / 终端以应用最新 PATH。" -ForegroundColor Green
  Write-Host "====================================================" -ForegroundColor Green
  ```

- [ ] **Step 2: 创建双击运行入口 `setup_env.cmd`**
  
  新建 [setup_env.cmd](file:///d:/C++pro/GRPD/setup_env.cmd)，使用 Bypass 策略提升权限或跳过限制执行 PowerShell 脚本。
  
  ```cmd
  @echo off
  chcp 65001 >nul
  cd /d "%~dp0"
  
  echo 正在以 Bypass 权限启动环境自动化部署脚本...
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0\setup_env.ps1"
  
  echo.
  pause
  ```

---

### Task 3: 整合并优化 `setup_build.cmd`

**Files:**
- Modify: `setup_build.cmd` 整合环境自检。

- [ ] **Step 1: 修改 `setup_build.cmd`**
  
  在 [setup_build.cmd](file:///d:/C++pro/GRPD/setup_build.cmd) 头部加上环境自检逻辑，优先确保 `setup_env.ps1` 执行，再进行 CMake 配置，保证无论任何新电脑都能无阻碍配置与编译。
  
  ```diff
  -@echo off
  -chcp 65001 >nul
  -echo ==============================================================
  -echo GRPD One-Click Setup and Build Script (Windows)
  -echo ==============================================================
  -
  -echo.
  -echo [1/4] Checking Python Dependencies for High-Performance Voxelization...
  -python -m pip install open3d numpy pyyaml pydantic
  -if %ERRORLEVEL% neq 0 (
  -    echo [ERROR] Failed to install Python dependencies. Please make sure Python 3.10-3.12 is installed and added to PATH.
  -    pause
  -    exit /b 1
  -)
  -
  -echo.
  -echo [2/4] Checking C++ Compiler (GCC) and CMake...
  -where gcc >nul 2>nul
  -if %ERRORLEVEL% neq 0 (
  -    echo [ERROR] GCC compiler not found in PATH!
  -    echo =^> Opening TDM-GCC download page... Please download, install it, and ensure you check "Add to PATH".
  -    start https://jmeubank.github.io/tdm-gcc/download/
  -    pause
  -    exit /b 1
  -)
  -
  -where cmake >nul 2>nul
  -if %ERRORLEVEL% neq 0 (
  -    echo [ERROR] CMake not found in PATH!
  -    echo =^> Opening CMake download page... Please install it and check "Add to PATH for all users".
  -    start https://cmake.org/download/
  -    pause
  -    exit /b 1
  -)
  +@echo off
  +chcp 65001 >nul
  +echo ==============================================================
  +echo GRPD One-Click Setup and Build Script (Windows)
  +echo ==============================================================
  +
  +echo.
  +echo [1/4] Checking Environment Setup...
  +where gcc >nul 2>nul
  +if %ERRORLEVEL% neq 0 (
  +    echo [WARNING] GCC compiler not found in PATH! Attempting to launch env auto-setup...
  +    call setup_env.cmd
  +)
  +
  +where cmake >nul 2>nul
  +if %ERRORLEVEL% neq 0 (
  +    echo [WARNING] CMake not found in PATH! Attempting to launch env auto-setup...
  +    call setup_env.cmd
  +)
  +
  +:: 确保 Python 依赖等项运行
  +python -c "import open3d, numpy, yaml, pydantic" 2>nul
  +if %ERRORLEVEL% neq 0 (
  +    echo [WARNING] Missing python dependencies! Attempting to launch env auto-setup...
  +    call setup_env.cmd
  +)
  ```

---

## Verification Plan

### Manual Verification
1. **测试模板生成**：手动运行 `setup_env.cmd`，验证是否能在当前工作区正确自动生成/更新：
   * `.vscode/settings.json`（确认里面的 `python.defaultInterpreterPath` 指向当前的 Python 真实路径）。
   * `.agents/settings.json`（确认里面所有的 `command` 和 `args` / `cwd` 都指向当前的 Python 路径以及项目实际根目录）。
2. **测试一键编译**：运行 `setup_build.cmd`，确保 CMake 配置与 Build 流程一次性通过。
3. **测试 CMake Tools 在 VS Code 中激活**：打开项目后，按下 `Ctrl + Shift + P` 执行 `CMake: Configure`，验证是否还会报错“无法确定要使用的 CMake 生成器”（应该能够自动检测并使用 `MinGW Makefiles` 并且完美配置）。
