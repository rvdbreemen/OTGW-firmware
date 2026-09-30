<#
  test/host/run_unknown_counters.ps1

  Host proof for the bounds of otUnknownCounters, the 3-strike unknown-ID
  counters in src/OTGW-firmware/OTDirect.ino (TASK-1173). By default it
  builds and runs test_unknown_counters.cpp, which calls the real helpers for
  MsgIDs 0-255 with a guard region directly after the array, checks that
  MsgIDs 128-255 leave the counters of MsgIDs 0-127 alone, and checks the
  in-range counter contract.

      pwsh -NoProfile -File test/host/run_unknown_counters.ps1
          test the working tree
      pwsh -NoProfile -File test/host/run_unknown_counters.ps1 -Rev <rev>
          test OTDirect.ino as committed in <rev>
      pwsh -NoProfile -File test/host/run_unknown_counters.ps1 -DiffAgainst <rev>
          also compare MsgIDs 0-127 with <rev> (a differential)
      pwsh -NoProfile -File test/host/run_unknown_counters.ps1 -Asan [-Rev <rev>]
          build test_unknown_counters_asan.cpp with MSVC AddressSanitizer
          instead: the real declaration as a plain global, no guard region.
          The transcript is kept in generated/asan_transcript.txt.

  Windows PowerShell 5.1 works too (powershell -NoProfile -ExecutionPolicy
  Bypass -File ...).

  Exit codes: 0 = the tested source keeps every access inside the array,
  ignores MsgIDs 128-255 (no counter changes, get() returns 0), keeps the
  in-range counter contract and, with -DiffAgainst, behaves the same as <rev>
  for MsgIDs 0-127. 1 = a check failed; with -Asan also an AddressSanitizer
  report, judged from the transcript. 2 = slicing, compilation or starting
  the test failed.

  Nothing is copied by hand. The code under test is cut out of the source by
  anchor: the declaration line of otUnknownCounters, and each helper from its
  signature to the matching closing brace. A missing or duplicate anchor stops
  the run instead of testing something else. The slices land in
  test/host/generated/ (git-ignored) with the revision and line numbers in a
  header comment.

  Compiler: MSVC (cl.exe) from Visual Studio 2022 Build Tools, located with
  vswhere as in build_and_run.ps1. -Asan also needs the Build Tools component
  "C++ AddressSanitizer" (clang_rt.asan_dynamic-x86_64.dll).
#>
[CmdletBinding()]
param(
  [string]$Rev = '',
  [string]$DiffAgainst = '',
  [switch]$Asan
)

$ErrorActionPreference = 'Stop'
if ($Asan -and -not [string]::IsNullOrEmpty($DiffAgainst)) {
  Write-Host '-Asan and -DiffAgainst cannot be combined: the ASan build has no reference side.' -ForegroundColor Red
  exit 2
}
$here   = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo   = (Resolve-Path (Join-Path $here '..\..')).Path
$gen    = Join-Path $here 'generated'
$srcRel = 'src/OTGW-firmware/OTDirect.ino'
$null   = New-Item -ItemType Directory -Force -Path $gen
$utf8   = New-Object System.Text.UTF8Encoding($false)

# The helpers the harness calls, in source order. Add a name here when the
# helpers start to depend on another function.
$functionNames = @('getUnknownCount', 'incUnknownCount', 'clearUnknownCount')
$arrayName     = 'otUnknownCounters'

