@echo off
chcp 65001 >nul
echo ==============================================================
echo GRPD One-Click Setup and Build Script (Windows)
echo ==============================================================

echo.
echo [1/4] Checking Environment Setup...
where gcc >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [WARNING] GCC compiler not found in PATH! Attempting to launch env auto-setup...
    call setup_env.cmd
)

where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [WARNING] CMake not found in PATH! Attempting to launch env auto-setup...
    call setup_env.cmd
)

:: 确保 Python 依赖等项运行
python -c "import open3d, numpy, yaml, pydantic" 2>nul
if %ERRORLEVEL% neq 0 (
    echo [WARNING] Missing python dependencies! Attempting to launch env auto-setup...
    call setup_env.cmd
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
