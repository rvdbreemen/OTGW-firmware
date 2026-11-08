---
id: TASK-1733
title: >-
  OT-Direct: TT= from the home dial shows 21.5 as the room temperature and the
  boiler does not fire (tjfs, beta.146+)
status: To Do
assignee: []
created_date: '2026-11-08T15:26:00Z'
updated_date: '2026-11-08T21:27:00Z'
labels:
  - bug
  - otdirect
  - field-report
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 840000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs 2026-11-08 15:12 #alpha-testing (msgs 1569006644661190766, 1569006818347458663, screenshot): turned the dial up to 21.5 with SAT off; UI said 'thermostat override 21.5 until its next program change', the hero big number (bRoom = model.room = MsgID 24 room temperature, v2.js:1215) showed 21.5 while 'target 20.0' (model.roomSet = MsgID 16) stayed, and the boiler did not fire. So the TT value appears as room temperature instead of the thermostat taking over the setpoint (TASK-1718 MsgID 9 answer, TASK-1717 dial). Needs: a capture from tjfs during a TT test, and a host replay of TT=21.5 in gateway mode checking which MsgID carries 21.5 towards the boiler/thermostat and what the web UI parser sees.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Capture or host replay shows why 21.5 lands on MsgID 24 / the room temperature display (file:line)
- [ ] #2 Fix: TT= in OT-Direct gateway mode changes the setpoint path only; room temperature display unchanged; red/green test
- [ ] #3 Field: tjfs confirms the thermostat takes the override and the boiler fires
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08 host analysis (session OTGW-firmware toegang). (1) Boiler not firing: tjfs's thermostat never READs MsgID 9. His 20-min capture 20261107-201007 (beta.127) has thermostat frames only for MsgID 0, 1, 15, 16, 17, 24, 25, 27 (the R 9 reads are the gateway polling the boiler). A TT= override only takes effect when the thermostat reads MsgID 9 and adopts it (TASK-1718 answers that read); MsgID 16 to the boiler is informational, so TT= cannot change this thermostat's heating, on OT-Direct or on a PIC. (2) 21.5 as room temperature: not reproduced. Host replay tests/test_otd_gateway_alt_slots.py case T2 (real OTDirect.ino slices incl. TASK-1707 alternative slots and the TASK-1718 MsgID 9 reply): gateway mode, boiler answers MsgID 24 Unknown-DataID as in tjfs's 39 s capture (so 24 becomes an alternative slot), tjfs's thermostat stream without MsgID 9, TT=21.5 applied as setRemoteOverride() does (queue WRITE 16 and 100, write cache, overrides). The bridge log decoded with v2.js's rules: MsgID 24 stays the thermostat's 0x1499, 0 frames carry 21.5 on 24; MsgID 16 (type 1) = 0x1580 = 21.5 in 11 R frames; the boiler gets 11 x WRITE 16 = 21.5. The firmware sets Tr (roomtemperature in /api/v2/otgw/otmonitor) only from a MsgID 24 frame (OTGW-Core.ino:5047). The screenshot is also inconsistent: v2.js renders the big number and the green tick from the same model.room (v2.js:1205-1215), yet the tick sits well below the setpoint arc. Test test_T2_tt_lands_on_the_setpoint_not_on_the_room_temperature passes on dev (characterisation: the defect does not occur in the replay). AC1 stays open: needs a capture from tjfs during TT=21.5 (via REST/MQTT now that TASK-1735 removes the dial) or the thermostat model; the coordinator asks tjfs. No firmware fix.

2026-11-08 17:1x (Discord agent): tjfs answered at 16:07 (msg 1569020552834850847): Ideal Halo Lite thermostat, Ideal Logic boiler. Acknowledged in #alpha-testing; capture during TT=21.5 still asked. (carried over from the main dev tree copy by otgw-firmware-4c, 2026-11-08)
<!-- SECTION:NOTES:END -->
