# Windows GRPD Environment One-Click Setup Script (PowerShell)
# Solves the issue of non-portable Python/CMake/GCC paths and VS Code/MCP configs

$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$ProjectRoot = Get-Item . | Select-Object -ExpandProperty FullName
$VscodeSettings = Join-Path $ProjectRoot ".vscode\settings.json"
$McpSettings = Join-Path $ProjectRoot ".agents\settings.json"
$McpTemplate = Join-Path $ProjectRoot ".agents\settings.template.json"

Write-Host "====================================================" -ForegroundColor Cyan
Write-Host " GRPD Windows Environment Auto-Configuration Script" -ForegroundColor Cyan
Write-Host "====================================================" -ForegroundColor Cyan
Write-Host "Project Root: $ProjectRoot`n"

# 1. Check and install C++ compiler & tools (MSYS2/TDM-GCC, CMake)
function Check-And-Install {
    param (
        [string]$Name,
        [string]$Command,
        [string]$WingetId
    )
    
    $installed = Get-Command $Command -ErrorAction SilentlyContinue
    if ($installed) {
        Write-Host "[√] Detected $Name, Path: $($installed.Source)" -ForegroundColor Green
    } else {
        Write-Host "[!] $Name not found. Installing via winget..." -ForegroundColor Yellow
        winget install -e --id $WingetId --accept-source-agreements --accept-package-agreements
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Failed to install $Name via winget. Please install manually and add to PATH."
        } else {
            Write-Host "[√] $Name installation request submitted! Restart IDE/terminal after install to update PATH." -ForegroundColor Green
        }
    }
}

# Check CMake
Check-And-Install -Name "CMake" -Command "cmake" -WingetId "Kitware.CMake"

# Check GCC (TDM-GCC)
Check-And-Install -Name "TDM-GCC Compiler" -Command "gcc" -WingetId "J-Meubank.TDM-GCC"

# Check Python
Check-And-Install -Name "Python 3" -Command "python" -WingetId "Python.Python.3.12"

# 2. Get real Python interpreter path
$PythonCmd = Get-Command python -ErrorAction SilentlyContinue
if ($PythonCmd) {
    $PythonRealPath = $PythonCmd.Source
} else {
    $PythonRealPath = "C:\Users\$env:USERNAME\AppData\Local\Programs\Python\Python312\python.exe"
}
Write-Host "Real Python Path: $PythonRealPath" -ForegroundColor Cyan

# 3. Install Python libraries
Write-Host "`nInstalling Python dependencies for GRPD..." -ForegroundColor Cyan
python -m pip install --upgrade pip
python -m pip install open3d numpy pyyaml pydantic

# 4. Update .vscode/settings.json
Write-Host "`nUpdating .vscode/settings.json..." -ForegroundColor Cyan
if (Test-Path $VscodeSettings) {
    try {
        $jsonContent = Get-Content $VscodeSettings -Encoding utf8 -Raw | ConvertFrom-Json
        $jsonContent.'python.defaultInterpreterPath' = $PythonRealPath
        $jsonContent | ConvertTo-Json -Depth 10 | Set-Content -Path $VscodeSettings -Encoding utf8
        Write-Host "[√] Successfully updated python.defaultInterpreterPath in .vscode/settings.json" -ForegroundColor Green
    } catch {
        Write-Warning "Failed to parse or write .vscode/settings.json: $_"
    }
} else {
    Write-Warning ".vscode/settings.json not found."
}

# 5. Render .agents/settings.json from template
Write-Host "`nUpdating .agents/settings.json (MCP configurations)..." -ForegroundColor Cyan
if (Test-Path $McpTemplate) {
    try {
        $templateContent = Get-Content $McpTemplate -Encoding utf8 -Raw
        $EscapedPythonPath = $PythonRealPath -replace '\\', '\\\\'
        $EscapedProjectRoot = $ProjectRoot -replace '\\', '\\\\'
        
        $renderedContent = $templateContent -replace '\{\{PYTHON_PATH\}\}', $EscapedPythonPath
        $renderedContent = $renderedContent -replace '\{\{PROJECT_ROOT\}\}', $EscapedProjectRoot
        
        [IO.File]::WriteAllText($McpSettings, $renderedContent, $Utf8NoBom)
        Write-Host "[√] Successfully rendered .agents/settings.json from template" -ForegroundColor Green
    } catch {
        Write-Warning "Failed to render .agents/settings.json: $_"
    }
} else {
    Write-Warning ".agents/settings.template.json not found."
}

# 6. Update config.yaml allowed paths for ansys and matlab mcp servers
Write-Host "`nUpdating config.yaml allowed paths for ansys and matlab mcp servers..." -ForegroundColor Cyan
$AnsysConfig = Join-Path $ProjectRoot ".agents\mcp\ansys-mcp-server\config.yaml"
$MatlabConfig = Join-Path $ProjectRoot ".agents\mcp\matlab-mcp-server\config.yaml"

