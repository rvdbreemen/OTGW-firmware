---
id: TASK-1901
title: >-
  A replayed thermostat MsgID 16 write must not change the stored SAT target
  (SATtargettemp)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T16:22:00Z'
updated_date: '2026-11-14T22:07:00Z'
labels:
  - sat
  - otgw
  - simulation
dependencies: []
priority: medium
ordinal: 969000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in d3's read-only analysis for TASK-1895 (workflow wf_a2aa95ff-0f6, trace + independent verifier); line numbers at origin/dev 1c7757419. OTGW-Core.ino:6051-6054 feeds a T Write-Data MsgID 16 to satOnThermostatSetpoint() without a bReplayed check, and with follow-thermostat on that ends in updateSetting("SATtargettemp") (SATcontrol.ino:2283-2293), which is persisted. So an OTGW simulation replay (/simulate/start or debug otgwsim) can overwrite the user's real SAT target. Precedents: TASK-1649 (replayed MsgID 27 must not set the outside temperature), TASK-1895 (replayed T lines no longer write the thermostat bitmaps). Fix: add && !OTdata.bReplayed to that condition.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: a replayed T Write-Data MsgID 16 does not reach satOnThermostatSetpoint() (or leaves SATtargettemp unchanged); red on the current code, green on the fix
- [x] #2 A real T MsgID 16 write still reaches it (regression check)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (92, lane wt-92-webui). Fix: OTGW-Core.ino MsgID 16 feed condition gets && !OTdata.bReplayed. test/host/test_replay_frame_guards.py slices the REAL processOT() blocks from OTGW-Core.ino (banner, PIC-recovery block after the banner, the T MsgID 16 feed) and compiles them with MSVC. --rev HEAD (5ac0a2f52, before the fix): FAIL 4 = replayed banner -> iBoardMode 1, state.pic.bAvailable 1, writeSettings 1; replayed T Write-Data 16 -> satOnThermostatSetpoint called once; every regression check PASS (real banner sets iBoardMode 1 + bAvailable + one write; satNotifyPicRestart still runs for a replayed banner on a PIC board; real T Write-Data 16 reaches SAT once with 20.50). Working tree (fix): 20/20 PASS. Evidence: comms scratchpad t1901/old.txt, new.txt.
<!-- SECTION:NOTES:END -->
