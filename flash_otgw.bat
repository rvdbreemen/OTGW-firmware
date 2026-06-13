@echo off
setlocal enabledelayedexpansion
title OTGW Flash Tool

REM ============================================================================
REM  flash_otgw.bat - Self-contained ESP flash tool for OTGW-firmware (Windows)
REM ----------------------------------------------------------------------------
REM  Distributed alongside merged binary releases. Downloads Espressif's
REM  standalone esptool binary on first run (no Python required).
REM
REM  Default behaviour: factory reset. The script erases flash and writes the
REM  merged-full image containing firmware and filesystem.
REM
REM  Usage:
REM    flash_otgw.bat
REM    flash_otgw.bat --port COMx
REM    flash_otgw.bat --bin <merged-full.bin>
REM    flash_otgw.bat --board esp32       (Nodoshop OTGW32 / ESP32-S3)
REM    flash_otgw.bat --baud N
REM    flash_otgw.bat --help
REM ============================================================================

set "ESPTOOL_VERSION=v4.8.1"
set "SCRIPT_DIR=%~dp0"
set "TOOLS_DIR=%SCRIPT_DIR%tools\esptool"
set "ESPTOOL_EXE=%TOOLS_DIR%\esptool.exe"
set "DOWNLOAD_URL=https://github.com/espressif/esptool/releases/download/%ESPTOOL_VERSION%/esptool-%ESPTOOL_VERSION%-win64.zip"

set "ARG_PORT="
set "ARG_BIN="
set "ARG_BOARD="
set "ARG_BAUD="
set "ARG_HELP=0"

REM ---- Parse arguments -------------------------------------------------------
if not "%~1"=="" call :parse_args %*
if errorlevel 1 exit /b %ERRORLEVEL%
if "%ARG_HELP%"=="1" goto show_help

echo.
echo ============================================================
echo  OTGW Flash Tool (Windows)
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

REM ---- Step 2: locate firmware bin ------------------------------------------
call :select_bin
if errorlevel 1 exit /b %ERRORLEVEL%
for %%F in ("%BIN_FILE%") do set "BIN_NAME=%%~nxF"

REM ---- Step 3: derive board from filename (or user override) ----------------
REM All 2.0.0 targets are ESP32-S3 (esp32-otgw32, esp32-classic, esp32-combo),
REM so any merged-full image maps to the esp32 board.
if "%ARG_BOARD%"=="" (
    echo %BIN_NAME% | findstr /I /C:"-esp32-" >nul && set "ARG_BOARD=esp32"
)
if "%ARG_BOARD%"=="" (
    echo [ERROR] Could not detect board from filename. Use --board esp32
    exit /b 1
)

if /I "%ARG_BOARD%"=="esp32"   goto board_esp32
echo [ERROR] Unknown board: %ARG_BOARD% (expected esp32)
exit /b 1

:board_esp32
set "ESPTOOL_CHIP=esp32s3"
set "BOARD_NAME=Nodoshop OTGW32 (ESP32-S3)"
if "%ARG_BAUD%"=="" set "ARG_BAUD=921600"
goto board_done

:board_done
echo [OK] Board:    %BOARD_NAME%
echo [OK] Baud:     %ARG_BAUD%

REM ---- Step 4: locate serial port -------------------------------------------
REM   ESP32-S3 has a fixed USB VID/PID (303A:1001 = built-in USB-Serial JTAG),
REM   so esptool can find it itself via --port-filter when no explicit --port
REM   is given.
if "%ARG_PORT%"=="" (
    if /I "%ARG_BOARD%"=="esp32" (
        set "ESPTOOL_PORT_ARGS=--port-filter vid=0x303A --port-filter pid=0x1001"
        echo [OK] Port:     auto-detect via USB VID/PID 303A:1001
        goto port_done
    )

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
        echo [ERROR] No serial ports found. Connect your OTGW via USB.
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
    set "ESPTOOL_PORT_ARGS=--port !ARG_PORT!"
    echo [OK] Port:     !ARG_PORT!
) else (
    set "ESPTOOL_PORT_ARGS=--port %ARG_PORT%"
    echo [OK] Port:     %ARG_PORT%
)
:port_done

echo [OK] Firmware: %BIN_NAME%

REM ---- Step 5: show selected flash action -----------------------------------
echo.
echo ------------------------------------------------------------
echo  Ready to flash:
echo    Firmware: %BIN_NAME%
echo    Board:    %BOARD_NAME%
echo    Baud:     %ARG_BAUD%
echo    Mode:     factory reset
echo    Effect:   Erases flash, then writes firmware and filesystem.
echo              WiFi credentials and settings will be removed.
echo ------------------------------------------------------------
echo.

