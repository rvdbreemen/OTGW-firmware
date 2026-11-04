---
id: TASK-698
title: 'feat-2.0.0: port TASK-697 — logHeapStats lifetime drop counters'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-24 21:38'
updated_date: '2026-05-25 21:54'
labels:
  - diagnostics
  - bug-fix
  - heap
  - port-from-dev
dependencies: []
priority: medium
ordinal: 65000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Companion port of dev TASK-697. logHeapStats (helperStuff.ino:1060-1061 on 2.0.0) prints window counters webSocketDropCount/mqttDropCount that reset after each throttle warning, instead of lifetime state.heapdiag.iWsDropsTotal/iMqttDropsTotal that every other consumer uses. Single 2-arg swap in the DebugTf call.\n\nCross-tree partner of dev TASK-697 / PR #642.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 logHeapStats prints state.heapdiag.iWsDropsTotal and state.heapdiag.iMqttDropsTotal instead of webSocketDropCount/mqttDropCount.
- [x] #2 Window counters webSocketDropCount/mqttDropCount continue to drive canSendWebSocket() / canPublishMQTT() throttle warnings — unchanged.
- [x] #3 pio run -e esp8266 SUCCESS.
- [x] #4 pio run -e esp32 SUCCESS (firmware still fits in 1,966,080 B app partition).
- [x] #5 python evaluate.py --quick no new failures vs current 2.0.0.
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Port of dev TASK-697 to 2.0.0. logHeapStats() in helperStuff.ino now prints state.heapdiag.iWsDropsTotal and state.heapdiag.iMqttDropsTotal (lifetime cumulative counters) instead of the window counters webSocketDropCount/mqttDropCount. Window counters continue to drive canSendWebSocket()/canPublishMQTT() throttle warning emissions unchanged (single 2-arg swap in the DebugTf call at line 1063). Build green for both ESP8266 and ESP32-S3 targets; evaluate.py --quick shows no new failures. Merged as PR #643 (2026-05-24).
<!-- SECTION:FINAL_SUMMARY:END -->
