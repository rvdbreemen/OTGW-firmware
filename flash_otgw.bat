@echo off
setlocal enabledelayedexpansion
title OTGW Flash Tool

REM ============================================================================
REM  flash_otgw.bat - Self-contained flash tool for OTGW-firmware (Windows)
REM ----------------------------------------------------------------------------
REM  Distributed alongside release binaries. Downloads Espressif's standalone
REM  esptool binary on first run - no Python required.
REM
REM  Default behaviour: factory reset. The script erases flash and writes the
REM  merged binary containing both firmware and filesystem.
REM
REM  Usage:
REM    flash_otgw.bat
REM    flash_otgw.bat --port COM3
REM    flash_otgw.bat --bin OTGW-firmware-1.5.0-merged.bin
REM    flash_otgw.bat --baud N
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
set "ARG_BIN="
set "ARG_BAUD=460800"
set "ARG_HELP=0"

REM ---- Parse arguments -------------------------------------------------------
if not "%~1"=="" call :parse_args %*
if errorlevel 1 exit /b %ERRORLEVEL%
if "%ARG_HELP%"=="1" goto show_help

echo.
echo ============================================================
echo  OTGW Flash Tool (Windows)
echo  Target: Nodoshop OTGW WiFi module (ESP8266)
echo  esptool standalone version: %ESPTOOL_VERSION%
echo  Mode: factory reset ^(erase flash, write firmware + filesystem^)
echo ============================================================
echo.

REM ---- Step 1: ensure esptool is present ------------------------------------
if exist "%ESPTOOL_EXE%" (
    echo [OK] esptool found: %ESPTOOL_EXE%
) else (
    echo [INFO] esptool not found. Downloading from Espressif...
    echo        URL: %DOWNLOAD_URL%
    if not exist "%TOOLS_DIR%" mkdir "%TOOLS_DIR%" >nul 2>&1
    set "ZIP_FILE=%TOOLS_DIR%\esptool.zip"

    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "$ProgressPreference='SilentlyContinue';" ^
        "try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12;" ^
        "Invoke-WebRequest -UseBasicParsing -Uri '%DOWNLOAD_URL%' -OutFile '!ZIP_FILE!' -ErrorAction Stop }" ^
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

    for /f "delims=" %%F in ('dir /b /s "%TOOLS_DIR%\esptool.exe" 2^>nul') do (
        copy /Y "%%F" "%ESPTOOL_EXE%" >nul
        goto esptool_copied
    )
    echo [ERROR] Extracted archive did not contain esptool.exe
    exit /b 1
    :esptool_copied
    del "!ZIP_FILE!" >nul 2>&1
    echo [OK] esptool installed: %ESPTOOL_EXE%
)

REM ---- Step 2: locate merged firmware binary --------------------------------
call :select_bin
if errorlevel 1 exit /b %ERRORLEVEL%
for %%F in ("%BIN_FILE%") do set "BIN_NAME=%%~nxF"

echo [OK] Firmware: %BIN_NAME%
echo [OK] Baud:     %ARG_BAUD%

REM ---- Step 3: locate serial port -------------------------------------------
if "%ARG_PORT%"=="" (
    echo.
    echo [INFO] Detecting available serial ports...
    set "PORT_LIST_FILE=%TEMP%\otgw_ports_%RANDOM%.txt"
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "Get-CimInstance Win32_SerialPort | Sort-Object DeviceID | ForEach-Object { '{0}`t{1}' -f $_.DeviceID, $_.Description }" > "!PORT_LIST_FILE!"

    set "PORT_COUNT=0"
    for /f "tokens=1,* delims=	" %%A in ('type "!PORT_LIST_FILE!"') do (
        set /a PORT_COUNT+=1
        set "PORT_!PORT_COUNT!=%%A"
        echo   [!PORT_COUNT!] %%A   %%B
    )

    if !PORT_COUNT! EQU 0 (
        echo [ERROR] No serial ports found. Connect your OTGW via USB and try again.
        echo         Install CP210x or CH340 USB-serial drivers if the port is missing.
        del "!PORT_LIST_FILE!" >nul 2>&1
        exit /b 1
    )

    if !PORT_COUNT! EQU 1 (
        set "ARG_PORT=!PORT_1!"
        echo [OK] Auto-selected port: !ARG_PORT!
    ) else (
        echo.
        set /p "PORT_CHOICE=Select port number [1-!PORT_COUNT!]: "
        call set "ARG_PORT=%%PORT_!PORT_CHOICE!%%"
        if "!ARG_PORT!"=="" (
            echo [ERROR] Invalid port selection.
            del "!PORT_LIST_FILE!" >nul 2>&1
            exit /b 1
        )
    )
    del "!PORT_LIST_FILE!" >nul 2>&1
    echo [OK] Port:     !ARG_PORT!
) else (
    echo [OK] Port:     %ARG_PORT%
)

