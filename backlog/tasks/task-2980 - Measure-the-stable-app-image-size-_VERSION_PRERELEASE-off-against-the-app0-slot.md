---
id: TASK-2980
title: >-
  2.0.0 flash trim before 29 Nov: measure the stable image, DER roots, debug
  port as its own feature, compile-time debug level (linked to the temporary
  99.5 % size gate)
status: To Do
assignee:
  - '@otgw-poc-dev'
  - '@otgw-dev'
created_date: '2026-11-20T17:04:00Z'
updated_date: '2026-11-20T17:26:00Z'
labels:
  - build
  - flash-size
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1112000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert, 2026-11-20 (DM 'A1', relayed by the coordinator): the TASK-1993 size gate is temporarily raised to 99.5 % until 2026-11-29, so remote telemetry (recorder + TLS, about 113 KB; lane 1a0ea63 at 2,266,576 B = 98.8 %) can enter 2.0.0; this task brings the image back under 98 % (2,247,884 B) by the end date. Needed: about 18.7 KB below the lane figure, minus what the diagRec removal (ADR-268, about 4-9 KB, otgw-poc-dev) frees. Items, each measured before/after on the same commit base: (1) measure the stable image (_VERSION_PRERELEASE off drops prerelease-only code; guess 1-5 KB); (2) the 4 PEM roots (about 5.9 KB text) as DER (about 1.6 KB; estimate from otgw-poc-dev); (3) the debug port as its own feature that a release build can leave out (scope to define in the plan); (4) a compile-time debug level for our telnet/debug literals (part of about 134 KB of our string pool; measure the debug share first). The other levers stay in TASK-2981 (2.1.0). Builds only through build.bat / ./build.sh.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Stable-image size measured and recorded (githash, .ino.bin bytes, TASK-1993 build-log percentage), difference against the prerelease build of the same commit stated
- [ ] #2 DER roots, debug-port feature and compile-time debug level each landed or explicitly dropped, with before/after .ino.bin bytes per item and run_tests + evaluate --quick green
- [ ] #3 By 2026-11-29 the esp32-combo image with remote telemetry is at or below 98 % of app0 (2,247,884 B), and build.py's temporary 99.5 % gate is returned to 98 %
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
otgw-dev input (2026-11-20, relayed by otgw-poc-dev): the BOL+printf merge per debug call site is already on the telemetry lane (-5.5 KB measured), and the debug gate stays inline, so the compile-time debug level item has less left than first estimated; measure before choosing it. The build.py comment for the temporary 99.5 % gate references this task; the gate change itself is TASK-2983.
<!-- SECTION:NOTES:END -->
