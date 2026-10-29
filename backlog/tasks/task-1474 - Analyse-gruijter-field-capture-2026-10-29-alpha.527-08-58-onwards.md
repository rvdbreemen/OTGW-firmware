---
id: TASK-1474
title: Analyse gruijter field capture 2026-10-29 (alpha.527, 08:58 onwards)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T12:49:00Z'
updated_date: '2026-10-29T14:41:00Z'
labels:
  - sat
  - field
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 634000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter #alpha-testing msg 1565344493475995819: capture transcript (53 MB), graph CSV + screenshot, alpha.527. Analyse SAT start-up, PWM/continuous phases, setpoints and commands sent, against thermo-nova (pinned 7beaaf5) and Sergeant D's entries; file a task per real finding; report in #alpha-testing. Files in %TEMP%/discord-attach/gruijter-20261029.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Timeline of SAT states and CS/MM/CH commands over the capture, with transcript line refs
- [x] #2 Each deviation from thermo-nova or Sergeant D's entries cited (file:line / entry no.) and checked by a second reader; already-fixed ones mapped to alpha.528-530
- [x] #3 Findings posted in #alpha-testing; new tasks filed for open ones
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29 ~13:10Z: analysis taken over by the 1459/1460 session (wt session, Claude) at the coordinator's request; coordinator records Sergeant D's verdict msgs 1565355195939033089/1565357952821624863 itself.

AC#1 timeline (transcript-20261029-085801-2.0.0-alpha.527+fd491ff ... A0F262F586F8.txt; Lnnn = physical line; extractor scripts in the session job dir, telnet lines are parsed per 'hh:mm:ss.us (heap|blk) fn(line):' entry because entries run together on some lines).
- 08:58:03 start. Unit up 54 min (boot ~08:04 after the 08:02/08:03 software-reset pair, the OTA pattern), banner L113-140: SAT active, continuous, PIC 6.8 gateway, maxLoopGap 5202 ms already.
- 08:58:29 target 15.0, room 18.8: CS=10.0 (L785), MM=100 (L1134), CH=0 (L1137).
- 08:59:33 target 19.5 (L1542). 09:00:03 continuous: CS=37.2 + CH=1 (L1885/L1888). Flame 09:00:13 (L2006). CS climbs 37.2 -> 43.9 (L1885-L15103).
- 09:11:33 switch to PWM, duty 0.70, MM=0 (L9127-L9150); flame stays on. 09:20:33 PWM OFF: CS=10 + CH=0 (L15246/L15249). Flame off 09:20:41, cycle #1 1228 s, class GOOD, overshoot 738 s (L15372-L15376).
- PWM ON starts (CS = last Tret + 18, then flow - 1 from 20 s after flame-on, CS=10 + CH=0 at PWM OFF): 09:37:08 (L25129), 10:01:08 (L39856), 10:22:08 (L52857), 10:51:08 (L69929), 11:13:08 (L83706), 12:04:08 (L114726), 12:34:08 (L134467, CS clamped at 48.0), 13:29:29 (L169946). Flame-on times 452/297/242/302/205/462+300/247/301 s, all GOOD except the DHW-interrupted one.
- 11:21:08 PWM IDLE -> continuous (L88814/L88822), CS=10. 11:50:08 PID 28.2 -> CS=28.2 + CH=1 (L105180); flame 11:50:28 (L105451) at CS 28 / flow 28; CS 30.7, 31.0, 32.0 -> PWM ON 11:52:08 (L106549-L106572).
- 12:03:08 target 20.0 (L114065). 12:11:38 PWM ON paused for hot water (L120250), resumed 12:13:08 (L121114).
- Flames at CS=10 (DHW draws, not SAT): 10:13:13 12 s (L48006), 12:20:44 11 s (L126212), 12:50:23-12:55:28 four flames, peak 61-65 C, modulation_state HOT_WATER (L144993-L148232, L145163).
- 12:55:23 MQTT command SW=40 from the broker (L148136-L148138).
- 12:56:17.287 last telnet line before the gap (L148780). Reboot: banner 'Boot : Task watchdog', Reboots 41 (L148787-L148788); reboot_log 12:56:52 Task watchdog (crashlog section ~L410530). 
- 12:56:54 boot commands CS=0, PM=3 (L149119/L149126); sat/active false->true (L149174/L149895). 12:57:24 CS=34.1 + CH=1 + MM=100 (L149858-L149864) with outside=0.0 in every tick until 13:01:24 (L152195-L152840); first weather fetch 13:01:48-13:01:53 (L153077), then outside 15.2, curve 28.0.
- 13:01:54 CS=10 + CH=0 (L153141). 13:02:24-13:12:24 PID 28.2-28.4 ~ cold setpoint: CS 28.2-28.4 + CH=1 (L153465-L159903); two short flames 13:03:46 27 s 'overshoot' (L154396/L154736) and 13:12:13 21 s (L159794/L160062). 13:27:29 CS=28.2 (L168673), flame 13:27:39, PWM ON 13:29:29, PWM OFF 13:32:29 (L172127), cycle 301 s GOOD. Capture ends ~13:38.
- Pressure alarm (sat/pressure_alarm, display only) 10 times, each cleared after 1-8 min: 09:15, 09:33, 09:58, 10:17, 10:41, 11:11, 11:25, 11:30, 12:23, 13:27 (L11646 ... L168670). CH pressure (MsgID 18, every ~5 min) is bimodal ~2.0 / ~1.70-1.74 bar.
- Loop stalls >= 1 s: 8, all 5186-5281 ms, all inside a weather fetch (L10264, L47932, L75012, L83694, L100962, L110728, L140935, L171735); 20 fetches, each ~5.15-5.4 s from 'fetching' to result.

