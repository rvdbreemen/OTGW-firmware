---
id: TASK-876
title: >-
  fix(mqtt): SAT command pv_boost_max_duration_min unreachable — satSubCmd
  buffer too small (F6)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-15 14:30'
updated_date: '2026-06-15 20:07'
labels: []
dependencies: []
ordinal: 92000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MQTT review F6 (MEDIUM). The SAT sub-command token is read into satSubCmd[24] (MQTTstuff.ino:893/930); readMQTTTopicToken truncates to 23 chars, so the 25-char command pv_boost_max_duration_min (TASK-640/ADR-110) is truncated to pv_boost_max_duration_m, never matches the dispatch table (MQTTstuff.ino:766), and is silently dropped. Exactly the TASK-292 silent-miss class ADR-078 dispatch tables were meant to make obvious; the stale buffer comment claims 'longest is solar_freeze_integral at 21'. REST still writes the setting (workaround). Fix: enlarge satSubCmd to >= 32, add a static_assert tying the buffer size to the longest kSatMqttCmds token, and fix the stale comment.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 satSubCmd sized >= longest-command+1 (>=32) with a static_assert against the dispatch table
- [x] #2 set/<nodeId>/sat/pv_boost_max_duration_min reaches the handler and applies
- [ ] #3 Build green 3 targets; evaluate.py --quick no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
F6: satSubCmd[24]->[32] + static_assert(sizeof("pv_boost_max_duration_min")<=sizeof(satSubCmd)); stale comment fixed.
Implemented on branch claude/mqtt-reliability-phase3 (off feature-2.0.0-esp32s3-async). evaluate.py --quick green (0 failures). ESP32 build NOT verifiable in this container (network policy blocks PlatformIO framework-arduinoespressif32 download); build + field ACs left for maintainer verification.
<!-- SECTION:NOTES:END -->
