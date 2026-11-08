---
id: TASK-1681
title: >-
  OT-Direct: fresh gateway shows 1 injected write override MsgID 1 = 0x2D00
  (45.0 C) with no Clear button
status: Done
assignee: []
created_date: '2026-11-07T09:03:00Z'
updated_date: '2026-11-08T10:17:00Z'
labels:
  - bug
  - otdirect
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 802000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by tjfs in #alpha-testing 2026-11-07 08:52 UTC (msg 1568548767014330449), board without PIC, beta.117. Write overrides (otOverrides, OTDirect.ino:397) come from CS= / setback (checkThermostatTimeout, OTDirect.ino:2662) / master-mode thermostat WRITE of MsgID 1 (OTDirect.ino:2718); the web UI renders write rows without a Clear action (v2.js renderOtdOvr, otdOvrRow(...,null)). Unknown which path set 45.0 C on a fresh unit (candidate: a retained MQTT command, or master mode with a thermostat). Asked tester for mode, thermostat, MQTT and a capture.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Source of the MsgID 1 write override on a fresh unit identified from tester data or a bench reproduction
- [x] #2 Write overrides can be cleared from the web UI, or the UI says why they cannot (mode-driven)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 likely cause found in code: in master mode with CH mode fixed (defaults iCHMode=1, fFlowTemp=45.0, OTDirecttypes.h:120-121), loopOTDirect calls enqueueWriteCommand(1, floatToF88(getFlowTemp()), "heating-curve") every 60 s (OTDirect.ino:2258-2263), which sets the MsgID 1 write override to 0x2D00 = 45.0 C. Bench .61 (master mode, no thermostat, SAT enabled) shows write overrides MsgID 1=9420 (0x24CC, 36.8 C from SAT) and 14=25600 (0x6400). So on a master-mode unit the entry is the gateway's own control, not a stray injection; a Clear button would be undone within 60 s. Fix direction: label master-mode/SAT writes as the gateway's own control in the UI instead of 'injected', no Clear for them. Waiting for tjfs to confirm the mode.

