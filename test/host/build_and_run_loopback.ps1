<#
  test/host/build_and_run_loopback.ps1

  Builds and runs the host harness for simulateLoopbackResponse(), the
  OT-Direct loopback boiler simulator in src/OTGW-firmware/OTDirect.ino
  (TASK-1072). Exits non-zero when a check fails.

      powershell -NoProfile -ExecutionPolicy Bypass -File test/host/build_and_run_loopback.ps1
      powershell -NoProfile -ExecutionPolicy Bypass -File test/host/build_and_run_loopback.ps1 -OldVsFix

  Default: slice the working-tree OTDirect.ino (FIX) and run the contract in
  test_loopback_range.cpp against it.

  -OldVsFix: also slice OTDirect.ino as it was before the TASK-1072 fix
  (git 9bfcd0868^, override with -OldRev) and compile the SAME harness
  against it. The run passes only when all three hold:
    1. FIX passes the contract (exit 0).
    2. OLD fails it (exit 1) with the exact defect signature: 3072 reads
       past the table (every id 128-255, every type, every data value),
       384 of 384 READ_DATA requests for ids 128-255 answered READ_ACK with
       the out-of-range stand-in 0xBEEF, 2688 of 2688 non-write requests
       past the table not answered UNKNOWN_DATA_ID, and only C1 and C6 failing.
    3. The 3072 cases for ids 0-127 are byte-identical between OLD and FIX.
  A compile or slicing failure never counts as "OLD reproduced the defect".

  The code under test is sliced by anchor, not copied: from a signature line
  to its matching closing brace, or one declaration line. Each anchor must
  match exactly one line, and the slices must appear in source order. They
  land in generated/loopback/<rev>/loopback_slice.inc (git-ignored).

  Compiler: MSVC (cl.exe) from Visual Studio 2022 Build Tools. /Od, so the
  old out-of-range index compiles to plain address arithmetic.
#>
[CmdletBinding()]
param(
  [switch]$OldVsFix,
  [string]$OldRev = '9bfcd0868^'
)

$ErrorActionPreference = 'Stop'
$here    = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo    = (Resolve-Path (Join-Path $here '..\..')).Path
$gen     = Join-Path $here 'generated\loopback'
$srcRel  = 'src/OTGW-firmware/OTDirect.ino'
$harness = Join-Path $here 'test_loopback_range.cpp'
$utf8    = New-Object System.Text.UTF8Encoding $false

# Anchors in source order. A declaration ends at its ';', a definition at the
# brace that closes its body, the table at the ';' after its closing brace.
$anchors = @(
  @{ Name = 'decl buildOTResponse';     Regex = '^static unsigned long buildOTResponse\([^)]*\);' },
  @{ Name = 'setOTParityBit';           Regex = '^static inline void setOTParityBit\(unsigned long &frame\)\s*\{' },
  @{ Name = 'otLoopbackData[]';         Regex = '^static const uint16_t PROGMEM otLoopbackData\[' },
  @{ Name = 'simulateLoopbackResponse'; Regex = '^static unsigned long simulateLoopbackResponse\(unsigned long request\)\s*\{' },
  @{ Name = 'buildOTResponse';          Regex = '^static unsigned long buildOTResponse\([^)]*\)\s*\{' }
)

# git blob as UTF-8 text, independent of the console code page.
function Get-GitBlobLines([string]$Spec) {
  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.FileName  = 'git'
  $psi.Arguments = "-C `"$repo`" cat-file blob `"$Spec`""
  $psi.UseShellExecute        = $false
  $psi.RedirectStandardOutput = $true
  $psi.RedirectStandardError  = $true
  $p  = [System.Diagnostics.Process]::Start($psi)
  $ms = New-Object System.IO.MemoryStream
  $p.StandardOutput.BaseStream.CopyTo($ms)
  $err = $p.StandardError.ReadToEnd()
  $p.WaitForExit()
  if ($p.ExitCode -ne 0) { throw "git cat-file blob $Spec failed: $err" }
  return @(([System.Text.Encoding]::UTF8.GetString($ms.ToArray())) -split "`r?`n")
}

