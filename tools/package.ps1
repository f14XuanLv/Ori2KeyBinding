$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$packageRoot = Join-Path $repoRoot 'dist\OriPadMod-0.1.2'
New-Item -ItemType Directory -Path $packageRoot, (Join-Path $packageRoot 'licenses'), (Join-Path $packageRoot 'docs') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot 'build\Release\version.dll') -Destination $packageRoot -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'README.md') -Destination $packageRoot -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs\design.md') -Destination (Join-Path $packageRoot 'docs') -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs\local-input-audit.md') -Destination (Join-Path $packageRoot 'docs') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Install.ps1') -Destination $packageRoot -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'vendor\imgui\LICENSE.txt') -Destination (Join-Path $packageRoot 'licenses\Dear-ImGui.txt') -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'vendor\minhook\LICENSE.txt') -Destination (Join-Path $packageRoot 'licenses\MinHook.txt') -Force
foreach ($command in @('Install','Uninstall')) {
    $extra = if ($command -eq 'Uninstall') { ' -Uninstall' } else { '' }
    $batch = "@echo off`r`npowershell.exe -NoProfile -ExecutionPolicy Bypass -File `"%~dp0Install.ps1`"$extra`r`nif errorlevel 1 pause`r`n"
    [IO.File]::WriteAllText((Join-Path $packageRoot "$command.cmd"), $batch, [Text.Encoding]::ASCII)
}
$zip = Join-Path $repoRoot 'dist\OriPadMod-0.1.2.zip'
Compress-Archive -LiteralPath $packageRoot -DestinationPath $zip -Force
Write-Output $zip
