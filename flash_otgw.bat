@echo off
setlocal enabledelayedexpansion
title OTGW Flash Tool

REM ============================================================================
REM  flash_otgw.bat - Self-contained ESP flash tool for OTGW-firmware (Windows)
REM ----------------------------------------------------------------------------
REM  Distributed alongside merged binary releases. Downloads Espressif's
REM  standalone esptool binary on first run (no Python required).
REM
REM  Default behaviour (no flags): asks which install path to use and
REM  recommends one based on a flash probe.
REM
REM  Usage:
REM    flash_otgw.bat                     interactive chooser with recommendation
REM    flash_otgw.bat --upgrade           force firmware-only upgrade
REM    flash_otgw.bat --factory           force full image flash
REM    flash_otgw.bat --erase             full clean wipe (loses everything)
REM    flash_otgw.bat --port COMx         use specific port
REM    flash_otgw.bat --bin <file>        use specific firmware file
REM    flash_otgw.bat --board esp8266     force board (otherwise from filename)
REM    flash_otgw.bat --board esp32       (Nodoshop OTGW32)
REM    flash_otgw.bat --baud N            override baud rate
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
set "ARG_HOST="
set "ARG_ERASE=0"
set "ARG_UPGRADE=0"
set "ARG_FACTORY=0"
set "ARG_PRESERVE_SETTINGS=0"
set "BACKUP_DIR="
set "HOST_BASE="

