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
