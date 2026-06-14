@echo off
cd /d "%~dp0..\.."
echo Running CMake Configuration (MinGW)...
if not exist "build" mkdir build
cd build
cmake -G "MinGW Makefiles" ..
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake Configuration Failed!
    exit /b 1
)
echo Building GRPD (Release Mode)...
cmake --build . --config Release -j %NUMBER_OF_PROCESSORS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] GRPD Build Failed!
    exit /b 1
)
echo Launching GRPD Engine...
cd ..
if exist "bin\release\GRPD.exe" (
    bin\release\GRPD.exe
) else if exist "bin\GRPD.exe" (
    bin\GRPD.exe
) else (
    echo [ERROR] Executable GRPD.exe not found!
    exit /b 1
)
