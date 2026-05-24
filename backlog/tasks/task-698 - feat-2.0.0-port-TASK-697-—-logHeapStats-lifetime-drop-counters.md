---
id: TASK-698
title: 'feat-2.0.0: port TASK-697 — logHeapStats lifetime drop counters'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-24 21:38'
updated_date: '2026-05-24 21:38'
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
- [ ] #1 logHeapStats prints state.heapdiag.iWsDropsTotal and state.heapdiag.iMqttDropsTotal instead of webSocketDropCount/mqttDropCount.
- [ ] #2 Window counters webSocketDropCount/mqttDropCount continue to drive canSendWebSocket() / canPublishMQTT() throttle warnings — unchanged.
- [ ] #3 pio run -e esp8266 SUCCESS.
- [ ] #4 pio run -e esp32 SUCCESS (firmware still fits in 1,966,080 B app partition).
- [ ] #5 python evaluate.py --quick no new failures vs current 2.0.0.
<!-- AC:END -->
