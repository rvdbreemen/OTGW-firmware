<#
  test/host/build_and_run_override_reply.ps1

  Builds and runs the host harness for the OT-Direct reply to a forwarded
  thermostat frame (TASK-1178): test_override_reply.cpp compiled against code
  sliced from src/OTGW-firmware/OTDirect.ino, OTDirecttypes.h and the OpenTherm
  library. Exits non-zero when a check fails.

      powershell -NoProfile -ExecutionPolicy Bypass -File test/host/build_and_run_override_reply.ps1
      powershell -NoProfile -ExecutionPolicy Bypass -File test/host/build_and_run_override_reply.ps1 -OldVsFix

  Default: slice the working tree (FIX) and run the cases against it.

  -OldVsFix: also slice OTDirect.ino and OTDirecttypes.h as they are at -OldRev
  (default HEAD, the code without the fix while the fix is uncommitted; once
  the fix is committed, pass the commit before it) and compile the SAME
  harness against them. The run passes only when all of these hold:
    1. FIX passes every case (exit 0).
    2. Every case the harness reports is in exactly one class below, and every
       class member is reported.
    3. Defect cases: OLD fails them with the defect signature, a thermostat
       frame that FIX answers left without any reply.
    4. Unchanged cases: OLD passes them and its dump line (replies, log, cache
       and bus state) is byte-identical to FIX's.
    5. Log-only cases: OLD sends the same replies as FIX and meets the case's
       cache and bus-state checks; only the T/R/B/A log differs.
  -Suite 1177 runs the boiler-cache cases of TASK-1177 (K1-K7) instead, with
  -OldRev a revision without otBoilerCacheStore(). The run then passes only
  when FIX passes every K case, OLD fails every K defect case with a dump that
  differs from FIX's, the K control cases are byte-identical, and every
  TASK-1178 case (run on both sides as a regression) is byte-identical too.
  A compile or slicing failure never counts as "OLD reproduced the defect".

  The OpenTherm library is read from -OpenThermDir (default: the submodule
  checkout src/libraries/OpenTherm/src). It is the same for OLD and FIX; the
  runner prints the files' SHA256 and the submodule commit recorded in git.

  The code under test is sliced by anchor, not copied: from a signature line
  to its matching closing brace or terminating ';', or a range from one anchor
  to the end of the construct at a second anchor. Each anchor must match
  exactly one line, and the slices must appear in source order. They land in
  generated/override_reply/<old|fix>/ (git-ignored).

  Compiler: MSVC (cl.exe) from Visual Studio 2022 Build Tools, /Od.
#>
[CmdletBinding()]
param(
  [switch]$OldVsFix,
  [string]$OldRev = 'HEAD',
  [string]$OpenThermDir = '',
  [ValidateSet('1178', '1177')][string]$Suite = '1178'
)

$ErrorActionPreference = 'Stop'
$here     = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo     = (Resolve-Path (Join-Path $here '..\..')).Path
$gen      = Join-Path $here 'generated\override_reply'
$otdRel   = 'src/OTGW-firmware/OTDirect.ino'
$typesRel = 'src/OTGW-firmware/OTDirecttypes.h'
$libRel   = 'src/libraries/OpenTherm'
$harness  = Join-Path $here 'test_override_reply.cpp'
$utf8     = New-Object System.Text.UTF8Encoding $false

