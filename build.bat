@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%" || exit /b 1

set "PIP_DISABLE_PIP_VERSION_CHECK=1"
set "PIP_NO_INPUT=1"
set "PYTHONUTF8=1"

set "BUILD_VENV_DIR=%SCRIPT_DIR%.build-venv"
set "BUILD_VENV_PY=%BUILD_VENV_DIR%\Scripts\python.exe"
set "DEV_VENV_PY=%SCRIPT_DIR%.venv\Scripts\python.exe"

call :use_python_if_valid "%BUILD_VENV_PY%"

if not defined PYTHON_EXE call :bootstrap_build_venv

if not defined PYTHON_EXE call :use_python_if_valid "%DEV_VENV_PY%"

if not defined PYTHON_EXE (
    echo ERROR: Python 3 not found. Install Python 3 or provide a working .venv. 1>&2
    exit /b 1
)

call :install_requirements
if errorlevel 1 exit /b 1

"%PYTHON_EXE%" "%SCRIPT_DIR%build.py" %*
exit /b %ERRORLEVEL%

:bootstrap_build_venv
rem Must be a flat subroutine, NOT a parenthesized block: BASE_PYTHON is set by
rem :find_python at runtime, but inside a ( ) block %BASE_PYTHON% expands at parse
rem time (empty, before find_python runs) and the venv command silently no-ops.
call :find_python
if errorlevel 1 exit /b 0
%BASE_PYTHON% -m venv "%BUILD_VENV_DIR%" >nul 2>nul
call :use_python_if_valid "%BUILD_VENV_PY%"
exit /b 0

:find_python
py -3 -c "import sys; raise SystemExit(0 if sys.version_info[0] == 3 else 1)" >nul 2>nul
if not errorlevel 1 (
    set "BASE_PYTHON=py -3"
    exit /b 0
)

python -c "import sys; raise SystemExit(0 if sys.version_info[0] == 3 else 1)" >nul 2>nul
if not errorlevel 1 (
    set "BASE_PYTHON=python"
    exit /b 0
)

python3 -c "import sys; raise SystemExit(0 if sys.version_info[0] == 3 else 1)" >nul 2>nul
if not errorlevel 1 (
    set "BASE_PYTHON=python3"
    exit /b 0
)

exit /b 1

:use_python_if_valid
if not exist "%~1" exit /b 1
"%~1" -c "import sys; raise SystemExit(0 if sys.version_info[0] == 3 else 1)" >nul 2>nul
if errorlevel 1 exit /b 1
set "PYTHON_EXE=%~1"
exit /b 0

:install_requirements
if exist "%SCRIPT_DIR%requirements-build.txt" (
    "%PYTHON_EXE%" -m pip install --disable-pip-version-check --no-input -q -r "%SCRIPT_DIR%requirements-build.txt"
    if errorlevel 1 exit /b 1
)

if exist "%SCRIPT_DIR%requirements.txt" (
    "%PYTHON_EXE%" -m pip install --disable-pip-version-check --no-input -q -r "%SCRIPT_DIR%requirements.txt"
    if errorlevel 1 exit /b 1
)

exit /b 0
