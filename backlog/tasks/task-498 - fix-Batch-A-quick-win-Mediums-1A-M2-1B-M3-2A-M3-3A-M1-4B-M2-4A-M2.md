---
id: TASK-498
title: 'fix: Batch A quick-win Mediums (1A-M2, 1B-M3, 2A-M3, 3A-M1, 4B-M2, 4A-M2)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-30 18:54'
labels:
  - code-review
  - follow-up
  - cleanup
dependencies:
  - TASK-466
  - TASK-487
  - TASK-488
  - TASK-494
  - TASK-496
  - TASK-497
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Six quick-win Mediums per `.full-review/06-followup-plan.md` Batch A.

- **1A-M2**: `lastThermostatVal = 0xFFFF` sentinel collides with valid f8.8 value -0.0039 °C. Add explicit `bHaveLastThermostat` flag.
- **1B-M3**: BLE-MQTT exports break `satBLE*` naming convention. Rename `bleMacToCompact`, `bleSensorPublish*` to `satBLE*` form.
- **2A-M3**: BTHome v2 parser drops temperature when sensor prefixes packet with `0x00` packet-ID byte. Add the `0x00` skip-1-byte case.
- **3A-M1**: tests/test_otdirect_override.cpp lacks coalesce-by-MsgID assertion. Add ~6 lines exercising the same-MsgID-replace pattern.
- **4B-M2**: `otCmdQueueHighWater` only visible via OTDDebugTf to telnet. Expose via OTDirect status JSON (REST `/api/v2/otdirect/status`) and PR= reporting.
- **4A-M2**: `SATBLEScanCallbacks` marked `final`.

Validation: ESP32 + ESP8266 build, evaluate.py, fixture validator.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 1A-M2: OTRemoteOverrideState gains `bool bHaveLastThermostat` companion to `lastThermostatVal`; sentinel-via-magic-value pattern removed
- [ ] #2 1B-M3: bleMacToCompact and bleSensorPublish* renamed to satBLE* form (or documented rationale for keeping bare names); callers updated
- [ ] #3 2A-M3: BTHome v2 parser handles `0x00` packet-id object-byte (skips 1, continues parsing) per spec
- [ ] #4 3A-M1: tests/test_otdirect_override.cpp gains coalesce-by-MsgID assertion (single-msgid-replace via otCmdEnqueue equivalent)
- [ ] #5 4B-M2: `otCmdQueueHighWater` exposed via OTDirect status JSON or PR= reporting; documented in MQTT.md or relevant API doc
- [ ] #6 4A-M2: `class SATBLEScanCallbacks final` compiles clean
- [ ] #7 ESP32 build SUCCESS
- [ ] #8 ESP8266 build SUCCESS
- [ ] #9 evaluate.py zero new violations
- [ ] #10 check_otdirect_fixture.py PASS
<!-- AC:END -->
