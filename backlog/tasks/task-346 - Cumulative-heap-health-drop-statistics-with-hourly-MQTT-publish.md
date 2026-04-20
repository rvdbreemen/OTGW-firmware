---
id: TASK-346
title: Cumulative heap health + drop statistics with hourly MQTT publish
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-20 07:53'
updated_date: '2026-04-20 07:53'
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
- [ ] #1 state.heapdiag struct added (wsDropsTotal, mqttDropsTotal, enteredLowCount, enteredWarningCount, enteredCriticalCount, dripQuiescedCount, dripSlowModeCount)
- [ ] #2 Counters incremented in canSendWebSocket, canPublishMQTT, getHeapHealth, loopMQTTDiscovery
- [ ] #3 getHeapHealth tracks previous level and increments entered-counters on transition
- [ ] #4 sendMQTTheapdiag() publishes JSON to otgw-firmware/stats/heap (retained)
- [ ] #5 hourChanged() call lifted to share between nightly restart and stats publish
- [ ] #6 devinfo REST endpoint exposes heapdiag fields
- [ ] #7 Full build passes on esp8266
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
