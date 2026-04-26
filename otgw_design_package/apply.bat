@echo off
rem ============================================================
rem  OTGW Design System — apply.bat
rem
rem  Dubbelklik-wrapper voor apply.ps1. Vraagt om de RepoPath
rem  als die niet via argv1 wordt meegegeven, en draait de PS1
rem  met -ExecutionPolicy Bypass zodat ondertekening geen issue
rem  is op standaard Windows-installs.
rem
rem  Gebruik:
rem    apply.bat                              (interactief, vraagt RepoPath)
rem    apply.bat C:\dev\OTGW-firmware          (interactief, RepoPath gegeven)
rem    apply.bat C:\dev\OTGW-firmware -Force   (geen prompts)
rem ============================================================

setlocal

set "SCRIPT_DIR=%~dp0"
set "PS1=%SCRIPT_DIR%apply.ps1"

if not exist "%PS1%" (
  echo [FAIL] apply.ps1 niet gevonden naast apply.bat:
  echo        %PS1%
  pause
  exit /b 1
)

set "REPO=%~1"
if "%REPO%"=="" (
  echo.
  echo  OTGW Design System
  echo  ------------------
  echo  Vul het pad in naar je OTGW-firmware checkout.
  echo  Voorbeeld: C:\dev\OTGW-firmware
  echo.
  set /p REPO=Pad naar OTGW-firmware:
)

if "%REPO%"=="" (
  echo [FAIL] Geen pad opgegeven. Afgebroken.
  pause
  exit /b 1
)

rem Tweede argument doorgeven (bv. -Force) — schift "%~1" eraf
shift
set "EXTRA="
:collect
if "%~1"=="" goto run
set "EXTRA=%EXTRA% %1"
shift
goto collect

:run
echo.
echo  Aanroepen: powershell -ExecutionPolicy Bypass -File apply.ps1 -RepoPath "%REPO%"%EXTRA%
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%PS1%" -RepoPath "%REPO%"%EXTRA%
set "RC=%ERRORLEVEL%"

echo.
if "%RC%"=="0" (
  echo [OK]   apply.ps1 voltooid.
) else (
  echo [FAIL] apply.ps1 eindigde met code %RC%.
)
echo.
pause
exit /b %RC%
