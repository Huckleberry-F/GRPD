# Fix GRPD:: -> PDCommon:: in PDSimulater.h and PDSimulater.cpp
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Fix-File {
    param([string]$Path)
    $c = [System.IO.File]::ReadAllText($Path, $utf8NoBom)
    $c = $c.Replace("namespace GRPD::Core", "namespace PDCommon::Core")
    $c = $c.Replace("GRPD::Model::", "PDCommon::Model::")
    $c = $c.Replace("GRPD::Material::", "PDCommon::Material::")
    $c = $c.Replace("GRPD::Field::", "PDCommon::Field::")
    $c = $c.Replace("GRPD::Initial::", "PDCommon::Initial::")
    $c = $c.Replace("GRPD::BC::", "PDCommon::BC::")
    $c = $c.Replace("GRPD::Core", "PDCommon::Core")
    [System.IO.File]::WriteAllText($Path, $c, $utf8NoBom)
    Write-Host "Fixed: $Path"
}

Fix-File "PDCommon/Core/include/PDSimulater.h"
Fix-File "PDCommon/Core/src/PDSimulater.cpp"

Write-Host "`n=== PDSimulater namespace fixed! ==="
