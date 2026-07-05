@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%" || exit /b 1

set "PIP_DISABLE_PIP_VERSION_CHECK=1"
set "PIP_NO_INPUT=1"
set "PYTHONUTF8=1"
set "PYTHONUNBUFFERED=1"

set "BUILD_VENV_DIR=%SCRIPT_DIR%.build-venv"
set "BUILD_VENV_PY=%BUILD_VENV_DIR%\Scripts\python.exe"
set "DEV_VENV_PY=%SCRIPT_DIR%.venv\Scripts\python.exe"
set "BOOTSTRAP_PY_DIR=%SCRIPT_DIR%.build-python"
set "BOOTSTRAP_PY_EXE=%BOOTSTRAP_PY_DIR%\python.exe"
set "BOOTSTRAP_PY_VER=3.12.10"

call :use_python_if_valid "%BUILD_VENV_PY%"

if not defined PYTHON_EXE call :bootstrap_build_venv

if not defined PYTHON_EXE call :use_python_if_valid "%DEV_VENV_PY%"

if not defined PYTHON_EXE (
    call :bootstrap_portable_python
)

if not defined PYTHON_EXE (
    echo ERROR: Python 3 not found. Install Python 3 or provide a working .venv. 1>&2
    exit /b 1
)

rem Put BOTH the interpreter dir and its pip "Scripts" dir on PATH. For a venv,
rem python.exe already lives in Scripts (PYTHON_DIR) so pio.exe is covered; for
rem the portable embed runtime python.exe is in the root and pip installs console
rem scripts (pio.exe) into %PYTHON_DIR%Scripts, which build.py would otherwise
rem never find ("Could not find or install PlatformIO").
set "PATH=%PYTHON_DIR%;%PYTHON_DIR%Scripts;%PATH%"
set "PYTHONPATH=%SCRIPT_DIR%;%PYTHONPATH%"

call :ensure_pip
if errorlevel 1 exit /b 1

call :install_requirements
if errorlevel 1 exit /b 1

"%PYTHON_EXE%" "%SCRIPT_DIR%build.py" %*
exit /b %ERRORLEVEL%

:bootstrap_build_venv
rem Flat subroutine, NOT a parenthesized block: BASE_PYTHON is set by :find_python
rem at runtime, but inside a ( ) block %BASE_PYTHON% expands at parse time (empty,
rem before find_python runs) so the venv command silently no-ops and .build-venv is
rem never created -> the bogus "Python 3 not found".
call :find_python
if errorlevel 1 exit /b 0
%BASE_PYTHON% -m venv "%BUILD_VENV_DIR%" >nul 2>nul
call :use_python_if_valid "%BUILD_VENV_PY%"
exit /b 0

:find_python
rem Only a 3.10-3.13 interpreter is acceptable as the venv base (see
rem :use_python_if_valid). If the host only has e.g. 3.14, this returns 1 so the
rem caller falls through to the portable-3.12 bootstrap instead of building a
rem .build-venv on an interpreter the espressif32 platform will later reject.
py -3 -c "import sys; v=sys.version_info; raise SystemExit(0 if (v[0]==3 and 10<=v[1]<=13) else 1)" >nul 2>nul
if not errorlevel 1 (
    set "BASE_PYTHON=py -3"
    exit /b 0
)

python -c "import sys; v=sys.version_info; raise SystemExit(0 if (v[0]==3 and 10<=v[1]<=13) else 1)" >nul 2>nul
if not errorlevel 1 (
    set "BASE_PYTHON=python"
    exit /b 0
)

python3 -c "import sys; v=sys.version_info; raise SystemExit(0 if (v[0]==3 and 10<=v[1]<=13) else 1)" >nul 2>nul
if not errorlevel 1 (
    set "BASE_PYTHON=python3"
    exit /b 0
)

exit /b 1

:use_python_if_valid
rem Accept ONLY Python 3.10-3.13: the espressif32 platform (pioarduino) refuses
rem anything outside that range ("Python version must be between 3.10 and 3.13"),
rem so a bare version_info[0]==3 check would happily pick the host 3.14 and the
rem pio run would fail late. Rejecting here forces the portable-3.12 bootstrap.
if not exist "%~1" exit /b 1
"%~1" -c "import sys; v=sys.version_info; raise SystemExit(0 if (v[0]==3 and 10<=v[1]<=13) else 1)" >nul 2>nul
if errorlevel 1 exit /b 1
set "PYTHON_EXE=%~1"
set "PYTHON_DIR=%~dp1"
exit /b 0

