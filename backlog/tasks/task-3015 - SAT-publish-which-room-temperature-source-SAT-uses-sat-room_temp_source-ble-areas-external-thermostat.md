---
id: TASK-3015
title: >-
  SAT: publish which room temperature source SAT uses (sat/room_temp_source: ble
  / areas / external / thermostat)
status: To Do
assignee: []
created_date: '2026-11-21T16:21:00Z'
labels:
  - sat
  - mqtt
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1132000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter, #alpha-testing msg 1573734878216855563 (2026-11-21): MQTT clients cannot see whether the averaged sensors (multi-area) are applied or SAT fell back. No such topic exists today (grep room_temp_source/temp_source in src/ at origin/dev: no hits). satGetRoomTemp() (SATcontrol.ino:2759+) picks in order: simulation, BLE, multi-area weighted average, MQTT external, OT MsgID 24, with staleness fallbacks, but does not record the choice. Related: TASK-3011 (unchosen BLE sensor wins silently) and TASK-3007 (graph history logs the thermostat room): a source topic would have made both visible. Tester request: needs Robert's OK. Flat per-value topic per ADR-101, on change + heartbeat per ADR-111, inside the ADR-088 burst wrapper.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert approves the topic and its values
- [ ] #2 satGetRoomTemp() records the chosen source; topic published on change with heartbeat (and REST/web UI if decided); HA discovery entity if decided
- [ ] #3 Host test on the real satGetRoomTemp(): each fallback path yields the right source value (red on old code, green on fix)
<!-- AC:END -->
