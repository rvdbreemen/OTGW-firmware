---
id: TASK-1302
title: >-
  Fix: setup() blocks ~48 s after WiFi is up on about 1 in 50 boots (web down 54
  s)
status: To Do
assignee: []
created_date: '2026-10-22T14:49:00Z'
labels:
  - bug
dependencies: []
priority: medium
ordinal: 256000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen twice on the bench (.68) during the TASK-1262/TASK-1301 reset series, about 2 in 110 RTS resets: the web interface came back after 54 s although WiFi was up at 0.6-1.3 s (GotIP in the diag ring).
Diag ring 15:54:52 (fix build 87da863 + stage markers): ev35 after startWebserver() at 1354 ms, next marker ev38 after updateRebootLog() at 49894 ms. The markers for startWebSocket() and startMQTT() were accidentally inside a comment, so the ~48.5 s gap covers startWebSocket(), startMQTT() (its INIT state runs WiFi.hostByName("homeassistant.local") synchronously), initWatchDog() (I2C), ESP.getResetReason(), updateRebootCount() and updateRebootLog() (LittleFS writes). The first case (14:35:21, 52 s) came after 6 h of uptime; bench MQTT password missing (auth fails) in both.
A field gateway that hits this is unreachable for ~50 s after a restart, and the loop (OT handling) does not run meanwhile.
Diag patch with corrected markers (ev36, ev37, ev42-ev46 incl. around hostByName): scratchpad t1262/diag_plus_fix1301.patch (temporary, not for commit).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The blocking call is identified from a bench capture of a slow boot (stage markers or telnet)
- [ ] #2 With the fix no boot in a series of at least 100 RTS resets blocks setup() for more than 10 s after WiFi is up, old build vs fix on the same rig
- [ ] #3 esp8266 build and evaluate.py --quick green
<!-- AC:END -->
