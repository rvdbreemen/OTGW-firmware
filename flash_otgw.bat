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
REM    UPDATE (--update):  writes the firmware image (app) and/or the filesystem
REM                        image (LittleFS) at their partition offsets WITHOUT
REM                        erasing flash. NVS is left untouched, so the stored
REM                        WiFi credentials (and other NVS state) SURVIVE. The
REM                        settings (settings.ini) live on LittleFS: writing a
REM                        filesystem image resets them, an app-only update keeps
REM                        them. With --app and/or --fs only those images are
REM                        written; bare --update auto-picks both.
REM
REM    MIGRATE (--migrate): one-time move of a unit to a new partition layout
REM                        (ADR-191 safeboot layout, TASK-1223.8). Saves
REM                        settings.ini over HTTP (--host), writes the two
REM                        *-migrate-*.bin images around NVS (WiFi kept), waits
REM                        for the unit, restores settings.ini and checks it.
REM
REM  Partition offsets (all 2.0.0 ESP32-S3 targets share partitions_otgw_esp32.csv, ADR-191):
REM    safeboot = 0x10000   app0 (firmware) = 0x100000   spiffs (LittleFS) = 0x330000   nvs = 0x9000
REM  --update refuses when the unit's partition table is not the TOOL_LAYOUT
REM  below; such a unit needs --migrate first.
REM
REM  Usage:
REM    flash_otgw.bat                         (factory: auto-pick merged-full)
REM    flash_otgw.bat --bin <merged-full.bin> (factory: explicit image)
REM    flash_otgw.bat --update                (update: auto-pick app + fs, keep WiFi, resets settings)
REM    flash_otgw.bat --update --app <fw.bin> --fs <littlefs.bin>
REM    flash_otgw.bat --update --app <fw.bin> (update: firmware only, keep WiFi)
REM    flash_otgw.bat --update --fs <littlefs.bin> (update: filesystem only, keep WiFi, resets settings)
REM    flash_otgw.bat --migrate --host <ip> [--password <pw>] [--no-backup]
REM    flash_otgw.bat --port COMx  --board esp32  --baud N
REM    flash_otgw.bat --help
REM ============================================================================

set "ESPTOOL_VERSION=v4.8.1"
set "SCRIPT_DIR=%~dp0"
set "TOOLS_DIR=%SCRIPT_DIR%tools\esptool"
set "ESPTOOL_EXE=%TOOLS_DIR%\esptool.exe"
set "DOWNLOAD_URL=https://github.com/espressif/esptool/releases/download/%ESPTOOL_VERSION%/esptool-%ESPTOOL_VERSION%-win64.zip"

REM Partition offsets (see partitions_otgw_esp32.csv). app0=0x100000, spiffs=0x330000.
set "APP_OFFSET=0x100000"
set "FS_OFFSET=0x330000"
REM Layout the images of this release are built for: "legacy" (app0 @0x10000)
REM or "safeboot" (ADR-191: safeboot factory @0x10000, app0 @0x100000).
REM The unit's layout is recognised by a "safeboot" label in its partition table.
set "TOOL_LAYOUT=safeboot"
REM Migration images: slice 1 at 0x0, slice 2 right after NVS (0x9000 + 0x5000).
set "MIGRATE_REST_OFFSET=0xE000"

set "ARG_PORT="
set "ARG_BIN="
set "ARG_APP="
set "ARG_FS="
set "ARG_BOARD="
set "ARG_BAUD="
set "ARG_UPDATE=0"
set "ARG_MIGRATE=0"
set "ARG_HOST="
set "ARG_PASSWORD="
set "ARG_NOBACKUP=0"
set "ARG_HELP=0"

REM ---- Parse arguments -------------------------------------------------------
if not "%~1"=="" call :parse_args %*
if errorlevel 1 exit /b %ERRORLEVEL%
if "%ARG_HELP%"=="1" goto show_help