# Anchors, in source order. Kind 'construct' ends at the ';' or closing brace of
# the construct that starts on the anchor line. Kind 'range' runs from the anchor
# line to the end of the construct at EndRegex, or to that line itself when
# EndKind is 'line' (a #define).
$otdAnchors = @(
  @{ Name = 'decl buildOTResponse';         Kind = 'construct'; Regex = '^static unsigned long buildOTResponse\([^)]*\);' },
  @{ Name = 'decl applyResponseModifiers';  Kind = 'construct'; Regex = '^static unsigned long applyResponseModifiers\([^)]*\);' },
  @{ Name = 'otMasterStatusFlags';          Kind = 'construct'; Regex = '^static uint8_t otMasterStatusFlags\b' },
  @{ Name = 'otCurrentMode, IS_*_MODE()';   Kind = 'range';     Regex = '^static OTDirectMode otCurrentMode\b';
     EndRegex = '^#define IS_LOOPBACK_MODE\(\)'; EndKind = 'line' },
  @{ Name = 'setOTParityBit';               Kind = 'construct'; Regex = '^static inline void setOTParityBit\(unsigned long &frame\)\s*\{' },
  @{ Name = 'thermostat seen state';        Kind = 'range';     Regex = '^static bool\s+otThermostatSeen\b';
     EndRegex = '^static uint32_t otLastThermostatMs\b'; EndKind = 'construct' },
  @{ Name = 'otLastAnySendMs';              Kind = 'construct'; Regex = '^static uint32_t otLastAnySendMs\b' },
  @{ Name = 'DHW push and PS=1 state';      Kind = 'range';     Regex = '^enum DHWPushState\b';
     EndRegex = '^static bool otSummaryPending\b'; EndKind = 'construct' },
  @{ Name = 'otBoilerCache[]';              Kind = 'range';     Regex = '^static uint16_t otBoilerCache\[128\];';
     EndRegex = '^static bool\s+otBoilerCacheValid\[128\];'; EndKind = 'construct' },
  @{ Name = 'otIsVentSlave, otDirectBoilerPresent'; Kind = 'range'; Regex = '^static inline bool otIsVentSlave\(\)\s*\{';
     EndRegex = '^bool otDirectBoilerPresent\(\)\s*\{'; EndKind = 'construct' },
  @{ Name = 'otSlaveFrame(Pending)';        Kind = 'range';     Regex = '^static bool\s+otSlaveFramePending\b';
     EndRegex = '^static unsigned long otSlaveFrame\b'; EndKind = 'construct' },
  @{ Name = 'otOverrides[]';                Kind = 'range';     Regex = '^struct OTFrameOverride\b';
     EndRegex = '^static constexpr uint8_t OT_OVERRIDE_COUNT\b'; EndKind = 'construct' },
  @{ Name = 'floatToF88';                   Kind = 'construct'; Regex = '^static inline uint16_t floatToF88\(float celsius\)\s*\{' },
  @{ Name = 'setOverride, clearOverride';   Kind = 'range';     Regex = '^static void setOverride\(uint8_t msgId, uint16_t value\)\s*\{';
     EndRegex = '^static void clearOverride\(uint8_t msgId\)\s*\{'; EndKind = 'construct' },
  @{ Name = 'otDHWOverride';                Kind = 'construct'; Regex = '^static uint8_t otDHWOverride\b' },
  @{ Name = 'SR, UI and RM tables';         Kind = 'range';     Regex = '^static constexpr uint8_t OT_RESPONSE_OVERRIDE_MAX\b';
     EndRegex = '^static void clearUnknownCount\(uint8_t msgId\)\s*\{'; EndKind = 'construct' },
  @{ Name = 'applyOverrides';               Kind = 'construct'; Regex = '^static unsigned long applyOverrides\(unsigned long frame, bool &modified\)\s*\{' },
  @{ Name = 'request state, handleSlaveRequest'; Kind = 'range'; Regex = '^static bool\s+otMasterRequestActive\b';
     EndRegex = '^static void handleSlaveRequest\(unsigned long request, OpenThermResponseStatus status\)\s*\{'; EndKind = 'construct' },
  @{ Name = 'otLoopbackData[] .. handleMasterResponse'; Kind = 'range'; Regex = '^static const uint16_t PROGMEM otLoopbackData\[';
     EndRegex = '^static void handleMasterResponse\(\)\s*\{'; EndKind = 'construct' },
  @{ Name = 'loopOTDirect';                 Kind = 'construct'; Regex = '^void loopOTDirect\(\)\s*\{' },
  @{ Name = 'buildOTResponse';              Kind = 'construct'; Regex = '^static unsigned long buildOTResponse\([^)]*\)\s*\{' },
  @{ Name = 'handleMasterModeSlaveFrame';   Kind = 'construct'; Regex = '^static void handleMasterModeSlaveFrame\(unsigned long frame\)\s*\{' },
  @{ Name = 'applyResponseModifiers';       Kind = 'construct'; Regex = '^static unsigned long applyResponseModifiers\(unsigned long response\)\s*\{' }
)
$typesAnchors = @(
  @{ Name = 'enum OTDirectRequestOrigin'; Kind = 'construct'; Regex = '^enum OTDirectRequestOrigin\b' },
  @{ Name = 'enum OTDirectMode';          Kind = 'construct'; Regex = '^enum OTDirectMode\b' }
)
$libEnumAnchors = @(
  @{ Name = 'enum OpenThermResponseStatus'; Kind = 'construct'; Regex = '^enum class OpenThermResponseStatus\b' },
  @{ Name = 'enum OpenThermMessageType';    Kind = 'construct'; Regex = '^enum class OpenThermMessageType\b' },
  @{ Name = 'enum OpenThermMessageID';      Kind = 'construct'; Regex = '^enum class OpenThermMessageID\b' }
)
$libStaticAnchors = @(
  @{ Name = 'OpenTherm::parity';          Kind = 'construct'; Regex = '^bool OpenTherm::parity\(unsigned long frame\)' },
  @{ Name = 'OpenTherm::getMessageType';  Kind = 'construct'; Regex = '^OpenThermMessageType OpenTherm::getMessageType\(unsigned long message\)' },
  @{ Name = 'OpenTherm::buildRequest';    Kind = 'construct'; Regex = '^unsigned long OpenTherm::buildRequest\(' },
  @{ Name = 'OpenTherm::isValidResponse'; Kind = 'construct'; Regex = '^bool OpenTherm::isValidResponse\(unsigned long response\)' },
  @{ Name = 'OpenTherm::isValidRequest';  Kind = 'construct'; Regex = '^bool OpenTherm::isValidRequest\(unsigned long request\)' }
)

