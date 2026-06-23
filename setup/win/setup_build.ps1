# setup/win/setup_build.ps1
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $ProjectRoot

Write-Host "==============================================================" -ForegroundColor Green
Write-Host "GRPD One-Click Setup & Build Utility (Windows PowerShell)" -ForegroundColor Green
Write-Host "==============================================================" -ForegroundColor Green

# 1. Check environment
Write-Host "`n[1/3] Checking local developer environment..." -ForegroundColor Cyan
& ".\setup\win\check_env.ps1"

# 2. Virtual environment and dependencies
Write-Host "`n[2/3] Preparing Python virtual environment & requirements..." -ForegroundColor Cyan
& ".\setup\win\install_deps.ps1"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Dependencies installation failed! Please check the logs above."
    Read-Host "Press Enter to exit..."
    Exit 1
}

# 3. Compile C++ Engine using CMake Presets
Write-Host "`n[3/3] Compiling C++ Core Engine (Adaptive CMake Presets)..." -ForegroundColor Cyan

# Try Ninja Preset
$ninjaSuccess = $false
Write-Host "Configuring CMake project via Ninja Preset..." -ForegroundColor Gray
cmake --preset win-ninja
if ($LASTEXITCODE -eq 0) {
    Write-Host "[INFO] Ninja configure succeeded. Compiling via Ninja Preset..." -ForegroundColor Green
    cmake --build build --preset win-ninja-build
    if ($LASTEXITCODE -eq 0) {
        $ninjaSuccess = $true
    }
}

if (-not $ninjaSuccess) {
    Write-Host "[INFO] Ninja build failed or not available. Trying MinGW Makefiles Preset..." -ForegroundColor Yellow
    cmake --preset win-mingw
    if ($LASTEXITCODE -ne 0) {
        Write-Error "MinGW Makefiles Preset configuration failed!"
        Read-Host "Press Enter to exit..."
        Exit 1
    }
    cmake --build build --preset win-mingw-build
    if ($LASTEXITCODE -ne 0) {
        Write-Error "MinGW Makefiles Preset compilation failed!"
        Read-Host "Press Enter to exit..."
        Exit 1
    }
}

Write-Host "`n==============================================================" -ForegroundColor Green
Write-Host "[SUCCESS] GRPD build & setup completed successfully!" -ForegroundColor Green
Write-Host "==============================================================" -ForegroundColor Green
Read-Host "Press Enter to exit..."
