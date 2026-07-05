@echo off
setlocal enabledelayedexpansion
title OTGW Flash Tool

REM ============================================================================
REM  flash_otgw.bat - Self-contained ESP flash tool for OTGW-firmware (Windows)
REM ----------------------------------------------------------------------------
REM  Downloads Espressif's standalone esptool binary on first run (no Python
REM  required). Two flashing modes:
REM
REM    FACTORY (default):  erases the whole flash and writes a single
REM                        *-merged-full.bin. WiFi credentials AND settings are
REM                        wiped. Use for a clean install / recovery.
REM
REM    UPDATE (--update):  writes the firmware image (app) and the filesystem
REM                        image (LittleFS) at their partition offsets WITHOUT
REM                        erasing flash. NVS is left untouched, so the stored
REM                        WiFi credentials (and other NVS state) SURVIVE. Use
REM                        to push a new build to an already-provisioned device.
REM
REM  Partition offsets (all 2.0.0 ESP32-S3 targets share partitions_otgw_esp32*.csv):
REM    app0 (firmware) = 0x10000    spiffs (LittleFS) = 0x270000    nvs = 0x9000
REM
REM  Usage:
REM    flash_otgw.bat                         (factory: auto-pick merged-full)
REM    flash_otgw.bat --bin <merged-full.bin> (factory: explicit image)
REM    flash_otgw.bat --update                (update: auto-pick app + fs, keep WiFi)
REM    flash_otgw.bat --update --app <fw.bin> --fs <littlefs.bin>
REM    flash_otgw.bat --update --app <fw.bin> (update: firmware only, keep WiFi)
REM    flash_otgw.bat --update --fs <littlefs.bin> (update: filesystem only, keep WiFi)
REM    flash_otgw.bat --port COMx  --board esp32  --baud N
REM    flash_otgw.bat --help
REM ============================================================================

set "ESPTOOL_VERSION=v4.8.1"
set "SCRIPT_DIR=%~dp0"
set "TOOLS_DIR=%SCRIPT_DIR%tools\esptool"
set "ESPTOOL_EXE=%TOOLS_DIR%\esptool.exe"
set "DOWNLOAD_URL=https://github.com/espressif/esptool/releases/download/%ESPTOOL_VERSION%/esptool-%ESPTOOL_VERSION%-win64.zip"

REM Partition offsets (see partitions_otgw_esp32*.csv). app0=0x10000, spiffs=0x270000.
set "APP_OFFSET=0x10000"
set "FS_OFFSET=0x270000"

set "ARG_PORT="
set "ARG_BIN="
set "ARG_APP="
set "ARG_FS="
set "ARG_BOARD="
set "ARG_BAUD="
set "ARG_UPDATE=0"
set "ARG_HELP=0"

REM ---- Parse arguments -------------------------------------------------------
if not "%~1"=="" call :parse_args %*
if errorlevel 1 exit /b %ERRORLEVEL%
if "%ARG_HELP%"=="1" goto show_help

if "%ARG_UPDATE%"=="1" ( set "MODE_NAME=update (keep WiFi credentials)" ) else ( set "MODE_NAME=factory reset (erase flash + WiFi credentials)" )

echo.
echo ============================================================
echo  OTGW Flash Tool (Windows)
echo  esptool standalone version: %ESPTOOL_VERSION%
echo  Mode: %MODE_NAME%
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

REM ---- Step 2: locate image(s) ----------------------------------------------
if "%ARG_UPDATE%"=="1" (
    call :select_update_images
    if errorlevel 1 exit /b %ERRORLEVEL%
) else (
    call :select_bin
    if errorlevel 1 exit /b %ERRORLEVEL%
    REM !BIN_FILE! (delayed) — select_bin set it INSIDE this block, so %BIN_FILE% would be stale.
    for %%F in ("!BIN_FILE!") do set "BIN_NAME=%%~nxF"
)

