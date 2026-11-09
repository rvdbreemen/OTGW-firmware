---
id: TASK-1734
title: >-
  Water pressure: show the decoded MsgID 18 value as is, remove the TASK-1682
  10-minute pressure-switch flag (Robert)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-08T15:28:00Z'
updated_date: '2026-11-09T05:12:00Z'
labels:
  - webui
  - maintainer-ok
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 841000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs 2026-11-08 15:13 #alpha-testing (msg 1569007058538201262): 'I think you are being too conservative with the pressure. no true boiler pressure can be 0.1 bar.' TASK-1682 (beta.138) flags a constant 0.10 bar for 10 minutes as a probable pressure switch. Proposal: flag 0.1 bar (or <= 0.1) immediately. Tester request: Robert decides.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert decides the rule (immediate vs 10-minute window, threshold)
- [x] #2 If changed: web UI flag follows the new rule, test red/green
- [x] #3 Robert decides the rule: show the decoded value, no special case (2026-11-08)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08 18:07 Robert in #dev-sat-mqtt (msg 1569050822783017041): 'when the pressure is low and unrealistic like 0.1 it's probably not a measuring pressure but a failure from a boiler to report properly. Ask tsjf in the alpha channel what he thinks it should be called.' Reading: Robert treats a constant 0.1 bar as a boiler reporting failure, not a measurement; naming question put to tjfs in #alpha-testing (Discord agent, 2026-11-08 ~21:58).

2026-11-08 21:29 tjfs #alpha-testing (msg 1569101468550828093): 'I think you could either hide it altogether or take it as the boiler being happy with the pressure, if it gets too low the boiler should shut down'. Tester input (two options: hide the value, or show it as OK/no fault); Robert decides.

2026-11-08 ~23:10 Robert decided (AskUserQuestion in coordinator session otgw-firmware-4c): flag immediately. A reported water pressure <= 0.1 bar gets the 'no real pressure sensor (switch)' flag at once, no 10-minute window; the value shows as a dash instead of a number. Rationale: no real boiler runs at 0.1 bar (tjfs 15:13, Robert 18:07: fixed 0.1 bar is a boiler reporting failure, not a measurement). Hiding the sensor entirely was rejected (loses the switch-OK signal).

2026-11-08 22:00-22:04 #alpha-testing, Robert to tjfs (msgs 1569109416714830008, 1569109799201939609, 1569109855040446585, 1569110087799144580, 1569110263586881579): 'I don't agree with this you know. The UI should just show whatever the OpenTherm bus is telling. Reason is that even if 0.1 is wrong it's not a firmware problem, it's your boilers problem.' / 'my firmware always showed whatever the boiler and thermostate report.' / 'If we are going to interpret and give our own meaning to things where does it stop.' / 'Maybe ask your manufacture to correct their firmware'. tjfs (1569109631542890629, 1569110116484260025): 'a toggle to show pressure or not so the user can choose' / 'a user toggle to hide the pressure display on the main display screen if it's known to be meaningless.' CONFLICT: this contradicts the decision recorded earlier tonight (<= 0.1 bar shown as a dash at once, 07b38ee49) and the bot's 21:43 message to tjfs. Needs Robert's final word before implementing.

2026-11-08 ~22:20 Robert (in session to the Discord agent), final: 'toon een streepje bij <= 0.1 bar'. Conflict above resolved: the dash decision (07b38ee49) stands; no correction to tjfs needed (Robert).

tjfs request (22:01/22:03): a user toggle to hide the pressure on the main screen when it is known to be meaningless. Separate from the <= 0.1 bar dash rule; put to Robert 2026-11-08 ~22:15 (rule: every tjfs request goes to Robert).

2026-11-08 22:11-22:13 #alpha-testing: Robert to tjfs (1569112073806549003): 'So let's not make an exception for a boiler that is NOT following the specfication. Let's keep following the specification.' tjfs (1569112379046895687): 'what about a user toggle to suppress it?' Robert (1569112432893231308, 1569112679962906675): 'I do not want my firmware to do that.' / 'The bot does not decide, I do.' Reading: user toggle rejected. Open: does 'keep following the specification' also withdraw the <= 0.1 bar dash (show the raw READ-ACK value)? Put to Robert in session; implementation on hold until answered.

