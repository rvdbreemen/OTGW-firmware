@echo off
rem ============================================================================
rem  capture-heap-soak.bat  -  OTGW long-run heap CONFIRMATION capture (Windows)
rem
rem  WHY A SEPARATE SCRIPT
rem  capture-heap-onset.bat hunts the leak ONSET window (<=90 min) on an
rem  unfixed build. This script does the opposite job: CONFIRM that a fixed
rem  build keeps the heap flat over a long soak (target 24h+), so a field
rem  tester can verify "no reboots" on a beta that carries the discovery-verify
rem  leak fix (TASK-1048 / ADR-087). Same browser-free, low-perturbation
rem  preset; only the framing, run length and output folder differ.
rem
rem  LOW PERTURBATION IS THE POINT
rem  A capture that drives its own HTTP load changes what it measures. This
rem  preset does NO browser polling and keeps only the low-volume debug
rem  channels on. logHeapStats prints once a minute regardless, which is the
rem  signal we need for a soak. Leave the OTGW web UI CLOSED for the whole run.
rem
rem  PRESET
rem    -SkipBrowserCapture          no browser polling of our own
rem    -QuietDebugToggles           silence only the highest-volume logging
rem    -KeepDebugToggles            keep REST API, MQTT, MQTTGate and NTP ON.
rem                                 Only OTmsg (per-frame OpenTherm decode) and
rem                                 Sensors are silenced (the firehose).
rem    -CrashlogPollSeconds 3600    hourly crashlog/reboot_log poll, no fast
rem                                 poll perturbing the run. A reboot during the
rem                                 soak is the failure signal.
rem    -OutputRoot logs/heap-soak   kept apart from onset and leak captures.
rem
rem  HOW TO READ THE RESULT
rem  Healthy fixed build: free heap and maxBlock stay flat (allocator jitter
rem  only, no downward trend) across every uptime hour boundary, and the
rem  crashlog gains no new reboot entries. A monotone ramp down toward OOM,
rem  or a fresh External-Watchdog reboot, means the leak is NOT fixed on this
rem  build.
rem
rem  Usage:
rem    capture-heap-soak.bat
rem    capture-heap-soak.bat -DeviceHost 192.168.1.150 -BrokerHost 192.168.1.11
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
echo   OTGW heap-SOAK confirmation capture  (TASK-1037 / TASK-1048)
echo  ============================================================
echo   Goal: confirm a FIXED build keeps the heap flat over a long
echo   run. Let it run 24 hours if you can, at least a few hours so
echo   it passes several uptime hour boundaries.
echo.
echo   KEEP THE OTGW WEB UI CLOSED for the whole run. One open
echo   dashboard tab invalidates the measurement. Home Assistant
echo   itself is fine: it talks MQTT, not HTTP.
echo.
echo   Leave this window open. Ctrl+C stops the capture and writes
echo   the merged transcript.
echo  ============================================================
echo.

call "%INNER%" -SkipBrowserCapture -QuietDebugToggles -KeepDebugToggles "REST API,MQTT,MQTTGate,NTP" -CrashlogPollSeconds 3600 -OutputRoot logs/heap-soak %*
endlocal & exit /b %ERRORLEVEL%

:help
call "%INNER%" -Help
endlocal & exit /b %ERRORLEVEL%
