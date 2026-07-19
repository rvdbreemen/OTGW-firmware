@echo off
rem ============================================================================
rem  capture-heap-leak.bat  -  OTGW heap-leak capture preset (Windows)
rem
rem  WHY THIS EXISTS
rem  capture-mqtt-debug.bat drives a headless browser against the web UI. That
rem  is right for reproducing web-load problems, but it makes ~365 REST
rem  requests per minute, which is itself heavy enough to drive the heap
rem  fragmentation path. A leak measurement taken that way shows the browser
rem  load, not the leak (TASK-1037).
rem
rem  This wrapper runs the SAME capture worker with the load removed, so the
rem  device does only its normal MQTT and OpenTherm work while we watch the
rem  heap. Telnet is the only extra connection, because that is where
rem  logHeapStats reports.
rem
rem  PRESET
rem    -SkipBrowserCapture          no browser, no REST load
rem    -SkipDebugToggles            do not switch on OTmsg/REST/MQTT/MQTTGate
rem                                 logging; keeps telnet output near-silent so
rem                                 the instrument is not itself a suspect.
rem                                 logHeapStats prints regardless, once a
rem                                 minute, which is the number we came for
rem    -CrashlogPollSeconds 3600    still catches a crash within the hour,
rem                                 without the 30s poll (~1021 ms, ~3.4 KB of
rem                                 free heap per poll) perturbing the run
rem    -OutputRoot logs/heap-leak   kept apart from normal captures
rem
rem  Usage:
rem    capture-heap-leak.bat
rem    capture-heap-leak.bat -DeviceHost 192.168.1.150 -BrokerHost 192.168.1.11
rem  Any argument you pass is forwarded AFTER the preset, so you can override
rem  any of it. Everything capture-mqtt-debug.bat accepts works here.
rem ============================================================================
setlocal EnableExtensions DisableDelayedExpansion

set "INNER=%~dp0capture-mqtt-debug.bat"
if not exist "%INNER%" (
    echo capture-mqtt-debug.bat was not found next to this script.>&2
    echo Expected: %INNER%>&2
    exit /b 1
)

rem --- help: call the worker with -Help only. The preset flags must not be
rem     passed here, because the inner .bat only maps --help when it is the
rem     first argument, and our preset would take that slot.
if /I "%~1"=="--help" goto :help
if /I "%~1"=="/?"     goto :help
if /I "%~1"=="-Help"  goto :help

echo.
echo  ============================================================
echo   OTGW heap-leak capture
echo  ============================================================
echo   Measuring: free heap and largest block over time, with the
echo   device otherwise idle. Let this run for at least 4 hours, or
echo   until the device reboots.
echo.
echo   KEEP THE OTGW WEB UI CLOSED for the whole run. One open
echo   dashboard tab polls the device hard enough to invalidate the
echo   measurement. Home Assistant itself is fine: it talks MQTT,
echo   not HTTP.
echo.
echo   Leave this window open. Ctrl+C stops the capture and writes
echo   the merged transcript.
echo  ============================================================
echo.

call "%INNER%" -SkipBrowserCapture -SkipDebugToggles -CrashlogPollSeconds 3600 -OutputRoot logs/heap-leak %*
endlocal & exit /b %ERRORLEVEL%

:help
call "%INNER%" -Help
endlocal & exit /b %ERRORLEVEL%
