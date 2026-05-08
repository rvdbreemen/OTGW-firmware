---
id: TASK-577
title: Pure JIT MQTT discovery — publish OT configs only when MsgID received
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-08 10:23'
updated_date: '2026-05-08 10:38'
labels:
  - mqtt
  - ha-discovery
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Replace unconditional boot-republish of all 256 OT discovery configs with a pure JIT approach: a discovery config for an OT MsgID is only published when that MsgID is actually received on the OpenTherm bus. Non-OT configs (Dallas, PIC firmware, OTDirect diagnostics, SAT, HA entity) are still published directly at trigger points.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 startMQTT() no longer calls markAllMQTTConfigPending(); only publishNonOTDiscoveryConfigs() is called
- [x] #2 OT message handler triggers markMQTTConfigPending(id) when MsgID received and done-bit not set
- [x] #3 publishNonOTDiscoveryConfigs() function extracted and called at boot, top-topic change, broker restart, and force
- [x] #4 Broker restart detected: MQTT disconnected >5 min while WiFi was up → clearMQTTConfigDone() + JIT
- [x] #5 Top-topic change: clearMQTTConfigDone() + clearMQTTConfigPending() + publishNonOTDiscoveryConfigs() (JIT for OT)
- [x] #6 Force (REST/debug F): markAllMQTTConfigPending() + publishNonOTDiscoveryConfigs() — all IDs queued immediately
- [x] #7 homeassistant/status=online handler no longer calls doAutoConfigure()
- [x] #8 Build passes: python build.py exits 0
- [x] #9 Evaluator green: python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Pure JIT MQTT discovery implemented on dev/MQTTstuff.ino. Removed markAllMQTTConfigPending() from startMQTT() and homeassistant/status online handler. Added clearMQTTConfigPending() and publishNonOTDiscoveryConfigs() (queues only climate/number/Dallas/heap-stats/firmware-info/PIC pseudo-IDs). Broker restart detection: offlineMs > 5 min resets both bitmaps + calls publishNonOTDiscoveryConfigs(). Force path (doAutoConfigure) unchanged. JIT trigger in OTGW-Core.ino was already correct. ADR-073 added as Proposed. Build green, evaluator unchanged, pushed as commit 1bb58d8f (beta.30).
<!-- SECTION:FINAL_SUMMARY:END -->
