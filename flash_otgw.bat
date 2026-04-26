@echo off
setlocal enabledelayedexpansion
title OTGW Flash Tool

REM ============================================================================
REM  flash_otgw.bat - Self-contained ESP flash tool for OTGW-firmware (Windows)
REM ----------------------------------------------------------------------------
REM  Distributed alongside merged binary releases. Downloads Espressif's
REM  standalone esptool binary on first run (no Python required).
REM
REM  Default behaviour (no flags): writes the merged-full image to flash WITHOUT
REM  erasing first. WiFi credentials in NVS survive; LittleFS settings are
REM  overwritten by the fresh filesystem image inside the merged-full bin.
REM
REM  Usage:
REM    flash_otgw.bat                     factory flash (preserves WiFi creds)
REM    flash_otgw.bat --upgrade           firmware-only (preserves WiFi + app
REM                                        settings; ESP32 only)
REM    flash_otgw.bat --erase             full clean wipe (loses everything)
REM    flash_otgw.bat --port COMx         use specific port
REM    flash_otgw.bat --bin <file>        use specific firmware file
REM    flash_otgw.bat --board esp8266     force board (otherwise from filename)
REM    flash_otgw.bat --board esp32       (Nodoshop OTGW32)
REM    flash_otgw.bat --baud N            override baud rate
REM    flash_otgw.bat --yes               skip confirmation prompts
REM    flash_otgw.bat --help              show this help
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
set "ARG_ERASE=0"
set "ARG_UPGRADE=0"
set "ARG_YES=0"