REM ---- Parse arguments -------------------------------------------------------
:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="--port"     ( set "ARG_PORT=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--bin"      ( set "ARG_BIN=%~2"  & shift & shift & goto parse_args )
if /I "%~1"=="--board"    ( set "ARG_BOARD=%~2"& shift & shift & goto parse_args )
if /I "%~1"=="--baud"     ( set "ARG_BAUD=%~2" & shift & shift & goto parse_args )
if /I "%~1"=="--erase"    ( set "ARG_ERASE=1"  & shift & goto parse_args )
if /I "%~1"=="--upgrade"  ( set "ARG_UPGRADE=1"& shift & goto parse_args )
if /I "%~1"=="--factory"  ( set "ARG_FACTORY=1"& shift & goto parse_args )
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
if "%ARG_ERASE%"=="1" if "%ARG_FACTORY%"=="1" (
    echo [ERROR] --erase and --factory are mutually exclusive.
    echo         --erase wipes everything including the filesystem.
    echo         --factory flashes the merged-full filesystem image.
    exit /b 2
)
if "%ARG_UPGRADE%"=="1" if "%ARG_FACTORY%"=="1" (
    echo [ERROR] --upgrade and --factory are mutually exclusive.
    echo         Both already select different flash layouts.
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
REM     --upgrade           -> *-merged.bin       (firmware-only; preserves WiFi + FS)
REM     --factory           -> *-merged-full.bin  (full image; updates filesystem)
REM     --erase             -> *-merged-full.bin  (full image + erase_all)
call :select_bin

:bin_done
for %%F in ("%BIN_FILE%") do set "BIN_NAME=%%~nxF"

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

if "%ARG_BIN%"=="" if "%ARG_ERASE%"=="0" if "%ARG_UPGRADE%"=="0" if "%ARG_FACTORY%"=="0" (
    call :probe_flash_blank
    if "!FLASH_IS_BLANK!"=="1" (
        set "FLASH_DEFAULT_MODE=1"
    ) else (
        set "FLASH_DEFAULT_MODE=2"
    )

    echo.
    echo ------------------------------------------------------------
    echo  Choose flash mode:
    echo    [1] Factory reset
    echo        Fresh install of firmware and filesystem.
    echo        Removes WiFi credentials and settings.
    echo    [2] Upgrade OTGW
    echo        Refreshes firmware and filesystem.
    echo        Keeps WiFi credentials; settings are reset.
    echo    [3] Firmware-only upgrade
    echo        Updates firmware only.
    echo        Keeps WiFi credentials and settings.
    echo ------------------------------------------------------------
    set /p "FLASH_CHOICE=Select option [1-3] (default !FLASH_DEFAULT_MODE!): "
    if "!FLASH_CHOICE!"=="" set "FLASH_CHOICE=!FLASH_DEFAULT_MODE!"
    if "!FLASH_CHOICE!"=="1" (
        set "ARG_ERASE=1"
    ) else if "!FLASH_CHOICE!"=="2" (
        set "ARG_FACTORY=1"
    ) else if "!FLASH_CHOICE!"=="3" (
        set "ARG_UPGRADE=1"
    ) else (
        echo [ERROR] Invalid selection.
        exit /b 1
    )

    call :select_bin
    for %%F in ("%BIN_FILE%") do set "BIN_NAME=%%~nxF"
)

if "%ARG_FACTORY%"=="1" if "%ARG_ERASE%"=="0" call :prompt_preserve_settings
if "%ARG_PRESERVE_SETTINGS%"=="1" if "%ARG_ERASE%"=="0" (
    call :backup_live_files
    if errorlevel 1 exit /b 1
)

echo [OK] Firmware: %BIN_NAME%

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
    echo    Use:      only when you want a factory reset.
) else if "%ARG_FACTORY%"=="1" (
    echo    Mode:     --factory  ^(full image^)
    echo    Effect:   Filesystem image is refreshed.
    echo              Existing WiFi/settings may be replaced.
) else if "%ARG_UPGRADE%"=="1" (
    echo    Mode:     --upgrade  ^(firmware-only^)
    echo    Effect:   WiFi credentials and filesystem/settings preserved.
    echo              Only the firmware app is updated.
) else (
    echo    Mode:     firmware-only
    echo    Effect:   WiFi credentials and filesystem/settings preserved.
)
echo ------------------------------------------------------------
echo.

REM ---- Step 6: run esptool --------------------------------------------------
REM   Tested baseline (Nodo-shop OT-Thing): -z compresses transfer; default-reset
REM   + hard-reset gives consistent strap timing on USB-JTAG boards. -e adds
REM   erase_all to write_flash for the --erase mode (single-pass, faster than
REM   a separate erase_flash).
set "WRITE_FLAGS=-z"
if "%ARG_ERASE%"=="1" set "WRITE_FLAGS=-z -e"

echo.
echo [STEP] Running esptool write_flash...
"%ESPTOOL_EXE%" --chip %ESPTOOL_CHIP% %ESPTOOL_PORT_ARGS% --baud %ARG_BAUD% --before default_reset --after hard_reset write_flash %WRITE_FLAGS% 0x0 "%BIN_FILE%"
if errorlevel 1 (
    echo [ERROR] write_flash failed.
    exit /b 1
)

echo.
echo ============================================================
echo  Flash complete.
if "%ARG_PRESERVE_SETTINGS%"=="1" (
    call :wait_for_health "after flash" 240
    if "!HEALTH_OK!"=="1" (
        call :restore_live_files
        call :trigger_reboot
        call :wait_for_health "after restore" 240
        if "!HEALTH_OK!"=="1" (
            echo  Settings and Dallas labels were restored from !ARG_HOST!.
        ) else (
            echo  [WARN] Device did not report healthy after restore within timeout.
        )
    ) else (
        echo  [WARN] Device did not report healthy after flash within timeout.
    )
) else if "%ARG_ERASE%"=="1" (
    echo  After reset: connect to WiFi AP "OTGW-AP" to configure.
) else (
    echo  WiFi credentials and settings preserved; the board should rejoin
    echo  your network automatically. Browse to http://otgw.local
    echo  if mDNS works on your network.
)
echo ============================================================
exit /b 0


:prompt_preserve_settings
set "PRESERVE_CHOICE="
set /p "PRESERVE_CHOICE=Preserve current settings via backup/restore? [Y/n]: "
if "%PRESERVE_CHOICE%"=="" set "PRESERVE_CHOICE=Y"
if /I "%PRESERVE_CHOICE%"=="Y" (
    set "ARG_PRESERVE_SETTINGS=1"
    set "ARG_HOST="
    set /p "ARG_HOST=Enter OTGW hostname or IP [otgw.local]: "
    if "%ARG_HOST%"=="" set "ARG_HOST=otgw.local"
    set "HOST_BASE=http://%ARG_HOST%"
) else (
    set "ARG_PRESERVE_SETTINGS=0"
)
exit /b 0


:backup_live_files
where curl.exe >nul 2>&1
if errorlevel 1 (
    echo [ERROR] curl.exe not found. Windows backup/restore requires curl.
    exit /b 1
)
set "BACKUP_DIR=%TEMP%\otgw_flash_%RANDOM%"
mkdir "%BACKUP_DIR%" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Could not create backup directory.
    exit /b 1
)
echo [STEP] Backing up settings.ini from %HOST_BASE%
curl.exe -fsS --connect-timeout 5 --retry 2 --retry-delay 1 -o "%BACKUP_DIR%\settings.ini" "%HOST_BASE%/settings.ini"
if errorlevel 1 (
    echo [ERROR] Could not download settings.ini from %HOST_BASE%.
    exit /b 1
)
echo [OK] Saved settings backup: %BACKUP_DIR%\settings.ini
echo [STEP] Backing up Dallas labels from %HOST_BASE%
curl.exe -fsS --connect-timeout 5 --retry 2 --retry-delay 1 -o "%BACKUP_DIR%\dallas_labels.ini" "%HOST_BASE%/api/v2/sensors/labels"
if errorlevel 1 (
    echo [ERROR] Could not download Dallas labels from %HOST_BASE%.
    exit /b 1
)
echo [OK] Saved Dallas labels backup: %BACKUP_DIR%\dallas_labels.ini
exit /b 0