:ensure_pip
"%PYTHON_EXE%" -m pip --version >nul 2>nul
if not errorlevel 1 exit /b 0
"%PYTHON_EXE%" -m ensurepip --upgrade >nul 2>nul
exit /b %ERRORLEVEL%

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

:bootstrap_portable_python
call :use_python_if_valid "%BOOTSTRAP_PY_EXE%"
if not errorlevel 1 (
    call :ensure_bootstrap_python_path
    exit /b 0
)

if /I "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set "PYTHON_EMBED_FILE=python-%BOOTSTRAP_PY_VER%-embed-amd64.zip"
) else (
    set "PYTHON_EMBED_FILE=python-%BOOTSTRAP_PY_VER%-embed-win32.zip"
)

set "PYTHON_EMBED_URL=https://www.python.org/ftp/python/%BOOTSTRAP_PY_VER%/%PYTHON_EMBED_FILE%"
set "PYTHON_EMBED_ZIP=%TEMP%\%PYTHON_EMBED_FILE%"
set "GET_PIP_FILE=%TEMP%\get-pip.py"

echo INFO: Python 3 not found. Bootstrapping portable Python %BOOTSTRAP_PY_VER%...

powershell -NoProfile -ExecutionPolicy Bypass -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -UseBasicParsing -Uri '%PYTHON_EMBED_URL%' -OutFile '%PYTHON_EMBED_ZIP%'"
if errorlevel 1 (
    echo ERROR: Failed to download portable Python from %PYTHON_EMBED_URL% 1>&2
    exit /b 1
)

if exist "%BOOTSTRAP_PY_DIR%" rd /s /q "%BOOTSTRAP_PY_DIR%"
mkdir "%BOOTSTRAP_PY_DIR%" >nul 2>nul
if errorlevel 1 (
    echo ERROR: Failed to create portable Python directory: %BOOTSTRAP_PY_DIR% 1>&2
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -Path '%PYTHON_EMBED_ZIP%' -DestinationPath '%BOOTSTRAP_PY_DIR%' -Force"
if errorlevel 1 (
    echo ERROR: Failed to unpack portable Python archive: %PYTHON_EMBED_ZIP% 1>&2
    exit /b 1
)

if exist "%BOOTSTRAP_PY_DIR%\python*._pth" (
    call :ensure_bootstrap_python_path
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -UseBasicParsing -Uri 'https://bootstrap.pypa.io/get-pip.py' -OutFile '%GET_PIP_FILE%'"
if errorlevel 1 (
    echo ERROR: Failed to download pip bootstrap script. 1>&2
    exit /b 1
)

"%BOOTSTRAP_PY_EXE%" "%GET_PIP_FILE%" --disable-pip-version-check --no-input --quiet
if errorlevel 1 (
    echo ERROR: Failed to install pip into portable Python runtime. 1>&2
    exit /b 1
)

call :use_python_if_valid "%BOOTSTRAP_PY_EXE%"
if errorlevel 1 (
    echo ERROR: Portable Python bootstrap did not produce a valid Python 3 runtime. 1>&2
    exit /b 1
)

echo INFO: Portable Python runtime bootstrapped successfully.
exit /b 0

:ensure_bootstrap_python_path
if not exist "%BOOTSTRAP_PY_DIR%\python*._pth" exit /b 0
powershell -NoProfile -ExecutionPolicy Bypass -Command "$repo = '%SCRIPT_DIR%'.TrimEnd('\\'); $pth = Get-ChildItem -Path '%BOOTSTRAP_PY_DIR%' -Filter 'python*._pth' | Select-Object -First 1; if ($pth) { $content = Get-Content -Path $pth.FullName; $updated = $false; $new = @(); foreach ($line in $content) { if ($line.Trim() -eq '#import site') { $new += 'import site'; $updated = $true } else { $new += $line } }; if (-not $updated -and ($new -notcontains 'import site')) { $new += 'import site' }; if ($new -notcontains $repo) { $new += $repo }; Set-Content -Path $pth.FullName -Value $new -Encoding Ascii }"
exit /b 0
