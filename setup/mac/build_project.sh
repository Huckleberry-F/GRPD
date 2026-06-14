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
