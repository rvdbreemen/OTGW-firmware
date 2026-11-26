---
id: TASK-3102
title: >-
  SAT: zone_count counts extra rooms only (default 0), zone diagnostics from 1
  zone with data, zone_count state topic, zones in sat/status (firmware part of
  TASK-3019)
status: To Do
assignee: []
created_date: '2026-11-26T10:22:00Z'
updated_date: '2026-11-26T10:27:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-3019
priority: low
ordinal: 1165000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Firmware part of TASK-3019 (split by otgw-webui 2026-11-26, plan in TASK-3019). Decisions: entry 459 (Robert, 2026-11-21: 'Fix this as suggested.' and DM 'A' = 0 means no extra rooms and is the default, stored values migrate once, zone diagnostics from 1 zone, zone_count state topic). Also relied on: entry 116 (zones follow thermo-nova area.py aggregation), entry 378 (eco: only the main room decides the band; comfort: any usable room), entry 224 (zone PID reset rule kept), entry 111 (zone PIDs step on the 60 s timer). Read all five first; none of them contradicts the points below, none sets the migration mapping.

Facts at origin/dev 9f3b8af60: the zone loops already count EXTRA rooms: satControlLoop SATcontrol.ino:8214-8225, satPublishZoneDiagnostics :3726-3735, satShouldDiscoverZone :3510-3514 run for z < iZoneCount over satZones[0..]; a zone without usable inputs counts as no zone. Only the default 1 (SATtypes.h:614), the clamp 1..4 (settingStuff.ino:1251), the comment and the diagnostics gate 'iZoneCount > 1' (:8330, deliberate TASK-1440) carry the old 'main room only' meaning.

PLAIN WARNING: default 1 to 0 and clamp 0..4 change a SAT setting default and range. Entry 459 settles the default (0) and the range follows from it. The MIGRATION is not settled: entry 459's own reading (ours, not Robert's words) maps stored 1 to 0 and N to N-1 'only if the old N counted the main room'. The code check says old N did NOT count the main room, so N to N-1 is wrong, and 1 to 0 would switch off a real zone for someone who pushes zone 1 data (gruijter does). Recommendation: leave stored values as they are (behaviour-preserving), change only the default for fresh and reset settings, and silence zones that never received data. This needs Robert's one-line yes before the code is written: 'a stored 1 stays 1' (A) or 'a stored 1 becomes 0' (B, drops a zone that has data). The coordinator puts it to Robert by DM; entry 459's Open line is closed by his answer (new entry).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert has answered the migration question (stored 1 stays 1, or becomes 0) and the answer is recorded as a register entry naming entry 459; the plan names entries 459, 116, 378, 224, 111
- [ ] #2 settings.sat.iZoneCount default 0, accepted range 0..4 in settingStuff, REST range and settings help; comment SATtypes.h fixed; with 0 no zone runs and nothing publishes; host test on the real parse code red on old, green on fix
- [ ] #3 sat/zone/<n>/active|output|error publish from 1 zone, only for zones that have ever received data (satZones[i].iLastUpdateMs != 0); a single-room install with no zone data publishes none of them (TASK-1440 concern); host test on the real publisher red before / green after
- [ ] #4 Retained state sat/zone_count through publishIfChangedI (ADR-111) with its shadow in the republish-all reset; host test red/green
- [ ] #5 /api/v2/sat/status carries a zones array (index, room_temp, setpoint, active) for up to iZoneCount zones, so the web UI rows of TASK-3019 can read it; host test on the real renderer
- [ ] #6 Bench run by the bench-tester: push zone 1 data, read the three zone topics and zone_count back, then with no zone data check none appear; evaluate --quick green; prerelease bump
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
PROOF of the reading 'old N does not count the main room' (otgw-webui 2026-11-26, origin/dev 9f3b8af60, SATcontrol.ino). MAIN ROOM enters on its own path: the PID output is satPidOutput(curveValue) from the primary room temperature (:8588-8589, roomValid / roomTemp from satGetRoomTemp), before any zone. ZONES are separate structs: satZones[4] (:3792); the push topic sat/zone/<n>/room_temp with n 1-based writes satZones[n-1] (satHandleZoneRoomTemp :3841-3852). The control loop runs z < iZoneCount over satZones[z] (:8602-8606) and feeds them to satAggregateZoneSetpoint (:3991), which raises the primary setpoint to the P75 of the counted zone outputs (area.py:294-301, climate.py:385-386 as cited in the code): with iZoneCount=1 and data in satZones[0], that single zone already counts in n and can raise the setpoint. So N = number of EXTRA rooms; nothing in the control loop adds 1 for the main room. WHY the 'living room' comment looks opposite: the simulator (:6877-6899) models satZones[0] as 'zone 1 = living room at the full target' and zones 2+ as bedrooms, and it only runs when iZoneCount > 1 (:6890); that is the simulator's invented house, it feeds the SAME extra-room structs, not the main room's temperature, and its '> 1' gate (like the diagnostics gate '> 1' at :8330 in the old tree and the SATtypes.h:614 comment 'default 1 = single-zone') is where the old 'main room only' meaning survives. Consequence: stored 1 = one extra room in control (counts only when it has data), while diagnostics and simulation treat 1 as 'no zones'. Option A (keep stored values) is behaviour-preserving for control; the '> 1' gates are what this task changes.
<!-- SECTION:NOTES:END -->
