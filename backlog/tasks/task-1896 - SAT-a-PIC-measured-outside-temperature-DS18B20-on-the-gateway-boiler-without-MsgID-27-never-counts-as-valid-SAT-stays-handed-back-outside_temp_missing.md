---
id: TASK-1896
title: >-
  SAT: a PIC-measured outside temperature (DS18B20 on the gateway, boiler
  without MsgID 27) never counts as valid, SAT stays handed back
  (outside_temp_missing)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T09:55:00Z'
updated_date: '2026-11-13T18:27:00Z'
labels:
  - bug
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 965000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Report: indigo_light (Discord 301077214374658048), #nederlandse-ondersteuning https://discord.com/channels/812969634638725140/815561033036333076/1570736532719935529 (2026-11-13 09:46 UTC, addressed to Robert), beta.218, Classic + S3 Mini, PIC 6.8, SAT on: SAT stays inactive with outside_temp_missing:true and external_outdoor_valid:false while the SAT page shows 5.6 C; outside temperature from a DS18B20 on the PIC, the boiler sends no MsgID 27; meanwhile the thermostat's CH demand passes unfiltered (that is the designed hand-back of entries 56/57: CS=0, CH=1, MM=T). Code (origin/dev d79210e23, unchanged in beta.219-221): satResolveOutsideTemp() SATcontrol.ino:2550-2569 accepts only external (MQTT/REST), boiler MsgID 27, weather, or the restored value; the MsgID 27 freshness stamp state.otBus.iToutsideBoilerMs is set only on a live boiler Read-Ack (rsptype OTGW_BOILER, type READ_ACK, not bGatewaySubstituted, not replayed) at OTGW-Core.ino:6097-6102. A gateway-made answer to the thermostat (OTGW_ANSWER_THERMOSTAT 'A' frame) or a substituted B frame never stamps it, and the PS=1 summary never stamps (ADR-181). The SAT page value comes from OTcurrentSystemState.Toutside, which those frames do set, hence 5.6 C on screen but outside_temp_missing. Inference, not verified: the PIC reports its own sensor as an A frame for MsgID 27 (or a substituted B frame); a capture is needed to see the actual frame type. Behaviour question: entry 57 lists the sources as external, weather and boiler MsgID 27; whether a gateway-measured value counts is a SAT decision (Sergeant D) and the fix scope Robert's. Workaround until then: the weather service (Settings > SAT > Weather) or an external outside value over MQTT/REST.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Capture from the reporter (or a bench reproduction) shows how the PIC reports the DS18B20 outside value (frame type for MsgID 27)
- [x] #2 Sergeant D decides whether a gateway-measured outside value counts as a valid source (recorded as an entry)
- [ ] #3 If yes: host test red on the old stamp rule, green on the fix; outside_temp_missing clears with only the PIC sensor present
- [ ] #4 Bench or field confirmation
- [ ] #5 Bench (Classic + PIC): with a PIC DS18B20 (or OT=) value and a boiler without MsgID 27, SAT reports the outside temperature as valid (osrc=O), outside_temp_missing is false, and no weather fetch is started
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 09:56 UTC follow-up from indigo_light (https://discord.com/channels/812969634638725140/815561033036333076/1570739066360561805): 'de weersafhankelijke regeling krijgt ook geen data door als ik mijn coordinaten ingeef en slaat dus niet aan' - the weather workaround also gets no data after entering coordinates. Not analysed: needs the capture (weather fetch log lines, sat/status weather fields). Possibly a separate weather-fetch problem.

2026-11-13 weather trace (comms, read-only, origin/dev 01e465054). Provider: Open-Meteo over plain HTTP, http://api.open-meteo.com/v1/forecast?latitude=..&longitude=.. (SATweather.ino:95/101), fetched on the webhook worker task (weatherRequestFetch :475-500), connect 2 s / read 5 s (:381-382). Preconditions in weatherLoop() (:605-640): settings.sat.bWeatherEnable (:608), 5 min after boot (WEATHER_POLL_MIN_SEC 300, :28, :611), coordinates not both 0 (:477-480, :630), heap not CRITICAL (:483-489); SAT on is NOT required. Interval: settings.sat.iWeatherInterval (default 900 s), first-fetch retry 60 s doubling to the interval (:557-565). LIKELY CAUSE of the follow-up: SATweather.ino:617  (TASK-511, f12a753eb) skips every weather fetch while the OT state holds a non-zero outside temperature. The PIC's own DS18B20 value fills OTcurrentSystemState.Toutside (the 5.6 C on his SAT page) but is rejected by satResolveOutsideTemp() (only a live boiler Read-Ack stamps it, OTGW-Core.ino:6098-6102). So the weather fallback never starts and SAT has no valid source at all: the two guards use different definitions of 'the boiler provides an outside temperature'. Failure reporting: telnet lines 'Weather: skipping fetch, no coordinates configured', 'Weather: no connection to the weather server', 'Weather: HTTP <code>', 'Weather: heap critical, fetch not started'; REST GET /api/v2/sat/weather fields enabled, valid, latitude, longitude, fetch_errors (:659-682). With the :617 skip there is NO log line and fetch_errors stays 0, valid false. No existing task for this skip (TASK-511 added it, archived). Fix direction (not decided): make :617 use the same validity as satResolveOutsideTemp (iToutsideBoilerMs fresh), not the raw state value.

Correction to the note above (a shell quoting error dropped the code): the skip at SATweather.ino:617 is: if (OTcurrentSystemState.Toutside != 0.0f) return;

2026-11-13 split (coordinator): the weather-fetch skip is TASK-1897 (fix without a Sergeant D decision). This task keeps only the question whether a gateway-measured outside value (DS18B20 on the PIC) counts as a SAT source: Sergeant D, after the capture.

MAINTAINER DECISION (Robert, Claude Code session of the comms agent, 2026-11-13 ~10:50 UTC), verbatim: "Ehhh, waarom telt de waarde die de Pic maakt zelf niet, dat kan toch ook met een sensor gelezen worden? Dan kan het toch heel goed zijn dat de buitentemperatuur niet van de OTbug komt, maar van de gateway zelf. Je kan wel OTcurrentSystemState.Toutside zien. Los dit op." Recorded as cross-reference entry 330 in docs/sat/sergeant-d-corrections.md (maintainer decision, not by Sergeant D, deviates from entries 57/139). Sergeant D notified: https://discord.com/channels/812969634638725140/1105556725714649128/1570753075830857779

PIC check (6.8 source, OTGW-other-projects/otgw-6.8/gateway.asm, same as 6.6 :3050-3071): StoreTempValue :4269-4279 stores a valid DS18B20 reading in outside1/2 (same storage as OT=). MessageID27: towards the boiler the thermostat Read-Data 27 becomes Write-Data 27 with the value (R frame, gateway-substituted); towards the thermostat a Read-Ack with the value (A frame). SensorInvalid -> Data-Invalid / Invalid-Data, so a failed reading does not freeze the value. Hence the boiler never answers Read-Ack while the PIC has a value (Write-Ack or Unknown-DataId), and the stamp OTGW-Core.ino:6098-6102 never fires. Fix: also stamp on the A-frame Read-Ack 27 and the R-frame Write-Data 27; never on a replay or a PS=1 summary (ADR-181). The helper satOtOutsideFresh() of TASK-1897 (lane 95) then picks it up. Not verified: an unplugged sensor (the presence-pulse path in ds1820.asm not fully traced), the path where the thermostat never asks MsgID 27 (PIC alternative table :1825), and the frame types on the reporter unit (capture needed).

Basis of the frame claim: read in the PIC 6.8 source itself (Schelte Bron, OTGW-other-projects/otgw-6.8/gateway.asm, MessageID27 and StoreTempValue; same code in 6.6 :3050-3071), which is level 2 in the reference precedence (other-projects/CLAUDE.md). Not checked against Schelte's docs on otgw.tclcode.com, and NOT verified on a real unit: no capture of a PIC with a DS18B20 exists yet. To be confirmed with indigo_light's capture (expected: T Read-Data 27 marked substituted, R Write-Data 27 with the sensor value, B Unknown-DataId, A Read-Ack with the value). If the capture shows other frames, the predicate must follow the capture.

RISK for Robert (from his decision): an OT= value has no age of its own. The PIC keeps substituting it, and OT-Direct re-writes it every 15 s (OTDirect.ino OT_WRITE_INTERVAL_MS, W_ENTRY 27), so a stale OT= value left by an automation keeps the stamp fresh until OT=- clears it. On the bus an OT= value and a DS18B20 value look the same (same PIC storage outside1/2), so the firmware cannot tell them apart.

Bench AC note: the bench check runs on the Classic + PIC with a DS18B20 or OT=; its frame sequence is also the first real confirmation of the PIC-source reading above.

2026-11-13 11:07 UTC: Sergeant D agrees with entry 330 ("hahahaha sorry. My answer is A", 1570756945470824540, after the ambiguous "of course yes" 1570753538420903997), recorded as entry 331. The coordinator replied to indigo_light in #nederlandse-ondersteuning; the comms follow-up on running HA SAT as a fallback (turn the firmware SAT off): 1570756471120076903.

AC2 evidence: Robert decided (entry 330), Sergeant D agrees (entry 331, 1570756945470824540). AC3 host-test part done in 1f4d62091: test/host/test_replay_frame_guards.py --rev origin/dev FAIL 'gateway Write-Data to the boiler stamps' (got 111 want 5000), PASS 9/9 on the fix; tests/test_outside_stamp_gateway_frames.py red 1F/8P on the behaviour-neutral extraction, green 9/9; full suite 229 files 228 passed 0 failed 1 skipped; evaluate --quick 0 FAIL. AC3 'outside_temp_missing clears with only the PIC sensor present' is NOT ticked: needs the build on a unit (AC5 bench). AC1 (frame sequence from a capture) open: the reading is from the PIC 6.8 source only. The commit goes into lane 95's combined build (cherry-pick); lane-comms stays local until 95 pushes.

Firmware fix commit rebased onto ed751972a (docs pushed separately); the code-only commit on lane-comms goes into lane 95's combined build.

Follow-up 1903 item 2 (2026-11-13, wt-95w lane-95-219b, coordinator decision after 92's check): otFrameIsFreshOutsideTemp() also returns false for bLocalAnswer, so an OT-Direct loopback B Read-Ack 27 (OTDirect.ino:973 marks B as local only in loopback; R frames are never local, so OT= on OT-Direct still stamps) no longer stamps iToutsideBoilerMs. SAT sim is unaffected (satResolveOutsideTemp returns the sim outdoor first, osrc S). Pre-existing gap, the old inline block had the same conditions. Tests: tests/test_outside_stamp_gateway_frames.py test_local_answer_boiler_read_ack_does_not_stamp and test/host/test_replay_frame_guards.cpp 'B Read-Ack 27 with bLocalAnswer=1 does not stamp'. Red on the predicate without the condition: py FAILED (failures=1), cpp 'got 5000 want 111'; green with it: py 10 OK, cpp PASS. --rev origin/dev fails on both the bLocalAnswer and the Write-Data case. Also green: sat_restart_quiet 20, boiler_unsupported_origin, ino_prototype_hoisting 2, boiler_support_bitmap_b_only 11, weather_worker_handoff 11.

Bench limit (2026-11-13, 09 via the coordinator, build 2.0.0-beta.226+e86aed9): not demonstrable on the bench unit .61. OT-Direct loopback never asks MsgID 27, and the bench has neither a thermostat nor a boiler that would make the PIC send a Write-Data 27. Evidence stays host tests (tests/test_outside_stamp_gateway_frames.py, test/host/test_replay_frame_guards.py with the --rev origin/dev red run, including the bLocalAnswer follow-up) plus field confirmation by indigo_light. Not shown on a device.
<!-- SECTION:NOTES:END -->
