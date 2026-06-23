# setup/win/install_deps.ps1
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $ProjectRoot

function Install-IfNeeded ($Name, $Cmd, $WingetId) {
    $cmdObj = Get-Command $Cmd -ErrorAction SilentlyContinue
    $isFake = $false
    if ($cmdObj -and $cmdObj.Source -like "*WindowsApps\*") {
        $isFake = $true
    }
    if (-not $cmdObj -or $isFake) {
        Write-Host "Installing $Name via winget..." -ForegroundColor Yellow
        winget install -e --id $WingetId --accept-source-agreements --accept-package-agreements
        # Refresh Path to detect newly installed tool in current session
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
    }
}
Install-IfNeeded -Name "CMake" -Cmd "cmake" -WingetId "Kitware.CMake"
Install-IfNeeded -Name "Ninja" -Cmd "ninja" -WingetId "Ninja-build.Ninja"


# Check and install GCC (TDM-GCC 10.3.0)
$gcc = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $gcc) {
    if (Test-Path "C:\TDM-GCC-64\bin\gcc.exe") {
        Write-Host "TDM-GCC detected at C:\TDM-GCC-64\bin\gcc.exe but not in PATH. Adding to PATH..." -ForegroundColor Yellow
        $userPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
        if ($userPath -split ';' -notcontains "C:\TDM-GCC-64\bin") {
            [System.Environment]::SetEnvironmentVariable("Path", $userPath + ";C:\TDM-GCC-64\bin", "User")
        }
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User") + ";C:\TDM-GCC-64\bin"
    } else {
        Write-Host "TDM-GCC 10.3.0 is missing. Downloading installer from GitHub..." -ForegroundColor Yellow
        New-Item -ItemType Directory -Force -Path "C:\Installers" | Out-Null
        Invoke-WebRequest `
          -Uri "https://github.com/jmeubank/tdm-gcc/releases/download/v10.3.0-tdm64-2/tdm64-gcc-10.3.0-2.exe" `
          -OutFile "C:\Installers\tdm64-gcc-10.3.0-2.exe"
        Write-Host "Starting TDM-GCC installer. Please follow the instructions to install to C:\TDM-GCC-64." -ForegroundColor Yellow
        Start-Process "C:\Installers\tdm64-gcc-10.3.0-2.exe" -Wait
        # Refresh Path to detect the newly installed gcc
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User") + ";C:\TDM-GCC-64\bin"
    }
}

Install-IfNeeded -Name "uv" -Cmd "uv" -WingetId "astral-sh.uv"

# Check and install Python 3.12.13 via uv
$pythonCmd = Get-Command python -ErrorAction SilentlyContinue
$isFake = $false
if ($pythonCmd -and $pythonCmd.Source -like "*WindowsApps\*") {
    $isFake = $true
}
$isCorrectVersion = $false
if ($pythonCmd -and -not $isFake) {
    $version = & python --version
    if ($version -match "3\.12\.13") {
        $isCorrectVersion = $true
    }
}
if (-not $isCorrectVersion) {
    Write-Host "Python 3.12.13 is missing or version mismatch. Installing via uv..." -ForegroundColor Yellow
    uv python install 3.12.13 --default
    # Refresh Path to detect newly installed python
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
}

# 创建虚拟环境并安装依赖
if (-not (Test-Path ".venv")) {
    Write-Host "Creating python virtual environment..." -ForegroundColor Cyan
    python -m venv .venv
}
& ".venv\Scripts\Activate.ps1"
python -m pip install --upgrade pip --quiet
Get-ChildItem -Path . -Filter "requirements.txt" -Recurse | ForEach-Object {
    Write-Host "Installing dependencies from: $_" -ForegroundColor Cyan
    python -m pip install -r $_.FullName
}
python -m pip install mcp fastmcp

# 运行 MCP 配置工具
if (Test-Path ".agents\setup_mcp.py") {
    Write-Host "Registering GRPD MCP Servers..." -ForegroundColor Cyan
    python ".agents\setup_mcp.py"
}
