@echo off
REM =====================================================================
REM run_tests.bat - build and run the host-compiled firmware unit tests.
REM
REM No test runner is vendored with this project; these tests compile the
REM real firmware sources against a small Arduino/ESP8266 platform shim
REM (test\host\arduino_shim.h) with the MSVC Build Tools compiler.
REM
REM Exits non-zero when any test fails or the build fails.
REM =====================================================================
setlocal enabledelayedexpansion

set "TESTDIR=%~dp0"
set "OUTDIR=%TESTDIR%build"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

REM --- locate the MSVC environment ------------------------------------
set "VCVARS="
for %%E in (
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
) do (
  if not defined VCVARS if exist %%E set "VCVARS=%%~E"
)

where cl.exe >nul 2>&1
if errorlevel 1 (
  if not defined VCVARS (
    echo ERROR: no C++ compiler found ^(cl.exe not on PATH, no VS 2022 vcvars64.bat^).
    exit /b 2
  )
  call "%VCVARS%" >nul
)

REM --- build + run each host test --------------------------------------
set "RC=0"
for %%T in (test_extractJsonField test_dhwWaterMeter) do (
  echo Building test\host\%%T.cpp ...
  cl /nologo /EHsc /std:c++17 /W3 /wd4996 ^
     /Fe:"%OUTDIR%\%%T.exe" ^
     /Fo:"%OUTDIR%\%%T." ^
     "%TESTDIR%host\%%T.cpp"
  if errorlevel 1 (
    echo BUILD FAILED: %%T
    exit /b 2
  )
  echo.
  "%OUTDIR%\%%T.exe"
  if errorlevel 1 set "RC=1"
  echo.
)

exit /b %RC%
