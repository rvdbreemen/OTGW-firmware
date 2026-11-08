<#
  test/host/build_and_run_stream_size.ps1   (TASK-1736)

  Builds and runs the host test for the presized AsyncResponseStream size
  classes. Exits non-zero when any check fails.

      powershell -NoProfile -ExecutionPolicy Bypass -File test/host/build_and_run_stream_size.ps1

  The sizing rules are sliced verbatim out of the shipped source between
  sentinel comments, so an edit to the firmware source is picked up on the
  next run:

      src/OTGW-firmware/webServerCompat.h   "host-testable stream size hint: BEGIN/END"

  Compiler: MSVC (cl.exe) from Visual Studio 2022 Build Tools, like
  build_and_run.ps1.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Resolve-Path (Join-Path $here '..\..')
$gen  = Join-Path $here 'generated'
$null = New-Item -ItemType Directory -Force -Path $gen

$source = Join-Path $repo 'src\OTGW-firmware\webServerCompat.h'
$begin  = 'host-testable stream size hint: BEGIN'
$end    = 'host-testable stream size hint: END'
$dest   = Join-Path $gen 'stream_size_hint.inc'

Write-Host "== extracting code under test from shipped sources =="
$lines = Get-Content -LiteralPath $source
$b = -1; $e = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
  if ($b -lt 0 -and $lines[$i] -like "*$begin*") { $b = $i; continue }
  if ($b -ge 0 -and $lines[$i] -like "*$end*")   { $e = $i; break }
}
if ($b -lt 0 -or $e -lt 0) { throw "Sentinels not found in $source ('$begin' .. '$end')." }
$body = $lines[($b + 1)..($e - 1)]
$header = @(
  "// GENERATED - DO NOT EDIT. Sliced verbatim from:",
  "//   $source",
  "// between '$begin' and '$end' by test/host/build_and_run_stream_size.ps1.",
  "// Edit the firmware source, not this file."
)
Set-Content -LiteralPath $dest -Value ($header + $body) -Encoding UTF8
Write-Host ("  sliced {0,4} lines -> {1}" -f $body.Count, (Split-Path -Leaf $dest))

# --- locate cl.exe -----------------------------------------------------------
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - Visual Studio Build Tools required." }
$vsRoot  = & $vswhere -latest -products * -property installationPath
$vcvars  = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found under $vsRoot" }

$exe = Join-Path $gen 'test_stream_size_hint.exe'
if (Test-Path $exe) { Remove-Item $exe -Force }

Write-Host "== compiling =="
$src = Join-Path $here 'test_stream_size_hint.cpp'
$bat = Join-Path $gen '_compile_stream_size.bat'
@(
  '@echo off',
  "call `"$vcvars`" >nul 2>nul",
  "cl /nologo /EHsc /W3 /std:c++17 /D_CRT_SECURE_NO_WARNINGS /Fo:`"$($gen -replace '\\','/')/`" /Fe:`"$exe`" `"$src`" /I`"$here`""
) | Set-Content -LiteralPath $bat -Encoding ASCII
cmd.exe /c "`"$bat`""
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) {
  Write-Host "COMPILATION FAILED" -ForegroundColor Red
  exit 2
}

Write-Host "== running =="
& $exe
$rc = $LASTEXITCODE
if ($rc -eq 0) { Write-Host "RESULT: PASS" -ForegroundColor Green }
else           { Write-Host "RESULT: FAIL (exit $rc)" -ForegroundColor Red }
exit $rc
