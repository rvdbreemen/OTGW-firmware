---
id: TASK-3016
title: >-
  SAT: MQTT command + retained state for the weather location (sat/weather_lat,
  sat/weather_lon)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-21T16:38:00Z'
updated_date: '2026-11-27T05:08:00Z'
labels:
  - sat
  - mqtt
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1133000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter, #alpha-testing msg 1573739161251356854 (2026-11-21): the weather location is REST-only, so MQTT clients cannot read or fill it; he wants Homey to fill it. Verified at origin/dev: settings.sat.fWeatherLat/Lon (SATtypes.h:566) are exposed in REST settings (restAPI.ino:4272-4273, 4530: satweatherlat/satweatherlon) and not in MQTTstuff.ino (no hit). Tester request: needs Robert's OK. Location is personal data: the retained state topic publishes the user's coordinates to the broker; weigh that (opt-in or command-only) in the design.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert decides (command only, or command + retained state)
- [ ] #2 MQTT set command for lat/lon validated to -90..90 / -180..180, persisted like the REST path; state published per the decision
- [x] #3 Host test on the real MQTT handler: valid value stored, out-of-range rejected (red before, green after)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
PLAN (otgw-webui, 2026-11-26; nothing built).
Decision: Robert 2026-11-21 16:39 UTC in #alpha-testing: 'Use this MQTT implementation.' (msg 1573739569545871583) and 'Yes, implement this request @BlueFlame Bot' (msg 1573739678606295074); the 'MQTT command + retained state' reading is the 21-11 task note; privacy goes into the docs. Coordinator 2026-11-26: otgw-webui takes it fully.
Scope: two set commands and two retained state topics, nothing else. No SAT control behaviour changes (the location only feeds the weather fetch SATweather.ino:468 and the sun elevation SATcontrol.ino:3396-3400), so no new Sergeant D entry; entries not touched. thermo-nova has no counterpart (HA gets the location from its own config), nothing to cite.
ADR check (adr_context on MQTT/SAT publish): ADR-111 governs (every sat/ publish through the on-change + jittered heartbeat helper: use publishIfChangedF, retained), ADR-196 (no behaviour claim made), ADR-106/098 naming (flat per-value topics, ADR-101). No conflict.
Code: (1) MQTTstuff.ino kSatMqttCmds: add weather_lat -> SATweatherlat and weather_lon -> SATweatherlon (same table as zone_count at :775). (2) Validation: the REST path (settingStuff.ino:1189-1190, clampSettingFloat) CLAMPS 95 to 90 and keeps the old value only for non-finite input. For a coordinate a silent clamp stores a wrong place, and AC2/AC3 say validated/rejected. So the MQTT command gets a small handler that parses the whole payload, requires finite and -90..90 / -180..180, else ignores it with one debug line, and only then calls updateSetting (persist exactly as REST). REST behaviour stays as it is (no change to an existing API); flag for the coordinator in case Robert wants REST aligned. (3) SATcontrol.ino satPublishMQTT: publishIfChangedF(sat/weather_lat|lon, settings.sat.fWeatherLat|Lon, shadow, eps 0.00005, 4 decimals, retained) next to the other retained setting echoes (:5728 area); add both shadows to the republish-all reset (tests/test_sat_mqtt_state.py checks this). 0.0000/0.0000 means not set and is published as is.
Docs: docs/api/MQTT.md SAT command table (+ state table) rows sat/weather_lat, sat/weather_lon with a privacy note: the state is RETAINED, so the coordinates sit on the broker and in every client/log that reads it; leave them 0 if that is not wanted; the REST/UI setting is the alternative. Manual ch05 weather section, same words, NL h05. Web UI: no change (the setting already has rows).
Tests (AC3), host, real code: extend the 1485-style harness that cuts kSatMqttCmds + dispatchSatMqttCmd and the publisher segment out of the sources and compiles them: valid lat/lon stored; 95/-181/abc/NaN/empty rejected and the old value kept; the state topic is retained and 4 decimals; shadows reset. RED on origin/dev (OTGW_SRC_REV=origin/dev, command unknown), GREEN on the fix; mutants: range check removed, retained flag false, shadow reset removed. Not verifiable on host: the real MQTT client and broker wire format; bench check by the bench-tester (set both topics, read retained state back, restart, value kept).
Gates: evaluate --quick, the SAT MQTT tests, test_settings_help if touched, prerelease bump via bin/bump-prerelease.sh. No build by me: I ask the coordinator with the HEAD id. Order: after the R1 dry run (11:57); commit only then.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21: released by Robert in #alpha-testing (msg 1573739678606295074): implement as requested (MQTT command + retained state for lat/lon). The privacy note above (coordinates in a retained topic) is for the implementer to mention in the docs, not a blocker.

