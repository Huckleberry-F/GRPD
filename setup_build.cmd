@echo off
chcp 65001 >nul
echo ==============================================================
echo GRPD One-Click Setup and Build Script (Windows)
echo ==============================================================

echo.
echo [1/4] Checking Python Dependencies for High-Performance Voxelization...
python -m pip install open3d numpy pyyaml pydantic
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to install Python dependencies. Please make sure Python 3.10-3.12 is installed and added to PATH.
    pause
    exit /b 1
)

echo.
echo [2/4] Checking C++ Compiler (GCC) and CMake...
where gcc >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] GCC compiler not found in PATH!
    echo =^> Opening TDM-GCC download page... Please download, install it, and ensure you check "Add to PATH".
    start https://jmeubank.github.io/tdm-gcc/download/
    pause
    exit /b 1
)

where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake not found in PATH!
    echo =^> Opening CMake download page... Please install it and check "Add to PATH for all users".
    start https://cmake.org/download/
    pause
    exit /b 1
)

echo.
echo [3/4] Configuring and Building GRPD (Release Mode)...
if not exist "build" mkdir build
cd build

echo Running CMake Configuration...
cmake -G "MinGW Makefiles" ..
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed!
    pause
    exit /b 1
)

echo.
echo Running CMake Build...
cmake --build . --config Release -j %NUMBER_OF_PROCESSORS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo [4/4] Build Successful! Launching GRPD Engine...
echo ==============================================================
cd ..
if exist "bin\release\GRPD.exe" (
    cd bin\release
    GRPD.exe
) else (
    echo [ERROR] Cannot find output executable GRPD.exe!
)
pause
