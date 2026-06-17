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
if ! brew list libomp &> /dev/null && [ ! -d "/opt/homebrew/opt/libomp" ] && [ ! -d "/usr/local/opt/libomp" ]; then
    echo "[!] OpenMP (libomp) is required on macOS. Please install via Homebrew: brew install libomp"
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
