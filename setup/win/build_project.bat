@echo off
cd /d "%~dp0..\.."
echo Running CMake Configuration (Adaptive Presets)...

:: Try win-ninja Preset first
cmake --preset win-ninja
if %ERRORLEVEL% eq 0 (
    echo [INFO] Succeeded using Ninja Preset. Building GRPD...
    cmake --build build --preset win-ninja-build -j %NUMBER_OF_PROCESSORS%
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] GRPD Build Failed!
        exit /b 1
    )
) else (
    echo [INFO] Ninja Preset failed or not available. Trying MinGW Makefiles Preset...
    cmake --preset win-mingw
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] CMake Configuration Failed!
        exit /b 1
    )
    cmake --build build --preset win-mingw-build -j %NUMBER_OF_PROCESSORS%
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] GRPD Build Failed!
        exit /b 1
    )
)

echo Launching GRPD Engine...
if exist "bin\release\GRPD.exe" (
    bin\release\GRPD.exe
) else if exist "bin\GRPD.exe" (
    bin\GRPD.exe
) else (
    echo [ERROR] Executable GRPD.exe not found!
    exit /b 1
)
