---
id: TASK-348
title: Fix discovery drip limbo on publish failure
status: Done
assignee:
  - '@claude'
created_date: '2026-04-20 19:23'
updated_date: '2026-04-21 17:03'
labels:
  - mqtt
  - discovery
  - 1.4.1
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The drip loop in loopMQTTDiscovery() currently clears the pending bit for an OT msgid regardless of whether doAutoConfigureMsgid() succeeded (MQTTstuff.ino:1139-1143). On heap pressure, low broker availability, or STREAM_HEAP_MIN failure, the discovery publish silently drops. The msgid then sits in limbo: not in MQTTautoConfigMap (not done), not in MQTTautoCfgPendingMap (not pending). Only an external markAllMQTTConfigPending() trigger recovers it.

Fix: clear the pending bit ONLY on success. On failure, leave it set so the next drip tick retries. Rate-limited by the drip timer itself (2s normal, 10s slow-mode when under heap pressure), so no busy-loop risk. Tester log debug_4f.txt showed 241 MQTT drops in 6 minutes on 1.4.0-beta baseline — a subset of those were first-publish-after-discovery and left msgids in this limbo state.

Part of the discovery verification + auto-heal plan. Ships first as lowest-risk cleanup.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTTstuff.ino:1134-1143 updated: pending-bit bitClear moved inside the if (success) branch
- [x] #2 On failure, added MQTTDebugTf logging OT ID %d publish failed, retaining pending
- [x] #3 On success, added MQTTDebugTf logging OT ID %d published OK
- [x] #4 Dallas sensor path (if msgId == OTGWdallasdataid) unchanged — already correctly clears pending after configSensors() returns
- [x] #5 Build passes esp8266 via build.py --firmware
- [x] #6 evaluate.py --quick reports 100% health
- [x] #7 Manual verify: under induced heap pressure, telnet log shows retaining pending, next drip tick re-attempts same msgid, eventually succeeds
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Edit MQTTstuff.ino around line 1135: move bitClear into the if (success) branch
2. Add failure-path MQTTDebugTf logging
3. Build esp8266 with build.py --firmware
4. Commit with descriptive title
5. Push to origin/1.4.1
6. Draft docs/adr/ADR-062 as Proposed
7. Stop for user review
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed the discovery drip limbo bug on 1.4.1. MQTTstuff.ino:1134-1143 now clears the pending bit only when doAutoConfigureMsgid returns true. On failure, pending stays set, and the next drip tick (2s normal, 10s slow-mode) retries automatically.

Added two new MQTTDebug log lines: "[drip] OT ID N published OK" and "[drip] OT ID N publish failed, retaining pending" for traceability.

Dallas sensor path unchanged (configSensors handles its own pending clear).

Build verified: clean esp8266 firmware compile, evaluate.py 100% health, no PROGMEM/String warnings.

Expected impact on tester workload (Crashevans debug_4f.txt baseline showed 241 MQTT drops in 6 min, some from first-publish-after-discovery): missing entities should now self-heal within 2-10 seconds rather than requiring HA restart or firmware reboot.
<!-- SECTION:FINAL_SUMMARY:END -->
