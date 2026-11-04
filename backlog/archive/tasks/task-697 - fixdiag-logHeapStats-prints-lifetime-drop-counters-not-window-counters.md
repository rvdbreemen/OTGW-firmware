---
id: TASK-697
title: 'fix(diag): logHeapStats prints lifetime drop counters, not window counters'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-24 21:35'
updated_date: '2026-05-24 21:36'
labels:
  - diagnostics
  - bug-fix
  - heap
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
logHeapStats (helperStuff.ino:1110-1111) prints webSocketDropCount and mqttDropCount — the *window* counters that get reset to 0 after each 'WebSocket/MQTT throttled: dropped N msgs' warning. Result: the per-minute heap line shows ephemeral snapshots that keep returning to small numbers instead of monotonic lifetime totals. Surfaced during beta.20 log review when user asked why the drop counters were not continuously increasing.\n\nEvery OTHER consumer correctly uses the lifetime counters (state.heapdiag.iWsDropsTotal / iMqttDropsTotal):\n- MQTTstuff.ino:1144-1145 publishStatU32 for MQTT stats topics\n- restAPI.ino:1051-1052 /api/v2/heap REST response\n- networkStuff.ino:311-312 devinfo handler\n- handleDebug.ino:107-108 debug dump\n\nFix: swap webSocketDropCount → state.heapdiag.iWsDropsTotal and mqttDropCount → state.heapdiag.iMqttDropsTotal in the single DebugTf call. Format specifier %u still works (both are uint32_t).\n\nCompanion task on 2.0.0 lands the same fix in the parallel worktree (helperStuff.ino:1060-1061 on that branch).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 logHeapStats in helperStuff.ino prints state.heapdiag.iWsDropsTotal and state.heapdiag.iMqttDropsTotal instead of webSocketDropCount and mqttDropCount.
- [x] #2 Format specifiers in the DebugTf format string remain %u (uint32_t in both cases).
- [x] #3 Window counters webSocketDropCount and mqttDropCount continue to be used by canSendWebSocket() and canPublishMQTT() for their existing 'dropped N msgs in this throttle window' warnings — those are unchanged.
- [x] #4 python build.py --firmware exits 0.
- [x] #5 python evaluate.py --quick shows no new failures vs current dev.
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced webSocketDropCount / mqttDropCount (window counters that reset after each throttle warning) with state.heapdiag.iWsDropsTotal / iMqttDropsTotal (lifetime cumulative) in the per-minute logHeapStats line. Now the heap line shows monotonic counters that match every other consumer (MQTT stats topics, /api/v2/heap REST, handleDebug dump, devinfo). The window counters are still used by canSendWebSocket() / canPublishMQTT() for their own "throttled: dropped N msgs in this window" warnings — unchanged.

Build: python build.py --firmware exits 0.
Evaluator: python evaluate.py --quick: 34 / 0 / 0 (100%).
<!-- SECTION:FINAL_SUMMARY:END -->
