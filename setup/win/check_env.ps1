# setup/win/check_env.ps1
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Write-Host "--- Windows Environment Checklist ---" -ForegroundColor Cyan

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) { Write-Host "[√] CMake detected: $($cmake.Source)" -ForegroundColor Green } else { Write-Warning "[!] CMake missing" }

$gcc = Get-Command gcc -ErrorAction SilentlyContinue
if ($gcc) { Write-Host "[√] GCC detected: $($gcc.Source)" -ForegroundColor Green } else { Write-Warning "[!] GCC compiler missing" }

$python = Get-Command python -ErrorAction SilentlyContinue
if ($python) { Write-Host "[√] Python detected: $($python.Source)" -ForegroundColor Green } else { Write-Warning "[!] Python missing" }

# Detect MATLAB
$matlabPath = ""
$matlabRoots = @("C:\Program Files\MATLAB", "D:\Program Files\MATLAB")
foreach ($root in $matlabRoots) {
    if (Test-Path $root) {
        $exes = Get-ChildItem $root -Recurse -Filter "matlab.exe" -ErrorAction SilentlyContinue
        if ($exes) { $matlabPath = $exes[0].FullName; break }
    }
}
if ($matlabPath) { Write-Host "[√] MATLAB detected: $matlabPath" -ForegroundColor Green } else { Write-Warning "[!] MATLAB not found" }

# Detect ANSYS
$ansysPath = ""
$ansysEnv = Get-ChildItem Env: | Where-Object { $_.Name -like "AWP_ROOT*" }
if ($ansysEnv) {
    $exePattern = Join-Path $ansysEnv[0].Value "ANSYS\bin\winx64\ansys*.exe"
    $found = Resolve-Path $exePattern -ErrorAction SilentlyContinue
    if ($found) { $ansysPath = $found[0].Path }
}
if ($ansysPath) { Write-Host "[√] ANSYS MAPDL detected: $ansysPath" -ForegroundColor Green } else { Write-Warning "[!] ANSYS MAPDL not found" }
