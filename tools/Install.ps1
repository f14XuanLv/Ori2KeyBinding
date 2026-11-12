[CmdletBinding()]
param(
    [string]$GameDir = 'C:\Game\Steam\steamapps\common\Ori and the Will of the Wisps',
    [switch]$Uninstall
)
$ErrorActionPreference = 'Stop'
$gameRoot = (Resolve-Path -LiteralPath $GameDir).Path
$gameExe = Join-Path $gameRoot 'oriwotw.exe'
if (-not (Test-Path -LiteralPath $gameExe -PathType Leaf)) { throw 'Selected folder does not contain oriwotw.exe.' }
Get-Process -Name oriwotw -ErrorAction SilentlyContinue | ForEach-Object {
    if ($_.Path -eq $gameExe) { throw 'Exit Ori and the Will of the Wisps before installing or uninstalling.' }
}
$targetDll = Join-Path $gameRoot 'version.dll'
$dataRoot = Join-Path $gameRoot 'OriPadMod'
$manifestPath = Join-Path $dataRoot 'install-manifest.json'
$installedHash = $null
if (Test-Path -LiteralPath $manifestPath) { $installedHash = (Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json).dllSha256 }
$existingHash = $null
if (Test-Path -LiteralPath $targetDll) { $existingHash = (Get-FileHash -LiteralPath $targetDll -Algorithm SHA256).Hash }
if ($Uninstall) {
    if ($existingHash) {
        if (-not $installedHash -or $existingHash -ne $installedHash) { throw 'version.dll does not match the OriPadMod installation manifest; no files were removed.' }
        Remove-Item -LiteralPath $targetDll
    }
    Write-Output 'OriPadMod DLL removed. Configuration and logs were preserved.'
    exit 0
}
$sourceDll = Join-Path $PSScriptRoot 'version.dll'
if (-not (Test-Path -LiteralPath $sourceDll -PathType Leaf)) { throw 'Install.ps1 must be run from an extracted OriPadMod release folder.' }
$newHash = (Get-FileHash -LiteralPath $sourceDll -Algorithm SHA256).Hash
if ($existingHash -and $existingHash -ne $newHash -and $existingHash -ne $installedHash) {
    throw 'Another version.dll already exists. It was not overwritten. Resolve the Mod loader conflict first.'
}
New-Item -ItemType Directory -Path $dataRoot -Force | Out-Null
Copy-Item -LiteralPath $sourceDll -Destination $targetDll -Force
foreach ($doc in @('README.md', 'licenses', 'docs')) {
    $sourceDoc = Join-Path $PSScriptRoot $doc
    if (Test-Path -LiteralPath $sourceDoc) { Copy-Item -LiteralPath $sourceDoc -Destination $dataRoot -Recurse -Force }
}
$configFile = Join-Path $dataRoot 'bindings.ini'
if (-not (Test-Path -LiteralPath $configFile)) {
    $controls = @('A','B','X','Y','LB','RB','LT','RT','L3','R3','Back','Start','DPadUp','DPadDown','DPadLeft','DPadRight')
    $configLines = @('# OriPadMod: original game button = physical controller button', 'Enabled=True', 'TriggerThreshold=30')
    $configLines += $controls | ForEach-Object { "$_=$_" }
    [IO.File]::WriteAllLines($configFile, $configLines, [Text.UTF8Encoding]::new($false))
}
$manifest = [ordered]@{ product = 'OriPadMod'; version = '0.1.2'; dllSha256 = $newHash; gameDirectory = $gameRoot; installedAt = [DateTimeOffset]::Now.ToString('o') }
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
Write-Output "OriPadMod installed: $targetDll"
Write-Output 'Start the game normally, pause, then press F8 to change controller buttons.'
