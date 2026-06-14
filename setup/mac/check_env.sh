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