# Case classes for -OldVsFix. Every case the harness reports must be in exactly one.
$defectCases    = @('C1','C2','C3','C6a','C6b','C6c','C6d','C7a','C7b','C7c','C7d','C7e','C7f','C7g','C7h',
                    'C8a','C8b','C10','C14','C15','C16','C20','C21','C26','C28')
$unchangedCases = @('C4','C5','C9','C11','C12','C13a','C13b','C18','C22','C23','C24','C25','C27')
$logOnlyCases   = @('C17','C19')
if ($Suite -eq '1177') {
  # TASK-1177: every TASK-1178 case runs on both sides as a byte-identical regression.
  $regressionCases = $defectCases + $unchangedCases + $logOnlyCases
  $defectCases     = @('K1','K2','K3','K4')
  $unchangedCases  = @('K5','K6','K7')
  $logOnlyCases    = @()
}

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

function Find-AnchorLine([string[]]$Lines, [string]$Regex, [string]$Name) {
  $hits = @()
  for ($i = 0; $i -lt $Lines.Count; $i++) { if ($Lines[$i] -cmatch $Regex) { $hits += $i } }
  if ($hits.Count -ne 1) { throw "$Name : anchor /$Regex/ matched $($hits.Count) lines, expected exactly 1" }
  return $hits[0]
}

