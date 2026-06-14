# Multiplatform Setup and Build Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建一个结构化的 `setup` 文件夹，按照平台（win, mac, linux）划分不同的子目录，每个平台都实现环境检测、工具链/Python 虚拟环境与依赖安装、MCP 注册、以及 C++ 项目编译构建的平台原生命令脚本。

**Architecture:** 
1. 新建 `setup/` 文件夹，下设 `win/`、`mac/`、`linux/` 三个平台子文件夹。
2. 每一平台下包含：
   - `check_env` 脚本：只做环境探测（编译器、CMake、Python、MATLAB、ANSYS 等）并输出报告，不修改系统。
   - `install_deps` 脚本：负责补全工具链（Win 使用 winget，Unix 使用平台包管理器提示），创建项目 `.venv` 并安装 pip 依赖，之后运行 `.agents/setup_mcp.py` 完成本地与全局 IDE MCP 的注册配置。
   - `build_project` 脚本：执行 CMake 的 Release 模式多线程编译，并在成功后运行 GRPD 引擎可执行文件。
   - `setup_and_build` 主控脚本：顺序调用上述子脚本。
3. 根目录的旧构建脚本重定向到对应的平台子目录中，保证向后兼容性。

**Tech Stack:** Bash shell scripting, PowerShell, Windows Batch script, Python.

---

### Task 1: 建立 Windows 平台脚本 (setup/win/)

**Files:**
- Create: `setup/win/check_env.ps1`
- Create: `setup/win/install_deps.ps1`
- Create: `setup/win/build_project.bat`
- Create: `setup/win/setup_and_build.bat`

- [ ] **Step 1: 创建 `setup/win/check_env.ps1`**
  负责检测 Windows 下的 GCC, CMake, Python 并输出报告，同时自动搜寻 MATLAB 与 ANSYS 路径。
  ```powershell
  # setup/win/check_env.ps1
  $ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
  Write-Host "--- Windows Environment Checklist ---" -ForegroundColor Cyan
  
  $cmake = Get-Command cmake -ErrorAction SilentlyContinue
  if ($cmake) { Write-Host "[√] CMake detected: $($cmake.Source)" -ForegroundColor Green } else { Write-Warning "[!] CMake missing" }
  
  $gcc = Get-Command gcc -ErrorAction SilentlyContinue
  if ($gcc) { Write-Host "[√] GCC detected: $($gcc.Source)" -ForegroundColor Green } else { Write-Warning "[!] GCC compiler missing" }
  
  $python = Get-Command python -ErrorAction SilentlyContinue
  if ($python) { Write-Host "[√] Python detected: $($python.Source)" -ForegroundColor Green } else { Write-Warning "[!] Python missing" }
  
  # Detect MATLAB
  $matlabPath = ""
  $matlabRoots = @("C:\Program Files\MATLAB", "D:\Program Files\MATLAB")
  foreach ($root in $matlabRoots) {
      if (Test-Path $root) {
          $exes = Get-ChildItem $root -Recurse -Filter "matlab.exe" -ErrorAction SilentlyContinue
          if ($exes) { $matlabPath = $exes[0].FullName; break }
      }
  }
  if ($matlabPath) { Write-Host "[√] MATLAB detected: $matlabPath" -ForegroundColor Green } else { Write-Warning "[!] MATLAB not found" }
  
  # Detect ANSYS
  $ansysPath = ""
  $ansysEnv = Get-ChildItem Env: | Where-Object { $_.Name -like "AWP_ROOT*" }
  if ($ansysEnv) {
      $exePattern = Join-Path $ansysEnv[0].Value "ANSYS\bin\winx64\ansys*.exe"
      $found = Resolve-Path $exePattern -ErrorAction SilentlyContinue
      if ($found) { $ansysPath = $found[0].Path }
  }
  if ($ansysPath) { Write-Host "[√] ANSYS MAPDL detected: $ansysPath" -ForegroundColor Green } else { Write-Warning "[!] ANSYS MAPDL not found" }
  ```

