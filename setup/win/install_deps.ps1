# setup/win/install_deps.ps1
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
cd $ProjectRoot

function Install-IfNeeded ($Name, $Cmd, $WingetId) {
    if (-not (Get-Command $Cmd -ErrorAction SilentlyContinue)) {
        Write-Host "Installing $Name via winget..." -ForegroundColor Yellow
        winget install -e --id $WingetId --accept-source-agreements --accept-package-agreements
    }
}
Install-IfNeeded -Name "CMake" -Cmd "cmake" -WingetId "Kitware.CMake"
Install-IfNeeded -Name "TDM-GCC" -Cmd "gcc" -WingetId "J-Meubank.TDM-GCC"
Install-IfNeeded -Name "Python 3" -Cmd "python" -WingetId "Python.Python.3.12"

# 创建虚拟环境并安装依赖
if (-not (Test-Path ".venv")) {
    Write-Host "Creating python virtual environment..." -ForegroundColor Cyan
    python -m venv .venv
}
& ".venv\Scripts\Activate.ps1"
python -m pip install --upgrade pip --quiet
python -m pip install -r requirements.txt
python -m pip install mcp fastmcp

# 运行 MCP 配置工具
if (Test-Path ".agents\setup_mcp.py") {
    Write-Host "Registering GRPD MCP Servers..." -ForegroundColor Cyan
    python ".agents\setup_mcp.py"
}
