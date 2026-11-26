---
id: TASK-3103
title: >-
  MQTT pacer: ask mqttPaceAllows() once per loop pass instead of once per SAT
  publisher ask (hd_pace_sat_no climbs about 2400/s with SAT off)
status: To Do
assignee: []
created_date: '2026-11-26T11:17:00Z'
updated_date: '2026-11-26T11:20:00Z'
labels:
  - perf
milestone: 2.0.0
dependencies: []
ordinal: 1166000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the T2 bench (.63, 2.0.0-beta.450+944e29d): hd_pace_sat_no grows about 2400/s with SAT off, hd_pace_sat_live stays 1. Every SAT shadow publisher asks satOutboxFull() on every loop pass (SATmqttPublish.cpp:104..314) and a refusal asks mqttPaceAllows() (MQTTstuff.ino:2118) = MQTTclient.queueSize() + ESP.getMaxAllocHeap() (heap free-list walk under the heap lock, platform_esp32.h:339), the old gate read only mqttOutboxDepth(). Proposal: evaluate mqttPaceAllows() once per loop pass and let all askers read the cached answer (the pass boundary already exists: s_satPublishDeferred reset at SATmqttPublish.cpp:60-62). Cost is estimated, not measured.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Measured cost of the asks before and after (asks per second, microseconds per mqttPaceAllows() call, loop-task share) on the same unit and build flags
- [ ] #2 hd_pace_sat_no growth with SAT off explained (outbox criterion or block criterion) with the outbox depth and max block values at the time
- [ ] #3 Host test on the real code: one evaluation per pass, same yes/no answers as before at the pass boundary, liveness bound (MQTT_PACE_MAX_WAIT_MS) unchanged
- [ ] #4 Own bench by the bench-tester: MQTT 100 %, PINGRESP, heap flat, counters before/after
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Bench numbers (bench-tester, .63 beta.450+944e29d, 12:17-12:18): hd_pace_sat_no +238577 in 60 s (~3976/s), hd_pace_sat_live 1 constant, outbox depth 0, largest block 19444 flat = hd_min_max_block, so the block criterion (MQTT_PACE_MIN_BLOCK 20480, SATmqttPublish.h:107) refuses every ask; only the 5 s liveness bound (MQTT_PACE_MAX_WAIT_MS) lets publishes go.

Scope added 2026-11-26 (coordinator, decision A for T2): (1) calibrate MQTT_PACE_MIN_BLOCK against the steady largest block of .63 (19444) and .61 (to measure), floor 16384 = espMqttClient refusal limit (Helpers.h); (2) answer why hd_pace_sat_live stays 1 (what is counted: ++s_satPaceLive only in the forced branch, SATmqttPublish.cpp, and s_satPaceWaitSinceMs resets whenever a pass defers nothing, satPublishDeferredClear): show it on a host run, not by argument; (3) measured cost of the heap walk before/after.
<!-- SECTION:NOTES:END -->