REM ---- Step 3: derive board from filename (or user override) ----------------
REM All 2.0.0 targets are ESP32-S3 (esp32-otgw32, esp32-classic, esp32-combo).
if "%ARG_BOARD%"=="" (
    if "%ARG_UPDATE%"=="1" (
        if defined APP_NAME echo !APP_NAME! | findstr /I /C:"-esp32" >nul && set "ARG_BOARD=esp32"
        if "!ARG_BOARD!"=="" if defined FS_NAME echo !FS_NAME! | findstr /I /C:"-esp32" >nul && set "ARG_BOARD=esp32"
    ) else (
        echo %BIN_NAME% | findstr /I /C:"-esp32" >nul && set "ARG_BOARD=esp32"
    )
)
REM Every 2.0.0 target is an ESP32-S3, so default the board when the filename did
REM not carry the token (raw .pio firmware.bin/littlefs.bin, or a custom name).
if "%ARG_BOARD%"=="" set "ARG_BOARD=esp32"

if /I "%ARG_BOARD%"=="esp32"   goto board_esp32
echo [ERROR] Unknown board: %ARG_BOARD% (expected esp32)
exit /b 1

:board_esp32
set "ESPTOOL_CHIP=esp32s3"
set "BOARD_NAME=Nodoshop OTGW32 / OTGW-classic ESP32-S3"
if "%ARG_BAUD%"=="" set "ARG_BAUD=921600"
goto board_done

:board_done
echo [OK] Board:    %BOARD_NAME%
echo [OK] Baud:     %ARG_BAUD%

REM ---- Step 4: locate serial port -------------------------------------------
REM   ESP32-S3 has a fixed USB VID/PID (303A:1001 = built-in USB-Serial JTAG),
REM   so esptool can find it itself via --port-filter when no explicit --port
REM   is given. With more than one S3 attached, pass --port COMx to disambiguate.
if "%ARG_PORT%"=="" (
    set "ESPTOOL_PORT_ARGS=--port-filter vid=0x303A --port-filter pid=0x1001"
    echo [OK] Port:     auto-detect via USB VID/PID 303A:1001
) else (
    set "ESPTOOL_PORT_ARGS=--port %ARG_PORT%"
    echo [OK] Port:     %ARG_PORT%
)

REM ---- Step 5: show selected flash action -----------------------------------
echo.
echo ------------------------------------------------------------
echo  Ready to flash:
if "%ARG_UPDATE%"=="1" ( call :summary_update ) else ( call :summary_factory )
echo ------------------------------------------------------------
echo.

REM ---- Step 6: run esptool --------------------------------------------------
echo.
echo [STEP] Running esptool write_flash...
if "%ARG_UPDATE%"=="1" (
    set "WRITE_ARGS="
    if defined APP_FILE set "WRITE_ARGS=!WRITE_ARGS! !APP_OFFSET! "!APP_FILE!""
    if defined FS_FILE  set "WRITE_ARGS=!WRITE_ARGS! !FS_OFFSET! "!FS_FILE!""
    "%ESPTOOL_EXE%" --chip %ESPTOOL_CHIP% %ESPTOOL_PORT_ARGS% --baud %ARG_BAUD% --before default_reset --after hard_reset write_flash -z !WRITE_ARGS!
) else (
    "%ESPTOOL_EXE%" --chip %ESPTOOL_CHIP% %ESPTOOL_PORT_ARGS% --baud %ARG_BAUD% --before default_reset --after hard_reset write_flash -z -e 0x0 "%BIN_FILE%"
)
if errorlevel 1 (
    echo [ERROR] write_flash failed.
    exit /b 1
)

echo.
echo ============================================================
echo  Flash complete.
if "%ARG_UPDATE%"=="1" ( call :done_update ) else ( call :done_factory )
echo ============================================================
exit /b 0

:summary_update
echo    Mode:     UPDATE  no-erase, WiFi credentials preserved
if defined APP_FILE echo    Firmware: !APP_NAME!   -^> !APP_OFFSET!
if defined FS_FILE  echo    Filesys:  !FS_NAME!   -^> !FS_OFFSET!
echo    Board:    %BOARD_NAME%
echo    Baud:     %ARG_BAUD%
echo    Effect:   Overwrites app/filesystem only. NVS WiFi + settings kept.
exit /b 0

