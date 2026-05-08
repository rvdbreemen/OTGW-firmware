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
REM  Serial port is auto-detected. If multiple ports are found the first one
REM  is used; supply --port to override.
REM
REM  Usage:
REM    flash_otgw.bat
REM    flash_otgw.bat --port COM3
REM    flash_otgw.bat --baud 115200
REM    flash_otgw.bat --help
REM
REM  Supported hardware: Nodoshop OTGW with Wemos D1 mini / NodeMCU (ESP8266)
REM ============================================================================

set "ESPTOOL_VERSION=v4.8.1"
set "SCRIPT_DIR=%~dp0"
set "TOOLS_DIR=%SCRIPT_DIR%tools\esptool"
set "ESPTOOL_EXE=%TOOLS_DIR%\esptool.exe"
set "DOWNLOAD_URL=https://github.com/espressif/esptool/releases/download/%ESPTOOL_VERSION%/esptool-%ESPTOOL_VERSION%-win64.zip"

set "ARG_PORT="
set "ARG_BAUD=460800"
set "ARG_HELP=0"

if not "%~1"=="" call :parse_args %*
if errorlevel 1 exit /b %ERRORLEVEL%
if "%ARG_HELP%"=="1" goto show_help

echo.
echo ============================================================
echo  OTGW Flash Tool (Windows)
echo  Target: Nodoshop OTGW WiFi module (ESP8266)
echo  esptool: %ESPTOOL_VERSION%
echo ============================================================
echo.

REM ---- Step 1: ensure esptool is present ------------------------------------
if exist "%ESPTOOL_EXE%" (
    echo [OK] esptool found
) else (
    echo [INFO] Downloading esptool from Espressif...
    if not exist "%TOOLS_DIR%" mkdir "%TOOLS_DIR%" >nul 2>&1
    set "ZIP_FILE=%TOOLS_DIR%\esptool.zip"

    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "$ProgressPreference='SilentlyContinue';" ^
        "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12;" ^
        "try { Invoke-WebRequest -UseBasicParsing -Uri '%DOWNLOAD_URL%' -OutFile '!ZIP_FILE!' -ErrorAction Stop }" ^
        "catch { Write-Host '[ERROR] Download failed:' $_.Exception.Message; exit 1 }"
    if errorlevel 1 (
        echo [ERROR] Could not download esptool. Check internet connection.
        exit /b 1
    )

    echo [INFO] Extracting esptool...
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "$ProgressPreference='SilentlyContinue';" ^
        "Expand-Archive -LiteralPath '!ZIP_FILE!' -DestinationPath '%TOOLS_DIR%' -Force"
    if errorlevel 1 (
        echo [ERROR] Could not extract esptool zip.
        exit /b 1
    )

    set "FOUND_EXE="
    for /f "delims=" %%F in ('dir /b /s "%TOOLS_DIR%\esptool.exe" 2^>nul') do (
        if not defined FOUND_EXE set "FOUND_EXE=%%F"
    )
    if not defined FOUND_EXE (
        echo [ERROR] Extracted archive did not contain esptool.exe
        exit /b 1
    )
    copy /Y "!FOUND_EXE!" "%ESPTOOL_EXE%" >nul
    del "!ZIP_FILE!" >nul 2>&1
    echo [OK] esptool installed
)

REM ---- Step 2: locate firmware and filesystem binaries ----------------------
set "FW_FILE="
set "FS_FILE="

for %%F in ("%SCRIPT_DIR%OTGW-firmware-*.ino.bin") do (
    if not defined FW_FILE set "FW_FILE=%%F"
)
for %%F in ("%SCRIPT_DIR%OTGW-firmware*.littlefs.bin") do (
    if not defined FS_FILE set "FS_FILE=%%F"
)

if not defined FW_FILE goto try_download
if not defined FS_FILE goto try_download
goto after_download

:try_download
call :download_release_binaries "%SCRIPT_DIR%"
if errorlevel 1 (
    echo [ERROR] Auto-download failed. Download the release binaries manually
    echo         and place them in the same directory as this script.
    exit /b 1
)
for %%F in ("%SCRIPT_DIR%OTGW-firmware-*.ino.bin") do (
    if not defined FW_FILE set "FW_FILE=%%F"
)
for %%F in ("%SCRIPT_DIR%OTGW-firmware*.littlefs.bin") do (
    if not defined FS_FILE set "FS_FILE=%%F"
)
if not defined FW_FILE (
    echo [ERROR] Firmware binary not found after download.
    exit /b 1
)
if not defined FS_FILE (
    echo [ERROR] Filesystem binary not found after download.
    exit /b 1
)

:after_download

for %%F in ("%FW_FILE%") do set "FW_NAME=%%~nxF"
for %%F in ("%FS_FILE%") do set "FS_NAME=%%~nxF"

echo [OK] Firmware:   %FW_NAME%
echo [OK] Filesystem: %FS_NAME%
echo [OK] Baud:       %ARG_BAUD%

