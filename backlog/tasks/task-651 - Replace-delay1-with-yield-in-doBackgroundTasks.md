---
id: TASK-651
title: Replace delay(1) with yield() in doBackgroundTasks()
status: To Do
assignee: []
created_date: '2026-05-21 20:23'
labels:
  - performance
  - mainloop
  - webserver
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The synchronous ESP8266WebServer is sensitive to per-tick latency. doBackgroundTasks() ends with delay(1), which adds a 1 ms wait every loop iteration and caps the loop at ~1000 Hz. yield() gives the SDK the same cooperative-scheduling hint without the wait, removing the latency floor on httpServer.handleClient().
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 delay(1) at OTGW-firmware.ino end of doBackgroundTasks() is replaced with yield()
- [ ] #2 python build.py --firmware exits 0
- [ ] #3 python evaluate.py --quick shows no new failures
- [ ] #4 Prerelease version is bumped (bin/bump-prerelease.sh) and staged with the change
- [ ] #5 Field validation: beta build remains stable under load (telnet + WS + MQTT + HTTP); reported back via Discord
<!-- AC:END -->
