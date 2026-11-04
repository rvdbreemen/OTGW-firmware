---
id: TASK-1271
title: >-
  Graph control-setpoint line mixes thermostat and boiler frames and mostly
  shows the thermostat's own 10
status: Done
assignee:
  - '@claude'
created_date: '2026-10-20 18:16'
updated_date: '2026-10-20 20:01'
labels:
  - bug
  - webui
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 401000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
First real SAT field run (gruijter, Intergas HReco 30 + Honeywell Round + PIC Classic, alpha.439/441, #alpha-testing 2026-10-20), captures in %TEMP%\discord-1020. data/v2.js ~464: case 1 sets model.chSet from every write-type MsgID 1 frame regardless of source (T thermostat, R gateway, B, A). Each exchange writes T=10, R=45, B=45, A=10; the A frame comes last, so the graph shows the Honeywell's own TSet 10 most of the time (CSV 11:42-11:51: 17% at 45 while the boiler received 45 throughout). Same pattern for MsgID 16 roomSet (~465), which is the thermostat's TrSet, not SAT's target.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Control-setpoint line shows what the boiler received (B write-ack) in gateway and monitor mode (frame-replay test, red on old code)
- [x] #2 Room-setpoint line labelled as thermostat setpoint, or SAT target added as its own line
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. data/v2.js applyFrame() case 1: set model.chSet only from the boiler's Write-Ack (raw[0]==='B' and type 5). That is the value the boiler accepted, in every topology: PIC gateway (T thermostat, R gateway->boiler, B boiler ack, A answer to thermostat), monitor mode (T/B) and OT-Direct (R/B, OTDirect.ino bridgeFrameToParser). Field log cap-1138: T10 84x, R10/R90 78x, BD0/B50 97x, AD0 80x.
2. roomSet (MsgID 16) keeps its source (the thermostat's TrSet write) and the graph legend label becomes 'thermostat setpoint' so it is not read as SAT's target (GSER table ~1250).
3. Check the REST seed applyOtmonToModel() chSet <- otmonitor 'controlsetpoint' (OTcurrentSystemState.TSet): note which side it reflects; it only seeds until the first frame.
Verification: node test in tests/webui/ that loads the real v2.js and replays MsgID 1 frame sequences (T10 then R/B/A with 45) and the captured MsgID 1 frames from gruijter's telnet.log: old v2.js ends/mostly sits at 10, fixed v2.js follows the boiler ack (45). node --check, existing webui tests, build.bat (fs), evaluate --quick. Bench screenshot optional (OTGW32 has no boiler traffic). UI-only, no SAT behaviour change, no port to 1.x (different UI).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Design change after the opus review: a MsgID 1 (and 16) Write-Ack's data is undefined (OTGW-Core.h:375 bSlaveEchoesValue=false; echo audit row 1: heat pump answers D0010000 to TSet 27.22; row 16: Intergas BD0100000), so 'boiler ack only' would show 0 forever on such slaves. Final rule mirrors is_value_valid_for_master_topic(): Write-Data (type 1) only; an R frame always counts, a T frame only when no R for MsgID 1 came in the last 30 s (a dropped R frame cannot bring back the thermostat's 10). MsgID 16 roomSet: type 1 only (fixes the old 0 flap on Intergas). REST seed unchanged (same boiler-side worldview). Evidence: tests/webui/graph-control-setpoint-source.test.mjs 10/10 on the fix, 4/10 on HEAD (6 red: PIC exchange, dropped-R, non-echo MsgID 1, non-echo MsgID 16, legend, field capture); field capture 1138: 45 in 66 of 78 slots after the first 45 on the fix (other 12 are real 40.x values), 44 of 119 on HEAD; the 23 T frames without an R in that capture were checked: the boiler acked 45/40.6 for them, so their R lines were lost in the flooded capture, not passed through. webui suite 44/44, static integrity 33 OK, evaluate --quick 0 FAIL, build.bat esp32-combo SUCCESS fw+fs. Bench OTGW32 .88.61, replay of tests/webui/fixtures/msgid1-pic-exchange.txt as /otgw_simulation.log (T10 R45 B45 A10): alpha.441 UI CSV chSet 12x10 + 3x45 (red), new filesystem 9f41ef6 via /update CSV chSet 14x45, 0x10 (green), 0 console errors both; screenshots %TEMP%\bench-1271\red-alpha441-graph.png, green-9f41ef6-graph.png; settings 0/567 changed by the fs OTA; original otgw_simulation.log backed up (%TEMP%\bench-1271\otgw_simulation.orig.log). Not changed on purpose: Home card label 'ROOM SET' (separate widget), manual text (describes the graph only generically).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The graph's control-setpoint line, the setpoint marker and the CH SETPOINT card now show what the boiler was sent: MsgID 1 Write-Data only, the gateway R frame taking precedence over the thermostat T frame for 30 s, Write-Acks ignored because their data is undefined (heat pumps answer 0). The thermostat-setpoint line (MsgID 16, relabelled from 'room setpoint') uses Write-Data only, which removes an old 0 flap on Intergas. AC1: frame-replay test through the real parser and graph slots, 6 red on HEAD / green on the fix, plus field capture 1138 (37% -> 85% of slots at the 45 the boiler got, rest real values) and an on-device replay on the OTGW32 (CSV 12x10 -> 0x10, screenshots). AC2: legend label 'thermostat setpoint', test-covered. Not verified: a live PIC gateway with a real thermostat (replay only).
<!-- SECTION:FINAL_SUMMARY:END -->