# --- source access -------------------------------------------------------------
function Get-OTDirectSource {
  param([string]$Revision)
  if ([string]::IsNullOrEmpty($Revision)) {
    $path = Join-Path $repo $srcRel
    return [pscustomobject]@{
      Lines = [IO.File]::ReadAllLines($path, $utf8)
      Label = "worktree:$srcRel"
    }
  }
  # git writes UTF-8; decode it as such (the declaration comment holds a
  # non-ASCII character).
  $prev = [Console]::OutputEncoding
  try {
    [Console]::OutputEncoding = $utf8
    $sha = & git -C $repo rev-parse --short "$($Revision)^{commit}"
    if ($LASTEXITCODE -ne 0 -or -not $sha) { throw "git rev-parse failed for '$Revision'" }
    $lines = @(& git -C $repo show "$($Revision):$srcRel")
    if ($LASTEXITCODE -ne 0 -or $lines.Count -eq 0) { throw "git show $($Revision):$srcRel failed" }
  } finally {
    [Console]::OutputEncoding = $prev
  }
  return [pscustomobject]@{ Lines = [string[]]$lines; Label = "$Revision ($sha):$srcRel" }
}

# --- slicing ---------------------------------------------------------------------
# Index of the line holding the brace that closes the block opened on or after
# line $Start. Braces inside comments, strings and char literals do not count.
function Get-BlockEnd {
  param([string[]]$Lines, [int]$Start)
  $depth = 0; $opened = $false; $inBlockComment = $false
  for ($i = $Start; $i -lt $Lines.Count; $i++) {
    $s = $Lines[$i]; $j = 0
    while ($j -lt $s.Length) {
      $c = $s[$j]
      $n = if ($j + 1 -lt $s.Length) { $s[$j + 1] } else { [char]0 }
      if ($inBlockComment) {
        if ($c -eq [char]'*' -and $n -eq [char]'/') { $inBlockComment = $false; $j += 2 } else { $j++ }
        continue
      }
      if ($c -eq [char]'/' -and $n -eq [char]'/') { break }
      if ($c -eq [char]'/' -and $n -eq [char]'*') { $inBlockComment = $true; $j += 2; continue }
      if ($c -eq [char]'"' -or $c -eq [char]"'") {
        $quote = $c; $j++
        while ($j -lt $s.Length -and $s[$j] -ne $quote) {
          if ($s[$j] -eq [char]'\') { $j++ }
          $j++
        }
        $j++
        continue
      }
      if ($c -eq [char]'{') { $depth++; $opened = $true }
      elseif ($c -eq [char]'}') {
        $depth--
        if ($opened -and $depth -eq 0) { return $i }
      }
      $j++
    }
  }
  throw "no closing brace found for the block starting at line $($Start + 1)"
}

function Get-DeclarationSlice {
  param($Src, [string]$Name)
  $rx = '^\s*static\s+[\w\s]*?\b' + [regex]::Escape($Name) + '\s*\['
  $hits = @()
  for ($i = 0; $i -lt $Src.Lines.Count; $i++) {
    if ($Src.Lines[$i] -cmatch $rx) { $hits += $i }
  }
  if ($hits.Count -ne 1) {
    throw "declaration anchor for $Name matched $($hits.Count) lines in $($Src.Label); expected exactly 1"
  }
  $i = $hits[0]
  $code = ($Src.Lines[$i] -replace '//.*$', '').TrimEnd()
  if (-not $code.EndsWith(';')) {
    throw "declaration of $Name at $($Src.Label) line $($i + 1) does not end on that line"
  }
  return [pscustomobject]@{ Name = $Name; First = $i + 1; Last = $i + 1; Body = @($Src.Lines[$i]) }
}

function Get-FunctionSlice {
  param($Src, [string]$Name)
  $rx = '^\s*static\s+[\w\s\*&:<>]*?\b' + [regex]::Escape($Name) + '\s*\('
  $defs = @()
  for ($i = 0; $i -lt $Src.Lines.Count; $i++) {
    $line = $Src.Lines[$i]
    if ($line -cnotmatch $rx) { continue }
    $code = ($line -replace '//.*$', '').TrimEnd()
    if ($code.EndsWith(';')) { continue }   # a prototype, not the definition
    $defs += $i
  }
  if ($defs.Count -ne 1) {
    throw "function anchor for $Name() matched $($defs.Count) definitions in $($Src.Label); expected exactly 1"
  }
  $b = $defs[0]
  $e = Get-BlockEnd -Lines $Src.Lines -Start $b
  return [pscustomobject]@{ Name = "$Name()"; First = $b + 1; Last = $e + 1; Body = $Src.Lines[$b..$e] }
}

function Write-Slices {
  param([string]$Path, $Src, $Slices)
  $out = New-Object System.Collections.Generic.List[string]
  $out.Add('// GENERATED by test/host/run_unknown_counters.ps1. DO NOT EDIT.')
  $out.Add("// Sliced verbatim from $($Src.Label):")
  foreach ($s in $Slices) { $out.Add(('//   {0}: lines {1}-{2}' -f $s.Name, $s.First, $s.Last)) }
  $out.Add('// Edit the firmware source, not this file.')
  foreach ($s in $Slices) {
    # Compiler messages point at the original source line.
    $out.Add(('#line {0} "{1}"' -f $s.First, $Src.Label))
    foreach ($l in $s.Body) { $out.Add($l) }
  }
  [IO.File]::WriteAllLines($Path, $out, $utf8)
}

function Export-Side {
  param([string]$Revision, [string]$Tag)
  $src   = Get-OTDirectSource -Revision $Revision
  $decl  = Get-DeclarationSlice -Src $src -Name $arrayName
  $funcs = @($functionNames | ForEach-Object { Get-FunctionSlice -Src $src -Name $_ })
  Write-Slices -Path (Join-Path $gen "uc_$($Tag)_decl.inc")  -Src $src -Slices @($decl)
  Write-Slices -Path (Join-Path $gen "uc_$($Tag)_funcs.inc") -Src $src -Slices $funcs
  $parts = @($decl) + $funcs | ForEach-Object {
    if ($_.First -eq $_.Last) { '{0} {1}' -f $_.Name, $_.First } else { '{0} {1}-{2}' -f $_.Name, $_.First, $_.Last }
  }
  $label = "$($src.Label) [lines: $($parts -join ', ')]"
  Write-Host "  $($Tag.ToUpper()): $label"
  return $label
}

function ConvertTo-CString([string]$s) { return '"' + ($s -replace '\\', '\\' -replace '"', '\"') + '"' }

# --- slice both sides --------------------------------------------------------------
Write-Host '== slicing the code under test =='
try {
  $sutLabel = Export-Side -Revision $Rev -Tag 'sut'
  $haveRef  = -not [string]::IsNullOrEmpty($DiffAgainst)
  $refLabel = ''
  if ($haveRef) { $refLabel = Export-Side -Revision $DiffAgainst -Tag 'ref' }
} catch {
  Write-Host "SLICING FAILED: $($_.Exception.Message)" -ForegroundColor Red
  exit 2
}
[IO.File]::WriteAllLines((Join-Path $gen 'uc_labels.inc'), [string[]]@(
  '// GENERATED by test/host/run_unknown_counters.ps1. DO NOT EDIT.',
  "#define UC_SUT_LABEL $(ConvertTo-CString $sutLabel)",
  "#define UC_REF_LABEL $(ConvertTo-CString $refLabel)"
), $utf8)

# --- locate cl.exe -----------------------------------------------------------------
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { Write-Host 'vswhere.exe not found: Visual Studio Build Tools required.' -ForegroundColor Red; exit 2 }
$vsRoot = & $vswhere -latest -products * -property installationPath
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { Write-Host "vcvars64.bat not found under $vsRoot" -ForegroundColor Red; exit 2 }

# trailing slashes are forward slashes on purpose: "...dir\" would escape the
# closing quote and cl would swallow the next argument.
$genFwd = $gen -replace '\\', '/'
if ($Asan) {
  $src   = Join-Path $here 'test_unknown_counters_asan.cpp'
  $exe   = Join-Path $gen 'test_unknown_counters_asan.exe'
  # /Zi names the helpers and OTDirect.ino lines in the ASan report. /Fd keeps
  # the compiler PDB in generated/ instead of the current directory.
  $flags = "/fsanitize=address /Zi /Fd:`"$genFwd/`""
} else {
  $src   = Join-Path $here 'test_unknown_counters.cpp'
  $exe   = Join-Path $gen 'test_unknown_counters.exe'
  $flags = if ($haveRef) { '/DUC_HAVE_REF=1' } else { '' }
}
if (Test-Path $exe) { Remove-Item $exe -Force }

# --- compile -----------------------------------------------------------------------
Write-Host '== compiling =='
# /Od keeps an index past the array a plain address computation, as on the
# device. /utf-8: the sliced declaration comment holds a non-ASCII character.
$bat = Join-Path $gen '_compile_unknown_counters.bat'
@(
  '@echo off',
  "call `"$vcvars`" >nul 2>nul",
  "cl /nologo /EHsc /W3 /Od /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS $flags /Fo:`"$genFwd/`" /Fe:`"$exe`" `"$src`" /I`"$here`""
) | Set-Content -LiteralPath $bat -Encoding ASCII
cmd.exe /c "`"$bat`""
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) {
  Write-Host 'COMPILATION FAILED' -ForegroundColor Red
  exit 2
}

# --- run: guard-region build ---------------------------------------------------------
if (-not $Asan) {
  Write-Host '== running =='
  & $exe
  $rc = $LASTEXITCODE
  if ($rc -eq 0) { Write-Host 'RESULT: PASS' -ForegroundColor Green }
  else           { Write-Host "RESULT: FAIL (exit $rc)" -ForegroundColor Red }
  exit $rc
}

# --- run: AddressSanitizer build -----------------------------------------------------
# Run inside the vcvars environment, so the exe finds clang_rt.asan_dynamic-x86_64.dll.
# One file for stdout and stderr keeps the "id=" lines and the ASan report in order.
Write-Host '== running under AddressSanitizer =='
$log    = Join-Path $gen 'asan_transcript.txt'
$runBat = Join-Path $gen '_run_unknown_counters_asan.bat'
if (Test-Path $log) { Remove-Item $log -Force }
@(
  '@echo off',
  "call `"$vcvars`" >nul 2>nul",
  "`"$exe`" > `"$log`" 2>&1",
  'exit /b %ERRORLEVEL%'
) | Set-Content -LiteralPath $runBat -Encoding ASCII
cmd.exe /c "`"$runBat`""
$rc   = $LASTEXITCODE
$text = if (Test-Path $log) { [IO.File]::ReadAllText($log) } else { '' }
Write-Host $text

# The verdict comes from the transcript. A crash or a missing DLL also gives a
# non-zero exit code, and that is not an ASan finding.
$report = [regex]::Match($text, 'ERROR: AddressSanitizer: ([\w-]+)')
if ($report.Success) {
  $before = $text.Substring(0, $report.Index)
  $ids    = [regex]::Matches($before, '(?m)^id=(\d+)\s*$')
  $lastId = if ($ids.Count -gt 0) { $ids[$ids.Count - 1].Groups[1].Value } else { '(none)' }
  $named  = $text -match "global variable '(\w+::)*otUnknownCounters'"
  Write-Host ("RESULT: FAIL: AddressSanitizer {0} after id={1}; report names otUnknownCounters: {2} (exit {3})" -f `
              $report.Groups[1].Value, $lastId, $(if ($named) { 'yes' } else { 'no' }), $rc) -ForegroundColor Red
  exit 1
}
if ($rc -eq 0 -and $text -match '== all cases passed ==') {
  Write-Host 'RESULT: PASS (no AddressSanitizer report, all cases passed)' -ForegroundColor Green
  exit 0
}
if ($rc -eq -1073741515) {
  Write-Host 'RESULT: could not start the ASan build: a DLL was not found (0xC0000135)' -ForegroundColor Red
  exit 2
}
Write-Host "RESULT: FAIL (exit $rc, no AddressSanitizer report)" -ForegroundColor Red
exit 1
