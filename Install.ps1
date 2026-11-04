param([string]$GamePath = (Join-Path $PSScriptRoot '..\..\..'))
$ErrorActionPreference = 'Stop'
$taskGame = (Resolve-Path -LiteralPath $GamePath).Path.TrimEnd('\')
$taskExe = Join-Path $taskGame 'WoW.exe'
$taskLauncher = Join-Path $taskGame 'VanillaFixes.exe'
if (!(Test-Path -LiteralPath $taskExe) -or !(Test-Path -LiteralPath $taskLauncher)) { throw 'Choose the game folder containing WoW.exe and VanillaFixes.exe.' }
$taskSupported = @('D8FC8B43006118B9E51572581DAD00DB498ACEF100B45A75D01D0E3791EB63E7', '3D019E9EE7F220B5FD90BA33E538130A19774159E4CDA0CBFD65563618E388D5')
if ($taskSupported -notcontains (Get-FileHash -LiteralPath $taskExe -Algorithm SHA256).Hash) { throw 'This release supports only the audited OctoWoW 1.12.1.5875 executable. No files were changed.' }
foreach ($taskProcess in (Get-Process -Name WoW,VanillaFixes -ErrorAction SilentlyContinue)) {
  if ($taskProcess.Path -and [IO.Path]::GetDirectoryName($taskProcess.Path) -ieq $taskGame) { throw 'Fully exit this game client before installing the native layer.' }
}
$taskSource = Join-Path $PSScriptRoot 'bin\zAPI.dll'
$taskManifest = Join-Path $PSScriptRoot 'bin\SHA256SUMS'
if (!(Test-Path -LiteralPath $taskSource) -or !(Test-Path -LiteralPath $taskManifest)) { throw 'The packaged native binary and SHA256SUMS are required. Run build.ps1 -Tests -Package if installing from source.' }
$taskExpected = ((Get-Content -LiteralPath $taskManifest -Raw).Trim() -split '\s+')[0]
if ($taskExpected -notmatch '^[a-fA-F0-9]{64}$' -or (Get-FileHash -LiteralPath $taskSource -Algorithm SHA256).Hash -ine $taskExpected) { throw 'Native binary checksum mismatch. No files were changed.' }
$taskConfig = Join-Path $taskGame 'dlls.txt'
$taskTarget = Join-Path $taskGame 'zAPI.dll'
$taskStamp = Get-Date -Format 'yyyyMMdd-HHmmss-ffff'
if (Test-Path -LiteralPath $taskConfig) { Copy-Item -LiteralPath $taskConfig -Destination ($taskConfig + '.before-zAPI-' + $taskStamp) }
if (Test-Path -LiteralPath $taskTarget) { Copy-Item -LiteralPath $taskTarget -Destination ($taskTarget + '.before-' + $taskStamp) }
$taskLines = @()
if (Test-Path -LiteralPath $taskConfig) { $taskLines = @(Get-Content -LiteralPath $taskConfig | Where-Object { $_.Trim() -ine 'zAPI.dll' -and $_.Trim() -ine 'zProfilesNative.dll' }) }
Copy-Item -LiteralPath $taskSource -Destination $taskTarget -Force
$taskText = (@('zAPI.dll') + $taskLines) -join "`r`n"
[IO.File]::WriteAllText($taskConfig, $taskText.TrimEnd("`r", "`n") + "`r`n", [Text.UTF8Encoding]::new($false))
if ((Get-FileHash -LiteralPath $taskTarget -Algorithm SHA256).Hash -ine $taskExpected) { throw 'Installed binary verification failed.' }
Write-Output 'Installed zAPI.dll first in dlls.txt. Existing loader configuration was backed up.'
Write-Output 'Existing account modes are preserved; unconfigured accounts use Default.'
