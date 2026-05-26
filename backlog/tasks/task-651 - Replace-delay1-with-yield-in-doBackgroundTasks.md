---
id: TASK-651
title: Replace delay(1) with yield() in doBackgroundTasks()
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-21 20:23'
updated_date: '2026-05-26 18:02'
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
- [x] #5 Field validation: beta build remains stable under load (telnet + WS + MQTT + HTTP); reported back via Discord
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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced delay(1) with yield() at end of doBackgroundTasks() (OTGW-firmware.ino:423). The synchronous ESP8266WebServer polls via httpServer.handleClient() inside this function every iteration; the trailing delay(1) capped the loop at ~1000 Hz and added a 1 ms latency floor to every poll. yield() gives the SDK the same esp_schedule() hint without the wait. feedWatchDog() at the top of the function and httpServer.handleClient() already yield internally, so this is just a final scheduling courtesy.

Audit also reviewed every other delay() in the firmware:
- networkStuff.ino:124,162 — setup() WiFi connect-poll loop (not in mainloop).
- networkStuff.ino:143,145; helperStuff.ino:512,649,657 — bracketing ESP.restart() / unreachable post-restart guards.
- helperStuff.ino:1135 — emergencyHeapRecovery(), rate-limited to once/30 s on HEAP_CRITICAL only.
- OTGW-Core.ino:976 — initWatchDog(), setup() only.
- libraries/OTGWSerial.cpp:891 — OTGWSerial::resetPic(), setup() / explicit reset.
- OTGW-ModUpdateServer-impl.h:201 — delay(0) yield.
None reachable from steady-state mainloop. No delayMicroseconds() anywhere.

Verification:
- python build.py --firmware: exit 0 (OTGW-firmware-1.6.0-beta.14+be6d64f.ino.bin, 0.71 MB).
- python evaluate.py --quick: 34 passed, 0 warnings, 0 failures, health 100%.

Prerelease bumped beta.13 -> beta.14 so field testers can A/B against beta.13.

PR: rvdbreemen/OTGW-firmware#617 (draft).

Blocking AC: field validation under telnet + WS + MQTT + HTTP load on a beta-tester device. Cannot be self-verified; leaving task In Progress until Discord #beta-testing confirms stability.
<!-- SECTION:FINAL_SUMMARY:END -->
