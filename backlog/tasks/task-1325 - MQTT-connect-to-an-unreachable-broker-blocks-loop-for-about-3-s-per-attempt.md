---
id: TASK-1325
title: MQTT connect to an unreachable broker blocks loop() for about 3 s per attempt
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T16:23:00Z'
updated_date: '2026-10-23T19:16:00Z'
labels:
  - mqtt
  - performance
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 448000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1307 (bench .88.61, alpha.471, broker 192.168.88.32:1883 unreachable): every MQTT connection attempt stalls loop() ~3.02 s ('[loop-stall] 3017-3037 ms gap' right after 'Attempting MQTT connection', 16 stalls in 5 min); with MQTT disabled on the same build: 0 stalls, max loop gap 148 ms. doTaskEvery1s ticks are skipped during the stall, so state.uptime lags the wall clock. Any user whose broker is down or misconfigured gets this. Look at handleMQTT's connect path and the vendored espMqttClient (ADR-186/187) connect: make the TCP connect non-blocking or run it off the loop task; check the library-change rule (ADR-186 vendored copy).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduction: unreachable broker -> loop-stall lines ~3 s per attempt on the current build (red)
- [x] #2 Fix: with an unreachable broker the max loop gap stays below 500 ms over 5 min, MQTT still connects when the broker comes back (green, bench)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Maintainer approved (session 2026-10-23) a patch of the vendored espMqttClient: Config.h EMC_CONNECT_TIMEOUT_MS (#ifndef default 3000 = framework/upstream behaviour), ClientSync::connect(ip|host, port) passes it on ESP32 (NetworkClient::connect(ip, port, timeout_ms); framework default WIFI_CLIENT_DEF_CONN_TIMEOUT_MS 3000, NetworkClient.cpp:30); platformio.ini [env] build_flags -DEMC_CONNECT_TIMEOUT_MS=300. Patch marked 'OTGW-firmware patch (TASK-1325)' next to the ADR-186 one (ADR-187: own patches named). Verification: build --target all (platformio.ini changed); bench .88.61 with the unreachable broker 192.168.88.32: red recorded in TASK-1307 (16 stalls ~3.03 s / 5 min, max gap 3037); green = same 5 min after telnet z with MQTT on: max_loop_gap < 500 ms, no 3 s loop-stall lines; reconnect works when a reachable broker exists (test against a local mosquitto on the PC: point the broker setting at the PC, start/stop it, restore the setting).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Also check while fixing: whether OT frames get lost during a 3 s stall (the serial task reads PIC bytes, but processOT runs on the loop task; is the hand-off queue deep enough for 3 s of frames?). Not checked in TASK-1307.

2026-10-23 21:15 evidence: patch EMC_CONNECT_TIMEOUT_MS (Config.h default 3000, ClientSync::connect(ip|host, port, timeout) on ESP32; NetworkClient.h:53/55 overloads exist; framework default 3000 at NetworkClient.cpp:30), platformio.ini -DEMC_CONNECT_TIMEOUT_MS=300. build.bat --target all: esp32, esp32-classic, esp32-combo SUCCESS (combo 21:05). Bench .88.61 (app flash, compiled 21:04:14), broker 192.168.88.32:1883 unreachable (the PC's mosquitto service PID 6948 listens on 127.0.0.1 only, which is why it was unreachable in TASK-1307 too): 5 min after telnet z with MQTT on: 20 connect attempts, max loop-stall 425 ms, hd_max_loop_gap_ms 425 (red, TASK-1307: 16 stalls 3017-3037 ms, max 3037). Reconnect: a temporary mosquitto on 0.0.0.0:1883 (anonymous) -> the device connected after ~20 s ('New client connected from 192.168.88.61 as OTGW1020BA21B4F8'), 252 publishes in the next 20 s; temporary broker stopped afterwards, the existing localhost-only service untouched. OT-frame loss during the old 3 s stall not measured (moot with the 425 ms bound).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
MQTT connection attempts to an unreachable broker no longer stall loop() for ~3 s: the vendored espMqttClient's sync transport now passes a connect timeout (EMC_CONNECT_TIMEOUT_MS, upstream default 3000 kept in Config.h, set to 300 ms in platformio.ini) to NetworkClient::connect. Maintainer approved the library patch (session 2026-10-23); it is marked 'OTGW-firmware patch (TASK-1325)' like the ADR-186 one. Evidence: AC1 red in TASK-1307 (max gap 3037 ms); AC2 same scenario on the fix: max gap 425 ms over 5 min, and a reachable broker is connected to within ~20 s with data flowing. Builds for all three targets SUCCESS. Not measured: OT frame loss during the old stall.
<!-- SECTION:FINAL_SUMMARY:END -->
