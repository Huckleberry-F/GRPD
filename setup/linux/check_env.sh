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