- [ ] **Step 2: 创建 `setup/win/install_deps.ps1`**
  负责在工具缺失时通过 `winget` 自动安装，创建本地 `.venv`，升级 pip 并安装 `requirements.txt`，同时启动 `.agents/setup_mcp.py` 写入 IDE 级配置。
  ```powershell
  # setup/win/install_deps.ps1
  $ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
  cd $ProjectRoot
  
  function Install-IfNeeded ($Name, $Cmd, $WingetId) {
      if (-not (Get-Command $Cmd -ErrorAction SilentlyContinue)) {
          Write-Host "Installing $Name via winget..." -ForegroundColor Yellow
          winget install -e --id $WingetId --accept-source-agreements --accept-package-agreements
      }
  }
  Install-IfNeeded -Name "CMake" -Cmd "cmake" -WingetId "Kitware.CMake"
  Install-IfNeeded -Name "TDM-GCC" -Cmd "gcc" -WingetId "J-Meubank.TDM-GCC"
  Install-IfNeeded -Name "Python 3" -Cmd "python" -WingetId "Python.Python.3.12"
  
  # 创建虚拟环境并安装依赖
  if (-not (Test-Path ".venv")) {
      Write-Host "Creating python virtual environment..." -ForegroundColor Cyan
      python -m venv .venv
  }
  & ".venv\Scripts\Activate.ps1"
  python -m pip install --upgrade pip --quiet
  python -m pip install -r requirements.txt
  python -m pip install mcp fastmcp
  
  # 运行 MCP 配置工具
  if (Test-Path ".agents\setup_mcp.py") {
      Write-Host "Registering GRPD MCP Servers..." -ForegroundColor Cyan
      python ".agents\setup_mcp.py"
  }
  ```

- [ ] **Step 3: 创建 `setup/win/build_project.bat`**
  负责在 Windows 环境下进行 CMake 配置与 MinGW 构建，并在成功后启动程序。
  ```cmd
  @echo off
  cd /d "%~dp0..\.."
  echo Running CMake Configuration (MinGW)...
  if not exist "build" mkdir build
  cd build
  cmake -G "MinGW Makefiles" ..
  if %ERRORLEVEL% neq 0 (
      echo [ERROR] CMake Configuration Failed!
      exit /b 1
  )
  echo Building GRPD (Release Mode)...
  cmake --build . --config Release -j %NUMBER_OF_PROCESSORS%
  if %ERRORLEVEL% neq 0 (
      echo [ERROR] GRPD Build Failed!
      exit /b 1
  )
  echo Launching GRPD Engine...
  cd ..
  if exist "bin\release\GRPD.exe" (
      bin\release\GRPD.exe
  ) else if exist "bin\GRPD.exe" (
      bin\GRPD.exe
  ) else (
      echo [ERROR] Executable GRPD.exe not found!
      exit /b 1
  )
  ```

- [ ] **Step 4: 创建 `setup/win/setup_and_build.bat`**
  主控脚本，将上述步骤串联。
  ```cmd
  @echo off
  chcp 65001 >nul
  echo ==============================================================
  echo GRPD One-Click Setup & Build for Windows
  echo ==============================================================
  cd /d "%~dp0"
  
  echo [1/3] Checking Environment...
  powershell -NoProfile -ExecutionPolicy Bypass -File ".\check_env.ps1"
  
  echo.
  echo [2/3] Installing Dependencies & Creating Virtual Env...
  powershell -NoProfile -ExecutionPolicy Bypass -File ".\install_deps.ps1"
  if %ERRORLEVEL% neq 0 (
      echo [ERROR] Dependencies setup failed!
      pause
      exit /b 1
  )
  
  echo.
  echo [3/3] Compiling Project & Running...
  call .\build_project.bat
  if %ERRORLEVEL% neq 0 (
      echo [ERROR] Build & Run failed!
      pause
      exit /b 1
  )
  echo ==============================================================
  echo Process Completed Successfully!
  echo ==============================================================
  pause
  ```

---

### Task 2: 建立 macOS 平台脚本 (setup/mac/)

**Files:**
- Create: `setup/mac/check_env.sh`
- Create: `setup/mac/install_deps.sh`
- Create: `setup/mac/build_project.sh`
- Create: `setup/mac/setup_and_build.sh`

- [ ] **Step 1: 创建 `setup/mac/check_env.sh`**
  ```bash
  #!/bin/bash
  # setup/mac/check_env.sh
  echo "--- macOS Environment Checklist ---"
  
  if command -v cmake &> /dev/null; then
      echo -e "\033[32m[√] CMake detected: $(which cmake)\033[0m"
  else
      echo -e "\033[33m[!] CMake missing\033[0m"
  fi
  
  if command -v clang++ &> /dev/null || command -v g++ &> /dev/null; then
      echo -e "\033[32m[√] C++ Compiler detected\033[0m"
  else
      echo -e "\033[33m[!] C++ Compiler (clang++ or g++) missing\033[0m"
  fi
  
  if command -v python3 &> /dev/null; then
      echo -e "\033[32m[√] Python detected: $(which python3)\033[0m"
  else
      echo -e "\033[33m[!] Python 3 missing\033[0m"
  fi
  
  # Detect MATLAB
  MATLAB_EXE=""
  for app in /Applications/MATLAB_R202*.app; do
      if [ -f "$app/bin/matlab" ]; then
          MATLAB_EXE="$app/bin/matlab"
          break
      fi
  done
  if [ -n "$MATLAB_EXE" ]; then
      echo -e "\033[32m[√] MATLAB detected: $MATLAB_EXE\033[0m"
  else
      echo -e "\033[33m[!] MATLAB not found\033[0m"
  fi
  ```

