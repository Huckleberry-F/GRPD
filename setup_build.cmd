@echo off
chcp 65001 >nul
echo ==============================================================
echo GRPD One-Click Setup and Build Script (Windows)
echo ==============================================================

echo.
echo [1/4] Setting up Python Virtual Environment ^& Dependencies...

REM ---- 1a. 探测系统 Python 解释器 ----
where python >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Python not found in PATH!
    echo =^> Please install Python 3.10-3.12 and ensure it is added to PATH.
    pause
    exit /b 1
)

for /f "delims=" %%V in ('python --version 2^>^&1') do set PYVER=%%V
echo Detected system Python: %PYVER%

REM ---- 1b. 自动创建虚拟环境（如果尚不存在） ----
if not exist ".venv" (
    echo Creating virtual environment in .venv ...
    python -m venv .venv
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] Failed to create virtual environment!
        echo =^> Please ensure the 'venv' module is available.
        pause
        exit /b 1
    )
    echo Virtual environment created successfully.
) else (
    echo Virtual environment already exists at .venv, reusing it.
)

REM ---- 1c. 激活虚拟环境 ----
call .venv\Scripts\activate.bat
echo Activated virtual environment: .venv

REM ---- 1d. 升级 pip 并安装依赖 ----
echo Upgrading pip...
python -m pip install --upgrade pip --quiet

echo Installing Python dependencies from requirements.txt...
python -m pip install -r requirements.txt
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to install Python dependencies.
    pause
    exit /b 1
)

echo All Python dependencies installed successfully.

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
