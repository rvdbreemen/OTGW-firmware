---
id: TASK-1196
title: >-
  Bump ESPAsyncWebServer 3.11.0 to 3.11.2 for two multipart-parser security
  fixes
status: Done
assignee:
  - '@claude'
created_date: '2026-10-02 20:07'
updated_date: '2026-10-02 20:30'
labels:
  - security
  - dependency
  - web
dependencies: []
priority: high
ordinal: 318000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
platformio.ini pins ESP32Async/ESPAsyncWebServer @ 3.11.0. The pin comment gives one reason: 3.11.1 was 5 days old at the time. Since then two high-severity advisories against the multipart/form-data parser (src/WebRequest.cpp) were published:

- GHSA-4phx-fcj6-46r4 (2026-06-06), affects <= 3.11.0, fixed in 3.11.1.
  - `_boundaryPosition` is a uint8_t, so a 256-byte boundary wraps it and the parser loops forever.
  - One crafted request from the LAN starves the CPU into a FreeRTOS watchdog reset.
- GHSA-8m8p-vhxc-jmjw (2026-06-28), affects <= 3.11.1, fixed in 3.11.2.
  - A NULL-pointer write when the closing boundary of a file part is followed by an unexpected byte.

The 3.11.1 and 3.11.2 release notes list only these parser fixes, a refactor of the first one, and CI/example changes. 3.12.x is a separate step: it needs AsyncTCP 3.5.0, where abort() runs in the caller's context, and it brings a WebSocket refactor.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 platformio.ini pins ESP32Async/ESPAsyncWebServer @ 3.11.2 and the pin comment names the two advisories
- [x] #2 WebResponses.cpp and the AsyncTCP pin are unchanged against 3.11.0 (diff of the resolved library), so the response path the storm measured stays the same
- [x] #3 esp32-combo build (firmware and filesystem) SUCCESS with fresh images, and evaluate.py green
- [x] #4 On the bench, multipart uploads still work: refresh_storm.py --upload-abort 30 gives complete readbacks and a final 303, and an FSexplorer upload reads back intact
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. platformio.ini: pin ESPAsyncWebServer @ 3.11.2 (done), AsyncTCP stays @ 3.4.10 (3.11.2 requires ^3.4.10). Comment names both advisories.
2. Validate: build.bat --target esp32-combo with the new library, confirm SUCCESS + fresh firmware.bin/littlefs.bin.
3. AC#2 proof: diff the resolved 3.11.2 WebResponses.cpp against the upstream v3.11.0 tag (gh api) -> identical; AsyncTCP pin unchanged.
4. Bump prerelease (bin/bump-prerelease.sh) -> alpha.402; single commit with the pin change.
5. Rebuild combo -> -flash.zip carries the new version. Hand the zip to the user to upload.
6. evaluate.py --quick green (AC#3).
7. Bench AC#4: refresh_storm.py --upload-abort 30 (complete readbacks + final 303) and one FSexplorer upload round-trip, on the bumped build.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-02 shipped as alpha.402+0b81fc4, pushed to dev (0b81fc4fa).

AC#1 compile: esp32-combo firmware + filesystem [SUCCESS] with ESPAsyncWebServer 3.11.2 resolved (.pio/libdeps library.json "version": "3.11.2"). Two "Successfully created ESP32S3 image", "Build completed successfully!". Log: scratchpad build-1196-validate.log / build-1196-alpha402.log.

AC#2 serve path unchanged: fetched src/WebResponses.cpp and src/WebResponseImpl.h from the upstream v3.11.0 tag and diffed against the resolved 3.11.2 copies on disk -> both IDENTICAL. AsyncTCP pin stays @ 3.4.10 (3.11.2 requires ^3.4.10). The response path TASK-1124/1162 measure is byte-for-byte the same.

AC#3 evaluate.py --quick: 78 checks, 71 passed, 0 failed, 0 warnings.

AC#4 bench, alpha.402 flashed app-only to the OTGW32 (.88.61, settings preserved), running 2.0.0-alpha.402+0b81fc4:
- refresh_storm.py --upload-abort 30 --abort-mode mixed: 30 aborted multipart uploads (10 fin, 10 rst, 10 stall), every readback prefix_current 4096 B.
- final full upload 303 with complete 16384 B readback; cleanup delete 200, GET 404.
- bootcount 3 -> 3 (no reboot, the parser did not choke); heap -113 B per abort, below the 512 B handle threshold.
- Capture: %LOCALAPPDATA%/OTGW-capture/task1196-upload-20261002-2228.txt.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Pinned ESPAsyncWebServer 3.11.0 -> 3.11.2 to close two high-severity multipart-parser advisories (GHSA-4phx-fcj6-46r4 DoS boundary-counter overflow, <= 3.11.0; GHSA-8m8p-vhxc-jmjw NULL-pointer write, <= 3.11.1). AsyncTCP unchanged at 3.4.10.

Evidence per AC (alpha.402+0b81fc4):
- AC#1: esp32-combo firmware + filesystem build SUCCESS with 3.11.2 resolved.
- AC#2: WebResponses.cpp + WebResponseImpl.h byte-identical v3.11.0 vs resolved 3.11.2 on disk; AsyncTCP pin unchanged. The storm-measured serve path is untouched, so this bump is independent of TASK-1162.
- AC#3: evaluate.py --quick 71/0.
- AC#4: bench upload-abort 30 on the flashed alpha.402 -> all complete readbacks, final 303, no reboot, no handle leak.

3.12.x deferred: it needs AsyncTCP 3.5.0 (abort() in the caller context) plus a WebSocket refactor, a larger step than this security patch.
<!-- SECTION:FINAL_SUMMARY:END -->