2026-11-26 otgw-webui: AC1 already decided by Robert 2026-11-21 (#alpha-testing msg 1573739678606295074): MQTT command + retained state for lat/lon; the privacy point goes into the docs, not a blocker (note of 2026-11-21 above). No new question needed.

Observation (not a change, coordinator 2026-11-26): the REST/settings path clamps an out-of-range coordinate (satweatherlat 95 becomes 90, settingStuff.ino:1189-1190 via clampSettingFloat; only non-finite input keeps the old value). The new MQTT command rejects instead; REST stays as is. Worktree wt-ba-3016 (lane/ba-3016, origin/dev 9f3b8af60) for the code.

2026-11-26 11:3x evidence (wt-ba-3016, lane/ba-3016 on origin/dev 9f3b8af60, NOT committed yet, waits for the R1 dry run at 11:57): code = MQTTstuff.ino handlers _satSetCoordinate/_satWeatherLatCmd/_satWeatherLonCmd + two kSatMqttCmds rows; SATcontrol.ino two retained publishIfChangedF lines + shadows; SATmqttPublish.h SAT_EPS_COORD. Test tests/test_sat_weather_location_mqtt_3016.py (8 tests, compiles the real handlers, table, dispatchSatMqttCmd and publish lines): GREEN 8/8 on the fix; RED 8/8 with OTGW_SRC_REV=origin/dev (handlers/topics absent). Mutants all killed (5): range check dropped, trailing garbage accepted, lat not retained, 1 decimal instead of 4, lon range set to lat range. Other gates: test_sat_mqtt_contract_1485 15 OK, test_sat_mqtt_fallback 8 OK, test_sat_cmd_cache 31 OK, test_sat_mqtt_state exit 0, test_manual_generated + test_mqtt_doc_entity_counts OK, evaluate --quick 88 pass / 0 warn / 0 fail. Docs: docs/api/MQTT.md (state table + command rows + privacy note), manual ch05 EN and h05 NL (MANUAL.md regenerated). NOT verified: the real MQTT client and broker wire format (retained flag on the wire, restart persistence): bench-tester run with the build; bump + commit pending.

2026-11-27 otgw-poc-dev took over the lane of otgw-webui (lane/ba-3016, commit 99146fb08, never pushed) on origin/dev 25875e928 as lane-poc-3016. Conflicts only in version.h and version.hash (took origin/dev, re-bumped to beta.453 with bin/bump-prerelease.sh).

Evidence (host, real handlers, table, dispatch and publish lines compiled): tests/test_sat_weather_location_mqtt_3016.py 8/8 green on the fix, 8/8 red on origin/dev src (found 0 publishIfChangedF lines). Neighbours green: test_sat_mqtt_state 1 run OK, test_sat_mqtt_contract_1485 15/15, test_sat_mqtt_fallback 8/8. AC3 ticked on that. AC2 stays open: broker wire format, retained read-back and persistence across a restart are NOT run on a unit; the bench-tester checks them. Register: no entry touches the weather location (grep latitude, longitude, location, weather_lat in sergeant-d-corrections.md: only entries 2358 and 5280 lines on sun elevation, unrelated). evaluate.py --quick: see commit run.
<!-- SECTION:NOTES:END -->