REM ---- Parse arguments -------------------------------------------------------
:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="--port"     ( set "ARG_PORT=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--bin"      ( set "ARG_BIN=%~2"  & shift & shift & goto parse_args )
if /I "%~1"=="--board"    ( set "ARG_BOARD=%~2"& shift & shift & goto parse_args )
if /I "%~1"=="--baud"     ( set "ARG_BAUD=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--erase"    ( set "ARG_ERASE=1"  & shift & goto parse_args )
if /I "%~1"=="--upgrade"  ( set "ARG_UPGRADE=1"& shift & goto parse_args )
if /I "%~1"=="--yes"      ( set "ARG_YES=1"    & shift & goto parse_args )
if /I "%~1"=="-y"         ( set "ARG_YES=1"    & shift & goto parse_args )
if /I "%~1"=="--help"     goto show_help
if /I "%~1"=="-h"         goto show_help
echo [ERROR] Unknown argument: %~1
echo Run "flash_otgw.bat --help" for usage.
exit /b 2
:args_done

if "%ARG_ERASE%"=="1" if "%ARG_UPGRADE%"=="1" (
    echo [ERROR] --erase and --upgrade are mutually exclusive.
    echo         --erase wipes everything including the filesystem.
    echo         --upgrade preserves the filesystem.
    exit /b 2
)

echo.
echo ============================================================
echo  OTGW Flash Tool (Windows)
echo  esptool standalone version: %ESPTOOL_VERSION%
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
REM   Mode selection:
REM     default            -> *-merged-full.bin   (full factory image; preserves WiFi)
REM     --upgrade  (esp32) -> *-merged.bin        (firmware-only; preserves WiFi + FS)
REM     --upgrade (esp8266)-> *.ino.bin           (firmware-only; preserves WiFi + FS)
REM     --erase            -> *-merged-full.bin   (full image + erase_all)
if not "%ARG_BIN%"=="" (
    if not exist "%ARG_BIN%" (
        echo [ERROR] Specified --bin file does not exist: %ARG_BIN%
        exit /b 1
    )
    set "BIN_FILE=%ARG_BIN%"
    goto bin_done
)

set "BIN_FILE="
if "%ARG_UPGRADE%"=="1" (
    REM Try ESP32 firmware-only merged first (has bootloader + partitions + app)
    for %%F in ("%SCRIPT_DIR%OTGW-firmware-esp32-*-merged.bin") do (
        if not defined BIN_FILE set "BIN_FILE=%%F"
    )
    REM Fall back to ESP8266 firmware-only (.ino.bin written at offset 0x0)
    if not defined BIN_FILE (
        for %%F in ("%SCRIPT_DIR%OTGW-firmware-esp8266-*.ino.bin") do (
            if not defined BIN_FILE set "BIN_FILE=%%F"
        )
    )
    REM Same searches in build/ (developer running from repo root)
    if not defined BIN_FILE (
        for %%F in ("%SCRIPT_DIR%build\OTGW-firmware-esp32-*-merged.bin") do (
            if not defined BIN_FILE set "BIN_FILE=%%F"
        )
    )
    if not defined BIN_FILE (
        for %%F in ("%SCRIPT_DIR%build\OTGW-firmware-esp8266-*.ino.bin") do (
            if not defined BIN_FILE set "BIN_FILE=%%F"
        )
    )
    if not defined BIN_FILE (
        echo [ERROR] --upgrade: no firmware-only bin found.
        echo         Expected: OTGW-firmware-esp32-*-merged.bin
        echo               or: OTGW-firmware-esp8266-*.ino.bin
        echo         Use --bin to specify a path.
        exit /b 1
    )
) else (
    for %%F in ("%SCRIPT_DIR%OTGW-firmware-*-merged-full.bin") do (
        if not defined BIN_FILE set "BIN_FILE=%%F"
    )
    if not defined BIN_FILE (
        for %%F in ("%SCRIPT_DIR%build\OTGW-firmware-*-merged-full.bin") do (
            if not defined BIN_FILE set "BIN_FILE=%%F"
        )
    )
    if not defined BIN_FILE (
        echo [ERROR] No OTGW-firmware-*-merged-full.bin found.
        echo         Expected in: %SCRIPT_DIR%
        echo                  or: %SCRIPT_DIR%build\
        echo         Use --bin to specify a path, or --upgrade for firmware-only.
        exit /b 1
    )
)

:bin_done
for %%F in ("%BIN_FILE%") do set "BIN_NAME=%%~nxF"
echo [OK] Firmware: %BIN_NAME%

REM ---- Step 3: derive board from filename (or user override) ----------------
if "%ARG_BOARD%"=="" (
    echo %BIN_NAME% | findstr /I /C:"-esp8266-" >nul && set "ARG_BOARD=esp8266"
    echo %BIN_NAME% | findstr /I /C:"-esp32-" >nul && set "ARG_BOARD=esp32"
)
if "%ARG_BOARD%"=="" (
    echo [ERROR] Could not detect board from filename. Use --board esp8266 ^| esp32
    exit /b 1
)

if /I "%ARG_BOARD%"=="esp8266" goto board_esp8266
if /I "%ARG_BOARD%"=="esp32"   goto board_esp32
echo [ERROR] Unknown board: %ARG_BOARD% (expected esp8266 or esp32)
exit /b 1

:board_esp8266
set "ESPTOOL_CHIP=esp8266"
set "BOARD_NAME=Nodoshop OTGW WiFi (ESP8266)"
if "%ARG_BAUD%"=="" set "ARG_BAUD=460800"
goto board_done

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
REM   so esptool can find it itself via --port-filter. ESP8266 boards use
REM   varied USB-serial chips (CH340 / CP2102 / FTDI), which makes a clean
REM   filter impractical; fall back to enumeration there.
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

REM ---- Step 5: confirm before flash -----------------------------------------
echo.
echo ------------------------------------------------------------
echo  Ready to flash:
echo    Firmware: %BIN_NAME%
echo    Board:    %BOARD_NAME%
echo    Baud:     %ARG_BAUD%
if "%ARG_ERASE%"=="1" (
    echo    Mode:     --erase  ^(full clean wipe^)
    echo    Effect:   ALL data wiped: WiFi credentials, NVS, filesystem.
) else if "%ARG_UPGRADE%"=="1" (
    echo    Mode:     --upgrade  ^(firmware-only^)
    echo    Effect:   WiFi credentials and app settings preserved.
    echo              Only the firmware app is updated.
) else (
    echo    Mode:     default factory flash
    echo    Effect:   WiFi credentials in NVS preserved.
    echo              Filesystem ^(MQTT/OTGW config^) replaced by fresh image.
)
echo ------------------------------------------------------------
echo.

if "%ARG_YES%"=="0" (
    set /p "CONFIRM=Type YES to continue: "
    if /I not "!CONFIRM!"=="YES" (
        echo [INFO] Aborted by user.
        exit /b 0
    )
)

REM ---- Step 6: run esptool --------------------------------------------------
REM   Tested baseline (Nodo-shop OT-Thing): -z compresses transfer; default-reset
REM   + hard-reset gives consistent strap timing on USB-JTAG boards. -e adds
REM   erase_all to write_flash for the --erase mode (single-pass, faster than
REM   a separate erase_flash).
set "WRITE_FLAGS=-z"
if "%ARG_ERASE%"=="1" set "WRITE_FLAGS=-z -e"

echo.
echo [STEP] Running esptool write_flash...
"%ESPTOOL_EXE%" --chip %ESPTOOL_CHIP% %ESPTOOL_PORT_ARGS% --baud %ARG_BAUD% --before default-reset --after hard-reset write_flash %WRITE_FLAGS% 0x0 "%BIN_FILE%"
if errorlevel 1 (
    echo [ERROR] write_flash failed.
    exit /b 1
)

echo.
echo ============================================================
echo  Flash complete. Reset the OTGW or unplug/replug USB.
if "%ARG_ERASE%"=="1" (
    echo  After reset: connect to WiFi AP "OTGW-AP" to configure.
) else (
    echo  WiFi credentials preserved; the board should rejoin
    echo  your network automatically. Browse to http://otgw.local
    echo  if mDNS works on your network.
)
echo ============================================================
exit /b 0


:show_help
echo flash_otgw.bat - Self-contained ESP flash tool for OTGW-firmware
echo.
echo Usage:
echo   flash_otgw.bat [options]
echo.
echo Mode:
echo   (no flag)            Default factory flash. Preserves WiFi credentials,
echo                        wipes filesystem (MQTT/OTGW config).
echo   --upgrade            Firmware-only flash. Preserves WiFi AND filesystem.
echo                        Picks *-merged.bin (esp32) or *.ino.bin (esp8266).
echo   --erase              Full clean wipe. Erases everything including WiFi.
echo.
echo Targeting:
echo   --port COMx          Serial port (auto-detected for esp32 via USB VID/PID,
echo                        port menu for esp8266).
echo   --bin ^<file^>         Firmware path. Overrides mode-based auto-detect.
echo   --board esp8266      Force board type.
echo   --board esp32        (Nodoshop OTGW32 = ESP32-S3)
echo   --baud N             Override baud rate (default: 460800/921600).
echo.
echo Other:
echo   --yes, -y            Skip the confirmation prompt.
echo   --help, -h           Show this help.
echo.
echo The script downloads esptool %ESPTOOL_VERSION% to .\tools\esptool\ on first run.
echo No Python required.
exit /b 0
