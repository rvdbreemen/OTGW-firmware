---
id: TASK-307
title: >-
  [ARCH-M3] Replace 76-branch SAT MQTT strcasecmp chain with dispatch table
  (plus ADR)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:21'
updated_date: '2026-04-19 07:36'
labels:
  - architecture
  - review-2026-04-18
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MQTTstuff.ino:477-620 uses chained strcasecmp_P for 76 SAT subcommand tokens. Violates CLAUDE.md typed-control-flow rule. TASK-292 was a silent miss from this exact pattern. kV2Routes[] shows precedent.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 New ADR describes MQTT subcommand dispatch table pattern
- [x] #2 kSatMqttCmds[] PROGMEM table replaces the chained strcasecmp block
- [x] #3 All 76 existing tokens land in the table; regression test confirms each routes correctly
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ADR-078 (MQTT Sub-command Dispatch Tables) added; MQTTstuff.ino SAT dispatch refactored into kSatMqttCmds[] PROGMEM-style table with sentinel terminator, mirroring kV2Routes[] in restAPI.ino. 76-branch strcasecmp_P chain replaced by a 10-line dispatch loop plus two special cases (area, zone) that still need sub-token parsing. Void-adapter wrappers handle the bool-returning handlers (satHandleTargetTemp/External/Humidity/SunElevation) and the no-arg handlers (satOvpStart/Stop/ResetIntegral/Flush) to keep the handler-signature uniform. Both ESP8266 and ESP32-S3 build clean; ESP8266 .ino.bin actually shrunk slightly (0.77 -> 0.76 MB) because the table has less branch-heavy code.
<!-- SECTION:FINAL_SUMMARY:END -->
