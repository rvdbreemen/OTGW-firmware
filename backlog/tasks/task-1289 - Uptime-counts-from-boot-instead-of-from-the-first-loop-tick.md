---
id: TASK-1289
title: Uptime counts from boot instead of from the first loop tick
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-22T04:09:00Z'
updated_date: '2026-10-22T04:10:00Z'
labels:
  - bug
  - wifi
  - diagnostics
dependencies: []
priority: medium
ordinal: 252000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
state.uptime.iSeconds is a tick counter in doTaskEvery1s (OTGW-firmware.ino:236) with SKIP_MISSED_TICKS. It starts only when loop() runs and never recovers a skipped second. The RTC diagnosis in TASK-1262 measured it at 53 against ~86 s real on a normal boot and 26 against ~228 s real on a boot that waited for WiFi in setup(). The value goes to Home Assistant (MQTT uptime), the REST API (runtime.uptime_sec), the web UI and the telnet banner, and anyone who derives a restart time from it is misled (it misled the TASK-1262 analysis itself). Derive it from micros64() instead. The boot-command gate (TASK-1121) uses the counter as a 3-second grace of loop time for the PIC banner; it keeps a loop-relative counter.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Uptime reported by /api/v2/device/info runtime.uptime_sec matches wall time since the reset within 2 s after a normal boot, measured from the laptop RTS reset; old build shows the deficit on the same test
- [ ] #2 Uptime matches wall time since the restart within 5 s after a WiFi supervisor restart whose boot waits for WiFi in setup(); old build shows the deficit on the same test
- [ ] #3 Boot commands are still sent after the PIC banner is read or after 3 s of loop time, not at the first loop tick (TASK-1121 grace kept)
- [ ] #4 Uptime does not wrap at 49.7 days (micros64-based, no 32-bit millis wrap)
- [ ] #5 python build.py via build.bat succeeds and python evaluate.py --quick shows no new failures
<!-- AC:END -->