2026-11-07 AC2: v2.js renderOtdOvr adds a note under the write rows: Write = a value the gateway itself sends to the boiler (SAT's control while SAT is on, or a command such as CS=, C2=, MM=); it cannot be cleared here: switch SAT off, or stop sending the command (CS= and C2= end 60 s after the last one: OT_CSC2_EXPIRY_MS = 60000, OTDirect.ino:484; SAT clears its own on satDisable, OTDirect.ino:800). Test tests/webui/otd-write-override-note.test.mjs (real v2.html/v2.js, headless Chrome, fake OT-Direct device with write MsgID 1 = 0x2D00 and SR 25): old v2.js 2 of 6 checks FAIL (no note), fix 6 of 6 PASS; stored responses keep their Clear button. Uncommitted in the main tree; commit + bump after 54's lane-1707 (132/133) and lane-1700. AC1 open: the source in code is the master-mode heating-curve writer (enqueueWriteCommand(1, floatToF88(getFlowTemp()), heating-curve) every 60 s, default fFlowTemp 45.0 = 0x2D00), removed by TASK-1684 (834902ac8); no bench reproduction on the old firmware and no confirmation of tjfs's mode yet.

2026-11-08: build waits: queue lane-1700 (beta.134) -> 54's TASK-1455 bench-only builds + 90 min .61 (Robert priority) -> this task (next free prerelease).

2026-11-08 (session 6b): my explanatory-note variant of AC2 is withdrawn (never pushed); session 'OTGW-firmware toegang' had a Robert-agreed AC2 design (per-command labels, Clear via the real clear command CS=0/C2=0/MM=T/TT=0/SW=0/SH=0, CC reason, sat_owns flag in GET /api/v2/otdirect/overrides) and takes the task. Reusable test harness in session 6b scratchpad t1681/0001-*.patch.

2026-11-08 (this session). AC1: the MsgID 1 = 0x2D00 (45.0 C) write override was the OT-Thing controller's 'heating-curve' write every 60 s in master mode; TASK-1684 (834902ac8) removed that controller: no 'heating-curve' enqueue and no getFlowTemp left in src (grep), and the TASK-1684 bench (.61, port 25238 raw frames, master mode, SAT off) showed 0 MsgID 1 writes on the new image vs 2 x 0x2D00 on the old one. AC2 (design agreed with Robert): v2.js names every write override (1 CS, 7 CC, 8 C2, 14 MM, 16 TT/TC, 56 SW, 57 SH, 100 TT/TC flags); Clear POSTs the firmware clear command to /api/v2/otgw/commands (CS=0, C2=0, MM=T, TT=0, SW=0, SH=0, taken from the OTDirect.ino command branches); CC has no clear command (CC=0 writes 0), so the row says it stays until restart (the overrides are RAM only); while overrides.sat_owns (new, = satOwnsControlSetpoint(), OTDirect.ino overrides JSON) is true, rows 1/8/14/16/100 say SAT controls them (TASK-1695 refuses those commands), SW/SH stay clearable. Seam (agreed): tests/webui/otd-override-clear.test.mjs, 5 tests, built TDD slice by slice (red each time on the right assertion first). Old v2.js (origin/dev data, OTGW_DATA_DIR): 5 of 5 fail on their assertions. New: 5 of 5 pass in every run where Chrome started; under load from other sessions' builds and browser tests, headless Chrome sometimes stalls at CDP Page.enable (also with OTGW_CDP_TIMEOUT_MS=60000): infrastructure, not the test logic. evaluate --quick 0 failed.

2026-11-08 code review (medium, forked) and fixes. Correction to the note above: MsgID 7 IS clearable: CL=0 calls clearWriteOverride(7) (OTDirect.ino CL branch), so the CC/CL row now offers Clear with CL=0 (CL is not SAT-blocked). MsgID 16 is also set by BS= and the MQTT room_setpoint route: the row now says 'TT/TC, BS or MQTT'. Clear now shows a toast when the POST is refused or fails, and rereads the list after 0.4 s and 2 s (the command is only queued). otdOvrRow takes a click callback, so the write rows no longer duplicate the Clear-button markup. Test matcher tightened to 'MsgID N' followed by '0x'. Docs: sat_owns is also true during the MQTT-loss fallback. Not taken: the review's 'ids 24/27/71/20 missing' is a false positive: setOverride() only touches the ids in otOverrides[] (1, 7, 8, 14, 16, 56, 57, 100), so no other id can appear as a write override. Tests after the fixes: new v2.js 5/5 pass; old v2.js (origin/dev data) 5/5 fail; evaluate --quick 0 failed. Rebased onto beta.142 (4f7fd7742, TASK-1718) and re-bumped to beta.143 at 6b's request.

2026-11-08 bench .61 (OT-Direct, master mode, SAT enabled+active), beta.143+0f8ffd4: ota_flash app+fs OK, ETag 0f8ffd4-/v2.html.gz, settings 164 keys changed: none. GET /api/v2/otdirect/overrides: write 1 (SAT CS) + 14 (SAT MM 100%), "sat_owns":true. POST SW=50 -> write 56 = 12800 appears. Real device UI in headless Chrome (Monitor > Connection): rows 'CS · control setpoint · MsgID 1 ... SAT controls this while SAT is enabled; switch SAT off to clear it.' (no button), same for MM 14, 'SW · DHW setpoint · MsgID 56 0x3200' with Clear; click -> POST {"command":"SW=0"} -> row gone in the UI and MsgID 56 gone from the firmware JSON. Screenshots: scratchpad bench1681/ui61.png and ui61-after.png. Health 5 min after the flash: bootcount 148 -> 150 (+2 = flash), no crashlog, MQTT connected, min free heap 22672. Not tested on the bench: the SAT-off Clear of CS/MM/C2/TT (SAT left as found), CL=0 and BS/MQTT-set MsgID 16 on hardware (host-tested in the web UI harness only). Cosmetic, not changed: the reason text renders bold on the label line, and the panel subtitle still says 'Set or clear stored responses / response modifiers'.
<!-- SECTION:NOTES:END -->
