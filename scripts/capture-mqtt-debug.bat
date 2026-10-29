@echo off
rem ============================================================================
rem  capture-mqtt-debug.bat  -  OTGW diagnostic capture launcher (Windows)
rem
rem  The capture itself is otgw_capture.py next to this file (ADR-202). This
rem  launcher only finds a Python 3.9+ interpreter and runs it:
rem    1. py -3, python or python3 on PATH (the Microsoft Store alias fails the
rem       version check and is skipped);
rem    2. otherwise a portable Python 3.12.10 from python.org, unpacked once into
rem       %LOCALAPPDATA%\OTGW-capture\ after its SHA-256 has been checked.
rem
rem  Usage:
rem    capture-mqtt-debug.bat                 (interactive prompts)
rem    capture-mqtt-debug.bat --help
rem    capture-mqtt-debug.bat -DeviceHost 192.168.1.50 -BrokerHost 192.168.1.10
rem  All arguments are forwarded verbatim to otgw_capture.py.
rem ============================================================================
setlocal EnableExtensions DisableDelayedExpansion

set "TOOL=%~dp0otgw_capture.py"
if not exist "%TOOL%" (
    echo otgw_capture.py was not found next to this launcher: %TOOL%>&2
    echo Unzip the whole flash zip, or run this from the scripts folder.>&2
    exit /b 1
)

set "PY_VER=3.12.10"
set "PY="
call :try_python py -3
if not defined PY call :try_python python
if not defined PY call :try_python python3
if not defined PY call :bootstrap_python
if not defined PY exit /b 1

rem start /b /wait keeps cmd from owning Ctrl+C ("Terminate batch job?"),
rem so the capture can stop cleanly and still write its transcript.
start "" /b /wait %PY% "%TOOL%" %*
set "RC=%ERRORLEVEL%"
endlocal & exit /b %RC%

:try_python
%* -c "import sys; raise SystemExit(0 if sys.version_info >= (3, 9) else 1)" >nul 2>nul
if not errorlevel 1 set "PY=%*"
exit /b 0

:bootstrap_python
set "ARCH=win32"
set "PY_SHA=084b9eb24cb848605c895d05b738fbc2572efc8b4c18c415a824065864a2b853"
if /I "%PROCESSOR_ARCHITECTURE%"=="AMD64" set "ARCH=amd64"
if /I "%PROCESSOR_ARCHITEW6432%"=="AMD64" set "ARCH=amd64"
if /I "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "ARCH=arm64"
if "%ARCH%"=="amd64" set "PY_SHA=4acbed6dd1c744b0376e3b1cf57ce906f9dc9e95e68824584c8099a63025a3c3"
if "%ARCH%"=="arm64" set "PY_SHA=3065efc3d382d1cda66757ac71ade11904fa6e350f5a97eb74811acd71ba5532"

set "BASE=%LOCALAPPDATA%"
if not defined BASE set "BASE=%TEMP%"
set "PY_DIR=%BASE%\OTGW-capture\python-%PY_VER%-%ARCH%"
if exist "%PY_DIR%\python.exe" call :try_python "%PY_DIR%\python.exe"
if defined PY exit /b 0

set "PY_ZIP=%TEMP%\otgw-capture-python-%PY_VER%-%ARCH%.zip"
set "PY_URL=https://www.python.org/ftp/python/%PY_VER%/python-%PY_VER%-embed-%ARCH%.zip"
echo Python 3.9 or newer was not found.
echo Downloading portable Python %PY_VER% (%ARCH%, about 11 MB) from python.org into
echo   %PY_DIR%
echo This happens once; later runs reuse it. Installing Python 3.9+ yourself also works.
rem Plain .NET calls only: started from a PowerShell 7 terminal, Windows
rem PowerShell 5.1 inherits a PSModulePath it cannot load cmdlets such as
rem Get-FileHash or Expand-Archive from. The empty PSModulePath does the rest.
set "PSModulePath="
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'Stop'; [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; (New-Object Net.WebClient).DownloadFile($env:PY_URL, $env:PY_ZIP); $sha = [Security.Cryptography.SHA256]::Create(); $fs = [IO.File]::OpenRead($env:PY_ZIP); $h = ([BitConverter]::ToString($sha.ComputeHash($fs))).Replace('-', '').ToLower(); $fs.Close(); if ($h -ne $env:PY_SHA) { [IO.File]::Delete($env:PY_ZIP); throw ('SHA-256 mismatch: ' + $h) }; if ([IO.Directory]::Exists($env:PY_DIR)) { [IO.Directory]::Delete($env:PY_DIR, $true) }; Add-Type -AssemblyName System.IO.Compression.FileSystem; [IO.Compression.ZipFile]::ExtractToDirectory($env:PY_ZIP, $env:PY_DIR); [IO.File]::Delete($env:PY_ZIP)"
if errorlevel 1 (
    echo ERROR: could not download or verify portable Python from %PY_URL%>&2
    echo Install Python 3.9 or newer from https://www.python.org/downloads/ and run this again.>&2
    exit /b 1
)
call :try_python "%PY_DIR%\python.exe"
if not defined PY (
    echo ERROR: the portable Python in %PY_DIR% does not start.>&2
    exit /b 1
)
exit /b 0