REM ---- Step 4: show selected flash action -----------------------------------
echo.
echo ------------------------------------------------------------
echo  Ready to flash:
echo    Firmware: %BIN_NAME%
echo    Baud:     %ARG_BAUD%
echo    Mode:     factory reset
echo    Effect:   Erases flash, then writes firmware and filesystem.
echo              WiFi credentials and settings will be removed.
echo ------------------------------------------------------------
echo.

REM ---- Step 5: run esptool --------------------------------------------------
echo [STEP] Running esptool write_flash...
"%ESPTOOL_EXE%" --chip esp8266 --port %ARG_PORT% --baud %ARG_BAUD% --before default_reset --after hard_reset write_flash -z -e 0x0 "%BIN_FILE%"
if errorlevel 1 (
    echo [ERROR] write_flash failed.
    echo         Troubleshooting tips:
    echo           - Try a different USB cable (data cable, not charge-only)
    echo           - Install CP210x or CH340 USB-serial drivers
    echo           - Try a lower baud rate: --baud 115200
    exit /b 1
)

echo.
echo ============================================================
echo  Flash complete.
echo  After reset: connect to WiFi AP "OTGW-^<MAC-address^>" and
echo  browse to http://192.168.4.1 to configure WiFi settings.
echo ============================================================
exit /b 0


:parse_args
if "%~1"=="" exit /b 0
if /I "%~1"=="--port"  ( set "ARG_PORT=%~2"  & shift & shift & goto parse_args )
if /I "%~1"=="--bin"   ( set "ARG_BIN=%~2"   & shift & shift & goto parse_args )
if /I "%~1"=="--baud"  ( set "ARG_BAUD=%~2"  & shift & shift & goto parse_args )
if /I "%~1"=="--help"  ( set "ARG_HELP=1" & exit /b 0 )
if /I "%~1"=="-h"      ( set "ARG_HELP=1" & exit /b 0 )
echo [ERROR] Unknown argument: %~1
echo Run "flash_otgw.bat --help" for usage.
exit /b 2


:select_bin
set "BIN_FILE="
if "%ARG_BIN%"=="" (
    for %%F in ("%SCRIPT_DIR%OTGW-firmware-*-merged.bin") do (
        if not defined BIN_FILE set "BIN_FILE=%%F"
    )
    if not defined BIN_FILE (
        for %%F in ("%SCRIPT_DIR%build\OTGW-firmware-*-merged.bin") do (
            if not defined BIN_FILE set "BIN_FILE=%%F"
        )
    )
    if not defined BIN_FILE (
        echo [ERROR] No merged firmware binary found.
        echo         Expected: OTGW-firmware-*-merged.bin
        echo         Search locations: %SCRIPT_DIR%
        echo                       or: %SCRIPT_DIR%build\
        echo         Use --bin to specify a path, or build the merged binary with:
        echo           python build.py --merged
        echo         (builds firmware + filesystem and creates the merged binary)
        exit /b 1
    )
) else (
    if not exist "%ARG_BIN%" (
        echo [ERROR] Specified --bin file does not exist: %ARG_BIN%
        exit /b 1
    )
    set "BIN_FILE=%ARG_BIN%"
)
exit /b 0


:show_help
echo flash_otgw.bat - Self-contained flash tool for OTGW-firmware (ESP8266)
echo.
echo Usage:
echo   flash_otgw.bat [options]
echo.
echo Default:
echo   Factory reset: erase flash, then write firmware and filesystem from
echo   the merged binary. WiFi credentials and settings are removed.
echo.
echo Options:
echo   --port COMx     Serial port (auto-detected when only one is present).
echo   --bin ^<file^>    Path to the merged binary (OTGW-firmware-*-merged.bin).
echo                   Auto-detected in the script directory or build\ subfolder.
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