AC#2 deviations, each opened in this session and checked by an independent second reader (opus agent, told to refute):
1. PWM vs thermo-nova heating_control.py:380-448 with const.py:35 (MINIMUM_SETPOINT 10), :151 (flame-off offset 18.0), :152 (suppression delay 20 s), :153 (offset 1.0): HOLDS. All 114 PWM ticks fit OFF=10 + CH=0 (:238), flame-off = Tret+18 clamped to max 48, hold, then flow-1. Tret is the PIC external return sensor (boiler answers Unknown-Data-Id to MsgID 28). Sanctioned differences: CS refresh every 3-6 s between ticks = entry 23 (TASK-1268); cold gate/floor 28.2 instead of 22 = entries 7, 71, 96 (ADR-199). Minor: the suppression value is clamped to max (12:17:08 48.25 -> 48.0); thermo-nova's suppression branch has no clamp; whether its coordinator clamps was not checked. No new SAT control deviation, consistent with Sergeant D's verdict (entry 119).
2. Weather fetch blocks the loop ~5 s on every fetch: HOLDS (second reader: arduino-esp32 HTTPClient keep-alive default HTTPClient.h:294/.cpp:1152, connected() stays true .cpp:398-403, SATweather.ino unchanged on origin/dev). -> TASK-1478, related TASK-1471.
3. TWDT reboot 12:56:52: PARTLY. Holds: reboot cause, weather not the cause, crashlog can never report an ESP32 TWDT (helperStuff.ino ~244, platform_esp32.h:527). Refuted premise: the TWDT does not watch the loop only, AsyncTCP's task subscribes too (AsyncTCP.h:41-42, AsyncTCP.cpp:321), so OTGW-Core.ino:1481 is wrong; the loop did stop (loop-driven MQTT silent after 12:56:17.287, MQTTstuff.ino:209). Core dump to flash is enabled, partition 0x3F0000 64K, never read. 5 earlier TWDTs in the unit's reboot_log. -> TASK-1479.
4. Pressure alarm (10 in telnet): HOLDS. Firmware window ~10 min (20 x 30 s ring, SATcontrol.ino:3865) and suspension on state.sat.bActive vs thermo-nova binary_sensor.py:335-345/357-388 window max(age,3600 s) and coordinator.active. Pressure is 2.0 bar with the slave CH-mode bit set, 1.70-1.74 bar without (pump head). Simulated: thermo-nova would alarm too (9 or 4-5 depending on the coordinator). Display only. -> TASK-1480.
5. outside=0.0 after the reboot (12:57:24-13:01:24, curve 33.2, CS 33.3-34.1): HOLDS; no MsgID 27 on the bus; fallback SATcontrol.ino ~1730-1733, 300 s start gate SATweather.ino:28. -> existing TASK-1390 (note added).
6. Sergeant D's 'CH misfire at 12:57:00' (msg 1565367551301779657): the boot release CS=0 (SATcontrol.ino:4240-4249 at fd491ff, still on dev ~4331, comment 'so the thermostat controls the boiler until SAT's first control loop iteration (~30s)') let the room thermostat's own CH on + TSet 48 through 12:56:55-12:57:24 (frames 12:57:01, :11, :15); boiler in CH mode [-C------], no flame (flow 61 C after the DHW draw), flow 61.5 -> 45 in 25 s (graph.csv). Not in thermo-nova (no PIC, no own boot). Start-up behaviour = Sergeant D question; handed to the coordinator with options A/B/C (not posted by this session).
Not covered: DHW-caused short flames (10:13, 12:20, 12:50-12:55) and whether SAT's cycle statistics should exclude them; not analysed.

