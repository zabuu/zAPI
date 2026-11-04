param([switch]$Tests, [switch]$Package)
$ErrorActionPreference = 'Stop'
$taskOutput = Join-Path $PSScriptRoot 'build'
$taskMinhook = Join-Path $PSScriptRoot 'vendor\minhook'
$taskVcvars = & (Join-Path $PSScriptRoot 'Find-Vcvars.ps1')
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
$taskMinhookSources = @('src\buffer.c','src\hook.c','src\trampoline.c','src\hde\hde32.c') | ForEach-Object { Join-Path $taskMinhook $_ }
$taskSources = @((Join-Path $PSScriptRoot 'src\zAPI.cpp'), (Join-Path $PSScriptRoot 'src\profiles\Profiles.cpp')) + $taskMinhookSources
$taskInputs = ($taskSources | ForEach-Object { '"' + $_ + '"' }) -join ' '
$taskOptions = '/nologo /O2 /MT /EHsc /std:c++17 /W4 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /I"' + (Join-Path $taskMinhook 'include') + '" /I"' + (Join-Path $taskMinhook 'src') + '" '
$taskCommand = 'call "' + $taskVcvars + '" x86 >nul && cl /LD ' + $taskOptions + $taskInputs + ' /link /DEF:"' + (Join-Path $PSScriptRoot 'src\zAPI.def') + '" /OUT:"' + (Join-Path $taskOutput 'zAPI.dll') + '" user32.lib kernel32.lib'
Push-Location -LiteralPath $taskOutput
try {
  & cmd.exe /d /s /c $taskCommand
  if ($LASTEXITCODE -ne 0) { throw "zAPI build failed: $LASTEXITCODE" }
  if ($Tests) {
    foreach ($taskTest in @('profiles_native','registration_native')) {
      $taskTestSource = Join-Path $PSScriptRoot ('tests\' + $taskTest + '.cpp')
      $taskTestInputs = (@($taskTestSource) + $taskMinhookSources | ForEach-Object { '"' + $_ + '"' }) -join ' '
      $taskTestExe = Join-Path $taskOutput ($taskTest + '.exe')
      $taskTestCommand = 'call "' + $taskVcvars + '" x86 >nul && cl ' + $taskOptions + $taskTestInputs + ' /link /OUT:"' + $taskTestExe + '" kernel32.lib user32.lib'
      & cmd.exe /d /s /c $taskTestCommand
      if ($LASTEXITCODE -ne 0) { throw "Native test build failed: $LASTEXITCODE" }
      & $taskTestExe
      if ($LASTEXITCODE -ne 0) { throw "Native tests failed: $LASTEXITCODE" }
    }
  }
} finally { Pop-Location }
if ($Package) {
  $taskBin = Join-Path $PSScriptRoot 'bin'
  New-Item -ItemType Directory -Path $taskBin -Force | Out-Null
  Copy-Item -LiteralPath (Join-Path $taskOutput 'zAPI.dll') -Destination (Join-Path $taskBin 'zAPI.dll') -Force
  $taskHash = (Get-FileHash -LiteralPath (Join-Path $taskBin 'zAPI.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
  [IO.File]::WriteAllText((Join-Path $taskBin 'SHA256SUMS'), "$taskHash  zAPI.dll`n", [Text.UTF8Encoding]::new($false))
}
Get-FileHash -LiteralPath (Join-Path $taskOutput 'zAPI.dll') -Algorithm SHA256
