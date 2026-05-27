@echo off
setlocal enabledelayedexpansion
title OTGW Flash Tool

REM ============================================================================
REM  flash_otgw.bat - Zero-install flash tool for OTGW-firmware (Windows)
REM ----------------------------------------------------------------------------
REM  Distributed with each OTGW-firmware release. Downloads Espressif's
REM  standalone esptool binary on first run - no Python or pip required.
REM
REM  Flashes the two release binaries onto an ESP8266:
REM    OTGW-firmware-*.ino.bin       firmware   -> 0x0
REM    OTGW-firmware*.littlefs.bin   filesystem -> 0x200000
REM
REM  If the release binaries are not present in the script directory, they are
REM  auto-downloaded from the latest GitHub release and integrity-checked
REM  against the release's SHA256SUMS asset.
REM
REM  Serial port is auto-detected by USB VID/PID (CP210x / CH340 / FTDI).
REM
REM  Usage:
REM    flash_otgw.bat
REM    flash_otgw.bat --port COM3
REM    flash_otgw.bat --list-ports
REM    flash_otgw.bat --yes
REM    flash_otgw.bat --help
REM
REM  Supported hardware: Nodoshop OTGW with Wemos D1 mini / NodeMCU (ESP8266)
REM ============================================================================

REM ---- Pinned versions / hashes ---------------------------------------------
REM Keep ESPTOOL_VERSION in sync with flash_otgw.sh
REM (enforced by .github/workflows/flash-scripts-lint.yml).
set "ESPTOOL_VERSION=v4.8.1"

