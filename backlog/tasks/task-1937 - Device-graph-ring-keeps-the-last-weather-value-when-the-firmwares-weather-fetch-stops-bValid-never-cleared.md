---
id: TASK-1937
title: >-
  Device graph ring keeps the last weather value when the firmware's weather
  fetch stops (bValid never cleared)
status: To Do
assignee: []
created_date: '2026-11-14T14:45:00Z'
labels:
  - bug
  - graph
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 992000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
By reading (09, TASK-1875 review): graphHistoryStuff.ino:67-69 stores state.sat.weather.fTemperature in the 24 h ring (ADR-193) whenever state.sat.weather.bValid is true, and bValid is only ever set to true (SATweather.ino:529); nothing clears it when the weather fetch stops working. SATcontrol uses an age check (wxFresh, SATcontrol.ino:~2700), the ring does not, so after a weather outage the ring records the last value as if it were current. Not seen in the field yet; not reproduced.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce: with the weather fetch failing for longer than its refresh age, the ring keeps writing the old weather value (host test or bench)
- [ ] #2 Fix: the ring uses the same freshness rule as SAT (or MsgID 27 when the weather value is stale); red-then-green
<!-- AC:END -->