function Update-Mcp-Config-Paths {
    param (
        [string]$Path
    )
    if (Test-Path $Path) {
        try {
            $content = Get-Content $Path -Encoding utf8 -Raw
            # Replace old hardcoded root path D:\\Project_C++\\GRPD with current project root
            $OldRoot1 = "D:\\\\Project_C\+\+\\\\GRPD"
            $OldRoot2 = "D:/Project_C\+\+/GRPD"
            $EscapedProjectRootYaml = $ProjectRoot -replace '\\', '\\\\'
            
            $newContent = $content -replace $OldRoot1, $EscapedProjectRootYaml
            $newContent = $newContent -replace $OldRoot2, $EscapedProjectRootYaml
            
            [IO.File]::WriteAllText($Path, $newContent, $Utf8NoBom)
            Write-Host "[√] Successfully updated allowed directories in $($Path | Split-Path -Leaf)" -ForegroundColor Green
        } catch {
            Write-Warning "Failed to update $($Path | Split-Path -Leaf): $_"
        }
    }
}

Update-Mcp-Config-Paths -Path $AnsysConfig
Update-Mcp-Config-Paths -Path $MatlabConfig

# 7. Auto-detect and configure local executable paths for ANSYS and MATLAB
Write-Host "`nDetecting local ANSYS and MATLAB installations to auto-configure paths..." -ForegroundColor Cyan

# Detect ANSYS
$DetectedAnsysPath = ""
# Method A: Scan system environment variables (AWP_ROOTXXX or ANSYSXXX_DIR)
$AnsysEnvVars = Get-ChildItem Env: | Where-Object { $_.Name -like "AWP_ROOT*" -or $_.Name -like "ANSYS*_DIR" }
foreach ($envVar in $AnsysEnvVars) {
    $baseDir = $envVar.Value
    $exePattern = Join-Path $baseDir "ANSYS\bin\winx64\ansys*.exe"
    $foundExes = Resolve-Path $exePattern -ErrorAction SilentlyContinue
    if ($foundExes) {
        $DetectedAnsysPath = $foundExes[0].Path
        break
    }
}

# Method B: Search default Windows installation roots
if (-not $DetectedAnsysPath) {
    $CommonAnsysRoots = @(
        "C:\Program Files\ANSYS Inc",
        "D:\Software\ANSYS\ANSYS Inc"
    )
    foreach ($root in $CommonAnsysRoots) {
        if (Test-Path $root) {
            $subDirs = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue
            foreach ($dir in $subDirs) {
                $exePattern = Join-Path $dir.FullName "ANSYS\bin\winx64\ansys*.exe"
                $foundExes = Resolve-Path $exePattern -ErrorAction SilentlyContinue
                if ($foundExes) {
                    $DetectedAnsysPath = $foundExes[0].Path
                    break
                }
            }
        }
        if ($DetectedAnsysPath) { break }
    }
}

# Apply detected ANSYS path to config.yaml
if ($DetectedAnsysPath -and (Test-Path $AnsysConfig)) {
    try {
        $content = Get-Content $AnsysConfig -Encoding utf8 -Raw
        $EscapedAnsysPath = $DetectedAnsysPath -replace '\\', '\\\\'
        # Match ansys_executable: "any_path"
        $newContent = $content -replace 'ansys_executable:\s*".*?"', "ansys_executable: `"$EscapedAnsysPath`""
        [IO.File]::WriteAllText($AnsysConfig, $newContent, $Utf8NoBom)
        Write-Host "[√] Auto-detected ANSYS MAPDL and updated config: $DetectedAnsysPath" -ForegroundColor Green
    } catch {
        Write-Warning "Failed to auto-configure ANSYS path in config: $_"
    }
} else {
    Write-Warning "Could not auto-detect ANSYS MAPDL in standard paths. Please verify ansys_executable in config.yaml manually."
}

# Detect MATLAB
$DetectedMatlabPath = ""
$MatlabInPath = Get-Command matlab -ErrorAction SilentlyContinue
if ($MatlabInPath) {
    Write-Host "[√] MATLAB is available in system PATH." -ForegroundColor Green
} else {
    $MatlabSearchRoots = @(
        "C:\Program Files\MATLAB",
        "D:\Program Files\MATLAB"
    )
    foreach ($root in $MatlabSearchRoots) {
        if (Test-Path $root) {
            $subDirs = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue
            foreach ($dir in $subDirs) {
                $exePath = Join-Path $dir.FullName "bin\win64\matlab.exe"
                if (Test-Path $exePath) {
                    $DetectedMatlabPath = $exePath
                    break
                }
            }
        }
        if ($DetectedMatlabPath) { break }
    }
    
    # Apply detected MATLAB path to config.yaml
    if ($DetectedMatlabPath -and (Test-Path $MatlabConfig)) {
        try {
            $content = Get-Content $MatlabConfig -Encoding utf8 -Raw
            $EscapedMatlabPath = $DetectedMatlabPath -replace '\\', '\\\\'
            $newContent = $content -replace 'matlab_executable:\s*".*?"', "matlab_executable: `"$EscapedMatlabPath`""
            $newContent = $newContent -replace 'matlab_executable:\s*\w+', "matlab_executable: `"$EscapedMatlabPath`""
            [IO.File]::WriteAllText($MatlabConfig, $newContent, $Utf8NoBom)
            Write-Host "[√] Auto-detected MATLAB and updated config: $DetectedMatlabPath" -ForegroundColor Green
        } catch {
            Write-Warning "Failed to auto-configure MATLAB path in config: $_"
        }
    }
}

Write-Host "`n====================================================" -ForegroundColor Green
Write-Host " Configuration Complete! Please restart VS Code / Terminal." -ForegroundColor Green
Write-Host "====================================================" -ForegroundColor Green