REM Pinned esptool SHA256 (https://github.com/espressif/esptool/releases/tag/v4.8.1)
set "ESPTOOL_SHA256_WIN64=2483d409e241d8826ae0ff023eecf31a7d4de6c10ca5ee855b1420cdfd53aaf6"

set "GITHUB_REPO=rvdbreemen/OTGW-firmware"
set "GITHUB_API_LATEST=https://api.github.com/repos/%GITHUB_REPO%/releases/latest"
set "GITHUB_LATEST_REDIRECT=https://github.com/%GITHUB_REPO%/releases/latest/download"

REM USB-serial VID/PID allowlist (CP210x, CH340, FTDI). Passed to PowerShell via env var.
set "USB_VID_PID_LIST=10C4:EA60;1A86:7523;0403:6001"

set "SCRIPT_DIR=%~dp0"
set "TOOLS_DIR=%SCRIPT_DIR%tools\esptool"
set "ESPTOOL_EXE=%TOOLS_DIR%\esptool.exe"
set "DOWNLOAD_URL=https://github.com/espressif/esptool/releases/download/%ESPTOOL_VERSION%/esptool-%ESPTOOL_VERSION%-win64.zip"

REM Exit codes
set "EXIT_OK=0"
set "EXIT_GENERIC=1"
set "EXIT_BAD_ARGS=2"
set "EXIT_SHA_MISMATCH=3"

set "ARG_PORT="
set "ARG_BAUD=460800"
set "ARG_HELP=0"
set "ARG_YES=0"
set "ARG_LIST_PORTS=0"

if not "%~1"=="" call :parse_args %*
if errorlevel 1 exit /b %ERRORLEVEL%
if "%ARG_HELP%"=="1" goto show_help

REM --list-ports short-circuits before any download
if "%ARG_LIST_PORTS%"=="1" (
    echo Detected serial ports:
    call :list_serial_ports
    exit /b %EXIT_OK%
)

echo.
echo ============================================================
echo  OTGW Flash Tool (Windows)
echo  Target: Nodoshop OTGW WiFi module (ESP8266)
echo  esptool: %ESPTOOL_VERSION%
echo ============================================================
echo.

REM ---- Step 1: ensure esptool is present (SHA256-verified) ------------------
if exist "%ESPTOOL_EXE%" (
    echo [OK] esptool found
) else (
    echo [INFO] Downloading esptool %ESPTOOL_VERSION% ^(win64^)...
    if not exist "%TOOLS_DIR%" mkdir "%TOOLS_DIR%" >nul 2>&1
    set "ZIP_FILE=%TOOLS_DIR%\esptool.zip"

    set "OTGW_DL_URL=%DOWNLOAD_URL%"
    set "OTGW_DL_OUT=!ZIP_FILE!"
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "$ProgressPreference='SilentlyContinue';" ^
        "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12;" ^
        "try { Invoke-WebRequest -UseBasicParsing -Headers @{'User-Agent'='OTGW-Flash-Tool'} -Uri $env:OTGW_DL_URL -OutFile $env:OTGW_DL_OUT -ErrorAction Stop }" ^
        "catch { Write-Host '[ERROR] Download failed:' $_.Exception.Message; exit 1 }"
    set "OTGW_DL_URL="
    set "OTGW_DL_OUT="
    if errorlevel 1 (
        echo [ERROR] Could not download esptool. Check internet connection.
        exit /b %EXIT_GENERIC%
    )

    echo [INFO] Verifying esptool SHA256...
    set "OTGW_DL_FILE=!ZIP_FILE!"
    set "OTGW_DL_EXPECTED=%ESPTOOL_SHA256_WIN64%"
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "$h = (Get-FileHash -Algorithm SHA256 -Path $env:OTGW_DL_FILE).Hash.ToLower();" ^
        "$e = $env:OTGW_DL_EXPECTED.ToLower();" ^
        "if ($h -ne $e) { Write-Host \"[ERROR] SHA256 mismatch:\"; Write-Host \"  expected: $e\"; Write-Host \"  actual:   $h\"; exit 3 }"
    set "_sha_err=!ERRORLEVEL!"
    set "OTGW_DL_FILE="
    set "OTGW_DL_EXPECTED="
    if !_sha_err! NEQ 0 (
        del "!ZIP_FILE!" >nul 2>&1
        exit /b %EXIT_SHA_MISMATCH%
    )
    echo [OK] esptool SHA256 verified

    echo [INFO] Extracting esptool...
    set "OTGW_DL_ZIP=!ZIP_FILE!"
    set "OTGW_DL_DEST=%TOOLS_DIR%"
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "$ProgressPreference='SilentlyContinue';" ^
        "Expand-Archive -LiteralPath $env:OTGW_DL_ZIP -DestinationPath $env:OTGW_DL_DEST -Force"
    set "OTGW_DL_ZIP="
    set "OTGW_DL_DEST="
    if errorlevel 1 (
        echo [ERROR] Could not extract esptool zip.
        exit /b %EXIT_GENERIC%
    )

    set "FOUND_EXE="
    for /f "delims=" %%F in ('dir /b /s "%TOOLS_DIR%\esptool.exe" 2^>nul') do (
        if not defined FOUND_EXE set "FOUND_EXE=%%F"
    )
    if not defined FOUND_EXE (
        echo [ERROR] Extracted archive did not contain esptool.exe
        exit /b %EXIT_GENERIC%
    )
    copy /Y "!FOUND_EXE!" "%ESPTOOL_EXE%" >nul
    del "!ZIP_FILE!" >nul 2>&1
    echo [OK] esptool installed
)

REM ---- Step 2: locate firmware and filesystem (version-aware) ---------------
call :find_highest_version "OTGW-firmware-*.ino.bin" FW_FILE
call :find_highest_version "OTGW-firmware*.littlefs.bin" FS_FILE

set "VERIFIED_FROM_RELEASE=0"
if not defined FW_FILE goto try_download
if not defined FS_FILE goto try_download
goto after_download

:try_download
echo [INFO] Release binaries not found locally - attempting auto-download.
call :download_release_binaries "%SCRIPT_DIR%"
if errorlevel 1 (
    echo [ERROR] Auto-download failed. Download the release binaries manually
    echo         from https://github.com/%GITHUB_REPO%/releases and place them
    echo         in the same directory as this script.
    exit /b %EXIT_GENERIC%
)
call :find_highest_version "OTGW-firmware-*.ino.bin" FW_FILE
call :find_highest_version "OTGW-firmware*.littlefs.bin" FS_FILE
if not defined FW_FILE (
    echo [ERROR] Firmware binary not found after download.
    exit /b %EXIT_GENERIC%
)
if not defined FS_FILE (
    echo [ERROR] Filesystem binary not found after download.
    exit /b %EXIT_GENERIC%
)
set "VERIFIED_FROM_RELEASE=1"

:after_download

REM ---- Step 3: SHA256 verify for auto-downloaded binaries -------------------
if "%VERIFIED_FROM_RELEASE%"=="1" (
    set "SUMS_FILE=%SCRIPT_DIR%SHA256SUMS"
    if not exist "!SUMS_FILE!" (
        echo [INFO] Fetching SHA256SUMS from release...
        set "OTGW_DL_URL=%GITHUB_LATEST_REDIRECT%/SHA256SUMS"
        set "OTGW_DL_OUT=!SUMS_FILE!"
        powershell -NoProfile -ExecutionPolicy Bypass -Command ^
            "$ProgressPreference='SilentlyContinue';" ^
            "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12;" ^
            "try { Invoke-WebRequest -UseBasicParsing -Headers @{'User-Agent'='OTGW-Flash-Tool'} -Uri $env:OTGW_DL_URL -OutFile $env:OTGW_DL_OUT -ErrorAction Stop }" ^
            "catch { Write-Host '[ERROR] Could not fetch SHA256SUMS:' $_.Exception.Message; exit 1 }"
        set "OTGW_DL_URL="
        set "OTGW_DL_OUT="
        if errorlevel 1 (
            echo [ERROR] Could not fetch SHA256SUMS. This release may not publish one yet.
            exit /b %EXIT_SHA_MISMATCH%
        )
    )
    echo [INFO] Verifying release binaries against SHA256SUMS...
    call :verify_against_sums "%FW_FILE%" "!SUMS_FILE!"
    if errorlevel 1 exit /b %EXIT_SHA_MISMATCH%
    call :verify_against_sums "%FS_FILE%" "!SUMS_FILE!"
    if errorlevel 1 exit /b %EXIT_SHA_MISMATCH%
) else (
    echo [INFO] Using local binaries - SHA256 verification skipped.
)

for %%F in ("%FW_FILE%") do set "FW_NAME=%%~nxF"
for %%F in ("%FS_FILE%") do set "FS_NAME=%%~nxF"
call :extract_firmware_version "%FW_NAME%" FW_VER

echo [OK] Firmware:   %FW_NAME%
echo [OK] Filesystem: %FS_NAME%
echo [OK] Version:    %FW_VER%
echo [OK] Baud:       %ARG_BAUD%

REM ---- Step 4: auto-detect serial port (VID/PID-aware) ----------------------
if not "%ARG_PORT%"=="" (
    echo [OK] Port:       %ARG_PORT% (specified^)
    goto after_port_detection
)

call :detect_port ARG_PORT
if not defined ARG_PORT (
    echo [WARN] No serial ports detected.
    echo        Connect your OTGW via USB, or install CP210x / CH340 USB-serial drivers.
    echo        Run with --list-ports to see what was detected.
    echo.
    set /p "ARG_PORT=Enter COM port manually (e.g. COM3), or press Enter to cancel: "
    if not defined ARG_PORT exit /b %EXIT_GENERIC%
    if "!ARG_PORT!"=="" exit /b %EXIT_GENERIC%
    echo [OK] Port:       !ARG_PORT! (manual^)
) else (
    echo [OK] Port:       %ARG_PORT% (auto-detected^)
)
:after_port_detection

REM ---- Step 5: summary ------------------------------------------------------
echo.
echo ------------------------------------------------------------
echo  Ready to flash:
echo    Firmware:   %FW_NAME%
echo    Filesystem: %FS_NAME%
echo    Version:    %FW_VER%
echo    Port:       %ARG_PORT%
echo    Baud:       %ARG_BAUD%
echo.
echo    Erases flash, then writes firmware at 0x0
echo    and filesystem at 0x200000.
echo ------------------------------------------------------------
echo.

REM ---- Step 6: confirmation -------------------------------------------------
if "%ARG_YES%"=="1" goto do_flash
choice /c yn /n /t 30 /d n /m "Continue? [y/N] (auto-cancel in 30s): "
if errorlevel 2 (
    echo [INFO] Aborted by user.
    exit /b %EXIT_OK%
)

:do_flash
REM ---- Step 7: flash --------------------------------------------------------
echo [STEP] Flashing firmware and filesystem...

"%ESPTOOL_EXE%" --chip esp8266 --port %ARG_PORT% --baud %ARG_BAUD% --before default_reset --after hard_reset write_flash --flash_mode dio -z --erase-all 0x0 "%FW_FILE%" 0x200000 "%FS_FILE%"

if errorlevel 1 (
    echo.
    echo [ERROR] Flash failed.
    echo         Troubleshooting tips:
    echo           - Try a different USB cable (data cable, not charge-only^)
    echo           - Install CP210x or CH340 USB-serial drivers
    echo           - Try a lower baud rate: --baud 115200
    echo           - Specify the correct port: --port COM3
    echo           - Boya flash chip ^(vendor ID 0x68^): requires --flash_mode dio ^(never qio^)
    exit /b %EXIT_GENERIC%
)

echo.
echo ============================================================
echo  Flash complete.
echo  Connect to WiFi AP "OTGW-^<MAC-address^>" and browse to
echo  http://192.168.4.1 to configure WiFi settings.
echo ============================================================
exit /b %EXIT_OK%


REM ===========================================================================
REM Subroutines
REM ===========================================================================

:find_highest_version
REM %1 = glob pattern (quoted), %2 = output var name.
REM Uses PowerShell to sort matches by parsed [Version], picks the highest.
REM Searches script directory first, then build\ subdirectory as fallback.
set "OTGW_FHV_DIR=%SCRIPT_DIR%"
set "OTGW_FHV_BDIR=%SCRIPT_DIR%build"
set "OTGW_FHV_PATTERN=%~1"
for /f "usebackq delims=" %%V in (`powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$dirs = @($env:OTGW_FHV_DIR);" ^
    "if (Test-Path $env:OTGW_FHV_BDIR) { $dirs += $env:OTGW_FHV_BDIR };" ^
    "$pat=$env:OTGW_FHV_PATTERN;" ^
    "$files = $dirs | ForEach-Object { Get-ChildItem -LiteralPath $_ -Filter $pat -ErrorAction SilentlyContinue };" ^
    "if (-not $files) { exit 0 }" ^
    "$ranked = foreach ($f in $files) {" ^
    "  $v = $f.Name -replace '^OTGW-firmware-?', '' -replace '\.(ino|littlefs)\.bin$', '';" ^
    "  $parsed = $null;" ^
    "  if ([Version]::TryParse($v, [ref]$parsed)) { [pscustomobject]@{ V = $parsed; F = $f.FullName } }" ^
    "  else { [pscustomobject]@{ V = [Version]'0.0.0'; F = $f.FullName } }" ^
    "}" ^
    "($ranked | Sort-Object -Property V -Descending | Select-Object -First 1).F"`) do (
    set "%~2=%%V"
)
set "OTGW_FHV_DIR="
set "OTGW_FHV_BDIR="
set "OTGW_FHV_PATTERN="
exit /b 0


:extract_firmware_version
REM %1 = filename (basename), %2 = output var name.
set "OTGW_EFV_NAME=%~1"
for /f "usebackq delims=" %%V in (`powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$n = $env:OTGW_EFV_NAME;" ^
    "$n -replace '^OTGW-firmware-?', '' -replace '\.(ino|littlefs)\.bin$', ''"`) do (
    set "%~2=%%V"
)
set "OTGW_EFV_NAME="
exit /b 0


:verify_against_sums
REM %1 = file path, %2 = SHA256SUMS path. Exit 0 on match, 1 on mismatch/missing.
set "OTGW_VAS_FILE=%~1"
set "OTGW_VAS_SUMS=%~2"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$file = $env:OTGW_VAS_FILE; $sums = $env:OTGW_VAS_SUMS;" ^
    "$fn = Split-Path -Leaf $file;" ^
    "$line = (Get-Content -LiteralPath $sums) | Where-Object { ($_ -split '\s+', 2)[1].TrimStart('*') -eq $fn } | Select-Object -First 1;" ^
    "if (-not $line) { Write-Host \"[ERROR] No SHA256 entry for $fn in SHA256SUMS\"; exit 1 }" ^
    "$expected = (($line -split '\s+', 2)[0]).ToLower();" ^
    "$actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $file).Hash.ToLower();" ^
    "if ($expected -ne $actual) { Write-Host \"[ERROR] SHA256 mismatch for $fn\"; Write-Host \"  expected: $expected\"; Write-Host \"  actual:   $actual\"; exit 1 }" ^
    "Write-Host \"[OK] SHA256 verified: $fn\""
set "_v_err=%ERRORLEVEL%"
set "OTGW_VAS_FILE="
set "OTGW_VAS_SUMS="
exit /b %_v_err%


:list_serial_ports
REM Enumerate USB-serial ports with VID/PID and description via Get-PnpDevice.
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "Get-PnpDevice -Class Ports -PresentOnly -ErrorAction SilentlyContinue |" ^
    "ForEach-Object {" ^
    "  $name = $_.FriendlyName;" ^
    "  $port = if ($name -match '\((COM\d+)\)') { $matches[1] } else { '' };" ^
    "  $vidpid = if ($_.InstanceId -match 'VID_([0-9A-Fa-f]{4})&PID_([0-9A-Fa-f]{4})') { ($matches[1] + ':' + $matches[2]).ToLower() } else { '?' };" ^
    "  if ($port) { '{0,-10} {1,-12} {2}' -f $port, $vidpid, $name }" ^
    "} | Sort-Object"
exit /b 0


:detect_port
REM %1 = output var name. Picks first port whose VID:PID is in USB_VID_PID_LIST,
REM falls back to the first SERIALCOMM entry.
set "OTGW_DP_LIST=%USB_VID_PID_LIST%"
for /f "usebackq delims=" %%P in (`powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$allow = ($env:OTGW_DP_LIST -split ';') | ForEach-Object { $_.ToLower() };" ^
    "$ports = Get-PnpDevice -Class Ports -PresentOnly -ErrorAction SilentlyContinue |" ^
    "  ForEach-Object {" ^
    "    $name = $_.FriendlyName;" ^
    "    $port = if ($name -match '\((COM\d+)\)') { $matches[1] } else { $null };" ^
    "    $vidpid = if ($_.InstanceId -match 'VID_([0-9A-Fa-f]{4})&PID_([0-9A-Fa-f]{4})') { ($matches[1] + ':' + $matches[2]).ToLower() } else { $null };" ^
    "    if ($port) { [pscustomobject]@{ Port = $port; VidPid = $vidpid } }" ^
    "  };" ^
    "$matched = $ports | Where-Object { $_.VidPid -and ($allow -contains $_.VidPid) } | Sort-Object Port | Select-Object -First 1;" ^
    "if ($matched) { $matched.Port; exit 0 }" ^
    "$reg = 'HKLM:\HARDWARE\DEVICEMAP\SERIALCOMM';" ^
    "if (Test-Path $reg) {" ^
    "  $first = Get-ItemProperty $reg | Get-Member -MemberType NoteProperty |" ^
    "    Where-Object { $_.Name -notlike 'PS*' } |" ^
    "    ForEach-Object { (Get-ItemProperty $reg).($_.Name) } | Sort-Object | Select-Object -First 1;" ^
    "  if ($first) { $first }" ^
    "}"`) do (
    set "%~1=%%P"
)
set "OTGW_DP_LIST="
exit /b 0


:download_release_binaries
REM %1 = destination directory (absolute path).
echo [INFO] Fetching latest release info from GitHub...
set "OTGW_DL_DIR=%~1"
set "OTGW_DL_REPO=%GITHUB_REPO%"
set "OTGW_DL_REDIRECT=%GITHUB_LATEST_REDIRECT%"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ProgressPreference='SilentlyContinue';" ^
    "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12;" ^
    "$dir = $env:OTGW_DL_DIR; $repo = $env:OTGW_DL_REPO; $fallback = $env:OTGW_DL_REDIRECT;" ^
    "$hdr = @{ 'User-Agent' = 'OTGW-Flash-Tool' };" ^
    "$apiOk = $false; $assets = $null;" ^
    "try {" ^
    "  $rel = Invoke-WebRequest -UseBasicParsing -Uri ('https://api.github.com/repos/' + $repo + '/releases/latest') -Headers $hdr -ErrorAction Stop | ConvertFrom-Json;" ^
    "  Write-Host ('[INFO] Release: ' + $rel.name);" ^
    "  $assets = $rel.assets; $apiOk = $true;" ^
    "} catch {" ^
    "  $code = $null; if ($_.Exception.Response) { $code = [int]$_.Exception.Response.StatusCode }" ^
    "  Write-Host ('[WARN] GitHub API failed (' + $code + ') - falling back to releases/latest/download/');" ^
    "}" ^
    "if (-not $apiOk) {" ^
    "  $sumsOut = Join-Path $dir 'SHA256SUMS';" ^
    "  try {" ^
    "    Invoke-WebRequest -UseBasicParsing -Headers $hdr -Uri ($fallback + '/SHA256SUMS') -OutFile $sumsOut -ErrorAction Stop;" ^
    "  } catch { Write-Host ('[ERROR] Fallback failed: could not fetch SHA256SUMS - ' + $_.Exception.Message); exit 1 }" ^
    "  $names = Get-Content -LiteralPath $sumsOut | ForEach-Object { ($_ -split '\s+', 2)[1].TrimStart('*') } | Where-Object { $_ -match '\.(ino|littlefs)\.bin$' };" ^
    "  foreach ($n in $names) {" ^
    "    $out = Join-Path $dir $n;" ^
    "    Write-Host ('[INFO] Downloading ' + $n + '...');" ^
    "    try {" ^
    "      Invoke-WebRequest -UseBasicParsing -Headers $hdr -Uri ($fallback + '/' + $n) -OutFile $out -ErrorAction Stop;" ^
    "      Write-Host ('[OK]   ' + $n);" ^
    "    } catch { Write-Host ('[ERROR] Download failed for ' + $n + ': ' + $_.Exception.Message); exit 1 }" ^
    "  }" ^
    "  exit 0" ^
    "}" ^
    "$found = 0;" ^
    "foreach ($a in $assets) {" ^
    "  if ($a.name -match '\.ino\.bin$' -or $a.name -match '\.littlefs\.bin$' -or $a.name -eq 'SHA256SUMS') {" ^
    "    $out = Join-Path $dir $a.name;" ^
    "    Write-Host ('[INFO] Downloading ' + $a.name + '...');" ^
    "    try {" ^
    "      Invoke-WebRequest -UseBasicParsing -Headers $hdr -Uri $a.browser_download_url -OutFile $out -ErrorAction Stop;" ^
    "      Write-Host ('[OK]   ' + $a.name);" ^
    "      $found++;" ^
    "    } catch { Write-Host ('[ERROR] Download failed for ' + $a.name + ': ' + $_.Exception.Message); exit 1 }" ^
    "  }" ^
    "}" ^
    "if ($found -eq 0) { Write-Host '[ERROR] No .ino.bin or .littlefs.bin assets found in release'; exit 1 }"
set "_dl_err=%ERRORLEVEL%"
set "OTGW_DL_DIR="
set "OTGW_DL_REPO="
set "OTGW_DL_REDIRECT="
exit /b %_dl_err%


:parse_args
if "%~1"=="" exit /b 0
if /I "%~1"=="--port"        ( set "ARG_PORT=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--baud"        ( set "ARG_BAUD=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--yes"         ( set "ARG_YES=1"    & shift & goto parse_args )
if /I "%~1"=="-y"            ( set "ARG_YES=1"    & shift & goto parse_args )
if /I "%~1"=="--list-ports"  ( set "ARG_LIST_PORTS=1" & shift & goto parse_args )
if /I "%~1"=="--help"        ( set "ARG_HELP=1"   & exit /b 0 )
if /I "%~1"=="-h"            ( set "ARG_HELP=1"   & exit /b 0 )
echo [ERROR] Unknown argument: %~1
echo Run "flash_otgw.bat --help" for usage.
exit /b %EXIT_BAD_ARGS%


:show_help
echo NAME
echo   flash_otgw.bat - Zero-install flash tool for OTGW-firmware ^(ESP8266^)
echo.
echo SYNOPSIS
echo   flash_otgw.bat [options]
echo.
echo DESCRIPTION
echo   Flashes the firmware ^(0x0^) and filesystem ^(0x200000^) onto a Nodoshop OTGW
echo   WiFi module. Looks for the release binaries next to this script:
echo       OTGW-firmware-*.ino.bin       firmware
echo       OTGW-firmware*.littlefs.bin   filesystem
echo   If either is missing, downloads the latest release from GitHub and
echo   verifies SHA256 against the release's SHA256SUMS asset.
echo.
echo   The serial port is auto-detected by USB VID/PID ^(CP210x, CH340, FTDI^).
echo   If multiple matching adapters are found, the first is used; pass --port
echo   to override.
echo.
echo OPTIONS
echo   --port COMx     Serial port ^(e.g. COM3, COM5^).
echo   --baud N        Override baud rate ^(default: 460800^).
echo   --yes, -y       Skip the "Continue? [y/N]" confirmation prompt.
echo   --list-ports    List detected serial ports ^(with VID/PID^) and exit.
echo   --help, -h      Show this help.
echo.
echo FIRST-RUN BEHAVIOUR
echo   Downloads esptool %ESPTOOL_VERSION% to .\tools\esptool\ ^(SHA256-verified^).
echo.
echo AFTER FLASHING
echo   Connect to WiFi AP "OTGW-^<MAC-address^>" and browse to
echo   http://192.168.4.1 to configure WiFi settings.
exit /b %EXIT_OK%