:summary_factory
echo    Mode:     FACTORY RESET  full erase
echo    Firmware: %BIN_NAME%
echo    Board:    %BOARD_NAME%
echo    Baud:     %ARG_BAUD%
echo    Effect:   Erases flash, then writes firmware + filesystem.
echo              WiFi credentials and settings WILL be removed.
exit /b 0

:done_update
echo  Update flashed. WiFi credentials preserved - the device
echo  should rejoin your network on reboot with no re-config.
exit /b 0

:done_factory
echo  Factory reset done. After reset, connect to the open
echo  "OTGW-^<MAC^>" AP to configure WiFi.
exit /b 0


:parse_args
if "%~1"=="" exit /b 0
if /I "%~1"=="--port"     ( set "ARG_PORT=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--bin"      ( set "ARG_BIN=%~2"  & shift & shift & goto parse_args )
if /I "%~1"=="--app"      ( set "ARG_APP=%~2"  & shift & shift & goto parse_args )
if /I "%~1"=="--fs"       ( set "ARG_FS=%~2"   & shift & shift & goto parse_args )
if /I "%~1"=="--board"    ( set "ARG_BOARD=%~2"& shift & shift & goto parse_args )
if /I "%~1"=="--baud"     ( set "ARG_BAUD=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--update"   ( set "ARG_UPDATE=1" & shift & goto parse_args )
if /I "%~1"=="--factory"  ( set "ARG_UPDATE=0" & shift & goto parse_args )
if /I "%~1"=="--help"     ( set "ARG_HELP=1" & exit /b 0 )
if /I "%~1"=="-h"         ( set "ARG_HELP=1" & exit /b 0 )
echo [ERROR] Unknown argument: %~1
echo Run "flash_otgw.bat --help" for usage.
exit /b 2


:select_bin
REM FACTORY mode: locate a single *-merged-full.bin (explicit --bin wins).
set "BIN_FILE="
if not "%ARG_BIN%"=="" (
    if not exist "%ARG_BIN%" (
        echo [ERROR] Specified --bin file does not exist: %ARG_BIN%
        exit /b 1
    )
    set "BIN_FILE=%ARG_BIN%"
    exit /b 0
)
set "BIN_COUNT=0"
for %%F in ("%SCRIPT_DIR%OTGW-firmware-*-merged-full.bin") do call :add_bin "%%F"
if !BIN_COUNT! EQU 0 (
    for %%F in ("%SCRIPT_DIR%build\OTGW-firmware-*-merged-full.bin") do call :add_bin "%%F"
)
if !BIN_COUNT! EQU 0 (
    echo [ERROR] No merged-full bin found.
    echo         Expected in: %SCRIPT_DIR%
    echo                  or: %SCRIPT_DIR%build\
    echo         Use --bin to specify a path, or --update to flash separate app+fs.
    exit /b 1
)
if !BIN_COUNT! EQU 1 (
    set "BIN_FILE=!BIN_1!"
    exit /b 0
)
echo.
echo [INFO] Multiple merged-full images found:
for /l %%I in (1,1,!BIN_COUNT!) do echo   [%%I] !BIN_%%I_NAME!
echo.
set /p "BIN_CHOICE=Select firmware number [1-!BIN_COUNT!]: "
call set "BIN_FILE=%%BIN_!BIN_CHOICE!%%"
if "!BIN_FILE!"=="" (
    echo [ERROR] Invalid firmware selection.
    exit /b 1
)
exit /b 0


:select_update_images
REM UPDATE mode: resolve the app (firmware) and filesystem (LittleFS) images.
REM Explicit --app/--fs win. Otherwise auto-detect release-named artifacts, then
REM fall back to raw .pio build outputs. At least one of app/fs must resolve.
set "APP_FILE="
set "FS_FILE="
set "APP_NAME="
set "FS_NAME="

if not "%ARG_APP%"=="" (
    if not exist "%ARG_APP%" ( echo [ERROR] --app file not found: %ARG_APP% & exit /b 1 )
    set "APP_FILE=%ARG_APP%"
)
if not "%ARG_FS%"=="" (
    if not exist "%ARG_FS%" ( echo [ERROR] --fs file not found: %ARG_FS% & exit /b 1 )
    set "FS_FILE=%ARG_FS%"
)

REM Auto-detect app (firmware) when not given explicitly.
if not defined APP_FILE (
    for %%F in ("%SCRIPT_DIR%OTGW-firmware-*.ino.bin") do if not defined APP_FILE set "APP_FILE=%%F"
    if not defined APP_FILE for %%F in ("%SCRIPT_DIR%build\OTGW-firmware-*.ino.bin") do if not defined APP_FILE set "APP_FILE=%%F"
    if not defined APP_FILE for %%F in ("%SCRIPT_DIR%.pio\build\*\firmware.bin") do if not defined APP_FILE set "APP_FILE=%%F"
)
REM Auto-detect filesystem (LittleFS) when not given explicitly.
if not defined FS_FILE (
    for %%F in ("%SCRIPT_DIR%OTGW-firmware-*.littlefs.bin") do if not defined FS_FILE set "FS_FILE=%%F"
    if not defined FS_FILE for %%F in ("%SCRIPT_DIR%build\OTGW-firmware-*.littlefs.bin") do if not defined FS_FILE set "FS_FILE=%%F"
    if not defined FS_FILE for %%F in ("%SCRIPT_DIR%.pio\build\*\littlefs.bin") do if not defined FS_FILE set "FS_FILE=%%F"
)

if not defined APP_FILE if not defined FS_FILE (
    echo [ERROR] Update mode found neither a firmware nor a filesystem image.
    echo         Provide --app ^<firmware.bin^> and/or --fs ^<littlefs.bin^>, or run
    echo         a build so .pio\build\^<env^>\firmware.bin / littlefs.bin exist.
    exit /b 1
)
if defined APP_FILE for %%N in ("%APP_FILE%") do set "APP_NAME=%%~nxN"
if defined FS_FILE  for %%N in ("%FS_FILE%")  do set "FS_NAME=%%~nxN"
if defined APP_FILE echo [OK] Firmware: %APP_NAME%
if defined FS_FILE  echo [OK] Filesys:  %FS_NAME%
exit /b 0


:add_bin
set /a BIN_COUNT+=1
set "BIN_!BIN_COUNT!=%~1"
for %%N in ("%~1") do set "BIN_!BIN_COUNT!_NAME=%%~nxN"
exit /b 0


:show_help
echo flash_otgw.bat - Self-contained ESP flash tool for OTGW-firmware
echo.
echo Two modes:
echo   FACTORY (default)  Erase flash, write a single *-merged-full.bin.
echo                      WiFi credentials and settings are REMOVED.
echo   UPDATE  (--update) Write firmware (app) and/or filesystem (LittleFS)
echo                      WITHOUT erasing. NVS WiFi credentials are PRESERVED.
echo.
echo Usage:
echo   flash_otgw.bat                          Factory: auto-pick merged-full.
echo   flash_otgw.bat --bin ^<merged-full.bin^>   Factory: explicit merged image.
echo   flash_otgw.bat --update                  Update: auto-pick app + fs, keep WiFi.
echo   flash_otgw.bat --update --app ^<fw.bin^> --fs ^<littlefs.bin^>
echo   flash_otgw.bat --update --app ^<fw.bin^>   Update firmware only, keep WiFi.
echo   flash_otgw.bat --update --fs ^<littlefs.bin^>  Update filesystem only, keep WiFi.
echo.
echo Targeting:
echo   --port COMx          Serial port (auto-detect via USB VID/PID 303A:1001).
echo                        Pass explicitly when more than one ESP32-S3 is attached.
echo   --board esp32        Force board type (all 2.0.0 S3 targets).
echo   --baud N             Override baud rate (default: 921600).
echo   --factory / --update Select mode explicitly (default is factory).
echo.
echo Offsets: app0=0x10000, spiffs/LittleFS=0x270000 (partitions_otgw_esp32*.csv).
echo Downloads esptool %ESPTOOL_VERSION% to .\tools\esptool\ on first run. No Python required.
exit /b 0
