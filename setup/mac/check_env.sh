#!/bin/bash
# setup/mac/check_env.sh
echo "--- macOS Environment Checklist ---"

if command -v cmake &> /dev/null; then
    echo -e "\033[32m[√] CMake detected: $(which cmake)\033[0m"
else
    echo -e "\033[33m[!] CMake missing\033[0m"
fi

if brew list libomp &> /dev/null || [ -d "/opt/homebrew/opt/libomp" ] || [ -d "/usr/local/opt/libomp" ]; then
    echo -e "\033[32m[√] OpenMP (libomp) detected\033[0m"
else
    echo -e "\033[33m[!] OpenMP (libomp) missing. CMake build will fail.\033[0m"
fi

if command -v clang++ &> /dev/null; then
    echo -e "\033[32m[√] Clang++ Compiler detected: $(which clang++)\033[0m"
else
    echo -e "\033[33m[!] Clang++ Compiler (macOS Command Line Tools) missing\033[0m"
fi

if command -v python3 &> /dev/null; then
    PY_VER=$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}")' 2>/dev/null)
    if [ "$PY_VER" = "3.12.13" ]; then
        echo -e "\033[32m[√] Python detected (version $PY_VER): $(which python3)\033[0m"
    else
        echo -e "\033[33m[!] Python version mismatch (found $PY_VER, expected 3.12.13)\033[0m"
    fi
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