REM ---- Step 6: run esptool --------------------------------------------------
echo.
echo [STEP] Running esptool write_flash...
"%ESPTOOL_EXE%" --chip %ESPTOOL_CHIP% %ESPTOOL_PORT_ARGS% --baud %ARG_BAUD% --before default_reset --after hard_reset write_flash -z -e 0x0 "%BIN_FILE%"
if errorlevel 1 (
    echo [ERROR] write_flash failed.
    exit /b 1
)

echo.
echo ============================================================
echo  Flash complete.
echo  After reset: connect to WiFi AP "OTGW-AP" to configure.
echo ============================================================
exit /b 0


:parse_args
if "%~1"=="" exit /b 0
if /I "%~1"=="--port"     ( set "ARG_PORT=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--bin"      ( set "ARG_BIN=%~2"  & shift & shift & goto parse_args )
if /I "%~1"=="--board"    ( set "ARG_BOARD=%~2"& shift & shift & goto parse_args )
if /I "%~1"=="--baud"     ( set "ARG_BAUD=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--help"     ( set "ARG_HELP=1" & exit /b 0 )
if /I "%~1"=="-h"         ( set "ARG_HELP=1" & exit /b 0 )
echo [ERROR] Unknown argument: %~1
echo Run "flash_otgw.bat --help" for usage.
exit /b 2


:select_bin
set "BIN_FILE="

REM Explicit --bin always wins, no enumeration.
if not "%ARG_BIN%"=="" (
    if not exist "%ARG_BIN%" (
        echo [ERROR] Specified --bin file does not exist: %ARG_BIN%
        exit /b 1
    )
    set "BIN_FILE=%ARG_BIN%"
    exit /b 0
)

REM Collect all merged-full candidates. Prefer the script dir; fall back to build\.
REM A release zip carries exactly one bin -> single match -> auto-selected. When
REM several coexist (e.g. both ESP32-S3 targets, esp32 and esp32-combo, in build\)
REM we list them and let the user choose instead of silently grabbing the first.
set "BIN_COUNT=0"
for %%F in ("%SCRIPT_DIR%OTGW-firmware-*-merged-full.bin") do call :add_bin "%%F"
if !BIN_COUNT! EQU 0 (
    for %%F in ("%SCRIPT_DIR%build\OTGW-firmware-*-merged-full.bin") do call :add_bin "%%F"
)

if !BIN_COUNT! EQU 0 (
    echo [ERROR] No merged-full bin found.
    echo         Expected in: %SCRIPT_DIR%
    echo                  or: %SCRIPT_DIR%build\
    echo         Use --bin to specify a path.
    exit /b 1
)

if !BIN_COUNT! EQU 1 (
    set "BIN_FILE=!BIN_1!"
    exit /b 0
)

echo.
echo [INFO] Multiple firmware images found:
for /l %%I in (1,1,!BIN_COUNT!) do echo   [%%I] !BIN_%%I_NAME!
echo.
set /p "BIN_CHOICE=Select firmware number [1-!BIN_COUNT!]: "
call set "BIN_FILE=%%BIN_!BIN_CHOICE!%%"
if "!BIN_FILE!"=="" (
    echo [ERROR] Invalid firmware selection.
    exit /b 1
)
exit /b 0


:add_bin
REM Append one bin path to the indexed BIN_<n> / BIN_<n>_NAME lists.
set /a BIN_COUNT+=1
set "BIN_!BIN_COUNT!=%~1"
for %%N in ("%~1") do set "BIN_!BIN_COUNT!_NAME=%%~nxN"
exit /b 0


:show_help
echo flash_otgw.bat - Self-contained ESP flash tool for OTGW-firmware
echo.
echo Usage:
echo   flash_otgw.bat [options]
echo.
echo Default:
echo   Factory reset: erase flash, then write firmware and filesystem from
echo   the merged-full image. WiFi credentials and settings are removed.
echo.
echo Targeting:
echo   --port COMx          Serial port (auto-detected via USB VID/PID 303A:1001).
echo   --bin ^<file^>         Firmware path. Use a merged-full image.
echo   --board esp32        Force board type (Nodoshop OTGW32 = ESP32-S3).
echo   --baud N             Override baud rate (default: 921600).
echo.
echo Other:
echo   --help, -h           Show this help.
echo.
echo The script downloads esptool %ESPTOOL_VERSION% to .\tools\esptool\ on first run.
echo No Python required.
exit /b 0