# Index of the line that ends the construct starting at $Start. Braces and
# semicolons inside comments, strings and char literals are ignored.
function Get-ConstructEnd([string[]]$Lines, [int]$Start, [string]$Name) {
  $depth = 0; $opened = $false; $inBlock = $false
  for ($i = $Start; $i -lt $Lines.Count; $i++) {
    $s = $Lines[$i]; $inStr = $false; $inChr = $false; $k = 0
    while ($k -lt $s.Length) {
      $c = $s[$k]
      $n = [char]0
      if ($k + 1 -lt $s.Length) { $n = $s[$k + 1] }
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
      elseif ($c -eq [char]';' -and $depth -eq 0) { return $i }
      $k++
    }
    if ($opened -and $depth -eq 0) { return $i }
  }
  throw "$Name : no end found after the anchor at line $($Start + 1)"
}

function Get-AnchoredSlice([string[]]$Lines, [hashtable]$A) {
  $start = Find-AnchorLine -Lines $Lines -Regex $A.Regex -Name $A.Name
  if ($A.Kind -eq 'range') {
    $endAnchor = Find-AnchorLine -Lines $Lines -Regex $A.EndRegex -Name "$($A.Name) (end)"
    if ($endAnchor -lt $start) { throw "$($A.Name) : end anchor at line $($endAnchor + 1) precedes the start at line $($start + 1)" }
    if ($A.EndKind -eq 'line') { $last = $endAnchor }
    else { $last = Get-ConstructEnd -Lines $Lines -Start $endAnchor -Name $A.Name }
  } else {
    $last = Get-ConstructEnd -Lines $Lines -Start $start -Name $A.Name
  }
  return [pscustomobject]@{ Name = $A.Name; First = $start + 1; Last = $last + 1; Lines = @($Lines[$start..$last]) }
}