AC#2 mapping to alpha.528-530: none of the findings is fixed by them. alpha.528 (TASK-1440 zones/ECO) and alpha.529 (TASK-1441 humidity) do not touch PWM, weather fetch, pressure, boot or TWDT. alpha.530 (TASK-1381 previous-CS term in the continuous flow clamp) only changes a falling request in continuous mode; the capture's continuous phases (09:00-09:11, 11:50-11:52, 12:57-13:01, 13:02-13:12, 13:27-13:29) show no falling request with a warm flow, so it would not have changed this run.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Analysis of gruijter's alpha.527 field capture (2026-10-29 08:58-13:38 local, PIC board, Intergas).
AC#1: timeline of SAT states and every CS/CH/MM/SW/PM command with transcript line refs in the notes (from a per-entry telnet parser; status topics from the sendMQTTData lines, pressure from MsgID 18, graph.csv for 12:53-13:04).
AC#2: deviations checked against thermo-nova (local copy taken as 7beaaf5 per CLAUDE.md) and Sergeant D's entries, each re-checked by an independent second reader told to refute. PWM matches thermo-nova heating_control.py:380-448 / const.py:35,151-153 (all 114 PWM ticks); sanctioned differences entries 23 and 7/71/96. Findings: weather fetch blocks the loop ~5 s per fetch (TASK-1478, linked to TASK-1471); Task-watchdog reboot 12:56:52 undiagnosable, TWDT comment wrong, core dump never read (TASK-1479); pressure drop-rate window/suspension differ from thermo-nova binary_sensor.py:335-388, alarm 10x on pump-head steps, display only (TASK-1480); outside=0.0 for 5 min after reboot (note on existing TASK-1390). Sergeant D's 'CH misfire 12:57:00' explained: boot CS=0 release (SATcontrol.ino:4240-4249) let the room thermostat's CH on + TSet 48 through for 30 s; the start-up design question (options A/B/C, recommendation B) is handed to the coordinator, not posted here.
AC#3: posted in #alpha-testing to Sergeant D (msg 1565374692959723581) and gruijter (msg 1565374738942140548); tasks 1478, 1479, 1480 filed.
Not covered: DHW-caused short flames and whether SAT's cycle statistics should exclude them; nothing verified on the unit itself.
<!-- SECTION:FINAL_SUMMARY:END -->
