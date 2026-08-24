<#
  test/host/build_and_run.ps1

  One command to build and run the host harness for extractJsonField()
  and expandPayload(). Exits non-zero when any check fails.

      powershell -NoProfile -ExecutionPolicy Bypass -File test/host/build_and_run.ps1

  The functions under test are NOT copied into the test. They are sliced
  verbatim out of the shipped sources between sentinel comments, so any edit
  to the firmware source is picked up on the next run:

      src/OTGW-firmware/jsonStuff.ino   "host-testable JSON scanner: BEGIN/END"
      src/OTGW-firmware/webhook.ino     "host-testable payload expander: BEGIN/END"

  Compiler: MSVC (cl.exe) from Visual Studio 2022 Build Tools. There is no
  gcc/clang on this machine; the xtensa cross-toolchain cannot run host code.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Resolve-Path (Join-Path $here '..\..')
$gen  = Join-Path $here 'generated'
$null = New-Item -ItemType Directory -Force -Path $gen

function Export-Sentineled {
  param([string]$Source, [string]$BeginMark, [string]$EndMark, [string]$Dest)

  $lines = Get-Content -LiteralPath $Source
  $b = -1; $e = -1
  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($b -lt 0 -and $lines[$i] -like "*$BeginMark*") { $b = $i; continue }
    if ($b -ge 0 -and $lines[$i] -like "*$EndMark*")   { $e = $i; break }
  }
  if ($b -lt 0 -or $e -lt 0) {
    throw "Sentinels not found in $Source ('$BeginMark' .. '$EndMark'). Was the marker removed or reworded?"
  }
  $body = $lines[($b + 1)..($e - 1)]
  $header = @(
    "// GENERATED - DO NOT EDIT. Sliced verbatim from:",
    "//   $Source",
    "// between '$BeginMark' and '$EndMark' by test/host/build_and_run.ps1.",
    "// Edit the firmware source, not this file."
  )
  Set-Content -LiteralPath $Dest -Value ($header + $body) -Encoding UTF8
  Write-Host ("  sliced {0,4} lines -> {1}" -f $body.Count, (Split-Path -Leaf $Dest))
}

Write-Host "== extracting code under test from shipped sources =="
Export-Sentineled `
  -Source    (Join-Path $repo 'src\OTGW-firmware\jsonStuff.ino') `
  -BeginMark 'host-testable JSON scanner: BEGIN' `
  -EndMark   'host-testable JSON scanner: END' `
  -Dest      (Join-Path $gen 'json_scanner.inc')

Export-Sentineled `
  -Source    (Join-Path $repo 'src\OTGW-firmware\webhook.ino') `
  -BeginMark 'host-testable payload expander: BEGIN' `
  -EndMark   'host-testable payload expander: END' `
  -Dest      (Join-Path $gen 'expand_payload.inc')

# --- locate cl.exe -----------------------------------------------------------
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - Visual Studio Build Tools required." }
$vsRoot  = & $vswhere -latest -products * -property installationPath
$vcvars  = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found under $vsRoot" }

$exe = Join-Path $gen 'test_json_and_webhook.exe'
if (Test-Path $exe) { Remove-Item $exe -Force }

Write-Host "== compiling =="
$src = Join-Path $here 'test_json_and_webhook.cpp'
# Run through a temp .bat: vcvars64.bat lives under a path with spaces, which
# cmd.exe /c "<one long string>" mangles.
$bat = Join-Path $gen '_compile.bat'
@(
  '@echo off',
  "call `"$vcvars`" >nul 2>nul",
  # trailing slashes are forward slashes on purpose: "...dir\" would escape the
  # closing quote and cl would swallow the next argument.
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
