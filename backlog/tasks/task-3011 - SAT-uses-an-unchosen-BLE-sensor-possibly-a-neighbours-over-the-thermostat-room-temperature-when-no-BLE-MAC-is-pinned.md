---
id: TASK-3011
title: >-
  SAT uses an unchosen BLE sensor (possibly a neighbour's) over the thermostat
  room temperature when no BLE MAC is pinned
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-21T15:38:00Z'
updated_date: '2026-11-21T19:35:00Z'
labels:
  - sat
  - ble
  - bug
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1130000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by gruijter, #alpha-testing msg 1573723870945353759 (2026-11-21). Verified in code at origin/dev: BLE scans by default on PSRAM boards (SATtypes.h:637 bBleEnable = true). With no pin (sBleMAC empty) satBLEUpdateState() drives SAT from the first fresh roster slot (SATble.ino:866-867, 'no pin set: first fresh slot in roster order'), and the auto-select block pins the only fresh sensor permanently (SATble.ino:777-797). satGetRoomTemp() gives BLE the highest priority, above the MQTT external sensor and MsgID 24 (SATcontrol.ino:2774-2782). Also: bBleFailover defaults true (SATtypes.h:639), so even a pinned sensor going stale falls over to any fresh roster sensor. Effect: a neighbour's sensor in radio range can silently become SAT's room temperature. The name filter only gates ingestion when bBleNameFilterIngest is on (default false, SATtypes.h:650). Tester suggestion: only use BLE once a sensor is chosen. Behaviour change, so Robert decides (tester request).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision recorded (Robert): BLE feeds SAT only from an explicitly chosen sensor, or another rule; including what happens to auto-select and failover
- [ ] #2 Host test on the real satBLEUpdateState()/satGetRoomTemp(): with no pin and a fresh unknown sensor, SAT keeps the thermostat/MQTT room (red on old code, green on fix)
- [ ] #3 Web UI / release notes say how to choose the BLE sensor
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Step 1 (plan only, no code). Entry 451: BLE feeds SAT only from a user-chosen sensor; a primary room-temperature source with fallbacks to be considered.

Today (origin/dev): satGetRoomTemp() SATcontrol.ino:2774-2782 order = simulation > BLE (when bBleEnable and a fresh BLE value) > multi-area > MQTT external (satexternaltemp) > MsgID 24. BLE is on by default on PSRAM boards (SATtypes.h:637). With no pin, satBLEUpdateState() drives SAT from the first fresh roster slot (SATble.ino:866-867), and any nearby advertiser is admitted to the roster (SATble.ino:~578). The auto-select block pins the only fresh sensor for good (SATble.ino:777-797, updateSetting SATblemac). With a stale pin, bBleFailover (default true, SATtypes.h:639) falls back to another fresh roster sensor, also an unchosen one.

Change (all options): BLE drives SAT only from the pinned MAC (sBleMAC). Remove the unpinned 'first fresh slot' path and the auto-select pin. No pin = BLE is display/HA only.

Open (a) default source order. A: one setting satroomsource = thermostat | external | ble | areas (primary), fallback chain fixed: primary -> thermostat (MsgID 24) -> heating curve only (room lost, entries 38/46). B: an ordered list setting (e.g. ble,areas,external,thermostat; gruijter's idea), editable over MQTT. C: keep today's fixed order, only BLE needs a pin. Recommendation A: one choice the user understands, one fallback to the device the user owns, no list parsing/validation or UI for ordering (B is YAGNI until someone needs two fallbacks); default derived per install at migration (see c), fresh install = thermostat.

Open (b) failover default. Two levels. Source level (primary stale -> thermostat): recommend ON by default (thermostat is the user's own device; off means SAT runs on the curve only). BLE sensor-to-sensor failover (bBleFailover): recommend default OFF and only to sensors the user also chose; failing over to 'another fresh roster sensor' is exactly the neighbour problem of entry 451. Consequence of OFF: a pinned BLE sensor whose battery dies -> SAT uses the thermostat until it is back.

Open (c) migration. An auto-pinned MAC (SATble.ino:777-797) and a user-picked MAC are stored identically in sBleMAC: the firmware cannot tell them apart. A: keep every existing pin as a user choice (satroomsource=ble when sBleMAC set), BLE without a pin stops feeding SAT. B: clear every pin on upgrade, user re-picks. C: A plus a one-time notice (narration + web UI banner on the SAT page: 'SAT uses BLE sensor <name/MAC>, confirm or change'). Recommendation C: no silent change for the many single-sensor users whose pin is right, and a visible prompt for the few with a wrong (neighbour) pin; B silently drops users back to the thermostat. Persisted bBleFailover=true on existing installs: reset to the new default only if Robert picks OFF in (b) (one-time migration flag).

TASK-3015 (sat/room_temp_source): recommend the same change. satGetRoomTemp() then decides the source in one place and returning it with the value is a few lines; publishing it is what makes the failover of (b) visible to MQTT/HA. It is labelled needs-maintainer-ok: Robert must release it.

Scope after the answers: SATcontrol.ino (satGetRoomTemp + source enum), SATble.ino (remove unpinned path + auto-select), settings (satroomsource + migration), web UI SAT page (setting + banner, otgw-webui), MQTT/HA (setting command + state, 3015), tests red/green on the real functions, ADR (new setting + behaviour change of the room source).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21: released by Robert in #alpha-testing (msg 1573738563596591167), recorded as entry 451 in the private SAT decision register: BLE feeds SAT only from a user-chosen sensor; a primary-source setting with multiple fallbacks is to be considered. Open per entry 451: default source order, failover default, migration of existing installs.

2026-11-21 tester input (gruijter, msg 1573738963787714691, not a decision): make the source order MQTT-accessible, a command plus a retained state (e.g. sat/room_temp_sources = areas,external,thermostat), and the active source as its own topic (sat/room_temp_source, see TASK-3015). Design input for the open part of entry 451.

2026-11-21: Robert chose gruijter's MQTT design (msg 1573739569545871583), recorded as entry 452 in the private register: source order via MQTT command + retained state, active source on its own topic (TASK-3015). Final topic names per ADR-101/111 in the plan.

2026-11-21 (coordinator): Robert approved gruijter's MQTT design for the source order (private entry 452) and released TASK-3015 (same change as 3011) and TASK-3016 (separate, later). Coordinator's reading: Q1 = B (ordered list, MQTT command + retained state sat/room_temp_sources, active source sat/room_temp_source). That reading is an interpretation, not a quoted choice; the implementation plan follows it unless Robert corrects it. Design consequences of B to settle in the ADR: allowed tokens (ble, areas, external, thermostat), validation (unknown or duplicate token -> reject and keep the old list), thermostat always implied last if left out (so SAT never runs on the curve only while MsgID 24 is valid), web UI field for the list, and the same list in settings.ini/REST. Q2 (failover) and Q3 (migration) still open; no code until answered.

2026-11-21 (coordinator, correction): Robert answered 'A' to the Q1 DM (one primary-source setting + fixed fallback primary -> thermostat -> curve only). That conflicts with the coordinator's earlier B reading of entry 452 (previous note); the coordinator asked Robert to settle it, recommending A with an MQTT command that sets that one value (plus sat/room_temp_source from TASK-3015). Q1 is therefore open again until Robert confirms; the B design consequences in the previous note apply only if he picks B. No code.

2026-11-21 (coordinator, Robert DM): Q2 = A. A stale primary source falls back to the next source (ON by default). BLE sensor-to-sensor failover OFF by default, and when used only to sensors the user chose (never to an unchosen roster sensor). Implies: bBleFailover default false plus a one-time reset of persisted true (details with Q3). Still open: Q1 list vs single setting (clarifying DM), Q3 migration. No code.

2026-11-21 (coordinator, Robert): Q1 settled = A. One primary room-source setting with the fixed fallback (primary -> next source -> thermostat -> curve only, per Q2 A). No ordered list: gruijter's MQTT command + retained state is only the interface that sets/reports that one value (for Homey), plus sat/room_temp_source (TASK-3015) for the active source. Q2 = A. Q3 (migration, recommendation C) with Robert. Code starts after Q3: 3011 + 3015 in one change, ADR via adr-kit (Proposed).

2026-11-21 (coordinator, Robert DM 1573741732494053547): Q3 = C. Every stored BLE pin stays as the user's choice (primary = BLE when a pin exists); unpinned BLE stops feeding SAT; a one-time notice on the SAT page and in the log: SAT uses BLE sensor <name>, confirm or change; a stored bBleFailover=true is reset once to off. All three answered (Q1 A, Q2 A, Q3 C): code GO for 3011 + 3015 in one change, ADR via adr-kit (Proposed).

2026-11-21 (otgw-dev) part 1 implemented (independent of the open primary-source question): SATble.ino satBLEUpdateState(): no pin -> BLE does not drive SAT; stale pin + bBleFailover -> only a fresh sensor the user assigned to an area (sSensorArea), never an unchosen roster sensor; TASK-508 auto-select pin removed. SATtypes.h: bBleFailover default false, new internal bBlePinNotice. settingStuff.ino: one-time migration [sat-blepinonly-migrate] behind the SATblepinonly marker (failover off, a stored pin kept + bBlePinNotice), the notice cleared by a SATblemac write outside settings loading (g_loadingSettingsFile). SAT log once per boot 'SAT uses BLE sensor <MAC>: confirm or change it on the SAT page'; /api/v2/sat/status BLE block field ble_pin_notice (the web UI banner is otgw-webui's part). Evidence: tests/test_sat_ble_pin_only.py 13 OK on the fix; on origin/dev sources 10 FAIL (no pin drives SAT, failover to an unchosen sensor, migration, source checks), 3 stay green (fresh pin, failover off, marker present). Related tests 25 files PASS (settings, progmem gz, humidity source, pwm margin migration, BLE nopsram/discovery, SAT MQTT/REST contracts, status). evaluate --quick 0 FAIL. Still open: the primary-source setting (Robert's A/B/C on the auto default), ADR, build, bench.

2026-11-21 (otgw-dev) part 2, primary room source (Robert follow-up A, then (a)/(b)): SATroomsource (SATtypes.h iRoomSource, SAT_ROOMSRC_*: 0 auto, 1 thermostat, 2 external, 3 ble, 4 areas; default 0 = auto = pinned BLE > areas > external > thermostat). satGetRoomTemp(): the primary goes first (a primary thermostat with a fresh MsgID 24 skips the others), then satTryRoomSource() in the auto order skipping the primary, then the thermostat tail, then NaN = heating curve only. Helpers satThermostatRoomTemp() (TASK-1157 staleness) and satTryRoomSource() (BLE, areas, external with their existing staleness rules). Interface: settings.ini SATroomsource, REST satroomsource (addInt 0..4, whitelist) + POST sat setting room_source, MQTT set/<node>/sat/room_source (name or number, satRoomSourceParse; anything else keeps the value), retained state sat/room_source (name), /api/v2/sat/status room_source. Help text + settingsHelpGz.h. Found during the change: satRoomTempSampleMs() (REST room_temp_age_s) mirrored the fixed source order; it now switches on the recorded source (_sat_roomSrc), so the age follows any primary. Migration = auto (no derivation); notice + one-time failover reset unchanged (Robert (a)). Evidence: tests/test_sat_ble_pin_only.py 20 OK, the 7 PrimarySource cases compile the real satGetRoomTemp() + helpers: on the part-1 code (aa89ee135) p2 thermostat-first, p4 external-first, p6 areas-first FAIL, auto order and fallbacks pass on both. tests/test_sat_status_live.py adapted (scenarios set the recorded source; the order-mirror check became a check on the source switch), test_sat_humidity_source harness got the new constants/field and a satRoomTempValid stub. 39 related test files PASS (settings, progmem gz, contracts, discovery, BLE, status, MQTT incl. test_mqtt_doc_entity_counts after MQTT.md 405 -> 406). evaluate --quick 0 FAIL. Open: ADR (number from the coordinator), build, bench, web UI (otgw-webui).

2026-11-21 (otgw-dev): ADR-293 (Proposed, number from the coordinator) written via adr-kit for TASK-3011 + TASK-3015: decision, contract, verification (tests/test_sat_ble_pin_only.py, test_sat_status_live, test_sat_energy_discovery), alternatives (ordered list: entry 454; default thermostat: entry 457; clear pins: entry 455), entries cited by number only. adr lint: only ADVISORY OPEN_QUESTIONS_UNRESOLVED (web UI part, otgw-webui); test_adr_governance 17 OK after the README bullet. Not accepted: Robert's acceptance is his.

2026-11-21 otgw-webui, UI part (coordinator GO; contract from otgw-dev's lane/d3-blesrc 399624013): lane/ba-3011 (wt-ba-2959), not pushed until otgw-dev's firmware lands. (1) SAT settings, group Indoor temperature: select 'Room temperature source' for satroomsource (0 Auto, 1 Thermostat, 2 MQTT/REST sensor, 3 BLE sensor, 4 Averaged room sensors); the help text comes from otgw-dev's settingsHelp.h entry. (2) SAT page room tile: 'Source: <name>' from room_temp_source (ble, areas, external, thermostat, simulation, estimate, none). (3) One-time banner while ble_pin_notice is true: 'SAT uses BLE sensor <name (MAC) or MAC> for the room temperature. Confirm it, or choose another sensor.' Confirm POSTs the same MAC to /api/v2/sat/settings/ble_mac (any ble_mac write clears the notice in the firmware), Change opens Settings > Sensors. EN/NL/DE/EL. Test tests/webui/sat-ble-pin-room-source.test.mjs: RED on origin/dev 0/4, GREEN 4/4; i18n-lang 6/0, lang-menu 7/0, i18n-no-hardcoded 3/0, sat-display-states 5/0, sat-area-sensor-mapping 1/0; evaluate --quick 0 failed. Not verified: against the real firmware (needs otgw-dev's build); the roster name only shows when the BLE roster was loaded (Settings > Sensors), otherwise the MAC.

2026-11-21 combined bench (otgw-bench-tester, beta.416+4874e79 on .61, no SAT sim): PASS githash/no restart/crashlog none; migration (SATblefailover false, SATblepinonly, SATroomsource 0, pin kept, ble_pin_notice true, BLE block present with BLE off; Confirm = re-POST ble_mac clears it, still false after a reboot); no pin -> room_temp_source never 'ble'; REST and MQTT room_source set by name, room_source=foo keeps the value; HA discovery SAT_Room_Temp_Source diagnostic; UI select (5 options), room tile Source, banner Confirm/Change light+dark (screenshots OTGW-field-data/bench-otgw32/2026-11-21_task3011-3015-4874e79/ui3011-61/). The once-per-boot SAT log line not shown with BLE off (expected). Open: sat/room_source and sat/room_temp_source not seen because satPublishMQTT() did not run (SAT enabled but not active, outside temperature missing: telnet shows only core-state topics, no satPublishMQTT topic at all); re-check with SAT running ordered from 09. Decisions (coordinator): settings-topic publishing stays as is; the primary only orders enabled sources (choosing external does not enable satexternaltemp), otgw-webui adds a hint when the chosen primary is not enabled (Robert may override). REST echo of a rejected value is generic (restAPI.ino handleSAT reply) -> TASK-3023 (low).

2026-11-21 otgw-webui add-on (coordinator): when the chosen 'Room temperature source' is not switched on, a one-line hint under the select reads 'This source is not enabled; SAT uses the next source in the auto order.' (external = satexternaltemp off; BLE = BLE off or no pinned MAC; averaged room sensors = satmultiarea off; auto/thermostat never). The SAT settings re-render when satroomsource, satexternaltemp or satmultiarea change. EN/NL/DE/EL. New case in sat-ble-pin-room-source.test.mjs (7 setting combinations): red on 4874e795b (no hint for external off), green; file 5/0 twice (the Confirm case now waits for the POST, it raced once). sat-areas-zones-groups 2/0, i18n tests green, evaluate --quick 0 failed. Commit on top of otgw-dev's combined 4874e795b for his cherry-pick.

2026-11-21 (otgw-dev): re-check by 09 with SAT running PASS. Gap fixed: sat/room_source moved from satPublishMQTT() (runs only inside SAT's control tick, so after a boot it was absent during the 60 s boot silence and whenever SAT was not controlling) to the always-on core set satPublishCoreState() (every loop pass, enabled or not), retained, first-seen on the first pass after a boot, its shadow reset by satRepublishCoreState() on reconnect / HA online. sat/room_temp_source stays in satPublishMQTT() (a measurement: first SAT tick, on change, heartbeat). Test: test_sat_ble_pin_only test_room_source_state_is_in_the_core_set, red on the previous code, 21 OK on the fix; 11 MQTT/REST/discovery files PASS.
<!-- SECTION:NOTES:END -->