2026-11-08 ~22:25 Robert FINAL (in session, supersedes the dash decision of 07b38ee49 and his in-session 'streepje bij <= 0.1 bar'): 'If there is time complexity to the CH water pressure, then remove that. It should simply display the value it decoded. That's all there is to it.' Scope: remove the TASK-1682 pressure-switch logic from src/OTGW-firmware/data/v2.js (PRESS_SWITCH_* consts + trackPressSwitch/pressIsSwitch ~1282-1305, model.pressFlatSince at 463, calls at 520 and 3343, pressText/pressIsSwitch uses at 1176, 1302-1303, 1325 -> plain fmt(model.pressure, 2, ' bar') and pressClass always), delete tests/webui/pressure-switch-flag.test.mjs, adjust tests/webui/graph-control-setpoint-source.test.mjs:57-61. No dash, no window, no toggle (toggle rejected by Robert 22:12).

2026-11-08 ~23:55 Robert, asked again in session 4c after the coordinator relayed his ~22:25 instruction (UI shows only the decoded value): final rule = show the decoded value only. This supersedes the 23:10 'flag at once / dash' note above. Scope: remove the TASK-1682 pressure-switch flag from v2.js (PRESS_SWITCH_*, trackPressSwitch, pressIsSwitch, pressText/setPress, model.pressFlatSince and their calls); every readout shows fmt(model.pressure, 2, ' bar') and the normal pressClass colour, as MQTT/REST/HA already do. Accepted consequence: a switch-only boiler's 0.10 bar shows in the low-pressure colour.

2026-11-08 23:50 evidence (otgw-firmware-4c): AC2 red/green: tests/webui/pressure-switch-flag.test.mjs guard, 1/3 pass on origin/dev 07b38ee49 (2 fail: switch code present, readouts not plain) and 3/3 on lane-1734 0ff5bcbb6; graph-control-setpoint-source 11/11, no-sample-values 2/2, test_sat_pressure_health 8 passed 1 skipped, evaluate --quick 0 FAIL 0 WARN. AC3: Robert's answer 'Alleen gedecodeerde waarde' via AskUserQuestion in session 4c (~23:55, recorded above). Open DoD: esp32-combo build, OTA bench on .61 (readouts show '<n> bar'), push; waits for int-157 to land (rebase + re-bump to beta.158).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Web UI shows the MsgID 18 CH water pressure exactly as decoded, like MQTT/REST/HA; the TASK-1682 0.1 bar 'switch?' mark ( · switch? suffix plus tooltip after a 10-min window) is removed. Shipped as 2.0.0-beta.158+8a4b25d (origin/dev 73f9654da..8a4b25d60, fix dc64d4aba). AC1/AC3: Robert's decision 'Alleen gedecodeerde waarde' (AskUserQuestion, 2026-11-08 ~23:55), superseding the 23:10 'direct markeren'. AC2: tests/webui/pressure-switch-flag.test.mjs 1/3 on old dev (switch code present, readouts not plain), 3/3 on the fix; graph-control-setpoint-source 11/11; evaluate --quick 0 FAIL. Build esp32-combo beta.158+8a4b25d (zip 5992501 B). Bench .61 OTA 2026-11-09 00:18: runs beta.158+8a4b25d, fs hash 8a4b25d, 164 settings keys unchanged, device-served UI has 0 switch-code matches and the plain fmt readouts; soak 00:21-00:37 16 samples, no reboot, HTTP 200, freeheap 70636-72292, maxfreeblock 31732. Zip posted #alpha-testing msg 1569217694610358313. NOT verified on a device: the rendering of 0.10 bar, because the bench has no boiler and so no MsgID 18 (port 25238 cannot inject boiler answers); that rests on the host test running the real fmt()/pressClass().
<!-- SECTION:FINAL_SUMMARY:END -->