if "%ARG_UPDATE%"=="1" ( set "MODE_NAME=update (keep WiFi credentials)" ) else ( set "MODE_NAME=factory reset (erase flash + WiFi credentials)" )
if "%ARG_MIGRATE%"=="1" (
    set "ARG_UPDATE=0"
    set "MODE_NAME=migrate to the new partition layout (keep WiFi, restore settings)"
    if "%ARG_HOST%"=="" if "%ARG_NOBACKUP%"=="0" (
        echo [ERROR] --migrate needs --host ^<device ip or name^> to save and restore settings.ini,
        echo         or --no-backup to migrate without keeping the settings.
        exit /b 2
    )
)

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
if "%ARG_MIGRATE%"=="1" (
    call :select_migrate_images
    if errorlevel 1 exit /b 1
    set "ARG_BOARD=esp32"
) else if "%ARG_UPDATE%"=="1" (
    call :select_update_images
    if errorlevel 1 exit /b 1
) else (
    call :select_bin
    if errorlevel 1 exit /b 1
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
if "%ARG_MIGRATE%"=="1" ( call :summary_migrate ) else if "%ARG_UPDATE%"=="1" ( call :summary_update ) else ( call :summary_factory )
echo ------------------------------------------------------------
echo.

REM ---- Step 5b: update needs the unit on this release's layout --------------
if "%ARG_UPDATE%"=="1" (
    call :check_device_layout
    if errorlevel 1 exit /b 1
)

REM ---- Step 5c: migrate saves settings.ini first ----------------------------
if "%ARG_MIGRATE%"=="1" if "%ARG_NOBACKUP%"=="0" (
    call :migrate_backup
    if errorlevel 1 exit /b 1
)

REM ---- Step 6: run esptool --------------------------------------------------
echo.
echo [STEP] Running esptool write_flash...
if "%ARG_MIGRATE%"=="1" (
    "%ESPTOOL_EXE%" --chip %ESPTOOL_CHIP% %ESPTOOL_PORT_ARGS% --baud %ARG_BAUD% --before default_reset --after hard_reset write_flash -z 0x0 "!MIG_FIRST!" %MIGRATE_REST_OFFSET% "!MIG_REST!"
) else if "%ARG_UPDATE%"=="1" (
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

if "%ARG_MIGRATE%"=="1" if "%ARG_NOBACKUP%"=="0" (
    call :migrate_restore
    if errorlevel 1 exit /b 1
)

echo.
echo ============================================================
echo  Flash complete.
if "%ARG_MIGRATE%"=="1" ( call :done_migrate ) else if "%ARG_UPDATE%"=="1" ( call :done_update ) else ( call :done_factory )
echo ============================================================
exit /b 0

:summary_update
echo    Mode:     UPDATE  no-erase, WiFi credentials preserved
if defined APP_FILE echo    Firmware: !APP_NAME!   -^> !APP_OFFSET!
if defined FS_FILE  echo    Filesys:  !FS_NAME!   -^> !FS_OFFSET!
echo    Board:    %BOARD_NAME%
echo    Baud:     %ARG_BAUD%
if defined FS_FILE (
    echo    Effect:   WiFi credentials ^(NVS^) kept. The filesystem is REPLACED:
    echo              settings.ini and every other file on it are lost. Save
    echo              http://^<device^>/settings.ini first if you want to keep them.
) else (
    echo    Effect:   Firmware only. WiFi credentials and settings kept.
)
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
if defined FS_FILE echo  The filesystem was replaced: settings are back at defaults. Restore a saved settings.ini through the File Explorer, or reconfigure.
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
if /I "%~1"=="--migrate"  ( set "ARG_MIGRATE=1" & shift & goto parse_args )
if /I "%~1"=="--host"     ( set "ARG_HOST=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--password" ( set "ARG_PASSWORD=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--no-backup" ( set "ARG_NOBACKUP=1" & shift & goto parse_args )
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

REM An explicit --app or --fs means exactly that image: auto-detection only runs
REM when neither was given. Pairing a picked-up LittleFS image with an explicit
REM --app silently replaced the filesystem, and with it settings.ini (TASK-1160).
if defined APP_FILE goto :select_update_done
if defined FS_FILE  goto :select_update_done

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

:select_update_done
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


:select_migrate_images
REM MIGRATE mode: a matching *-migrate-0x0.bin + *-migrate-rest.bin pair (build.py
REM cuts both from the merged-full image around NVS). Release folder first, then build\.
REM --bin <...-migrate-0x0.bin> picks one explicitly. Without it exactly one
REM candidate must exist: the images are board-specific, so with several targets
REM in build\ the tool refuses to guess.
set "MIG_FIRST="
set "MIG_REST="
set "MIG_COUNT=0"
if not "%ARG_BIN%"=="" (
    if not exist "%ARG_BIN%" ( echo [ERROR] --bin file not found: %ARG_BIN% & exit /b 1 )
    set "MIG_FIRST=%ARG_BIN%"
    goto :migrate_have_first
)
for %%F in ("%SCRIPT_DIR%OTGW-firmware-*-migrate-0x0.bin") do ( set /a MIG_COUNT+=1 & set "MIG_FIRST=%%F" )
if !MIG_COUNT! EQU 0 for %%F in ("%SCRIPT_DIR%build\OTGW-firmware-*-migrate-0x0.bin") do ( set /a MIG_COUNT+=1 & set "MIG_FIRST=%%F" )
if !MIG_COUNT! EQU 0 (
    echo [ERROR] No *-migrate-0x0.bin found next to this script or in build\.
    exit /b 1
)
if !MIG_COUNT! GTR 1 (
    echo [ERROR] Several migration images found; they are board-specific. Pick one with --bin:
    for %%F in ("%SCRIPT_DIR%OTGW-firmware-*-migrate-0x0.bin" "%SCRIPT_DIR%build\OTGW-firmware-*-migrate-0x0.bin") do echo           %%~nxF
    exit /b 1
)
:migrate_have_first
set "MIG_REST=!MIG_FIRST:-migrate-0x0.bin=-migrate-rest.bin!"
if not exist "!MIG_REST!" (
    echo [ERROR] Missing the matching image: !MIG_REST!
    exit /b 1
)
for %%N in ("!MIG_FIRST!") do set "MIG_FIRST_NAME=%%~nxN"
for %%N in ("!MIG_REST!") do set "MIG_REST_NAME=%%~nxN"
echo [OK] Migrate:  !MIG_FIRST_NAME! + !MIG_REST_NAME!
exit /b 0


:check_device_layout
REM Read the unit's partition table (0x8000) and refuse an --update whose images
REM are built for a different layout: writing the app to the wrong offset leaves
REM a unit that does not boot.
set "PT_FILE=%TEMP%\otgw-pt-%RANDOM%.bin"
echo [STEP] Reading the partition table of the unit...
"%ESPTOOL_EXE%" --chip %ESPTOOL_CHIP% %ESPTOOL_PORT_ARGS% --baud %ARG_BAUD% --before default_reset --after no_reset read_flash 0x8000 0x1000 "%PT_FILE%" >nul
if errorlevel 1 (
    echo [ERROR] Could not read the partition table from the unit.
    exit /b 1
)
set "DEVICE_LAYOUT=legacy"
powershell -NoProfile -Command "if ([Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes('%PT_FILE%')).Contains('safeboot')) { exit 0 } else { exit 1 }"
if not errorlevel 1 set "DEVICE_LAYOUT=safeboot"
del "%PT_FILE%" >nul 2>&1
echo [OK] Unit layout: !DEVICE_LAYOUT!   images: %TOOL_LAYOUT%
if /I not "!DEVICE_LAYOUT!"=="%TOOL_LAYOUT%" (
    echo [ERROR] This unit uses the !DEVICE_LAYOUT! partition layout; these images are for %TOOL_LAYOUT%.
    echo         Run a one-time migration first:
    echo           flash_otgw.bat --migrate --host ^<device ip^>
    exit /b 1
)
exit /b 0


:migrate_backup
REM Save settings.ini (and dallas_labels.ini when present) over HTTP before the
REM filesystem is replaced. Stops the migration when settings.ini cannot be saved.
set "CURL_AUTH="
if not "%ARG_PASSWORD%"=="" set "CURL_AUTH=-u admin:%ARG_PASSWORD%"
for /f %%T in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "STAMP=%%T"
set "BACKUP_DIR=%SCRIPT_DIR%backup\%ARG_HOST%-!STAMP!"
mkdir "!BACKUP_DIR!" >nul 2>&1
echo [STEP] Saving settings from http://%ARG_HOST% to !BACKUP_DIR!
curl.exe -s -f --max-time 20 !CURL_AUTH! -o "!BACKUP_DIR!\settings.ini" "http://%ARG_HOST%/settings.ini"
if errorlevel 1 (
    echo [ERROR] Could not download settings.ini from http://%ARG_HOST%/ . Migration stopped, nothing written.
    exit /b 1
)
for %%A in ("!BACKUP_DIR!\settings.ini") do set "BACKUP_SIZE=%%~zA"
if "!BACKUP_SIZE!"=="0" (
    echo [ERROR] settings.ini downloaded empty. Migration stopped, nothing written.
    exit /b 1
)
curl.exe -s -f --max-time 20 !CURL_AUTH! -o "!BACKUP_DIR!\dallas_labels.ini" "http://%ARG_HOST%/dallas_labels.ini"
if errorlevel 1 del "!BACKUP_DIR!\dallas_labels.ini" >nul 2>&1
echo [OK] Saved settings.ini (!BACKUP_SIZE! bytes)
exit /b 0


:migrate_restore
REM After the migration flash the unit boots with default settings. Wait for it,
REM upload the saved files, restart it and check settings.ini came back intact.
call :wait_for_host
if errorlevel 1 exit /b 1
echo [STEP] Restoring settings.ini
curl.exe -s -f --max-time 30 !CURL_AUTH! -o nul -F "upload=@!BACKUP_DIR!\settings.ini;filename=settings.ini" "http://%ARG_HOST%/upload?path=/"
if errorlevel 1 (
    echo [ERROR] Upload of settings.ini failed. Restore it by hand from !BACKUP_DIR! via the File Explorer.
    exit /b 1
)
if exist "!BACKUP_DIR!\dallas_labels.ini" curl.exe -s -f --max-time 30 !CURL_AUTH! -o nul -F "upload=@!BACKUP_DIR!\dallas_labels.ini;filename=dallas_labels.ini" "http://%ARG_HOST%/upload?path=/"
curl.exe -s --max-time 10 !CURL_AUTH! -o nul "http://%ARG_HOST%/ReBoot"
REM Give the unit time to go down before polling it again.
ping -n 11 127.0.0.1 >nul
call :wait_for_host
if errorlevel 1 exit /b 1
curl.exe -s -f --max-time 20 !CURL_AUTH! -o "!BACKUP_DIR!\settings-after.ini" "http://%ARG_HOST%/settings.ini"
fc /b "!BACKUP_DIR!\settings.ini" "!BACKUP_DIR!\settings-after.ini" >nul
if errorlevel 1 (
    echo [ERROR] settings.ini on the unit differs from the saved copy. Restore it by hand from !BACKUP_DIR!.
    exit /b 1
)
echo [OK] Settings restored and verified (byte-identical)
exit /b 0


:wait_for_host
echo [STEP] Waiting for http://%ARG_HOST% (up to 3 minutes)...
for /l %%I in (1,1,60) do (
    curl.exe -s -f --max-time 3 -o nul !CURL_AUTH! "http://%ARG_HOST%/api/v2/device/info" && (echo [OK] Unit is back & exit /b 0)
    ping -n 4 127.0.0.1 >nul
)
echo [ERROR] The unit did not come back at http://%ARG_HOST%/ . Saved settings: !BACKUP_DIR!
exit /b 1


:summary_migrate
echo    Mode:     MIGRATE  one-time move to the new partition layout
echo    Images:   !MIG_FIRST_NAME!   -^> 0x0
echo              !MIG_REST_NAME!   -^> %MIGRATE_REST_OFFSET%
echo    Board:    %BOARD_NAME%
if "%ARG_NOBACKUP%"=="1" (
    echo    Effect:   WiFi credentials ^(NVS^) kept. Settings are NOT saved: they return to defaults.
) else (
    echo    Effect:   WiFi credentials ^(NVS^) kept. settings.ini is saved from %ARG_HOST% first
    echo              and restored after the flash.
)
exit /b 0

:done_migrate
echo  Migration done. The unit runs the new layout; from now on firmware
echo  updates work over WiFi through the /update page.
if "%ARG_NOBACKUP%"=="0" echo  Saved settings: !BACKUP_DIR!
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
echo   flash_otgw.bat --update                  Update: auto-pick app + fs, keep WiFi, resets settings.
echo   flash_otgw.bat --update --app ^<fw.bin^> --fs ^<littlefs.bin^>
echo   flash_otgw.bat --update --app ^<fw.bin^>   Update firmware only, keep WiFi.
echo   flash_otgw.bat --update --fs ^<littlefs.bin^>  Update filesystem only, keep WiFi, resets settings.
echo   flash_otgw.bat --migrate --host ^<ip^>     One-time move to a new partition layout: saves
echo                                            settings.ini, writes the *-migrate-*.bin images
echo                                            (WiFi kept), then restores and checks settings.ini.
echo     --password ^<pw^>   admin password, when the unit has one set
echo     --no-backup         migrate without saving settings (they return to defaults)
echo     --bin ^<x-migrate-0x0.bin^>  pick the board's migration image when several are present
echo.
echo   --update checks the unit's partition table first and refuses when it needs --migrate.
echo.
echo Targeting:
echo   --port COMx          Serial port (auto-detect via USB VID/PID 303A:1001).
echo                        Pass explicitly when more than one ESP32-S3 is attached.
echo   --board esp32        Force board type (all 2.0.0 S3 targets).
echo   --baud N             Override baud rate (default: 921600).
echo   --factory / --update Select mode explicitly (default is factory).
echo.
echo Offsets: safeboot=0x10000, app0=0x100000, spiffs/LittleFS=0x330000 (partitions_otgw_esp32.csv).
echo Downloads esptool %ESPTOOL_VERSION% to .\tools\esptool\ on first run. No Python required.
exit /b 0