function Export-Slices([string[]]$Lines, [object[]]$Anchors, [string]$Origin, [string]$OutFile, [string]$OriginMacro) {
  $out = New-Object System.Collections.Generic.List[string]
  $out.Add('// GENERATED - DO NOT EDIT. Sliced verbatim, by anchor, from:')
  $out.Add("//   $Origin")
  $out.Add('// by test/host/build_and_run_override_reply.ps1. Edit the source, not this file.')
  if ($OriginMacro) { $out.Add("#define $OriginMacro `"$Origin`"") }
  $prevLast = 0
  foreach ($a in $Anchors) {
    $s = Get-AnchoredSlice -Lines $Lines -A $a
    if ($s.First -le $prevLast) { throw "$($s.Name) : slices out of source order (line $($s.First) after $prevLast)" }
    $prevLast = $s.Last
    Write-Host ("  {0,-40} source lines {1,4}-{2,-4} ({3} sliced)" -f $s.Name, $s.First, $s.Last, $s.Lines.Count)
    $out.Add("// ---- $($s.Name): source lines $($s.First)-$($s.Last) ----")
    foreach ($l in $s.Lines) { $out.Add($l) }
  }
  [System.IO.File]::WriteAllLines($OutFile, $out.ToArray(), $utf8)
}

# --- OpenTherm library source --------------------------------------------------------
$defaultLibDir = Join-Path $repo 'src\libraries\OpenTherm\src'
$libDir = $OpenThermDir
if (-not $libDir) { $libDir = $defaultLibDir }
$libH   = Join-Path $libDir 'OpenTherm.h'
$libCpp = Join-Path $libDir 'OpenTherm.cpp'
if (-not ((Test-Path -LiteralPath $libH) -and (Test-Path -LiteralPath $libCpp))) {
  throw "OpenTherm library not found in $libDir. Run 'git submodule update --init src/libraries/OpenTherm' or pass -OpenThermDir."
}
$gitlinkHead = ((& git -C $repo ls-tree HEAD $libRel) -split '\s+')[2]
Write-Host "== OpenTherm library =="
Write-Host "  directory      : $libDir"
Write-Host ("  OpenTherm.h    : sha256 {0}" -f (Get-FileHash -Algorithm SHA256 -LiteralPath $libH).Hash)
Write-Host ("  OpenTherm.cpp  : sha256 {0}" -f (Get-FileHash -Algorithm SHA256 -LiteralPath $libCpp).Hash)
Write-Host "  gitlink at HEAD: $gitlinkHead"
if (-not $OpenThermDir) {
  $subHead = (& git -C (Join-Path $repo $libRel) rev-parse HEAD).Trim()
  $subDirty = @(& git -C (Join-Path $repo $libRel) status --porcelain -- src)
  Write-Host "  checkout commit: $subHead"
  if ($subHead -ne $gitlinkHead) { throw "the OpenTherm checkout ($subHead) is not the commit HEAD records ($gitlinkHead)" }
  if ($subDirty.Count -ne 0) { throw "the OpenTherm checkout has local changes in src/" }
} else {
  Write-Host "  checkout commit: not checked (caller-supplied -OpenThermDir; compare the SHA256 above)"
}
$libHLines   = [System.IO.File]::ReadAllLines($libH, [System.Text.Encoding]::UTF8)
$libCppLines = [System.IO.File]::ReadAllLines($libCpp, [System.Text.Encoding]::UTF8)

# --- locate cl.exe ----------------------------------------------------------------------
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - Visual Studio Build Tools required." }
$vsRoot = & $vswhere -latest -products * -property installationPath
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found under $vsRoot" }

function Export-Revision([string[]]$OtdLines, [string[]]$TypesLines, [string]$OtdOrigin, [string]$TypesOrigin, [string]$Dir) {
  $null = New-Item -ItemType Directory -Force -Path $Dir
  Write-Host "  -- OpenTherm.h"
  Export-Slices -Lines $libHLines -Anchors $libEnumAnchors -Origin "$libDir\OpenTherm.h" -OutFile (Join-Path $Dir 'lib_enums.inc') -OriginMacro ''
  Write-Host "  -- OpenTherm.cpp"
  Export-Slices -Lines $libCppLines -Anchors $libStaticAnchors -Origin "$libDir\OpenTherm.cpp" -OutFile (Join-Path $Dir 'lib_statics.inc') -OriginMacro ''
  Write-Host "  -- $TypesOrigin"
  Export-Slices -Lines $TypesLines -Anchors $typesAnchors -Origin $TypesOrigin -OutFile (Join-Path $Dir 'otd_types.inc') -OriginMacro ''
  Write-Host "  -- $OtdOrigin"
  Export-Slices -Lines $OtdLines -Anchors $otdAnchors -Origin $OtdOrigin -OutFile (Join-Path $Dir 'otd_slice.inc') -OriginMacro 'OTD_SLICE_ORIGIN'
}

function Build-Harness([string]$Dir, [string]$Label) {
  $exe = Join-Path $Dir 'test_override_reply.exe'
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

function Invoke-Harness([string]$Exe, [string]$Dir, [string]$Label, [string]$CaseSuite) {
  Write-Host "== running ($Label) =="
  $out = @(& $Exe $Dir $CaseSuite)
  $rc  = $LASTEXITCODE
  foreach ($l in $out) { Write-Host $l }
  $cases = @{}
  $order = New-Object System.Collections.Generic.List[string]
  foreach ($l in $out) {
    if ($l -match '^CASE (\S+) (pass|FAIL) want_replies=(\d+) got_replies=(\d+) replies_ok=([yn]) log_ok=([yn]) boiler_ok=([yn]) state_ok=([yn])$') {
      $cases[$Matches[1]] = [pscustomobject]@{
        Pass = ($Matches[2] -eq 'pass'); Want = [int]$Matches[3]; Got = [int]$Matches[4]
        RepliesOk = ($Matches[5] -eq 'y'); LogOk = ($Matches[6] -eq 'y'); BoilerOk = ($Matches[7] -eq 'y')
        StateOk = ($Matches[8] -eq 'y')
      }
      $order.Add($Matches[1])
    }
  }
  $dump = @{}
  $dumpFile = Join-Path $Dir "cases-$CaseSuite.txt"
  if (Test-Path -LiteralPath $dumpFile) {
    foreach ($l in [System.IO.File]::ReadAllLines($dumpFile)) {
      $id = ($l -split ' ', 2)[0]
      if ($id) { $dump[$id] = $l }
    }
  }
  return [pscustomobject]@{ Rc = $rc; Cases = $cases; Order = $order; Dump = $dump }
}

function Get-ReplyField([string]$DumpLine) {
  if ($DumpLine -match ' replies=(\S+) ') { return $Matches[1] }
  return '<missing>'
}

# --- FIX: the working tree ----------------------------------------------------------
$head = (& git -C $repo rev-parse --short=12 HEAD).Trim()
& git -C $repo diff --quiet HEAD -- $otdRel $typesRel
$fixState = 'matches HEAD'
if ($LASTEXITCODE -ne 0) { $fixState = 'differs from HEAD' }
$fixOtdOrigin   = "$otdRel (working tree at $head, $fixState)"
$fixTypesOrigin = "$typesRel (working tree at $head)"
$fixDir = Join-Path $gen 'fix'
Write-Host "== slicing FIX: $fixOtdOrigin =="
$fixOtd   = [System.IO.File]::ReadAllLines((Join-Path $repo $otdRel), [System.Text.Encoding]::UTF8)
$fixTypes = [System.IO.File]::ReadAllLines((Join-Path $repo $typesRel), [System.Text.Encoding]::UTF8)
Export-Revision -OtdLines $fixOtd -TypesLines $fixTypes -OtdOrigin $fixOtdOrigin -TypesOrigin $fixTypesOrigin -Dir $fixDir
$fixExe = Build-Harness -Dir $fixDir -Label 'FIX'
$fix = Invoke-Harness -Exe $fixExe -Dir $fixDir -Label 'FIX' -CaseSuite $Suite

if (-not $OldVsFix) {
  if ($fix.Rc -eq 0) { Write-Host "RESULT: PASS" -ForegroundColor Green }
  else               { Write-Host "RESULT: FAIL (exit $($fix.Rc))" -ForegroundColor Red }
  exit $fix.Rc
}

# --- OLD: the source without the fix -----------------------------------------------
$oldSha = (& git -C $repo rev-parse --short=12 "$OldRev").Trim()
if ($LASTEXITCODE -ne 0 -or -not $oldSha) { throw "cannot resolve $OldRev" }
$gitlinkOld = ((& git -C $repo ls-tree $OldRev $libRel) -split '\s+')[2]
if ($gitlinkOld -ne $gitlinkHead) { throw "OLD ($OldRev) records OpenTherm $gitlinkOld, HEAD records $gitlinkHead; this runner uses one library copy for both" }
$oldOtd   = Get-GitBlobLines "${OldRev}:$otdRel"
$oldTypes = Get-GitBlobLines "${OldRev}:$typesRel"
if ($Suite -eq '1178' -and (@($oldTypes | Where-Object { $_ -match 'OT_DIRECT_ORIGIN_THERMOSTAT_OVERRIDDEN' })).Count -gt 0) {
  throw "OLD ($OldRev, $oldSha) already contains the TASK-1178 fix; pass -OldRev <the commit before the fix>"
}
if ($Suite -eq '1177' -and (@($oldOtd | Where-Object { $_ -match '\botBoilerCacheStore\(' })).Count -gt 0) {
  throw "OLD ($OldRev, $oldSha) already contains the TASK-1177 fix; pass -OldRev <the commit before the fix>"
}
$oldOtdOrigin   = "git $OldRev ($oldSha):$otdRel"
$oldTypesOrigin = "git $OldRev ($oldSha):$typesRel"
$oldDir = Join-Path $gen 'old'
Write-Host "== slicing OLD: $oldOtdOrigin =="
Export-Revision -OtdLines $oldOtd -TypesLines $oldTypes -OtdOrigin $oldOtdOrigin -TypesOrigin $oldTypesOrigin -Dir $oldDir
$oldExe = Build-Harness -Dir $oldDir -Label 'OLD'
$old = Invoke-Harness -Exe $oldExe -Dir $oldDir -Label 'OLD' -CaseSuite $Suite
if ($Suite -eq '1177') {
  $fixReg = Invoke-Harness -Exe $fixExe -Dir $fixDir -Label 'FIX, TASK-1178 cases' -CaseSuite '1178'
  $oldReg = Invoke-Harness -Exe $oldExe -Dir $oldDir -Label 'OLD, TASK-1178 cases' -CaseSuite '1178'
}

# --- verdict -----------------------------------------------------------------------------
Write-Host "== old-vs-fix verdict =="
$ok = $true

$fixFailed = @($fix.Order | Where-Object { -not $fix.Cases[$_].Pass })
$fixGreen = ($fix.Rc -eq 0) -and ($fix.Order.Count -gt 0) -and ($fixFailed.Count -eq 0)
Write-Host ("  FIX passes every case (exit {0}, {1} cases, failed: {2}): {3}" -f `
  $fix.Rc, $fix.Order.Count, $(if ($fixFailed.Count) { $fixFailed -join ',' } else { 'none' }), $(if ($fixGreen) { 'yes' } else { 'NO' }))
if (-not $fixGreen) { $ok = $false }

$classOf = @{}
$dupes = New-Object System.Collections.Generic.List[string]
foreach ($pair in @(@('defect', $defectCases), @('unchanged', $unchangedCases), @('log-only', $logOnlyCases))) {
  foreach ($id in $pair[1]) {
    if ($classOf.ContainsKey($id)) { $dupes.Add($id) } else { $classOf[$id] = $pair[0] }
  }
}
$unclassified = @($fix.Order | Where-Object { -not $classOf.ContainsKey($_) })
$missing      = @($classOf.Keys | Where-Object { -not $fix.Cases.ContainsKey($_) -or -not $old.Cases.ContainsKey($_) } | Sort-Object)
$sameSet      = ($fix.Order -join ',') -eq ($old.Order -join ',')
$classesOk    = ($dupes.Count -eq 0) -and ($unclassified.Count -eq 0) -and ($missing.Count -eq 0) -and $sameSet
Write-Host ("  every case in exactly one class ({0} defect, {1} unchanged, {2} log-only; unclassified: {3}; missing: {4}; duplicates: {5}; same case list OLD/FIX: {6}): {7}" -f `
  $defectCases.Count, $unchangedCases.Count, $logOnlyCases.Count,
  $(if ($unclassified.Count) { $unclassified -join ',' } else { 'none' }),
  $(if ($missing.Count) { $missing -join ',' } else { 'none' }),
  $(if ($dupes.Count) { $dupes -join ',' } else { 'none' }),
  $(if ($sameSet) { 'yes' } else { 'NO' }),
  $(if ($classesOk) { 'yes' } else { 'NO' }))
if (-not $classesOk) { $ok = $false }

$oldRedExit = ($old.Rc -eq 1)
Write-Host ("  OLD exits 1: {0} (exit {1})" -f $(if ($oldRedExit) { 'yes' } else { 'NO' }), $old.Rc)
if (-not $oldRedExit) { $ok = $false }

$bad = New-Object System.Collections.Generic.List[string]
foreach ($id in $defectCases) {
  $o = $old.Cases[$id]; $f = $fix.Cases[$id]
  if ($Suite -eq '1178') {
    $sig = ($null -ne $o) -and ($null -ne $f) -and (-not $o.Pass) -and ($o.Got -eq 0) -and ($f.Want -ge 1) -and ($f.Got -eq $f.Want)
  } else {
    $sig = ($null -ne $o) -and ($null -ne $f) -and (-not $o.Pass) -and $f.Pass -and
           $old.Dump.ContainsKey($id) -and $fix.Dump.ContainsKey($id) -and ($old.Dump[$id] -cne $fix.Dump[$id])
  }
  if (-not $sig) { $bad.Add($id) }
}
$defectText = if ($Suite -eq '1178') { 'OLD leaves the thermostat without a reply that FIX sends' }
              else { 'OLD fails them and its dump (replies, cache, flame, presence) differs from FIX' }
Write-Host ("  defect cases: {0} ({1}/{2}; not matching: {3}): {4}" -f `
  $defectText, ($defectCases.Count - $bad.Count), $defectCases.Count, $(if ($bad.Count) { $bad -join ',' } else { 'none' }), $(if ($bad.Count -eq 0) { 'yes' } else { 'NO' }))
if ($bad.Count -ne 0) { $ok = $false }

if ($Suite -eq '1177') {
  $bad = New-Object System.Collections.Generic.List[string]
  foreach ($id in $regressionCases) {
    $same = $fixReg.Cases.ContainsKey($id) -and $fixReg.Cases[$id].Pass -and $oldReg.Dump.ContainsKey($id) -and
            $fixReg.Dump.ContainsKey($id) -and ($oldReg.Dump[$id] -ceq $fixReg.Dump[$id])
    if (-not $same) { $bad.Add($id) }
  }
  Write-Host ("  TASK-1178 cases as a regression: FIX passes them and OLD/FIX dumps are byte-identical ({0}/{1}; differing: {2}): {3}" -f `
    ($regressionCases.Count - $bad.Count), $regressionCases.Count, $(if ($bad.Count) { $bad -join ',' } else { 'none' }), $(if ($bad.Count -eq 0) { 'yes' } else { 'NO' }))
  if ($bad.Count -ne 0) { $ok = $false }
}

$bad = New-Object System.Collections.Generic.List[string]
foreach ($id in $unchangedCases) {
  $o = $old.Cases[$id]
  $same = ($null -ne $o) -and $o.Pass -and $old.Dump.ContainsKey($id) -and $fix.Dump.ContainsKey($id) -and ($old.Dump[$id] -ceq $fix.Dump[$id])
  if (-not $same) { $bad.Add($id) }
}
Write-Host ("  unchanged cases: OLD passes and its trace is byte-identical to FIX ({0}/{1}; differing: {2}): {3}" -f `
  ($unchangedCases.Count - $bad.Count), $unchangedCases.Count, $(if ($bad.Count) { $bad -join ',' } else { 'none' }), $(if ($bad.Count -eq 0) { 'yes' } else { 'NO' }))
if ($bad.Count -ne 0) { $ok = $false }

$bad = New-Object System.Collections.Generic.List[string]
foreach ($id in $logOnlyCases) {
  $o = $old.Cases[$id]
  $same = ($null -ne $o) -and $o.RepliesOk -and $o.BoilerOk -and $o.StateOk -and (-not $o.LogOk) -and $old.Dump.ContainsKey($id) -and $fix.Dump.ContainsKey($id) -and
          ((Get-ReplyField $old.Dump[$id]) -ceq (Get-ReplyField $fix.Dump[$id]))
  if (-not $same) { $bad.Add($id) }
}
Write-Host ("  log-only cases: OLD sends the same replies as FIX and meets the state checks, only the log differs ({0}/{1}; not matching: {2}): {3}" -f `
  ($logOnlyCases.Count - $bad.Count), $logOnlyCases.Count, $(if ($bad.Count) { $bad -join ',' } else { 'none' }), $(if ($bad.Count -eq 0) { 'yes' } else { 'NO' }))
if ($bad.Count -ne 0) { $ok = $false }

if ($ok) { Write-Host "RESULT: PASS (OLD red with the defect signature, FIX green, unchanged paths byte-identical)" -ForegroundColor Green; exit 0 }
Write-Host "RESULT: FAIL" -ForegroundColor Red
exit 1
