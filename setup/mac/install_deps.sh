#!/bin/bash
# setup/mac/install_deps.sh
cd "$(dirname "$0")/../.."

# 提示与安装系统依赖
if ! command -v brew &> /dev/null; then
    echo "[!] Homebrew is not installed. Please install it to manage packages."
fi
if ! command -v clang++ &> /dev/null; then
    echo "[!] Clang++ is missing. Installing macOS Command Line Tools..."
    xcode-select --install
fi
if ! command -v cmake &> /dev/null; then
    echo "Installing CMake via Homebrew..."
    brew install cmake
fi
if ! brew list libomp &> /dev/null && [ ! -d "/opt/homebrew/opt/libomp" ] && [ ! -d "/usr/local/opt/libomp" ]; then
    echo "Installing libomp via Homebrew..."
    brew install libomp
fi

# 安装 uv
if ! command -v uv &> /dev/null; then
    echo "Installing uv..."
    if command -v brew &> /dev/null; then
        brew install uv
    else
        curl -LsSf https://astral.sh/uv/install.sh | sh
        export PATH="$HOME/.local/bin:$PATH"
    fi
fi

# 用 uv 安装 Python 3.12.13
IS_CORRECT_VERSION=false
if command -v python3 &> /dev/null; then
    PY_VER=$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}")' 2>/dev/null)
    if [ "$PY_VER" = "3.12.13" ]; then
        IS_CORRECT_VERSION=true
    fi
fi

if [ "$IS_CORRECT_VERSION" = false ]; then
    echo "Python 3.12.13 is missing or version mismatch. Installing via uv..."
    uv python install 3.12.13 --default
    export PATH="$HOME/.local/bin:$PATH"
fi

# 配置 Python 虚拟环境
if [ ! -d ".venv" ]; then
    echo "Creating Python virtual environment in .venv..."
    python3 -m venv .venv
fi

source .venv/bin/activate
pip install --upgrade pip --quiet
find . -name "requirements.txt" | while read -r req; do
    echo "Installing dependencies from: $req"
    pip install -r "$req"
done
pip install mcp fastmcp

# 注册 MCP 服务
if [ -f ".agents/setup_mcp.py" ]; then
    echo "Registering GRPD MCP Servers..."
    python3 .agents/setup_mcp.py
fi
