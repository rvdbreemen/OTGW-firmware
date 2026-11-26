---
id: TASK-3102
title: >-
  SAT: zone_count counts extra rooms only (default 0), zone diagnostics from 1
  zone with data, zone_count state topic, zones in sat/status (firmware part of
  TASK-3019)
status: To Do
assignee: []
created_date: '2026-11-26T10:22:00Z'
updated_date: '2026-11-26T11:53:00Z'
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
- [x] #1 Robert has answered the migration question (stored 1 stays 1, or becomes 0) and the answer is recorded as a register entry naming entry 459; the plan names entries 459, 116, 378, 224, 111
- [ ] #2 settings.sat.iZoneCount default 0, accepted range 0..4 in settingStuff, REST range and settings help; comment SATtypes.h fixed; with 0 no zone runs and nothing publishes; host test on the real parse code red on old, green on fix
- [ ] #3 sat/zone/<n>/active|output|error publish from 1 zone, only for zones that have ever received data (satZones[i].iLastUpdateMs != 0); a single-room install with no zone data publishes none of them (TASK-1440 concern); host test on the real publisher red before / green after
- [ ] #4 Retained state sat/zone_count through publishIfChangedI (ADR-111) with its shadow in the republish-all reset; host test red/green
- [ ] #5 /api/v2/sat/status carries a zones array (index, room_temp, setpoint, active) for up to iZoneCount zones, so the web UI rows of TASK-3019 can read it; host test on the real renderer
- [ ] #6 Bench run by the bench-tester: push zone 1 data, read the three zone topics and zone_count back, then with no zone data check none appear; evaluate --quick green; prerelease bump
- [ ] #7 STEP 0 (before step 1): installs that never saved SATzonecount keep 1: a key sentinel g_sawSatZoneCountKey (set in the SATzonecount branch of updateSetting, reset in readSettings, same pattern as g_sawSatBlePinOnlyMarkerKey) and, after the parse, 'if (!seen) { iZoneCount = 1; settingsDirty = true; }' next to the other migrations (settingStuff.ino ~739); host test on the real readSettings/updateSetting path: absent key + existing file = 1; absent key + no file = 0; stored 1 = 1; stored 0 = 0 (red on the old default, green on the fix)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
FINAL PLAN (otgw-webui 2026-11-26; implementation owner otgw-dev after the pacer fix and the REST study, about 17:00). Entries: 459 (default 0, zone_count counts extra rooms, diagnostics from 1 zone, Q12 A: no value migration), 116 (zones follow area.py aggregation), 378 (eco: main room only), 224 (zone PID reset rule), 111 (zone PID timer): none changed, the control loop (SATcontrol.ino:8602-8606) is not touched. ORDER inside this one task (each step its own commit, red/green host test on the real code): (1) default 0 (SATtypes.h:614 comment + initialiser), clamp 0..4 in settingStuff.ino:1251, REST range and settings help text, 'zone count 0 = no extra rooms'; with 0 no zone runs and nothing publishes. (2) satPublishZoneDiagnostics and the simulator zone block publish only zones with data: gate '> 1' replaced by 'z < iZoneCount and satZones[z].iLastUpdateMs != 0' (simulator: its own synthetic data counts as data, so the sim keeps working from zone_count 1); single-room installs with no zone data publish nothing new (TASK-1440 concern). (3) retained sat/zone_count via publishIfChangedI (ADR-111), shadow handled by the epoch. (4) zones array in /api/v2/sat/status: index, room_temp, setpoint, active for z < iZoneCount, written next to the other status fields in satSendStatusJSON (SATcontrol.ino ~4921); restcap/JSON size checked against the REST cap. (5) docs: MQTT.md rows sat/zone_count and the 'published from 1 zone with data' text, manual ch05 5.15 and h05, settings help. (6) bench-tester: push zone 1 room_temp and setpoint, see the three zone topics and zone_count, see none without data. After (4) lands, otgw-webui builds the web UI zone rows (TASK-3019: temperature, target, active per zone, hidden at 0 zones, EN/NL/DE/EL, red/green webui test, screenshots) as a separate lane; (4) is the only dependency.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
PROOF of the reading 'old N does not count the main room' (otgw-webui 2026-11-26, origin/dev 9f3b8af60, SATcontrol.ino). MAIN ROOM enters on its own path: the PID output is satPidOutput(curveValue) from the primary room temperature (:8588-8589, roomValid / roomTemp from satGetRoomTemp), before any zone. ZONES are separate structs: satZones[4] (:3792); the push topic sat/zone/<n>/room_temp with n 1-based writes satZones[n-1] (satHandleZoneRoomTemp :3841-3852). The control loop runs z < iZoneCount over satZones[z] (:8602-8606) and feeds them to satAggregateZoneSetpoint (:3991), which raises the primary setpoint to the P75 of the counted zone outputs (area.py:294-301, climate.py:385-386 as cited in the code): with iZoneCount=1 and data in satZones[0], that single zone already counts in n and can raise the setpoint. So N = number of EXTRA rooms; nothing in the control loop adds 1 for the main room. WHY the 'living room' comment looks opposite: the simulator (:6877-6899) models satZones[0] as 'zone 1 = living room at the full target' and zones 2+ as bedrooms, and it only runs when iZoneCount > 1 (:6890); that is the simulator's invented house, it feeds the SAME extra-room structs, not the main room's temperature, and its '> 1' gate (like the diagnostics gate '> 1' at :8330 in the old tree and the SATtypes.h:614 comment 'default 1 = single-zone') is where the old 'main room only' meaning survives. Consequence: stored 1 = one extra room in control (counts only when it has data), while diagnostics and simulation treat 1 as 'no zones'. Option A (keep stored values) is behaviour-preserving for control; the '> 1' gates are what this task changes.

2026-11-26 Robert answered Q12 'A' by DM (message 1575478931442901093, via the coordinator): a stored zone_count 1 stays 1, no value migration; the default for NEW installs is 0 (entry 459, Open line closed by this answer; the new register entry is BlueFlame's to write before the code). Note for the implementer: an install that has never saved the setting reads the code default, so it moves from 1 to 0; installs whose settings file already holds SATzonecount (it is written on every save, settingStuff.ino:438) keep their stored 1. With the data gate below both behave the same until zone data is pushed.

2026-11-26 step 0 (coordinator's gap, otgw-webui check of the load path at origin/dev 5a5401a28, settingStuff.ino): readSettings() CAN tell old from new installs. (a) No file: readSettings :610-616 calls writeSettings() with the in-RAM defaults (:444 writes SATzonecount), then re-reads, so the key is present: a fresh install gets the new default 0. (b) Existing file without the key (written by a build before the key existed, or a 1.x upgrade): the parse never reaches the SATzonecount branch (:1319), so the value stays at the code default; with the default changed to 0 this silently moves such an install from 1 to 0 (gruijter's case if he never saved since the key existed). The load path already has the exact tool for it: key-seen sentinels reset at :599-609 and read after the parse (g_sawSatBlePinOnlyMarkerKey :739, g_sawOnboardedKey, TASK-3011/TASK-997 pattern). Fix = a sentinel for SATzonecount, and when it is absent in an existing file set iZoneCount = 1 and settingsDirty = true so the key is persisted and the migration runs once. Not covered by the sentinel, say so in the test notes: an empty or unopenable settings file takes the early returns (:621-632) with RAM defaults (0); no user data exists in such a file, so nothing is lost. Order is now: step 0, 1, 2, 3, 4, docs, bench.
<!-- SECTION:NOTES:END -->