- [ ] **Step 2: 创建 `setup/mac/install_deps.sh`**
  ```bash
  #!/bin/bash
  # setup/mac/install_deps.sh
  cd "$(dirname "$0")/../.."
  
  # 提示安装系统依赖
  if ! command -v cmake &> /dev/null; then
      echo "[!] CMake is required. Please install via Homebrew: brew install cmake"
  fi
  if ! command -v brew &> /dev/null; then
      echo "[!] Homebrew is not installed. Please install it to manage packages."
  fi
  
  # 配置 Python 虚拟环境
  if [ ! -d ".venv" ]; then
      echo "Creating Python virtual environment in .venv..."
      python3 -m venv .venv
  fi
  
  source .venv/bin/activate
  pip install --upgrade pip --quiet
  pip install -r requirements.txt
  pip install mcp fastmcp
  
  # 注册 MCP 服务
  if [ -f ".agents/setup_mcp.py" ]; then
      echo "Registering GRPD MCP Servers..."
      python3 .agents/setup_mcp.py
  fi
  ```

- [ ] **Step 3: 创建 `setup/mac/build_project.sh`**
  ```bash
  #!/bin/bash
  # setup/mac/build_project.sh
  cd "$(dirname "$0")/../.."
  
  echo "Configuring GRPD C++ Project..."
  mkdir -p build
  cd build
  cmake ..
  if [ $? -ne 0 ]; then
      echo "[ERROR] CMake Configuration Failed!"
      exit 1
  fi
  
  CPU_CORES=$(sysctl -n hw.ncpu)
  echo "Building GRPD (Release Mode) with $CPU_CORES threads..."
  cmake --build . --config Release -j $CPU_CORES
  if [ $? -ne 0 ]; then
      echo "[ERROR] GRPD Build Failed!"
      exit 1
  fi
  
  echo "Launching GRPD Engine..."
  cd ..
  if [ -f "bin/release/GRPD" ]; then
      ./bin/release/GRPD
  elif [ -f "bin/GRPD" ]; then
      ./bin/GRPD
  else
      echo "[ERROR] Executable GRPD not found!"
      exit 1
  fi
  ```

- [ ] **Step 4: 创建 `setup/mac/setup_and_build.sh`**
  ```bash
  #!/bin/bash
  # setup/mac/setup_and_build.sh
  chmod +x "$(dirname "$0")"/*.sh
  cd "$(dirname "$0")"
  
  echo "=============================================================="
  echo "GRPD One-Click Setup & Build for macOS"
  echo "=============================================================="
  
  echo "[1/3] Checking Environment..."
  ./check_env.sh
  
  echo ""
  echo "[2/3] Installing Dependencies & Virtual Env..."
  ./install_deps.sh
  if [ $? -ne 0 ]; then
      echo "[ERROR] Dependencies setup failed!"
      exit 1
  fi
  
  echo ""
  echo "[3/3] Compiling Project & Running..."
  ./build_project.sh
  if [ $? -ne 0 ]; then
      echo "[ERROR] Build & Run failed!"
      exit 1
  fi
  ```

---

### Task 3: 建立 Linux 平台脚本 (setup/linux/)

**Files:**
- Create: `setup/linux/check_env.sh`
- Create: `setup/linux/install_deps.sh`
- Create: `setup/linux/build_project.sh`
- Create: `setup/linux/setup_and_build.sh`

- [ ] **Step 1: 创建 `setup/linux/check_env.sh`**
  ```bash
  #!/bin/bash
  # setup/linux/check_env.sh
  echo "--- Linux Environment Checklist ---"
  
  if command -v cmake &> /dev/null; then
      echo -e "\033[32m[√] CMake detected: $(which cmake)\033[0m"
  else
      echo -e "\033[33m[!] CMake missing\033[0m"
  fi
  
  if command -v g++ &> /dev/null || command -v clang++ &> /dev/null; then
      echo -e "\033[32m[√] C++ Compiler detected\033[0m"
  else
      echo -e "\033[33m[!] C++ Compiler (g++ or clang++) missing\033[0m"
  fi
  
  if command -v python3 &> /dev/null; then
      echo -e "\033[32m[√] Python detected: $(which python3)\033[0m"
  else
      echo -e "\033[33m[!] Python 3 missing\033[0m"
  fi
  
  # Detect MATLAB
  MATLAB_EXE=""
  if [ -d "/usr/local/MATLAB" ]; then
      for version in /usr/local/MATLAB/R202*; do
          if [ -f "$version/bin/matlab" ]; then
              MATLAB_EXE="$version/bin/matlab"
              break
          fi
      done
  fi
  if [ -n "$MATLAB_EXE" ]; then
      echo -e "\033[32m[√] MATLAB detected: $MATLAB_EXE\033[0m"
  else
      echo -e "\033[33m[!] MATLAB not found\033[0m"
  fi
  ```

