---
id: TASK-3015
title: >-
  SAT: publish which room temperature source SAT uses (sat/room_temp_source: ble
  / areas / external / thermostat)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-21T16:21:00Z'
updated_date: '2026-11-21T17:05:00Z'
labels:
  - sat
  - mqtt
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21: released by Robert in #alpha-testing (msg 1573739250422386720, reply to our TASK-3015 notice): implement as suggested (sat/room_temp_source with values ble / areas / external / thermostat). Design input from gruijter on TASK-3011 (source-order command + retained state) belongs to the same design.

2026-11-21 (otgw-dev, same change as TASK-3011): satGetRoomTemp() records its source in _sat_roomSrc at every return (S/B/A/E/T/F/N); satRoomSourceName() gives simulation / ble / areas / external / thermostat / estimate / none. Published retained on sat/room_temp_source (publishIfChangedS, next to sat/room_temp) and in /api/v2/sat/status as room_temp_source (snapshotted right after the resolve). Checked by tests/test_sat_ble_pin_only.py (every return records a source; topic + REST field present). Not yet: HA discovery entity for it, bench check on a device.

2026-11-21 (otgw-dev, coordinator decision 4 = yes): HA diagnostic sensor entity SAT_Room_Temp_Source on sat/room_temp_source (id 252 row, HaEntityCat::diagnostic, flat topic per ADR-101); MQTT_HA_SENSOR_COUNT 405 -> 406 and mqttHaSensorIndex recomputed (6 entries after the insert shift). Discovery tests 9 files PASS (test_sat_energy_discovery, test_ble_discovery_1462, test_sat_health_discovery and others; the first run failed 3 files on the stale index before the recompute). test_sat_ble_pin_only checks the entity row.
<!-- SECTION:NOTES:END -->
