---
id: TASK-2946
title: >-
  Web UI: with SAT on, the Home room temperature shows the thermostat's MsgID 24
  instead of the room temperature SAT regulates on (will_85903)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-20T08:30:00Z'
updated_date: '2026-11-20T10:46:00Z'
labels:
  - webui
  - sat
  - field-report
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1076000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field report will_85903, #beta-testing 2026-11-20 06:44 (screenshots): SAT uses his MQTT room temperature, but the main overview still shows the thermostat's room temperature. Code reading (origin/dev 9e28c7795): every Home readout of the room temperature (design a aRoom v2.js:3005, design b bRoom and the dial tick v2.js:3094/3102) shows model.room, which comes only from MsgID 24 (WS frame v2.js:2397, otmonitor roomtemperature v2.js:5420). SAT regulates on satGetRoomTemp() (SATcontrol.ino:2727: simulation > BLE > multi-area > external MQTT/REST push > MsgID 24), published as sat/status room_temp (SATcontrol.ino:4819). The setpoint next to it already follows SAT while SAT controls (TASK-1342, v2.js:3007-3011), and the HA climate entity's current temperature follows satGetRoomTemp() while SAT owns the setpoint (ADR-248, SATcontrol.ino:5238). So the Home tile pairs SAT's target with the thermostat's room temperature: a gap, not a recorded design choice (TASK-1735, Robert 2026-11-08, covers SAT off only: thermostat values). Proposal: while sat.owns (enabled or fallback_active), the big room number and the dial tick show sat/status room_temp; the thermostat's MsgID 24 is shown second, as TASK-1342 does for the setpoint; SAT off unchanged. v2.js only, no firmware change, no extra API call (fetchSatStatus already reads sat/status). Out of scope: the graph room line (live and device history store MsgID 24, graphHistoryStuff.ino:66).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SAT on with an external room temperature (sat/status room_temp 21.3, MsgID 24 19.8): design a and b show 21.3 as the room temperature and 19.8 as the thermostat's; webui test red on origin/dev, green on the fix
- [x] #2 SAT off: the room readouts show MsgID 24 exactly as now (TASK-1735 test home-dial-sat-only stays green)
- [x] #3 SAT on with room_temp null or room_temp_lost: the readout falls back to MsgID 24 and shows no stale SAT value (test)
- [x] #4 Full tests/webui sweep green; bench .61 or .63 screenshot with SAT on and an external room temperature (09)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 otgw-webui, implementation (v2.js only, no firmware, no extra API call). roomShown()/showRoom(): while sat.owns (enabled or fallback_active) and sat/status room_temp is a number and room_temp_lost is false, aRoom, the mobile chip strip, bRoom, the dial tick and the design c ROOM cell show sat/status room_temp; aRoom/bRoom then carry a tooltip 'Room temperature SAT controls on; the thermostat reports <MsgID 24>' (new key home.room_sat_tip in I18N_EN, nl, de, el). Otherwise MsgID 24 as before, no tooltip. sat.room is refreshed by the existing fetchSatStatus (30 s poll), so the SAT value can be up to 30 s old; MsgID 24 updates live. Graph room line unchanged (out of scope).

Evidence. New test tests/webui/home-room-follows-sat.test.mjs (real v2.html/v2.js, one Chrome, 5 page loads; thermostat 19.8, SAT room_temp 21.3). Red on origin/dev f18206e62 data (OTGW_DATA_DIR): 4 fail of 6 (SAT on: aRoom actual '19.8°' expected '21.3°'; takeover same; tick [22,74.6] equal on and off), the SAT-off and null/lost guards pass on both, as intended. Green on the fix: 6/6, 26 s. Full tests/webui sweep, one file at a time, timeout 180, commit charge 54 %: 91 files rc 0, pass 420 fail 0 (ha-discovery-golden prints its usual SKIP for block A, LEGACY_DUMP not set); home-dial-sat-only (TASK-1735) green. i18n audit: nl/de/el 1619 keys, 0 missing, 0 orphan. evaluate.py --quick: 88 passed, 0 failed. AC4 open: bench screenshot by 09 with SAT on and an external room temperature.

2026-11-20 AC4 bench by 09 on .63, beta.402+b885f3d (Robert A, DM 1573285721786159155, satsimulation one-off), files OTGW-field-data/bench-63/2026-11-20_task2946-room/. .63 has no thermostat: no MsgID 24 at all (otmonitor has no roomtemperature). SAT on (satsimulation off): sat/status room_temp 24.84 (BLE sensor, satbleenable true; the external push could not be staged: simulation overrides it, BLE outranks it in satGetRoomTemp()) -> design a aRoom '24.9°' and design b bRoom '24.9°' (screenshots sat-on-design-a/b.png, sat-on-dom.json). The old code would show '—' here (MsgID 24 absent), so the device shows the fix working with a real SAT source. The v2.js logic reads sat/status room_temp only and does not care which source satGetRoomTemp() chose, so BLE instead of an external push exercises the same path. SAT off: aRoom/bRoom '—' (MsgID 24 absent), no tooltip. Settings restored, ini diff 0. The DOM dump shows title '' because it read the .title property, which SVG text elements do not have (undefined); a host repro of the same situation (room_temp 24.84, no MsgID 24) reads getAttribute('title') = 'Room temperature SAT controls on; the thermostat reports —' and .title = undefined. The both-values case (SAT room and MsgID 24 present, tooltip naming the thermostat value) is covered by the host test (AC1), not by the bench. Follow-up polish: no tooltip when MsgID 24 is absent ('reports —' says nothing).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2.js: while SAT owns the temperature (enabled or fallback_active) and sat/status room_temp is valid, the Home room readouts (design a, chip strip, design b number and tick, design c cell) show the room temperature SAT controls on; the thermostat's MsgID 24 goes in a tooltip (home.room_sat_tip, nl/de/el). Otherwise MsgID 24 as before. AC1: tests/webui/home-room-follows-sat.test.mjs 4/6 red on origin/dev f18206e62, 6/6 green on the fix. AC2: SAT-off case in the same test green on both; home-dial-sat-only green. AC3: null and room_temp_lost fall back to MsgID 24 (test). AC4: full tests/webui sweep 91 files, 420 pass, 0 fail; bench .63 beta.402+b885f3d by 09: SAT on shows SAT's room 24.9° where MsgID 24 is absent (old code: '—'), SAT off '—'; source on .63 was BLE, not an external push (see notes); both-values tooltip case covered by the host test only. evaluate --quick 88 passed, 0 failed. Not verified on hardware: a unit with a thermostat on the bus and SAT on (tooltip with a real MsgID 24 value).
<!-- SECTION:FINAL_SUMMARY:END -->
