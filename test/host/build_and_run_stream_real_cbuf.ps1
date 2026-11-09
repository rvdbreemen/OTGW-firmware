<#
  test/host/build_and_run_stream_real_cbuf.ps1   (TASK-1736 AC2)

  Builds and runs test_stream_real_cbuf.cpp: the real ESPAsyncWebServer
  AsyncResponseStream::write() on the real Arduino-ESP32 cbuf, with only the
  ESP-IDF ring buffer and cbuf's malloc/free faked to count allocations.

      powershell -NoProfile -ExecutionPolicy Bypass -File test/host/build_and_run_stream_real_cbuf.ps1
      $env:OTGW_NO_PRESIZE = '1'   # then run again: the presize part must fail

  Sliced verbatim, nothing copied by hand:
    src/OTGW-firmware/webServerCompat.h  between "host-testable stream size hint: BEGIN/END"
    .pio/libdeps/esp32-combo/ESPAsyncWebServer/src/WebResponses.cpp  AsyncResponseStream::write(const uint8_t *, size_t)
  Compiled as is:
    <PlatformIO core>/packages/framework-arduinoespressif32/cores/esp32/cbuf.cpp
  The library and the core come from a firmware build (build.bat); without them the
  test prints SKIP and exits 0.
  Compiler: MSVC (cl.exe) from Visual Studio 2022 Build Tools.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Resolve-Path (Join-Path $here '..\..')
$gen  = Join-Path $here 'generated'
$null = New-Item -ItemType Directory -Force -Path $gen

function Write-Slice($dest, $source, $body, $what) {
  $header = @("// GENERATED - DO NOT EDIT. Sliced verbatim from:", "//   $source", "// ($what) by test/host/build_and_run_stream_real_cbuf.ps1.")
  Set-Content -LiteralPath $dest -Value ($header + $body) -Encoding UTF8
  Write-Host ("  sliced {0,4} lines -> {1}" -f $body.Count, (Split-Path -Leaf $dest))
}

Write-Host "== extracting code under test =="
# 1. presize rule and table (firmware)
$fw = Join-Path $repo 'src\OTGW-firmware\webServerCompat.h'
$lines = Get-Content -LiteralPath $fw
$b = -1; $e = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
  if ($b -lt 0 -and $lines[$i] -like '*host-testable stream size hint: BEGIN*') { $b = $i; continue }
  if ($b -ge 0 -and $lines[$i] -like '*host-testable stream size hint: END*')   { $e = $i; break }
}
if ($b -lt 0 -or $e -lt 0) { throw "stream size hint sentinels not found in $fw" }
Write-Slice (Join-Path $gen 'stream_size_hint.inc') $fw $lines[($b + 1)..($e - 1)] 'stream size hint'

# 2. the library write path
$lib = Join-Path $repo '.pio\libdeps\esp32-combo\ESPAsyncWebServer\src\WebResponses.cpp'
$coreDir = if ($env:PLATFORMIO_CORE_DIR) { $env:PLATFORMIO_CORE_DIR } else { Join-Path $HOME '.platformio' }
$cbufDir = Join-Path $coreDir 'packages\framework-arduinoespressif32\cores\esp32'
if (-not (Test-Path $lib) -or -not (Test-Path (Join-Path $cbufDir 'cbuf.cpp'))) {
  Write-Host "SKIP: needs a firmware build first (missing $lib or $cbufDir\cbuf.cpp)" -ForegroundColor Yellow
  exit 0
}
$ll = Get-Content -LiteralPath $lib
$sig = 'size_t AsyncResponseStream::write(const uint8_t *data, size_t len) {'
$s = -1
for ($i = 0; $i -lt $ll.Count; $i++) { if ($ll[$i] -eq $sig) { $s = $i; break } }
if ($s -lt 0) { throw "AsyncResponseStream::write signature not found in $lib (library version changed?)" }
$t = -1
for ($i = $s + 1; $i -lt $ll.Count; $i++) { if ($ll[$i] -eq '}') { $t = $i; break } }
if ($t -lt 0) { throw "end of AsyncResponseStream::write not found" }
$body = @('size_t StreamUnderTest::write(const uint8_t *data, size_t len) {') + $ll[($s + 1)..$t]
Write-Slice (Join-Path $gen 'stream_write.inc') $lib $body 'AsyncResponseStream::write'

# --- locate cl.exe -----------------------------------------------------------
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - Visual Studio Build Tools required." }
$vsRoot  = & $vswhere -latest -products * -property installationPath
$vcvars  = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found under $vsRoot" }

$exe = Join-Path $gen 'test_stream_real_cbuf.exe'
if (Test-Path $exe) { Remove-Item $exe -Force }
Write-Host "== compiling (real cbuf.cpp from $cbufDir) =="
$src  = Join-Path $here 'test_stream_real_cbuf.cpp'
# Byte-for-byte copies, so cbuf.cpp's quoted includes (esp32-hal-log.h, sdkconfig.h) resolve to
# the fakes and not to the real core headers next to it.
Copy-Item -LiteralPath (Join-Path $cbufDir 'cbuf.cpp') -Destination (Join-Path $gen 'cbuf.cpp') -Force
Copy-Item -LiteralPath (Join-Path $cbufDir 'cbuf.h')   -Destination (Join-Path $gen 'cbuf.h')   -Force
$cbuf = Join-Path $gen 'cbuf.cpp'
$fake = Join-Path $here 'fake_rtos'
$bat  = Join-Path $gen '_compile_stream_real_cbuf.bat'
@(
  '@echo off',
  "call `"$vcvars`" >nul 2>nul",
  "cl /nologo /EHsc /W3 /std:c++17 /D_CRT_SECURE_NO_WARNINGS /Fo:`"$($gen -replace '\\','/')/`" /Fe:`"$exe`" `"$src`" `"$cbuf`" /I`"$gen`" /I`"$here`" /I`"$fake`""
) | Set-Content -LiteralPath $bat -Encoding ASCII
cmd.exe /c "`"$bat`""
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) { Write-Host "COMPILATION FAILED" -ForegroundColor Red; exit 2 }

Write-Host "== running =="
& $exe
$rc = $LASTEXITCODE
if ($rc -eq 0) { Write-Host "RESULT: PASS" -ForegroundColor Green } else { Write-Host "RESULT: FAIL (exit $rc)" -ForegroundColor Red }
exit $rc
