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
