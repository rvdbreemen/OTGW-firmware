---
id: TASK-1885
title: >-
  SAT REST: add actual flow temperature, flame state and sensor ages to
  /api/v2/sat/status, and plot real flow in the SAT chart
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T06:23:00Z'
updated_date: '2026-11-13T13:16:00Z'
labels:
  - webui
  - sat
  - rest
dependencies:
  - TASK-1879
priority: medium
ordinal: 958000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
UI proposals U7 and U8 from the VTherm UI review (docs/sat/vtherm-comparison-2026-11-13.md, section Web UI). Today the SAT loop chart labels heating_curve as Flow (v2.js:7170) and sat/status carries no actual boiler flow temperature or flame state outside simulation (sim_flow_temp, sim_flame_on only). Add flow_temp, return_temp (when valid), flame_on, current_modulation already exists, and room/outside temperature age in seconds to satSendStatusJSON (SATcontrol.ino:4266-4548, JsonEmit, ADR-146). UI: setpoint vs actual flow in one panel, heating curve kept separately labelled, modulation % as a filled area on a right axis with a flame band; sensor ages shown as plain minutes (U8). Fields are additive (API contract: additive only). Firmware commit needs its prerelease bump.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test of the status JSON contains flow_temp, flame_on and the two age fields with correct values from a fixture state; absent on the old code
- [ ] #2 Chart shows actual flow next to the setpoint and labels the heating curve as such (webui test on a fixture payload)
- [ ] #3 esp32-combo build green; evaluate --quick no new FAIL; bench run by otgw-bench-tester shows flow_temp/flame_on matching the OT data on a PIC and an OT-Direct unit
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Lane wt-ba-1885 (lane/ba-1885 off origin/dev d34723882), separate from TASK-1871; commit and prerelease bump only after 1871 is pushed and posted (build order 220 d3, 1871, 1885, 1880). Firmware: marker block 'sat-status-live record' in SATcontrol.ino before SatStatusSnap: SatStatusLive struct, satSampleAgeS(), satStatusFlowTemp()/satStatusReturnTemp() (no satSimNoiseOffset, which advances the sim LCG), satRoomTempSampleMs() (read-only mirror of satGetRoomTemp source order), satEmitStatusLive(). Snapshot fills snap->live right after satGetRoomTemp(); satGetOutsideTemp() replaced by satResolveOutsideTemp(&v,&ms) (same call, gives the sample time); closure emits after current_modulation. Web UI: satHist gets flow (flow_temp), curve, mod, flame; setpoint panel = setpoint + actual flow + dashed heating curve; modulation 'm' panel and flame 'l' lane as Mission control; tile age lines (U8). Docs: openapi + README. Tests: host test tests/test_sat_status_live.py, webui tests/webui/sat-chart-actual-flow.test.mjs, both red on origin/dev.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Decisions: (1) age sentinel -1 = no age known, *_age_s names: the convention the coordinator set for TASK-1871 last_seen_age_s, no new question to Robert. (2) Room age per source: sim 0; BLE iBleTempLastMs; multi-area = oldest contributing area (valid, weight > 0); external iExternalTempLastMs; OT Tr iTrThermostatMs; -1 for no reading, the thermal fallback estimate, and a Tr without a thermostat stamp (TR= feed). (3) flow_temp/return_temp null when SAT has none (the 0.0 missing sentinel of satGetFlowTemp/satGetReturnTemp mapped to NaN, as TASK-886 M1). (4) Deviation from the description: modulation and flame are their own stacked panels (drawPanelChart kind m and l, the TASK-1841 Monitor > Graph style Robert chose), not a right axis on the flow panel; drawPanelChart has no right axis. (5) A separate read-only mirror for the room age instead of refactoring satGetRoomTemp(): the control path stays byte-identical; a wiring test pins that both keep the same source order. (6) No MQTT topics added (out of scope). (7) Open source: trivial project-specific glue, no library search needed.

Evidence so far (2026-11-13, uncommitted in wt-ba-1885): python tests/test_sat_status_live.py 11/11 OK on the fix; OTGW_SAT_INO=<origin/dev SATcontrol.ino> red: setUpClass error (markers not found) + 2 wiring FAIL. node --test tests/webui/sat-chart-actual-flow.test.mjs pass 4 fail 0; OTGW_DATA_DIR=<origin/dev data> pass 0 fail 4. Slicing tests rerun on the fix, all OK: test_sat_humidity_source, test_sat_outside_order, test_sat_restart_quiet, test_sat_mqtt_contract_1485, test_sat_pid_cadence, test_sat_solar_gain_no_sun, test_sat_rest_contract_1496. Web UI one file per run, all pass: panel-chart-style 2, sat-page-order 1, v2-nested-helper-scope 2, i18n-lang 6, i18n-no-hardcoded 3, i18n-followups 4, no-sample-values 2, sat-beta-notice 1. evaluate.py --quick: 85 PASS, 0 WARN, 0 FAIL. openapi.yaml parses (yaml.safe_load). Not yet: full tests folder, bump, build, bench (AC3).

2026-11-13 rebased onto ed751972a (via f1b2731fd, 359f2dec5). Conflict with TASK-1884 room trend in v2.html (satRoom tile) and v2.js (satRenderRoomTrend call) resolved by keeping both; the webui test's markup regex follows the new tile (satRoomTrend span, then the age line). Full tests folder 303 files (tests/, test/host/, tests/webui one file per run, commit <= 82 %): all green. After the final rebase: test_sat_status_live 11/11, sat-chart-actual-flow 4/4; evaluate.py --quick 85 PASS 0 WARN 0 FAIL. Bump beta.222 -> beta.223.

2026-11-13 first esp32-combo build failed: OTGW-firmware.ino:888:51 error: 'SatStatusLive' does not name a type (the Arduino builder hoists a prototype of satEmitStatusLive(JsonEmit&, const SatStatusLive&) above the struct). Fix: the emitter takes the five values as plain parameters; test_sat_status_live follows (11/11). The host test could not catch it: it compiles the block on its own, without the hoisted prototypes. Also found: in a fresh worktree build.bat ends silently at :find_python because C:\tools\Codex\python.cmd (a .cmd shim, first python on PATH) is called without 'call'; this build ran with C:\tools\Codex left out of PATH for that process only. Reported to the coordinator, not fixed here.

2026-11-13 new gate tests/test_ino_prototype_hoisting.py (coordinator request a): merges the .ino files in Arduino builder order and flags a function whose parameters name a struct/class/enum an .ino declares after the first function definition, unless the sketch declares that function itself (the FlameRatioBuf pattern, OTDirect.ino:2303). Evidence: green on 33e3a0728; red on 4391eaf19 ('satEmitStatusLive(JsonEmit& je, const SatStatusLive& l) uses SatStatusLive'); red on a mutant without the flameRatioBufCommit prototype. The first version missed the old code because a '/*' inside a string literal opened a fake comment; the comment stripper is string-aware now. Build 4 of 33e3a0728: SUCCESS, beta.223+33e3a07 in the zip binaries.
<!-- SECTION:NOTES:END -->
