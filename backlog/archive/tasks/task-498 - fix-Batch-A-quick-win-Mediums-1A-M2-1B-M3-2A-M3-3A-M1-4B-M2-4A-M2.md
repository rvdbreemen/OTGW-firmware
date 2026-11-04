---
id: TASK-498
title: 'fix: Batch A quick-win Mediums (1A-M2, 1B-M3, 2A-M3, 3A-M1, 4B-M2, 4A-M2)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-30 18:54'
updated_date: '2026-05-01 05:21'
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
- [x] #1 1A-M2: OTRemoteOverrideState gains `bool bHaveLastThermostat` companion to `lastThermostatVal`; sentinel-via-magic-value pattern removed
- [x] #2 1B-M3: bleMacToCompact and bleSensorPublish* renamed to satBLE* form (or documented rationale for keeping bare names); callers updated
- [x] #3 2A-M3: BTHome v2 parser handles `0x00` packet-id object-byte (skips 1, continues parsing) per spec
- [x] #4 3A-M1: tests/test_otdirect_override.cpp gains coalesce-by-MsgID assertion (single-msgid-replace via otCmdEnqueue equivalent)
- [x] #5 4B-M2: `otCmdQueueHighWater` exposed via OTDirect status JSON or PR= reporting; documented in MQTT.md or relevant API doc
- [x] #6 4A-M2: `class SATBLEScanCallbacks final` compiles clean
- [x] #7 ESP32 build SUCCESS
- [x] #8 ESP8266 build SUCCESS
- [x] #9 evaluate.py zero new violations
- [x] #10 check_otdirect_fixture.py PASS
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## TASK-498 — Batch A quick-win Mediums

Six Phase 1-4 Medium findings closed in commit `90608866`:

- **1A-M2**: `bHaveLastThermostat` flag added to `OTRemoteOverrideState`; sentinel-via-magic-value pattern removed across 5 touchpoints (init list, setRemoteOverride, clearRemoteOverride, both branches in onThermostatMsgID16, resetTransientState).
- **1B-M3**: BLE-MQTT helpers renamed to `satBLE*` form for naming consistency with `satBLEInit/Loop/UpdateState/PublishMQTT/SendStatusJSON`. Touches MQTTstuff.ino, SATble.ino, OTGW-firmware.h.
- **2A-M3**: BTHome v2 parser handles object-id `0x00` (packet-id) by skipping 1 byte and continuing. Spec-compliant devices that prefix temperature with packet-id no longer have their reading dropped.
- **3A-M1**: tests/test_otdirect_override.cpp gains 4 assertions exercising coalesce-by-MsgID semantics (same-MsgID does not grow depth; latest-value-wins; different-MsgID grows depth; position-preserving).
- **4B-M2**: `otCmdQueueHighWater` exposed via `sendOTDirectOverridesJSON()` as a sibling `queue` field with `{depth, highWater, capacity}`. Visible to web UI / MQTT dashboards without telnet.
- **4A-M2**: `class SATBLEScanCallbacks final`.

### Verification
- ESP32 build SUCCESS (2m22s, build 3865).
- ESP8266 build SUCCESS (56s, byte-identical RAM/Flash).
- Grep confirms no orphan `ble*` BLE-helper references in firmware code (renames complete).

All 10 ACs satisfied.
<!-- SECTION:FINAL_SUMMARY:END -->
