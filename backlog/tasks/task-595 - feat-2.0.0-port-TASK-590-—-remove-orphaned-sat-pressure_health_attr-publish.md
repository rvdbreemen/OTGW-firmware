---
id: TASK-595
title: 'feat-2.0.0: port TASK-590 — remove orphaned sat/pressure_health_attr publish'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 21:56'
updated_date: '2026-05-08 22:52'
labels:
  - sat
  - mqtt
  - cleanup
  - port-from-dev
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of dev TASK-590. SATcontrol.ino:2050-2070 publishes sat/pressure_health_attr (20-line JSON build + send). No HA discovery entry exists for sat/pressure_health (the binary sensor) in MQTTHaDiscovery.cpp, so the _attr sub-topic has no consumer. The underlying pressure data is already available as flat scalar topics sat/pressure, sat/pressure_drop_rate, sat/pressure_alarm. Remove the orphaned pressAttrBuf block (lines 2050-2070), leaving just the sat/pressure_health ON/OFF publish. Same rationale as dev TASK-590.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 sat/pressure_health_attr publish block removed from SATcontrol.ino; sat/pressure_health ON/OFF publish retained
- [x] #2 static char pressAttrBuf[200] local buffer removed (frees ~200 bytes per call, no static persistence)
- [x] #3 Build exits 0 for both ESP8266 and ESP32 targets
- [x] #4 evaluate.py --quick shows no new failures
- [x] #5 Prerelease bump committed alongside the firmware change
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported TASK-590 from dev: removed orphaned sat/pressure_health_attr publish path that published a non-existent attribute.

Changes:
- SATcontrol.ino: removed the pressAttrBuf allocation and sat/pressure_health_attr MQTT publish call that was left over after the pressure attribute was migrated to the json_attributes topic; the attribute is now covered by TASK-594's json_attributes_topic
- Eliminates a redundant / stale MQTT retain message on a topic HA no longer expects

Tests:
- python build.py (ESP8266 + ESP32-S3 both clean, alpha.34)
- python evaluate.py --quick: 95.6% health, no new violations
- AC 1-5 all verified
<!-- SECTION:FINAL_SUMMARY:END -->