REM ---- Step 3: auto-detect serial port --------------------------------------
if not "%ARG_PORT%"=="" (
    echo [OK] Port:       %ARG_PORT% (specified^)
) else (
    set "PORT_LIST_FILE=%TEMP%\otgw_ports_%RANDOM%.txt"
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "Get-CimInstance Win32_SerialPort | Sort-Object DeviceID |" ^
        "ForEach-Object { $_.DeviceID }" > "!PORT_LIST_FILE!"

    set "PORT_COUNT=0"
    for /f "usebackq tokens=*" %%A in ("!PORT_LIST_FILE!") do (
        set /a PORT_COUNT+=1
        set "PORT_!PORT_COUNT!=%%A"
    )
    del "!PORT_LIST_FILE!" >nul 2>&1

    if !PORT_COUNT! EQU 0 (
        echo [WARN] No serial ports detected automatically.
        echo        Connect your OTGW via USB, or install CP210x / CH340 USB-serial drivers.
        echo.
        set /p "ARG_PORT=Enter COM port manually (e.g. COM3), or press Enter to cancel: "
        if not defined ARG_PORT exit /b 1
        if "!ARG_PORT!"=="" exit /b 1
        echo [OK] Port:       !ARG_PORT! (manual^)
        goto after_port_detection
    )

    set "ARG_PORT=!PORT_1!"
    if !PORT_COUNT! EQU 1 (
        echo [OK] Port:       !ARG_PORT! (auto-detected^)
    ) else (
        echo [INFO] Multiple ports found - using first: !ARG_PORT!
        echo [INFO] Use --port to select a different port.
    )
)
:after_port_detection

REM ---- Step 4: summary ------------------------------------------------------
echo.
echo ------------------------------------------------------------
echo  Ready to flash:
echo    Firmware:   %FW_NAME%
echo    Filesystem: %FS_NAME%
echo    Port:       %ARG_PORT%
echo    Baud:       %ARG_BAUD%
echo.
echo    Erases flash, then writes firmware at 0x0
echo    and filesystem at 0x200000.
echo ------------------------------------------------------------
echo.

REM ---- Step 5: flash --------------------------------------------------------
echo [STEP] Flashing firmware and filesystem...

"%ESPTOOL_EXE%" --chip esp8266 --port %ARG_PORT% --baud %ARG_BAUD% --before default_reset --after hard_reset write_flash -z --erase-all 0x0 "%FW_FILE%" 0x200000 "%FS_FILE%"

if errorlevel 1 (
    echo.
    echo [ERROR] Flash failed.
    echo         Troubleshooting tips:
    echo           - Try a different USB cable (data cable, not charge-only^)
    echo           - Install CP210x or CH340 USB-serial drivers
    echo           - Try a lower baud rate: --baud 115200
    echo           - Specify the correct port: --port COM3
    exit /b 1
)

echo.
echo ============================================================
echo  Flash complete.
echo  Connect to WiFi AP "OTGW-^<MAC-address^>" and browse to
echo  http://192.168.4.1 to configure WiFi settings.
echo ============================================================
exit /b 0


:download_release_binaries
echo [INFO] Fetching latest release info from GitHub...
set "DL_DIR=%~1"
set "_DL_PS=%TEMP%\otgw_dl_%RANDOM%.ps1"
(
    echo $ProgressPreference = 'SilentlyContinue'
    echo [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    echo try {
    echo     $hdr = @{ 'User-Agent' = 'OTGW-Flash-Tool' }
    echo     $rel = Invoke-WebRequest -UseBasicParsing -Uri 'https://api.github.com/repos/rvdbreemen/OTGW-firmware/releases/latest' -Headers $hdr ^| ConvertFrom-Json
    echo     Write-Host "[INFO] Release: $($rel.name)"
    echo     $found = 0
    echo     foreach ($a in $rel.assets) {
    echo         if ($a.name -match '\.ino\.bin$' -or $a.name -match '\.littlefs\.bin$') {
    echo             $out = Join-Path '%DL_DIR%' $a.name
    echo             Write-Host "[INFO] Downloading $($a.name)..."
    echo             Invoke-WebRequest -UseBasicParsing -Uri $a.browser_download_url -OutFile $out -ErrorAction Stop
    echo             Write-Host "[OK]   $($a.name)"
    echo             $found++
    echo         }
    echo     }
    echo     if ($found -eq 0) { Write-Host '[ERROR] No .ino.bin or .littlefs.bin assets found in release'; exit 1 }
    echo } catch {
    echo     Write-Host "[ERROR] Download failed: $_"
    echo     exit 1
    echo }
) > "%_DL_PS%"
powershell -NoProfile -ExecutionPolicy Bypass -File "%_DL_PS%"
set "_dl_err=%ERRORLEVEL%"
del "%_DL_PS%" >nul 2>&1
exit /b %_dl_err%


:parse_args
if "%~1"=="" exit /b 0
if /I "%~1"=="--port"  ( set "ARG_PORT=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--baud"  ( set "ARG_BAUD=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--help"  ( set "ARG_HELP=1"   & exit /b 0 )
if /I "%~1"=="-h"      ( set "ARG_HELP=1"   & exit /b 0 )
echo [ERROR] Unknown argument: %~1
echo Run "flash_otgw.bat --help" for usage.
exit /b 2


:show_help
echo flash_otgw.bat - Zero-install flash tool for OTGW-firmware (ESP8266)
echo.
echo Usage:
echo   flash_otgw.bat [options]
echo.
echo Flashes both firmware and filesystem binaries found next to this script:
echo   OTGW-firmware-*.ino.bin       firmware   -^> 0x0
echo   OTGW-firmware*.littlefs.bin   filesystem -^> 0x200000
echo.
echo The serial port is auto-detected. If multiple ports are present, the first
echo one is used; specify --port to override.
echo.
echo Options:
echo   --port COMx     Serial port (e.g. COM3, COM5).
echo   --baud N        Override baud rate (default: 460800).
echo   --help, -h      Show this help.
echo.
echo First run:
echo   Downloads esptool %ESPTOOL_VERSION% to .\tools\esptool\ - no Python required.
echo.
echo After flashing:
echo   Connect to WiFi AP "OTGW-^<MAC-address^>" and browse to
echo   http://192.168.4.1 to configure WiFi settings.
exit /b 0
