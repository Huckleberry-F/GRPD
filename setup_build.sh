#!/bin/bash

# ==============================================================================
# GRPD One-Click Setup and Build Script (macOS/Linux)
# ==============================================================================

# 设置 UTF-8 编码，防止中文注释乱码
export LANG=en_US.UTF-8

echo "=============================================================="
echo "GRPD One-Click Setup and Build Script (macOS/Linux)"
echo "=============================================================="

# 1. 查找最合适的 Python 解释器并安装依赖
echo ""
echo "[1/4] Checking Python Dependencies..."

PYTHON_EXE=""

# 优先探测 Homebrew 常见的独立 Python 3.12 版本
if [ -f "/opt/homebrew/bin/python3.12" ]; then
    PYTHON_EXE="/opt/homebrew/bin/python3.12"
elif [ -f "/usr/local/bin/python3.12" ]; then
    PYTHON_EXE="/usr/local/bin/python3.12"
elif command -v python3 &> /dev/null; then
    PYTHON_EXE="python3"
elif command -v python &> /dev/null; then
    PYTHON_EXE="python"
fi

if [ -z "$PYTHON_EXE" ]; then
    echo "[ERROR] Python not found! Please install Python 3.10-3.12 first."
    exit 1
fi

echo "Using Python interpreter: $PYTHON_EXE"
echo "Installing Python dependencies from requirements.txt..."
$PYTHON_EXE -m pip install -r requirements.txt

if [ $? -ne 0 ]; then
    echo "[ERROR] Failed to install Python dependencies. Please ensure pip is installed."
    exit 1
fi

# 2. 检查编译器与 CMake
echo ""
echo "[2/4] Checking C++ Compiler and CMake..."

if ! command -v clang++ &> /dev/null && ! command -v g++ &> /dev/null; then
    echo "[ERROR] C++ compiler (clang++ or g++) not found!"
    echo "=> On macOS, please run 'xcode-select --install' to install command line tools."
    exit 1
fi

if ! command -v cmake &> /dev/null; then
    echo "[ERROR] CMake not found in PATH!"
    echo "=> On macOS, please run 'brew install cmake' to install CMake."
    exit 1
fi

# 3. 配置与构建 C++ 项目
echo ""
echo "[3/4] Configuring and Building GRPD (Release Mode)..."

if [ -d "build" ]; then
    echo "Build directory already exists. Reusing it..."
else
    mkdir build
fi

cd build

echo "Running CMake Configuration..."
cmake ..
if [ $? -ne 0 ]; then
    echo "[ERROR] CMake configuration failed!"
    exit 1
fi

# 获取 CPU 核心数用于并行编译
if [[ "$OSTYPE" == "darwin"* ]]; then
    CPU_CORES=$(sysctl -n hw.ncpu)
else
    CPU_CORES=$(nproc)
fi

echo ""
echo "Running CMake Build with $CPU_CORES threads..."
cmake --build . --config Release -j $CPU_CORES
if [ $? -ne 0 ]; then
    echo "[ERROR] Build failed!"
    exit 1
fi

# 4. 运行引擎
echo ""
echo "[4/4] Build Successful! Launching GRPD Engine..."
echo "=============================================================="
cd ..

EXEC_FILE="bin/release/GRPD"
# 兼容不同编译配置输出的路径
if [ ! -f "$EXEC_FILE" ] && [ -f "bin/GRPD" ]; then
    EXEC_FILE="bin/GRPD"
fi

if [ -f "$EXEC_FILE" ]; then
    ./$EXEC_FILE
else
    echo "[ERROR] Cannot find output executable GRPD!"
    exit 1
fi
