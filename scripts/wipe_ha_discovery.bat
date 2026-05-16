@echo off
rem Launcher for wipe_ha_discovery.py (Windows).
rem Finds a Python 3 interpreter and, if none is present, attempts to install
rem one with winget or Chocolatey before running the wiper.
rem Set WIPE_HA_NO_AUTOINSTALL=1 to disable the auto-install step.
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "PY_SCRIPT=%SCRIPT_DIR%wipe_ha_discovery.py"
set "PY="

call :find_python
if defined PY goto run

if "%WIPE_HA_NO_AUTOINSTALL%"=="1" (
    echo Python 3 not found and auto-install disabled.>&2
    echo Install Python 3 from https://www.python.org/downloads/ and re-run.>&2
    exit /b 1
)

echo Python 3 not found. Attempting to install it ...
where winget >nul 2>&1
if %ERRORLEVEL%==0 (
    echo Installing Python 3 via winget ...
    winget install -e --id Python.Python.3.12 --silent --accept-package-agreements --accept-source-agreements
    goto recheck
)
where choco >nul 2>&1
if %ERRORLEVEL%==0 (
    echo Installing Python 3 via Chocolatey ...
    choco install -y python
    goto recheck
)
echo Could not auto-install Python 3 ^(winget/choco not found^).>&2
echo Install it manually from https://www.python.org/downloads/ and re-run.>&2
exit /b 1

:recheck
call :find_python
if defined PY (
    echo Python 3 installed successfully.
    goto run
)
echo Python was installed but is not on PATH yet.>&2
echo Close and reopen this terminal, then run this script again.>&2
exit /b 1

:run
"%PY%" %PY_LAUNCH_ARG% "%PY_SCRIPT%" %*
exit /b %ERRORLEVEL%

:find_python
rem Prefer the py launcher (py -3), then python / python3 on PATH.
py -3 -c "import sys; raise SystemExit(0 if sys.version_info[0]==3 else 1)" >nul 2>&1
if %ERRORLEVEL%==0 (
    set "PY=py"
    set "PY_LAUNCH_ARG=-3"
    exit /b 0
)
for %%C in (python python3) do (
    %%C -c "import sys; raise SystemExit(0 if sys.version_info[0]==3 else 1)" >nul 2>&1
    if not errorlevel 1 (
        set "PY=%%C"
        set "PY_LAUNCH_ARG="
        exit /b 0
    )
)
exit /b 1
