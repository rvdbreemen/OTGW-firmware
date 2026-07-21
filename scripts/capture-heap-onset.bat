@echo off
rem ============================================================================
rem  capture-heap-onset.bat  -  OTGW onset-investigation capture (Windows)
rem
rem  WHY A SEPARATE SCRIPT
rem  This is the NEW capture for the heap-death investigation (TASK-1037). It is
rem  deliberately named differently from capture-heap-leak.bat so there is no
rem  confusion with an older copy: earlier field runs kept using a stale
rem  blanket-quiet version, which hid exactly the events we need to see.
rem
rem  Four captures pinned the death to a deterministic onset at ~3900-3960s of
rem  uptime, independent of mDNS, timezone, NTP resync and the crashlog poll
rem  (all ruled out). The remaining suspect is whatever runs in that window
rem  (the 5-minute event around uptime 3934s). To catch it we must SEE the
rem  MQTT publish decisions and REST activity around that moment, not silence
rem  them.
rem
rem  PRESET
rem    -SkipBrowserCapture          no browser polling of our own
rem    -QuietDebugToggles           silence only the highest-volume logging
rem    -KeepDebugToggles            keep REST API, MQTT, MQTTGate and NTP ON, so
rem                                 the onset window shows publish decisions and
rem                                 any REST/browser hits. Only OTmsg (per-frame
rem                                 OpenTherm decode) and Sensors are silenced,
rem                                 those are the firehose. logHeapStats prints
rem                                 regardless
rem    -CrashlogPollSeconds 3600    catches a crash without a fast poll
rem                                 perturbing the run
rem    -OutputRoot logs/heap-onset  kept apart from the older heap-leak captures
rem
rem  Usage:
rem    capture-heap-onset.bat
rem    capture-heap-onset.bat -DeviceHost 192.168.1.150 -BrokerHost 192.168.1.11
rem  Any argument you pass is forwarded AFTER the preset and overrides it.
rem ============================================================================
setlocal EnableExtensions DisableDelayedExpansion

set "INNER=%~dp0capture-mqtt-debug.bat"
if not exist "%INNER%" (
    echo capture-mqtt-debug.bat was not found next to this script.>&2
    echo Expected: %INNER%>&2
    exit /b 1
)

if /I "%~1"=="--help" goto :help
if /I "%~1"=="/?"     goto :help
if /I "%~1"=="-Help"  goto :help

echo.
echo  ============================================================
echo   OTGW heap-ONSET capture  (new script, TASK-1037)
echo  ============================================================
echo   Goal: capture the moment around ~65 minutes of uptime where
echo   the heap starts falling. Let it run until the device reboots,
echo   or at least 90 minutes so it passes that window.
echo.
echo   KEEP THE OTGW WEB UI CLOSED for the whole run. One open
echo   dashboard tab invalidates the measurement. Home Assistant
echo   itself is fine: it talks MQTT, not HTTP.
echo.
echo   Leave this window open. Ctrl+C stops the capture and writes
echo   the merged transcript.
echo  ============================================================
echo.

call "%INNER%" -SkipBrowserCapture -QuietDebugToggles -KeepDebugToggles "REST API,MQTT,MQTTGate,NTP" -CrashlogPollSeconds 3600 -OutputRoot logs/heap-onset %*
endlocal & exit /b %ERRORLEVEL%

:help
call "%INNER%" -Help
endlocal & exit /b %ERRORLEVEL%
