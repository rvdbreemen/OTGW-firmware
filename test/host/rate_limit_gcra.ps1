<#
  test/host/rate_limit_gcra.ps1

  Builds and runs the host harness for the REST poll-budget limiter in
  src/OTGW-firmware/restAPI.ino (ADR-172). Exits non-zero when any check fails.

      powershell -NoProfile -ExecutionPolicy Bypass -File test/host/rate_limit_gcra.ps1
      powershell -NoProfile -ExecutionPolicy Bypass -File test/host/rate_limit_gcra.ps1 -Rev ef84b5980

  Without -Rev the code under test comes from the working tree. With -Rev it
  comes from 'git show <rev>:src/OTGW-firmware/restAPI.ino'. That is how the old
  side of an old-vs-fix comparison runs: same test, older source.

  Nothing under test is copied into the test. It is sliced out of restAPI.ino
  by anchor into generated/rate_limit_under_test.inc:
    - a declaration line (the #define, the route strings, the enum, the structs);
    - or a signature plus its brace-matched body (the three functions and the
      two table initialisers).
  The brace matcher skips braces inside // and /* */ comments, string literals
  and character literals. Every anchor must occur exactly once, so a rename
  stops the run instead of testing stale code.

  Compiler: MSVC (cl.exe) from Visual Studio 2022 Build Tools, located the same
  way as build_and_run.ps1 does it.
#>
[CmdletBinding()]
param(
  [string]$Rev = ''
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = (Resolve-Path (Join-Path $here '..\..')).Path
$gen  = Join-Path $here 'generated'
$null = New-Item -ItemType Directory -Force -Path $gen

$relSource = 'src/OTGW-firmware/restAPI.ino'

# --- read the source under test ------------------------------------------------
if ($Rev) {
  [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
  $lines = & git -C $repo show "${Rev}:$relSource"
  if ($LASTEXITCODE -ne 0) { throw "git show ${Rev}:$relSource failed" }
  $text  = ($lines -join "`n") + "`n"
  $label = "git show ${Rev}:$relSource"
} else {
  $text  = [System.IO.File]::ReadAllText((Join-Path $repo $relSource))
  $label = "$relSource (working tree)"
}
Write-Host "== slicing code under test from $label =="

function Find-BlockEnd {
  # Index of the '}' that closes the first '{' at or after $From. Braces inside
  # comments and string/char literals do not count. Comments are recognised
  # before quotes, so an apostrophe in a comment cannot open a char literal.
  param([string]$Text, [int]$From)
  $state = 'code'; $depth = 0; $i = $From; $n = $Text.Length
  while ($i -lt $n) {
    $c = $Text[$i]
    $next = if ($i + 1 -lt $n) { $Text[$i + 1] } else { [char]0 }
    if ($state -eq 'code') {
      if     ($c -eq '/' -and $next -eq '/') { $state = 'line';  $i += 2; continue }
      elseif ($c -eq '/' -and $next -eq '*') { $state = 'block'; $i += 2; continue }
      elseif ($c -eq '"') { $state = 'str' }
      elseif ($c -eq "'") { $state = 'chr' }
      elseif ($c -eq '{') { $depth++ }
      elseif ($c -eq '}') {
        $depth--
        if ($depth -eq 0) { return $i }
        if ($depth -lt 0) { throw "closing brace before any opening brace at offset $i" }
      }
    } elseif ($state -eq 'line') {
      if ($c -eq "`n") { $state = 'code' }
    } elseif ($state -eq 'block') {
      if ($c -eq '*' -and $next -eq '/') { $state = 'code'; $i += 2; continue }
    } else {
      if ($c -eq '\') { $i += 2; continue }
      if (($state -eq 'str' -and $c -eq '"') -or ($state -eq 'chr' -and $c -eq "'")) { $state = 'code' }
    }
    $i++
  }
  throw "unbalanced braces after offset $From"
}

function Get-Slice {
  param([string]$Anchor, [switch]$Block, [switch]$ThroughSemicolon)
  $count = [regex]::Matches($text, [regex]::Escape($Anchor)).Count
  if ($count -ne 1) { throw "anchor '$Anchor' occurs $count times in $label (expected exactly 1)" }
  # Char overloads of IndexOf/LastIndexOf: ordinal, no culture rules.
  $nl        = [char]10
  $start     = $text.IndexOf($Anchor, [System.StringComparison]::Ordinal)
  $lineStart = if ($start -eq 0) { 0 } else { $text.LastIndexOf($nl, $start - 1) + 1 }
  $lineNo    = ([regex]::Matches($text.Substring(0, $lineStart), "`n")).Count + 1
  if ($Block) {
    $end = Find-BlockEnd -Text $text -From $start
    if ($ThroughSemicolon) {
      $semi = $text.IndexOf([char]';', $end)
      if ($semi -lt 0 -or $text.Substring($end + 1, $semi - $end - 1).Trim() -ne '') {
        throw "anchor '$Anchor': expected ';' right after the closing brace"
      }
      $end = $semi
    }
  } else {
    $end = $text.IndexOf($nl, $start)
    if ($end -lt 0) { $end = $text.Length }
    $end--
  }
  $body = $text.Substring($lineStart, $end - $lineStart + 1).TrimEnd("`r")
  $nLines = ($body -split "`n").Count
  Write-Host ("  {0,4} line(s) from line {1,5}: {2}" -f $nLines, $lineNo, $Anchor)
  return "#line $lineNo `"$relSource`"`n$body`n"
}

# Source order is dependency order, so the slices are emitted in it.
$slices = @(
  (Get-Slice 'static int16_t restResponseStatus'),
  (Get-Slice '#define API_WORD_LEN'),
  (Get-Slice 'static const char kRouteDevice[]'),
  (Get-Slice 'static const char kRouteOtgw[]'),
  (Get-Slice 'static const char kSubOtmonitor[]'),
  (Get-Slice 'static const char kSubTelegraf[]'),
  (Get-Slice 'static const char kSubTime[]'),
  (Get-Slice 'enum : uint8_t { RL_BUDGET_OTMONITOR'),
  (Get-Slice 'struct ApiRateLimitRoute {'),
  (Get-Slice 'static const ApiRateLimitRoute kApiRateLimitRoutes[] PROGMEM =' -Block -ThroughSemicolon),
  (Get-Slice 'struct ApiRateLimitBudget {'),
  (Get-Slice 'static ApiRateLimitBudget gApiRateLimitBudgets[RL_BUDGET_COUNT] =' -Block -ThroughSemicolon),
  (Get-Slice 'static void sendApiRateLimited(' -Block),
  (Get-Slice 'static uint32_t rateLimitTryAdmit(' -Block),
  (Get-Slice 'static bool checkApiRateLimit(' -Block)
)

$inc = Join-Path $gen 'rate_limit_under_test.inc'
$header = @(
  "// GENERATED - DO NOT EDIT. Sliced by anchor from:",
  "//   $label",
  "// by test/host/rate_limit_gcra.ps1. Edit the firmware source, not this file.",
  ""
) -join "`n"
[System.IO.File]::WriteAllText($inc, $header + "`n" + ($slices -join "`n"), (New-Object System.Text.UTF8Encoding($false)))

# --- locate cl.exe (same as build_and_run.ps1) ---------------------------------
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - Visual Studio Build Tools required." }
$vsRoot  = & $vswhere -latest -products * -property installationPath
$vcvars  = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found under $vsRoot" }

$exe = Join-Path $gen 'test_rate_limit_gcra.exe'
if (Test-Path $exe) { Remove-Item $exe -Force }

Write-Host "== compiling =="
$src = Join-Path $here 'test_rate_limit_gcra.cpp'
# Through a temp .bat: vcvars64.bat lives under a path with spaces, which
# cmd.exe /c "<one long string>" mangles.
$bat = Join-Path $gen '_compile_rate_limit.bat'
@(
  '@echo off',
  "call `"$vcvars`" >nul 2>nul",
  # forward-slash trailing separator on purpose: "...dir\" would escape the quote
  "cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fo:`"$($gen -replace '\\','/')/`" /Fe:`"$exe`" `"$src`" /I`"$here`""
) | Set-Content -LiteralPath $bat -Encoding ASCII
cmd.exe /c "`"$bat`""
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) {
  Write-Host "COMPILATION FAILED" -ForegroundColor Red
  exit 2
}

Write-Host "== running (code under test: $label) =="
& $exe
$rc = $LASTEXITCODE
if ($rc -eq 0) { Write-Host "RESULT: PASS" -ForegroundColor Green }
else           { Write-Host "RESULT: FAIL (exit $rc)" -ForegroundColor Red }
exit $rc