# Slice from the one line matching $Regex to the end of that construct.
# Braces and semicolons inside comments, strings and char literals are ignored.
function Get-AnchoredSlice([string[]]$Lines, [string]$Regex, [string]$Name) {
  $hits = @()
  for ($i = 0; $i -lt $Lines.Count; $i++) { if ($Lines[$i] -cmatch $Regex) { $hits += $i } }
  if ($hits.Count -ne 1) { throw "$Name : anchor /$Regex/ matched $($hits.Count) lines, expected exactly 1" }
  $start = $hits[0]
  $depth = 0; $opened = $false; $inBlock = $false
  for ($i = $start; $i -lt $Lines.Count; $i++) {
    $s = $Lines[$i]; $inStr = $false; $inChr = $false; $k = 0
    while ($k -lt $s.Length) {
      $c = $s[$k]
      $n = if ($k + 1 -lt $s.Length) { $s[$k + 1] } else { [char]0 }
      if ($inBlock) {
        if ($c -eq [char]'*' -and $n -eq [char]'/') { $inBlock = $false; $k += 2 } else { $k++ }
        continue
      }
      if ($inStr -or $inChr) {
        if ($c -eq [char]'\') { $k += 2; continue }
        if (($inStr -and $c -eq [char]'"') -or ($inChr -and $c -eq [char]"'")) { $inStr = $false; $inChr = $false }
        $k++; continue
      }
      if ($c -eq [char]'/' -and $n -eq [char]'/') { break }
      if ($c -eq [char]'/' -and $n -eq [char]'*') { $inBlock = $true; $k += 2; continue }
      if ($c -eq [char]'"') { $inStr = $true; $k++; continue }
      if ($c -eq [char]"'") { $inChr = $true; $k++; continue }
      if ($c -eq [char]'{') { $depth++; $opened = $true }
      elseif ($c -eq [char]'}') {
        $depth--
        if ($depth -lt 0) { throw "$Name : unbalanced '}' at line $($i + 1)" }
      }
      elseif ($c -eq [char]';' -and $depth -eq 0) {
        return [pscustomobject]@{ Name = $Name; First = $start + 1; Last = $i + 1; Lines = @($Lines[$start..$i]) }
      }
      $k++
    }
    if ($opened -and $depth -eq 0) {
      return [pscustomobject]@{ Name = $Name; First = $start + 1; Last = $i + 1; Lines = @($Lines[$start..$i]) }
    }
  }
  throw "$Name : no end found after the anchor at line $($start + 1)"
}

function Export-LoopbackSlice([string[]]$Lines, [string]$Origin, [string]$Dir) {
  $null = New-Item -ItemType Directory -Force -Path $Dir
  $out = New-Object System.Collections.Generic.List[string]
  $out.Add('// GENERATED - DO NOT EDIT. Sliced verbatim, by anchor, from:')
  $out.Add("//   $Origin")
  $out.Add('// by test/host/build_and_run_loopback.ps1. Edit the firmware source, not this file.')
  $out.Add("#define LOOPBACK_SLICE_ORIGIN `"$Origin`"")
  $prevLast = 0
  foreach ($a in $anchors) {
    $s = Get-AnchoredSlice -Lines $Lines -Regex $a.Regex -Name $a.Name
    if ($s.First -le $prevLast) { throw "$($s.Name) : slices out of source order (line $($s.First) after $prevLast)" }
    $prevLast = $s.Last
    Write-Host ("  {0,-26} source lines {1,4}-{2,-4} ({3} sliced)" -f $s.Name, $s.First, $s.Last, $s.Lines.Count)
    $out.Add("// ---- $($s.Name): source lines $($s.First)-$($s.Last) ----")
    foreach ($l in $s.Lines) { $out.Add($l) }
  }
  [System.IO.File]::WriteAllLines((Join-Path $Dir 'loopback_slice.inc'), $out.ToArray(), $utf8)
}

# --- locate cl.exe -----------------------------------------------------------
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - Visual Studio Build Tools required." }
$vsRoot = & $vswhere -latest -products * -property installationPath
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found under $vsRoot" }

function Build-Harness([string]$Dir, [string]$Label) {
  $exe = Join-Path $Dir 'test_loopback_range.exe'
  if (Test-Path $exe) { Remove-Item $exe -Force }
  # Through a temp .bat: vcvars64.bat lives under a path with spaces, which
  # cmd.exe /c "<one long string>" mangles. The /Fo directory ends in a forward
  # slash on purpose: "...dir\" would escape the closing quote.
  $bat = Join-Path $Dir '_compile.bat'
  @(
    '@echo off',
    "call `"$vcvars`" >nul 2>nul",
    "cl /nologo /EHsc /W3 /std:c++17 /Od /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fo:`"$($Dir -replace '\\','/')/`" /Fe:`"$exe`" `"$harness`" /I`"$Dir`""
  ) | Set-Content -LiteralPath $bat -Encoding ASCII
  Write-Host "== compiling ($Label, /Od) =="
  cmd.exe /c "`"$bat`"" | Out-Host      # to the host: a function's stray output would join its return value
  if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) {
    Write-Host "COMPILATION FAILED ($Label)" -ForegroundColor Red
    exit 2
  }
  return $exe
}

function Invoke-Harness([string]$Exe, [string]$Dir, [string]$Label) {
  Write-Host "== running ($Label) =="
  $out = @(& $Exe $Dir)
  $rc  = $LASTEXITCODE
  foreach ($l in $out) { Write-Host $l }
  $summary = @{}
  $line = $out | Where-Object { $_ -like 'SUMMARY *' } | Select-Object -Last 1
  if ($line) {
    foreach ($kv in (($line -replace '^SUMMARY\s+', '') -split '\s+')) {
      $p = $kv -split '=', 2
      if ($p.Count -eq 2) { $summary[$p[0]] = $p[1] }
    }
  }
  return [pscustomobject]@{ Rc = $rc; Summary = $summary }
}

# --- FIX: the working tree ---------------------------------------------------
$head = (& git -C $repo rev-parse --short=12 HEAD).Trim()
& git -C $repo diff --quiet HEAD -- $srcRel
$fixState = if ($LASTEXITCODE -eq 0) { 'matches HEAD' } else { 'differs from HEAD' }
$fixOrigin = "$srcRel (working tree at $head, $fixState)"
$fixDir = Join-Path $gen 'fix'
Write-Host "== slicing FIX: $fixOrigin =="
$fixLines = [System.IO.File]::ReadAllLines((Join-Path $repo $srcRel), [System.Text.Encoding]::UTF8)
Export-LoopbackSlice -Lines $fixLines -Origin $fixOrigin -Dir $fixDir
$fixExe = Build-Harness -Dir $fixDir -Label 'FIX'
$fix = Invoke-Harness -Exe $fixExe -Dir $fixDir -Label 'FIX'

if (-not $OldVsFix) {
  if ($fix.Rc -eq 0) { Write-Host "RESULT: PASS" -ForegroundColor Green }
  else               { Write-Host "RESULT: FAIL (exit $($fix.Rc))" -ForegroundColor Red }
  exit $fix.Rc
}

# --- OLD: the source before the fix ----------------------------------------
$oldSha = (& git -C $repo rev-parse --short=12 "$OldRev").Trim()
if ($LASTEXITCODE -ne 0 -or -not $oldSha) { throw "cannot resolve $OldRev" }
$oldOrigin = "git $OldRev ($oldSha):$srcRel"
$oldDir = Join-Path $gen 'old'
Write-Host "== slicing OLD: $oldOrigin =="
$oldLines = Get-GitBlobLines "${OldRev}:$srcRel"
Export-LoopbackSlice -Lines $oldLines -Origin $oldOrigin -Dir $oldDir
$oldExe = Build-Harness -Dir $oldDir -Label 'OLD'
$old = Invoke-Harness -Exe $oldExe -Dir $oldDir -Label 'OLD'

# --- verdict -------------------------------------------------------------------
Write-Host "== old-vs-fix verdict =="
$ok = $true

$fixGreen = ($fix.Rc -eq 0) -and ($fix.Summary['oob_reads'] -eq '0') -and ($fix.Summary['failed'] -eq 'none')
Write-Host ("  FIX contract holds (exit {0}, oob_reads={1}, failed={2}): {3}" -f `
  $fix.Rc, $fix.Summary['oob_reads'], $fix.Summary['failed'], $(if ($fixGreen) { 'yes' } else { 'NO' }))
if (-not $fixGreen) { $ok = $false }

# The exact numbers follow from 128 ids past the table x 8 types x 3 data values.
$sig = [ordered]@{
  'cases'                   = '6144'
  'oob_reads'               = '3072'
  'read_hi_cases'           = '384'
  'read_hi_readack_standin' = '384'
  'nonwrite_hi_cases'       = '2688'
  'nonwrite_hi_unknown0'    = '0'
  'failed'                  = 'C1,C6'
}
$oldRed = ($old.Rc -eq 1)
foreach ($k in $sig.Keys) { if ($old.Summary[$k] -ne $sig[$k]) { $oldRed = $false } }
Write-Host ("  OLD fails with the defect signature (exit {0}, oob_reads={1}, READ_ACK 0xBEEF for {2}/{3} READs past the table, failed={4}): {5}" -f `
  $old.Rc, $old.Summary['oob_reads'], $old.Summary['read_hi_readack_standin'], $old.Summary['read_hi_cases'],
  $old.Summary['failed'], $(if ($oldRed) { 'yes' } else { 'NO' }))
if (-not $oldRed) {
  Write-Host "    expected: exit 1 and $(($sig.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join ' ')"
  $ok = $false
}

$loName = 'cases_ids000-127.txt'
$fixLo = Join-Path $fixDir $loName
$oldLo = Join-Path $oldDir $loName
$same = $false
if ((Test-Path $fixLo) -and (Test-Path $oldLo)) {
  $hFix  = (Get-FileHash -Algorithm SHA256 -LiteralPath $fixLo).Hash
  $hOld  = (Get-FileHash -Algorithm SHA256 -LiteralPath $oldLo).Hash
  $count = @(Get-Content -LiteralPath $fixLo).Count
  $same  = ($hFix -eq $hOld) -and ($count -eq 3072)
  Write-Host ("  ids 0-127 byte-identical ({0} cases, sha256 FIX {1} OLD {2}): {3}" -f `
    $count, $hFix.Substring(0, 16), $hOld.Substring(0, 16), $(if ($same) { 'yes' } else { 'NO' }))
} else {
  Write-Host "  ids 0-127 byte-identical: NO (dump file missing)"
}
if (-not $same) { $ok = $false }

if ($ok) { Write-Host "RESULT: PASS (OLD red with the defect signature, FIX green, ids 0-127 unchanged)" -ForegroundColor Green; exit 0 }
Write-Host "RESULT: FAIL" -ForegroundColor Red
exit 1
