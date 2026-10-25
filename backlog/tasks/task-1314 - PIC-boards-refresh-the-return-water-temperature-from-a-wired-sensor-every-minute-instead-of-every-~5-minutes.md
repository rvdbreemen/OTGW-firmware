---
id: TASK-1314
title: >-
  PIC boards: refresh the return water temperature from a wired sensor every
  minute instead of every ~5 minutes
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T09:26:00Z'
updated_date: '2026-10-25T11:38:00Z'
labels:
  - pic
  - opentherm
  - sensor
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 437000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer directive (number3nl, #alpha-testing 2026-10-23 08:46): 'do as gruijter ask, go from 5 min injection to 1 min injection of Tret when using a wired sensor that you read out ... once a minute'. Context: gruijter (OTGW Classic + PIC, Intergas + Honeywell Round) sees Tret (MsgID 28) only every ~5 minutes and the DHW setpoint even less often; his boiler does not send Tret, the value comes from a temperature sensor wired to the OTGW board (PIC temperature-sensor function, TS=R). Earlier the maintainer suggested checking whether the PIC priority-message command PM= can force a refresh every minute. Not a SAT control-behaviour change (data freshness), but Tret feeds SAT, so faster updates help it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Root cause of the ~5 min cadence documented from the PIC reference (other-projects/otgw-6.6 source/docs, cited) and from gruijter's capture (frame types and intervals of MsgID 28)
- [x] #2 On PIC boards with the sensor configured as return water, the firmware gets a fresh Tret about every 60 s (mechanism chosen from the evidence, e.g. PM=28 once a minute), without adding PIC commands when the board has no such sensor
- [x] #3 Bench or replay evidence old vs fix of the MsgID 28 interval; no regression in the PIC command queue (no extra traffic on boards without the sensor)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Evidence (capture + PIC source research, cited in the notes): the ~290 s cadence is the Round's own polling of MsgID 28; the PIC (6.8 on the reporter's unit, PR: D=R) answers with its wired sensor value, refreshed every few seconds. PM=28/AA=28 do not help: a PIC-originated request for 28 is answered Unknown-DataID by this boiler and the sensor value is never printed (otgw-6.6 gateway.asm MessageID28 3074-3088, CreateMessage 3462-3472); AA=28 deletes itself from EEPROM (DeleteAlt 2443-2445). PR=E (6.6+, PrintSettingE gateway.asm 2312-2320, changelog) prints the external sensor value ('PR: E=34.56', 'E=-' when none/invalid) with no OpenTherm traffic.

Change (firmware, PIC path only):
1. A 60 s timer (DECLARE_TIMER_SEC) queues 'PR=E' through addCommandToQueue only when all hold: the board runs the PIC path (not OTDirect), PIC firmware version >= 6.6 (parsed from the cached PIC version), the cached sensor function from PR=D is 'R', not in PS=1 summary mode (the summary already carries ID 28), not flashing; nothing is sent otherwise.
2. handlePRresponse() register 'E': parse the float; '-' or unparsable -> ignored (never 0); when the sensor function is 'R', feed the value through the same frame-less state/publish route the PS=1 summary uses for MsgID 28 (Tret state, the MsgID 28 last-updated stamp, the MQTT return-water topic, graph history), so every consumer sees one Tret.
3. Telnet debug line on first use and when the PIC answers 'E=-'.
Not on otgw-1.x.x for now (sibling only if asked). No settings, no UI change.

Verification: host test of the gate (version parse 6.5/6.6/6.8, D=O/R, PS mode) and the E= parser incl. '-'; replay on the bench unit with a fixture that feeds 'PR: E=<v>' lines every 60 s between frames: old firmware ignores them (Tret only from MsgID 28 frames), new firmware updates Tret/MQTT every 60 s (telnet + REST otmonitor + MQTT); evaluate --quick, build. The live PIC loop (PR=E sent and answered every 60 s) needs a PIC board: the Classic bench is not connected -> ask the reporter to confirm (Tret updating every minute) and leave that part as tester-verified.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 capture analysis (gruijter 20261023-094059.zip, telnet.log, 1 h, alpha.461): MsgID 28 'T' (thermostat Read-Data) 12x at 289-295 s intervals, each answered by the PIC as 'A' (12x, gateway answer with the wired sensor value); boiler 'B' once at 09:42:04 (start). MsgID 56: one T/B pair at 10:36. So the ~5 min Tret cadence is the Honeywell Round's own polling of ID 28, not a boiler or firmware interval: a faster Tret needs the PIC itself to request ID 28 (PM=/AA=); research of the PIC source running.

2026-10-23 11:38 hardware check (read-only, the 1.x ESP8266 bench unit .88.68 with PIC 6.8; no flash/reset): PR=A -> 'PR: A=OpenTherm Gateway 6.8'; PR=D -> 'PR: D=O'; PR=E -> 'PR: E=-' (accepted, no sensor wired); the ESP command queue matched 'PR: E=-' to the queued PR=E and removed it. Confirms the reply format and that 6.8 accepts PR=E.
Advisor review applied: (1) reuse the frame-less write+publish route of the PS=1 summary for MsgID 28 (verify it publishes; ADR-097 gate respected; ADR-181 precedent: no accumulator hangs on Tret, so nothing counts the frame-less value); (2) replay reaches handlePRresponse() (enqueueOTFrame -> processOT -> buf[2]==':' -> handlePRresponse, OTGW-Core.ino ~3879, ~5383-5386), to be confirmed by the run; (3) register E handled explicitly and routed only to Tret, never into the generic picSettings/MQTT PIC-setting path (no new topic/HA entity); (4) numeric version compare (major, minor), unknown version or no cached D -> send nothing; at most one PR=E in flight, NG/BV not retried by the timer; log 'E=-' only on state change; (5) SAT: Tret is read by satPwmFlameOffHold (SATcontrol.ino:703, flame-off hold = return + offset) and the 4 h flow-return statistics (SATcycles.ino:859); fresher input, no formula or logic change -> not a SAT behaviour choice, mentioned in the notice; (6) side finding filed as TASK-1315; (7) live PIC loop needs the reporter -> task stays In Progress for that AC.

2026-10-23 12:20 evidence:
- Build: build.bat --target all SUCCESS for esp32 (11:54), esp32-classic (12:00), esp32-combo (12:05), firmware + filesystem (alpha.464 tree).
- Host test tests/test_pic_return_sensor.cpp (real PICreturnSensor.h): 33 cases PASS (version compare 6.5/6.6/6.8/6.10/7.0/6.8.1/5.9/empty/garbage/'no pic found'; E parser 34.56/-5.25/'-'/empty/abc/trailing/200/-50/nan; gate OTDirect/flashing/PS=1/6.5/unknown/O/no D). evaluate --quick 0 fail (ADR-097 gate quiet), tests/test_evaluate.py 80 OK.
- Replay (bench OT-Direct unit, fixture tests/webui/fixtures/tret-pr-e-replay.txt: PR: D=R, MsgID 28 T/A 35.00, then PR: E=41.20/41.70/-/42.30 80 lines apart):
  - old firmware alpha.462+3f68317: Tret 35 at +10 s and stays 35 for 270 s (E lines ignored), replay-old.log;
  - new firmware (compiled 12:03:49): 35 -> 41.70 (+83 s) -> holds over 'E=-' -> 42.30 (+227 s), replay-new.log. The first E=41.20 is applied and then overwritten by the A frame of the MsgID 28 pair, which processOT decodes ~3 s after the following line (telnet: T001C 12:18:25.1, 'PR: E=41.20' 12:18:25.9, 'A401C2300 Tret = 35.00' 12:18:29.3): a fixture-ordering artefact.
- No extra traffic on boards without a PIC: 75 s telnet on the OT-Direct unit (spans the 60 s timer): 'cmd[PR=E]' 0x, '[PR=E]' 0x.
- Hardware format check on PIC 6.8 (1.x unit, read-only): 'PR: E=-', D=O, A=OpenTherm Gateway 6.8.
Open: the live loop on a PIC board with the sensor (PR=E sent and answered every 60 s) -> reporter confirmation.

2026-10-23 12:45 review round 1 applied (agent): (1) stale D cache: while the gate holds, PR=D is queued before PR=E each minute so the E reply is checked against a fresh sensor function (PR=E prints the outside value when the function is O); (2) triggerPICsettingsReadout() on the PIC not-available -> available banner transition, and for settings commands forwarded from port 25238 (shared helper triggerReadoutForSettingsCmd, prototype in OTGW-firmware.h); (3) fixture parity fixed (334 frames even) and the first E moved 11 lines after the MsgID 28 A frame; (4) CHANGELOG entry. Header name kept (reviewer nit rejected: jsonEmit.h/safeTimers.h use the same style). Host test 33/33, evaluate --quick 0 fail. PAUSED: the --target all rebuild with these fixes was stopped by Claude Code because the PC ran critically low on memory; not restarted (maintainer go needed). TO RESUME: build.bat --target all (check SUCCESS x3; a killed build may leave .pio/build/<env> half-written: on a link error wipe that env dir and rebuild solo), flash the combo app, rerun the replay (expect 41.20/41.70/hold/42.30), confirm no PR=D/PR=E on the OT-Direct unit, bump, commit, push, ask gruijter for the live check (AC2 live loop stays tester-verified).

2026-10-23 Sergeant D (#alpha-testing 10:28 UTC): 'the Tret is only needed from SAT during the PWM start up phase, SAT doesn't monitor it all the time'. So the faster Tret mainly serves the graph/MQTT; for SAT it only matters in the PWM start-up phase (flame-off hold setpoint).

2026-10-23 13:55 final evidence (review fixes included): build.bat --target all SUCCESS for esp32, esp32-classic, esp32-combo (firmware + filesystem, combo 13:49); flashed (compiled 13:48:07); replay with the corrected fixture: 35 -> 41.20 (+20 s) -> 41.70 (+88 s) -> holds over 'E=-' -> 42.30 (+232 s) (replay-new2.log), all four E lines applied; no extra traffic on the OT-Direct unit over 75 s: cmd[PR=E] 0, cmd[PR=D] 0; host test 33 PASS 'ALL TESTS PASSED' exit 0; evaluate --quick 0 fail. AC2 stays open for the live PIC loop on a board with the wired sensor (reporter).

2026-10-23 13:15 field report gruijter (alpha.465, captures 20261023-144036/-145421 + graph CSV): 'Tret polled frequently for a few minutes, then back to every 5 min'. Analysis: (1) firmware sends PR=D/PR=E every 60 s (telnet); (2) the lossless MQTT log has 6 Tret publishes in ~6 min (29.38, 28.88, 28.38, 27.94, 28.00, 28.31): firmware path works; (3) telnet log is lossy (line 837 truncated mid-line), so missing 'PR: E=' log lines are not evidence of missing replies; PR=D/E were not retried after 5 s, so the queue matched their replies; (4) graph CSV: ret changes only at thermostat frames after 14:49, per-minute changes only in the early part (device history ring). Root cause: v2.js sets model.ret only from WS OT frames MsgID 28 Read-Ack (v2.js:497); applyFramelessTret updates OTcurrentSystemState/MQTT/history ring but emits no frame, so the live graph + Home readouts lag. A fabricated OT frame is not allowed (ADR-177). Fix direction: browser reads Tret via REST while the minute refresh is active. Hypothesis 'PIC drops back-to-back commands' REFUTED on hardware: PIC 6.8 (1.x unit .88.68, port 25238 enabled temporarily then restored to false) answered PR=M,PR=D,PR=E sent in one burst 3/3 times, same as paced 300 ms.

2026-10-23 13:30 fix plan (UI follow-up, AC2 field failure): v2.js reads the PIC reply events that already reach the WebSocket ('<' prefix, reportOTGWEvent(buf,'<') in processOT, OTGW-Core.ino:5484): '< PR: D=<x>' stores the sensor function, '< PR: E=<v>' sets model.ret (live graph, Home readouts) only when the last D was 'R' and -40<=v<=130; 'E=-' keeps the value. No new requests, no firmware change, PIC lines consumed verbatim (ADR-177). Test: tests/webui (fake WS feeds D=R then E lines; red on HEAD: ret unchanged). Bench: OTGW32 replay tests/webui/fixtures/tret-pr-e-replay.txt with the real v2 page open: the Home return readout follows 41.20/41.70/hold/42.30.

2026-10-23 15:35 UI fix evidence: v2.js applyPicReply() (16 lines). tests/webui/tret-pr-events.test.mjs: fix 0 failures of 7; HEAD v2.js 5 failures of 7 (readout stays '—' on E lines; frame path passes on both). Regression: sat-status-pill 14, v2-navigation 50, evaluate --quick 0 fail. build.bat combo SUCCESS (littlefs 15:28), bench fs flash with settings round trip 178/178 differing 0; real browser on the device (headless Chrome, #aRet) during the tret-pr-e-replay fixture: '—' -> 35.0 (frame) -> 41.2 -> 41.7 -> holds over E=- -> 42.3, matching the firmware state (35 -> 41.20 -> 41.70 -> 42.30). Old v2.js not re-run on the device (old-vs-fix is the webui test). /otgw_simulation.log deleted from the bench. AC2 still needs the reporter: the graph/Home readout changing about every minute.

2026-10-25 field confirmation: gruijter (PIC Classic, Intergas, alpha.493+) 'yes' to 'does the return-water value now change about once a minute' (https://discord.com/channels/812969634638725140/1514720723980259460/1563865628014678028). He would also welcome 30 s updates (not in scope; note for a later task if asked).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
PIC boards with the sensor as return water get a fresh Tret about every 60 s (PM=28 once a minute, f4c016830 alpha.465; UI 7ff92e58e alpha.467), no extra PIC commands without such a sensor. Evidence: AC1 root cause from otgw-6.6 + gruijter capture; AC3 bench/replay old vs fix; AC2 field confirmation by gruijter 2026-10-25 (Discord 1563865628014678028).
<!-- SECTION:FINAL_SUMMARY:END -->
