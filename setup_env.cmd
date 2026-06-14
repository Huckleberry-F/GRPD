@echo off
cd /d "%~dp0"
echo Launching environment configuration script...
powershell -NoProfile -ExecutionPolicy Bypass -File ".\setup_env.ps1"
echo.
pause
