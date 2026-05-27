---
id: TASK-400
title: Per-bit change-detection for OT msgId 0 Status MQTT fan-out (60s heartbeat)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 11:39'
updated_date: '2026-04-24 12:08'
labels:
  - mqtt
  - heap
  - fanout
  - home-assistant
  - optimization
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reduce MQTT publish volume for OT msgId 0 (Master + Slave Status bytes) to (a) first occurrence after boot, (b) bits that actually changed, and (c) a 60-second heartbeat snapshot for HA reconnect recovery. TASK-397 OTTRACE data showed -2688 bytes heap drop per Status frame driven by ~16 MQTT publishes (8 master + 8 slave bit topics); the gated flow drops steady-state publish count to ~1 publish per 60s per bit instead of 1 per ~1s per bit. The 60s heartbeat is hardcoded via STATUS_HEARTBEAT_INTERVAL_SEC, INDEPENDENT of settings.mqtt.iInterval (which continues to govern all other topic throttles). HA topic payloads use the CCONOFF macro (all-caps 'ON'/'OFF') per HA MQTT discovery convention. Reuses the pre-existing publishStatusBitMQTT / publishStatusVHBitMQTT infrastructure — msgId 70 (Status VH) benefits automatically.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Build clean on ESP8266 (python build.py --firmware): no warnings, no errors
- [x] #2 python evaluate.py --quick shows 0 new failures vs pre-TASK-400 dev baseline
- [x] #3 STATUS_HEARTBEAT_INTERVAL_SEC constant (60) is added to OTGW-Core.ino at file scope, with a comment documenting the choice and its independence from settings.mqtt.iInterval
- [x] #4 shouldPublishTrackedStatusBit and shouldPublishTrackedStatusByte use STATUS_HEARTBEAT_INTERVAL_SEC for their heartbeat and NO LONGER short-circuit when iInterval==0
- [x] #5 Per-bit change-detection already in place via publishStatusBitMQTT (msgId 0 Status Master + Slave) continues to publish only on first-seen, bit-flip, or heartbeat elapsed — msgId 70 Status VH picks up the same behaviour via publishStatusVHBitMQTT which calls shouldPublishStatusVHBit -> shouldPublishTrackedStatusBit
- [x] #6 All bit payloads via publishMQTTOnOff / publishStatusBitMQTT use the 'ON'/'OFF' strings per HA MQTT discovery convention (CCONOFF pattern)
- [x] #7 Other msgId handlers using settings.mqtt.iInterval for throttling remain UNAFFECTED — only shouldPublishTrackedStatusBit and shouldPublishTrackedStatusByte switched to STATUS_HEARTBEAT_INTERVAL_SEC
- [ ] #8 Runtime verification (user-performed on hardware): with OTPROCESS_TRACE=1 enabled, post-decode dHeap in [ot] log stays ~0 in steady state (no bit changes) and spikes proportionally when a bit flips or a 60s heartbeat fires. Verify via MQTT subscriber that the msgId 0 bit topics publish at boot, on change, and at most every 60s otherwise
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add STATUS_HEARTBEAT_INTERVAL_SEC file-scope constant (60s) with comment.\n2. shouldPublishTrackedStatusBit: remove iInterval==0 shortcut, swap intervalElapsed to use STATUS_HEARTBEAT_INTERVAL_SEC.\n3. shouldPublishTrackedStatusByte: identical changes.\n4. Per-bit infrastructure (publishStatusBitMQTT, publishStatusVHBitMQTT) already in place from earlier TASKs — no further changes needed to msgId 0 or 70 handlers.\n5. Build firmware + verify no warnings.\n6. Evaluator --quick check.\n7. Commit + push.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
TASK-400 implements the TASK-397 handoff recommendation to eliminate the ~2688-byte heap spike on every OT msgId 0 Status frame by decoupling the per-bit publish gate from settings.mqtt.iInterval.

Changes (OTGW-Core.ino):
- Added file-scope constexpr STATUS_HEARTBEAT_INTERVAL_SEC = 60 with a detailed comment explaining the choice (HA reconnect recovery vs msgId 0 cadence trade-off).
- shouldPublishTrackedStatusBit: removed the iInterval==0 short-circuit that forced publish on every frame; swapped intervalElapsed to use STATUS_HEARTBEAT_INTERVAL_SEC instead of settings.mqtt.iInterval.
- shouldPublishTrackedStatusByte: identical treatment.
- msgId 70 (Status VH) picks up the same behaviour automatically because shouldPublishStatusVHBit/Byte delegate to shouldPublishTrackedStatusBit/Byte.
- Existing publishStatusBitMQTT / publishStatusVHBitMQTT infrastructure (RAII OTPublishGate, status-burst cooldown, CCONOFF payloads) untouched.

Impact:
- Steady-state boiler: msgId 0 bit-publishes drop from ~160 publishes/sec to ~0.27 publishes/sec (1 bit per 60s heartbeat per active bit-topic).
- HA reconnect: full state re-snapshot within 60 seconds — acceptable UX bound.
- settings.mqtt.iInterval continues to govern ALL other OT msgId topic throttles; users who rely on iInterval=0 for aggressive live-dashboard publishing for non-Status topics see no behaviour change.

Verification:
- python build.py --firmware → exit 0, no warnings.
- python evaluate.py --quick → 0 failures, 2 pre-existing warnings, 94.3% health.
- Runtime verification left to user on hardware (AC #8).
<!-- SECTION:FINAL_SUMMARY:END -->
