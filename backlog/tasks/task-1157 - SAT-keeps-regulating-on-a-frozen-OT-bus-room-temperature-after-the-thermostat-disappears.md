---
id: TASK-1157
title: >-
  SAT keeps regulating on a frozen OT-bus room temperature after the thermostat
  disappears
status: Done
assignee:
  - '@claude'
created_date: '2026-09-23 18:14'
updated_date: '2026-09-23 20:20'
labels:
  - sat
  - safety
  - bug
dependencies: []
priority: high
ordinal: 289000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTcurrentSystemState.Tr (MsgID 24) is written on decode and never expires. When the thermostat goes quiet, satGetRoomTemp() keeps returning the last Tr forever, so SAT regulates on a frozen value and the invalid-input skip -> safety trip chain never fires. Observed 2026-09-23 on the OTGW32 bench: after a fixture replay ended, SAT ran 10+ min on room=23.4 from the replayed Tr. Found during TASK-1150 validation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Tr that has not been refreshed by a thermostat-sourced frame within a bounded window is treated exactly like never-seen Tr (NaN) by satGetRoomTemp()
- [x] #2 Gateway-originated MsgID 24 traffic (master scheduler, replayed request frames) cannot keep a stale Tr alive
- [x] #3 Bench A/B: with the thermostat source gone, SAT trips within the existing skip window on the fix build and does not on the previous build
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Design: state.otBus.iTrThermostatMs is stamped only when the THERMOSTAT writes MsgID 24 (live T-frame Write-Data in processOT, or the PS=1 summary Tr). satGetRoomTemp() treats Tr as NaN once that stamp exists and is older than SAT_STALE_TEMP_MS (5 min), which hands the loop to the existing skip -> safety-trip chain. Gateway-originated MsgID 24 (master scheduler re-sending its write cache, TR= overrides) cannot restamp it; a Tr that never came from a thermostat keeps its previous behaviour. PS=1 is covered by stamping in updatePSSummaryFloatState. Bench A/B on OTGW32 (gateway mode, coverage fixture replay): pre-fix alpha.371 07:40-07:50, SAT kept regulating on room=23.4 for 10+ min after the replay ended. alpha.375: replayed 'Thermostat T90181766 24 Write-Data > Tr = 23.40' at 21:35:45, replay stopped, SAT enabled; room=23.4 at Tr age 41..290 s, room=null from 324 s, SAFETY TRIPPED at 588 s. Test-method note: the device-side /otgw_simulation.log had been replaced by a smaller log without MsgID 24; the coverage fixture was re-uploaded for this run.

Hardware path check: OTDirect feeds physical thermostat frames to processOT as 'T' lines (bridgeFrameToParser('T', ...) at OTDirect.ino:1231 and :1974), so the stamp is set on real OTGW32 hardware too, not only by the replay.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT no longer regulates forever on the last room temperature of a thermostat that went quiet: once the thermostat has reported Tr, a reading it has not refreshed within 5 minutes counts as unknown and the existing safety-trip chain takes over. Only thermostat-sourced MsgID 24 refreshes it. Bench A/B on the OTGW32: trip at 588 s after the last thermostat Tr, versus none in 10+ minutes before.
<!-- SECTION:FINAL_SUMMARY:END -->
