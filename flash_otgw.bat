@echo off
setlocal enabledelayedexpansion
title OTGW Flash Tool

REM ============================================================================
REM  flash_otgw.bat - Self-contained ESP flash tool for OTGW-firmware (Windows)
REM ----------------------------------------------------------------------------
REM  Distributed alongside merged binary releases. Downloads Espressif's
REM  standalone esptool binary on first run (no Python required).
REM
REM  Usage:
REM    flash_otgw.bat                     auto-detect bin and COM port
REM    flash_otgw.bat --port COM4         use specific port
REM    flash_otgw.bat --bin <file>        use specific firmware file
REM    flash_otgw.bat --board esp8266     force board (otherwise from filename)
REM    flash_otgw.bat --board esp32       (Nodoshop OTGW32)
REM    flash_otgw.bat --baud 921600       override baud rate
REM    flash_otgw.bat --no-erase          skip erase_flash before write_flash
REM    flash_otgw.bat --yes               skip all confirmation prompts
REM    flash_otgw.bat --help              show this help
REM ============================================================================

set "ESPTOOL_VERSION=v4.8.1"
set "SCRIPT_DIR=%~dp0"
set "TOOLS_DIR=%SCRIPT_DIR%tools\esptool"
set "ESPTOOL_EXE=%TOOLS_DIR%\esptool.exe"
set "DOWNLOAD_URL=https://github.com/espressif/esptool/releases/download/%ESPTOOL_VERSION%/esptool-%ESPTOOL_VERSION%-win64.zip"

REM Defaults (filled from args / auto-detect)
set "ARG_PORT="
set "ARG_BIN="
set "ARG_BOARD="
set "ARG_BAUD="
set "ARG_NO_ERASE=0"
set "ARG_YES=0"

REM ---- Parse arguments -------------------------------------------------------
:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="--port"     ( set "ARG_PORT=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--bin"      ( set "ARG_BIN=%~2"  & shift & shift & goto parse_args )
if /I "%~1"=="--board"    ( set "ARG_BOARD=%~2"& shift & shift & goto parse_args )
if /I "%~1"=="--baud"     ( set "ARG_BAUD=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--no-erase" ( set "ARG_NO_ERASE=1"& shift & goto parse_args )
if /I "%~1"=="--yes"      ( set "ARG_YES=1"    & shift & goto parse_args )
if /I "%~1"=="-y"         ( set "ARG_YES=1"    & shift & goto parse_args )
if /I "%~1"=="--help"     goto show_help
if /I "%~1"=="-h"         goto show_help
echo [ERROR] Unknown argument: %~1
echo Run "flash_otgw.bat --help" for usage.
exit /b 2
:args_done

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

    REM Locate esptool.exe in the extracted tree (release ships in subdir)
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
if not "%ARG_BIN%"=="" (
    if not exist "%ARG_BIN%" (
        echo [ERROR] Specified --bin file does not exist: %ARG_BIN%
        exit /b 1
    )
    set "BIN_FILE=%ARG_BIN%"
) else (
    REM Search order: same dir as script, then ./build/
    set "BIN_FILE="
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
        echo         Use --bin to specify a path.
        exit /b 1
    )
)

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
)
echo [OK] Port:     %ARG_PORT%

REM ---- Step 5: confirm before destructive flash -----------------------------
echo.
echo ------------------------------------------------------------
echo  Ready to flash:
echo    Firmware: %BIN_NAME%
echo    Board:    %BOARD_NAME%
echo    Port:     %ARG_PORT%  @ %ARG_BAUD% baud
if "%ARG_NO_ERASE%"=="1" (
    echo    Flash:    write_flash only (NOT erasing first)
) else (
    echo    Flash:    erase_flash + write_flash
    echo              All settings, WiFi credentials and stored data WILL BE WIPED.
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
if "%ARG_NO_ERASE%"=="0" (
    echo.
    echo [STEP] Running esptool erase_flash...
    "%ESPTOOL_EXE%" --chip %ESPTOOL_CHIP% --port %ARG_PORT% --baud %ARG_BAUD% erase_flash
    if errorlevel 1 (
        echo [ERROR] erase_flash failed.
        exit /b 1
    )
)

echo.
echo [STEP] Running esptool write_flash 0x0 %BIN_NAME%...
"%ESPTOOL_EXE%" --chip %ESPTOOL_CHIP% --port %ARG_PORT% --baud %ARG_BAUD% write_flash 0x0 "%BIN_FILE%"
if errorlevel 1 (
    echo [ERROR] write_flash failed.
    exit /b 1
)

echo.
echo ============================================================
echo  Flash complete. Reset the OTGW or unplug/replug USB.
echo  Then connect to WiFi AP "OTGW-AP" to configure credentials,
echo  or browse to http://otgw.local once on your network.
echo ============================================================
exit /b 0


REM ---- Help ------------------------------------------------------------------
:show_help
echo flash_otgw.bat - Self-contained ESP flash tool for OTGW-firmware
echo.
echo Usage:
echo   flash_otgw.bat [options]
echo.
echo Options:
echo   --port COMx          Serial port (auto-detected if omitted)
echo   --bin ^<file^>         Firmware path (auto-detect *-merged-full.bin if omitted)
echo   --board esp8266      Force board type
echo   --board esp32        (Nodoshop OTGW32 = ESP32-S3)
echo   --baud N             Override baud rate (default: 460800 esp8266 / 921600 esp32)
echo   --no-erase           Skip erase_flash before write_flash (preserves data, risky)
echo   --yes, -y            Skip all confirmation prompts (for automation)
echo   --help, -h           Show this help
echo.
echo The script downloads esptool %ESPTOOL_VERSION% to .\tools\esptool\ on first run.
echo No Python required.
exit /b 0