:restore_live_files
where curl.exe >nul 2>&1
if errorlevel 1 (
    echo [ERROR] curl.exe not found. Windows backup/restore requires curl.
    exit /b 1
)
echo [STEP] Restoring settings.ini to %HOST_BASE%
curl.exe -fsS --connect-timeout 5 --retry 2 --retry-delay 1 -X POST -F "path=/" -F "upload=@%BACKUP_DIR%\settings.ini;filename=settings.ini" "%HOST_BASE%/upload" >nul
if errorlevel 1 (
    echo [ERROR] Could not restore settings.ini.
    exit /b 1
)
echo [OK] Restored settings.ini
echo [STEP] Restoring Dallas labels to %HOST_BASE%
curl.exe -fsS --connect-timeout 5 --retry 2 --retry-delay 1 -X POST -F "path=/" -F "upload=@%BACKUP_DIR%\dallas_labels.ini;filename=dallas_labels.ini" "%HOST_BASE%/upload" >nul
if errorlevel 1 (
    echo [ERROR] Could not restore Dallas labels.
    exit /b 1
)
echo [OK] Restored Dallas labels
exit /b 0


:trigger_reboot
where curl.exe >nul 2>&1
if errorlevel 1 exit /b 1
echo [STEP] Triggering reboot on %HOST_BASE%
curl.exe -fsS --connect-timeout 5 --retry 1 --retry-delay 1 "%HOST_BASE%/ReBoot" >nul 2>&1
if errorlevel 1 (
    echo [WARN] Reboot request may have been interrupted; continuing to wait.
) else (
    echo [OK] Reboot request sent
)
exit /b 0


:wait_for_health
set "HEALTH_OK=0"
set "WAIT_LABEL=%~1"
set "WAIT_TIMEOUT=%~2"
if "%WAIT_TIMEOUT%"=="" set "WAIT_TIMEOUT=180"
echo [STEP] Waiting for device %WAIT_LABEL%...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$deadline=(Get-Date).AddSeconds(%WAIT_TIMEOUT%);" ^
    "$url='http://'+$env:ARG_HOST+'/api/v2/health?t='+[DateTime]::UtcNow.Ticks;" ^
    "while((Get-Date) -lt $deadline) {" ^
    "  try {" ^
    "    $r=Invoke-WebRequest -UseBasicParsing -Uri $url -Headers @{Accept='application/json'} -TimeoutSec 5;" ^
    "    if($r.Content -match '\"status\"\s*:\s*\"UP\"') { exit 0 }" ^
    "  } catch {}" ^
    "  Start-Sleep -Seconds 2" ^
    "}" ^
    "exit 1"
