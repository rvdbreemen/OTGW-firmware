---
id: TASK-651
title: Replace delay(1) with yield() in doBackgroundTasks()
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-21 20:23'
updated_date: '2026-05-21 20:27'
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
- [x] #1 delay(1) at OTGW-firmware.ino end of doBackgroundTasks() is replaced with yield()
- [x] #2 python build.py --firmware exits 0
- [x] #3 python evaluate.py --quick shows no new failures
- [x] #4 Prerelease version is bumped (bin/bump-prerelease.sh) and staged with the change
- [ ] #5 Field validation: beta build remains stable under load (telnet + WS + MQTT + HTTP); reported back via Discord
<!-- AC:END -->



## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Replace delay(1) with yield() at OTGW-firmware.ino:423.
2. Bump prerelease via bin/bump-prerelease.sh.
3. Stage src/OTGW-firmware/OTGW-firmware.ino + version.h + data/version.hash.
4. Run python build.py --firmware.
5. Run python evaluate.py --quick.
6. Commit and push to origin/dev (per project push policy: build+evaluator gated).
7. Open draft PR.
8. Field validation AC stays open until beta tester confirmation.
<!-- SECTION:PLAN:END -->
