@echo off
rem Windows launcher for capture-mqtt-debug.ps1.
rem Forwards all arguments to PowerShell so the diagnostic can be started from cmd.exe.
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "PS_SCRIPT=%SCRIPT_DIR%capture-mqtt-debug.ps1"
set "PS_EXE="

where pwsh >nul 2>&1
if %ERRORLEVEL%==0 (
    set "PS_EXE=pwsh"
    goto run
)

where powershell >nul 2>&1
if %ERRORLEVEL%==0 (
    set "PS_EXE=powershell"
    goto run
)

echo PowerShell was not found on PATH.>&2
exit /b 1

:run
"%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%PS_SCRIPT%" %*
exit /b %ERRORLEVEL%
