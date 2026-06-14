@echo off
chcp 65001 >nul
echo ==============================================================
echo GRPD One-Click Setup & Build for Windows
echo ==============================================================
cd /d "%~dp0"

echo [1/3] Checking Environment...
powershell -NoProfile -ExecutionPolicy Bypass -File ".\check_env.ps1"

echo.
echo [2/3] Installing Dependencies & Creating Virtual Env...
powershell -NoProfile -ExecutionPolicy Bypass -File ".\install_deps.ps1"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Dependencies setup failed!
    pause
    exit /b 1
)

echo.
echo [3/3] Compiling Project & Running...
call .\build_project.bat
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build & Run failed!
    pause
    exit /b 1
)
echo ==============================================================
echo Process Completed Successfully!
echo ==============================================================
pause
