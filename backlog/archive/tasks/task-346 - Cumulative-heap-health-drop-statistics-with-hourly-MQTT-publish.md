---
id: TASK-346
title: Cumulative heap health + drop statistics with hourly MQTT publish
status: Done
assignee:
  - '@claude'
created_date: '2026-04-20 07:53'
updated_date: '2026-04-21 17:02'
labels:
  - mqtt
  - heap
  - observability
  - 1.4.1
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add lifetime counters for MQTT drops, WebSocket drops, heap-health tier transitions, and discovery-drip quiesce events. Publish hourly via the hourChanged() hook (piggybacked on the same hook the nightly restart check uses). Expose in /api/v2/devinfo so the WebUI Device Information section picks them up automatically.

**Why**
Existing webSocketDropCount/mqttDropCount in helperStuff.ino reset every 10s after a warning log, so cumulative totals are lost. Heap-health transitions are not tracked at all. Without these, we cannot correlate user reports ("my OTGW restarted") with actual pressure events.

**Scope**
- Add state.heapdiag struct per ADR-051
- Counters reset on reboot (correlate with state.uptime.iRebootCount if long-term tracking needed)
- Hourly MQTT publish to otgw-firmware/stats/heap as a small JSON blob, retained
- Hour boundary shared with Nightly Restart via single hourChanged() call
- Expose in devinfo JSON so UI renders them in the Device Information tab

**Out of scope**
- HA auto-discovery entities for these stats (separate follow-up if wanted)
- Persistent counters across reboots (requires LittleFS, not needed yet)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 state.heapdiag struct added (wsDropsTotal, mqttDropsTotal, enteredLowCount, enteredWarningCount, enteredCriticalCount, dripQuiescedCount, dripSlowModeCount)
- [x] #2 Counters incremented in canSendWebSocket, canPublishMQTT, getHeapHealth, loopMQTTDiscovery
- [x] #3 getHeapHealth tracks previous level and increments entered-counters on transition
- [x] #4 sendMQTTheapdiag() publishes JSON to otgw-firmware/stats/heap (retained)
- [x] #5 hourChanged() call lifted to share between nightly restart and stats publish
- [x] #6 devinfo REST endpoint exposes heapdiag fields
- [x] #7 Full build passes on esp8266
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add HeapDiagSection struct to OTGW-firmware.h + add to OTGWState
2. Add getHeapHealth transition tracking (static lastLevel, increment on tier entry)
3. Add drop counter increments in canSendWebSocket and canPublishMQTT
4. Add drip quiesce + slow-mode increments in loopMQTTDiscovery
5. Add sendMQTTheapdiag() function in MQTTstuff.ino
6. Refactor doTaskEvery60s to compute hourBoundary once, use for both nightly restart AND stats publish
7. Expose heapdiag fields in devinfo REST endpoint
8. Build + commit + push + close task
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added cumulative heap-pressure diagnostics on branch 1.4.1.

**Counters** (state.heapdiag per ADR-051, reset on reboot):
- iWsDropsTotal, iMqttDropsTotal: lifetime drops from the canSendWebSocket / canPublishMQTT throttle gates
- iEnteredLowCount, iEnteredWarningCount, iEnteredCriticalCount: tier-entry transitions in getHeapHealth (INTO stricter only, recovery not counted)
- iDripQuiescedCount: discovery drip ticks skipped during Status-burst (from TASK-342)
- iDripSlowModeCount: transitions to 10s slow-mode drip (from TASK-339)
- iLastPublishedEpoch: unix-epoch of last MQTT publish

**MQTT publish**: sendMQTTheapdiag() emits a single ~200-byte retained JSON to otgw-firmware/stats/heap. Hourly via hourChanged() hook in doTaskEvery60s. 24 publishes/day, ~4.8 KB/day extra traffic.

**Shared hour-boundary**: doTaskEvery60s now calls hourChanged() ONCE and dispatches to both nightly restart and stats publish. Eliminates the consume-on-read race warned about in TASK-345.

**REST/UI**: /api/v2/devinfo exposes 8 new hd_* fields. translateFields in index.js labels them for the Device Information tab (e.g. "Heap Fragmentation (%)", "MQTT Drops (since boot)"). No new UI card needed; existing refreshDeviceInfo renderer picks them up.

**Build verified**: esp8266 firmware 724,592 bytes (+912 from pre-TASK-346 baseline), littlefs 1.98MB. Commit 9bd51f0b on origin/1.4.1.

---

**Erratum (2026-04-21, per TASK-367)**

The claim above that the hourly publish runs via "hourChanged() hook in doTaskEvery60s" is no longer accurate after TASK-350. Per ADR-086 (unified time-boundary dispatcher), the sendMQTTheapdiag call site was moved out of doTaskEvery60s and into the if(hourFlag) block inside doTaskMinuteChanged. The dispatch is still once-per-hour and still shares its hour boundary with the nightly restart check, but the containing function and trigger path have changed. Behaviour is preserved; only the call-site location moved.
<!-- SECTION:FINAL_SUMMARY:END -->