if not errorlevel 1 set "HEALTH_OK=1"
if "%HEALTH_OK%"=="1" (
    echo [OK] Device reports healthy.
) else (
    echo [WARN] Device did not report healthy within timeout.
)
exit /b 0


:select_bin
set "BIN_FILE="
if "%ARG_BIN%"=="" (
    if "%ARG_FACTORY%"=="1" (
        REM Use the full merged image when the user explicitly asked for factory flash.
        for %%F in ("%SCRIPT_DIR%OTGW-firmware-*-merged-full.bin") do (
            if not defined BIN_FILE set "BIN_FILE=%%F"
        )
        if not defined BIN_FILE (
            for %%F in ("%SCRIPT_DIR%build\OTGW-firmware-*-merged-full.bin") do (
                if not defined BIN_FILE set "BIN_FILE=%%F"
            )
        )
        if not defined BIN_FILE (
            echo [ERROR] --factory: no merged-full bin found.
            echo         Expected in: %SCRIPT_DIR%
            echo                  or: %SCRIPT_DIR%build\
            echo         Use --bin to specify a path.
            exit /b 1
        )
    ) else if "%ARG_ERASE%"=="1" (
        REM Erase-all uses the full merged image too.
        for %%F in ("%SCRIPT_DIR%OTGW-firmware-*-merged-full.bin") do (
            if not defined BIN_FILE set "BIN_FILE=%%F"
        )
        if not defined BIN_FILE (
            for %%F in ("%SCRIPT_DIR%build\OTGW-firmware-*-merged-full.bin") do (
                if not defined BIN_FILE set "BIN_FILE=%%F"
            )
        )
        if not defined BIN_FILE (
            echo [ERROR] --erase: no merged-full bin found.
            echo         Expected in: %SCRIPT_DIR%
            echo                  or: %SCRIPT_DIR%build\
            echo         Use --bin to specify a path.
            exit /b 1
        )
    ) else (
        REM Default to firmware-only so WiFi/settings are preserved.
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
            echo [ERROR] No firmware-only bin found.
            echo         Expected: OTGW-firmware-esp32-*-merged.bin
            echo               or: OTGW-firmware-esp8266-*.ino.bin
            echo         Use --bin to specify a path.
            exit /b 1
        )
    )
) else (
    set "BIN_FILE=%ARG_BIN%"
)
exit /b 0


:probe_flash_blank
set "FLASH_IS_BLANK=0"
set "PROBE_FILE=%TEMP%\otgw_flash_probe_%RANDOM%.bin"
"%ESPTOOL_EXE%" --chip %ESPTOOL_CHIP% %ESPTOOL_PORT_ARGS% --baud %ARG_BAUD% read-flash 0x0 0x1000 "%PROBE_FILE%" >nul 2>&1
if errorlevel 1 (
    if exist "%PROBE_FILE%" del "%PROBE_FILE%" >nul 2>&1
    exit /b 0
)
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$b=[IO.File]::ReadAllBytes('%PROBE_FILE%'); if (($b | Where-Object { $_ -ne 255 } | Select-Object -First 1) -eq $null) { exit 0 } else { exit 1 }"
if not errorlevel 1 set "FLASH_IS_BLANK=1"
if exist "%PROBE_FILE%" del "%PROBE_FILE%" >nul 2>&1
exit /b 0


:show_help
echo flash_otgw.bat - Self-contained ESP flash tool for OTGW-firmware
echo.
echo Usage:
echo   flash_otgw.bat [options]
echo.
echo Mode:
echo   (no flag)            Interactive chooser with auto-detected default.
echo                        1 = factory reset, 2 = upgrade OTGW, 3 = firmware-only
echo   --upgrade            Force firmware-only upgrade.
echo   --factory            Full image flash. You will be asked whether to
echo                        back up and restore settings from a live OTGW.
echo   --erase              Full clean wipe. Erases WiFi credentials and settings.
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
echo   --help, -h           Show this help.
echo.
echo The script downloads esptool %ESPTOOL_VERSION% to .\tools\esptool\ on first run.
echo No Python required.
exit /b 0
