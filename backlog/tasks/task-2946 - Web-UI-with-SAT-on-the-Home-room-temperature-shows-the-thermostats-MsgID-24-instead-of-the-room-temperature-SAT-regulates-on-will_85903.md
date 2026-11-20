---
id: TASK-2946
title: >-
  Web UI: with SAT on, the Home room temperature shows the thermostat's MsgID 24
  instead of the room temperature SAT regulates on (will_85903)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-20T08:30:00Z'
updated_date: '2026-11-20T10:14:00Z'
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
- [ ] #4 Full tests/webui sweep green; bench .61 or .63 screenshot with SAT on and an external room temperature (09)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 otgw-webui, implementation (v2.js only, no firmware, no extra API call). roomShown()/showRoom(): while sat.owns (enabled or fallback_active) and sat/status room_temp is a number and room_temp_lost is false, aRoom, the mobile chip strip, bRoom, the dial tick and the design c ROOM cell show sat/status room_temp; aRoom/bRoom then carry a tooltip 'Room temperature SAT controls on; the thermostat reports <MsgID 24>' (new key home.room_sat_tip in I18N_EN, nl, de, el). Otherwise MsgID 24 as before, no tooltip. sat.room is refreshed by the existing fetchSatStatus (30 s poll), so the SAT value can be up to 30 s old; MsgID 24 updates live. Graph room line unchanged (out of scope).

Evidence. New test tests/webui/home-room-follows-sat.test.mjs (real v2.html/v2.js, one Chrome, 5 page loads; thermostat 19.8, SAT room_temp 21.3). Red on origin/dev f18206e62 data (OTGW_DATA_DIR): 4 fail of 6 (SAT on: aRoom actual '19.8°' expected '21.3°'; takeover same; tick [22,74.6] equal on and off), the SAT-off and null/lost guards pass on both, as intended. Green on the fix: 6/6, 26 s. Full tests/webui sweep, one file at a time, timeout 180, commit charge 54 %: 91 files rc 0, pass 420 fail 0 (ha-discovery-golden prints its usual SKIP for block A, LEGACY_DUMP not set); home-dial-sat-only (TASK-1735) green. i18n audit: nl/de/el 1619 keys, 0 missing, 0 orphan. evaluate.py --quick: 88 passed, 0 failed. AC4 open: bench screenshot by 09 with SAT on and an external room temperature.
<!-- SECTION:NOTES:END -->