- [ ] **Step 2: 创建 `setup/linux/install_deps.sh`**
  ```bash
  #!/bin/bash
  # setup/linux/install_deps.sh
  cd "$(dirname "$0")/../.."
  
  # 提示安装系统依赖
  if ! command -v cmake &> /dev/null || ! command -v g++ &> /dev/null; then
      echo "[!] Missing build tools. Please install them first, e.g.:"
      echo "    sudo apt update && sudo apt install -y build-essential cmake libomp-dev python3 python3-venv"
  fi
  
  # 配置 Python 虚拟环境
  if [ ! -d ".venv" ]; then
      echo "Creating Python virtual environment in .venv..."
      python3 -m venv .venv
  fi
  
  source .venv/bin/activate
  pip install --upgrade pip --quiet
  pip install -r requirements.txt
  pip install mcp fastmcp
  
  # 注册 MCP 服务
  if [ -f ".agents/setup_mcp.py" ]; then
      echo "Registering GRPD MCP Servers..."
      python3 .agents/setup_mcp.py
  fi
  ```

- [ ] **Step 3: 创建 `setup/linux/build_project.sh`**
  ```bash
  #!/bin/bash
  # setup/linux/build_project.sh
  cd "$(dirname "$0")/../.."
  
  echo "Configuring GRPD C++ Project..."
  mkdir -p build
  cd build
  cmake ..
  if [ $? -ne 0 ]; then
      echo "[ERROR] CMake Configuration Failed!"
      exit 1
  fi
  
  CPU_CORES=$(nproc)
  echo "Building GRPD (Release Mode) with $CPU_CORES threads..."
  cmake --build . --config Release -j $CPU_CORES
  if [ $? -ne 0 ]; then
      echo "[ERROR] GRPD Build Failed!"
      exit 1
  fi
  
  echo "Launching GRPD Engine..."
  cd ..
  if [ -f "bin/release/GRPD" ]; then
      ./bin/release/GRPD
  elif [ -f "bin/GRPD" ]; then
      ./bin/GRPD
  else
      echo "[ERROR] Executable GRPD not found!"
      exit 1
  fi
  ```

- [ ] **Step 4: 创建 `setup/linux/setup_and_build.sh`**
  ```bash
  #!/bin/bash
  # setup/linux/setup_and_build.sh
  chmod +x "$(dirname "$0")"/*.sh
  cd "$(dirname "$0")"
  
  echo "=============================================================="
  echo "GRPD One-Click Setup & Build for Linux"
  echo "=============================================================="
  
  echo "[1/3] Checking Environment..."
  ./check_env.sh
  
  echo ""
  echo "[2/3] Installing Dependencies & Virtual Env..."
  ./install_deps.sh
  if [ $? -ne 0 ]; then
      echo "[ERROR] Dependencies setup failed!"
      exit 1
  fi
  
  echo ""
  echo "[3/3] Compiling Project & Running..."
  ./build_project.sh
  if [ $? -ne 0 ]; then
      echo "[ERROR] Build & Run failed!"
      exit 1
  fi
  ```

---

### Task 4: 整合根目录入口

**Files:**
- Modify: `setup_build.sh`
- Modify: `setup_build.cmd`
- Modify: `setup_env.cmd`

- [ ] **Step 1: 修改 `setup_build.sh`（直接判定平台并跳转）**
  ```bash
  #!/bin/bash
  if [[ "$OSTYPE" == "darwin"* ]]; then
      exec ./setup/mac/setup_and_build.sh
  else
      exec ./setup/linux/setup_and_build.sh
  fi
  ```

- [ ] **Step 2: 修改 `setup_build.cmd`**
  ```cmd
  @echo off
  call "%~dp0setup\win\setup_and_build.bat"
  ```

- [ ] **Step 3: 修改 `setup_env.cmd`**
  ```cmd
  @echo off
  cd /d "%~dp0"
  powershell -NoProfile -ExecutionPolicy Bypass -File ".\setup\win\install_deps.ps1"
  pause
  ```

---

## Verification Plan

### Manual Verification
1. 在 macOS 平台（当前系统）下执行 `./setup_build.sh`，检查：
   - 权限是否正确赋予；
   - 依赖检查输出是否高亮正常；
   - `.venv` 是否建立，且 pip 正常安装；
   - 能够正常运行 `setup_mcp.py` 写入 `settings.json`；
   - C++ 正常完成编译并启动 GRPD 主引擎。
2. 在 Windows 虚拟机/物理机上尝试双击运行 `setup_build.cmd`。
3. 在 Linux 环境下尝试运行 `setup_build.sh`。
