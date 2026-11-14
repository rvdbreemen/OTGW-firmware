---
id: TASK-1902
title: >-
  A replayed PIC banner must not set the persisted board mode
  (settings.iBoardMode)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T16:23:00Z'
updated_date: '2026-11-14T22:07:00Z'
labels:
  - otgw
  - simulation
dependencies: []
priority: low
ordinal: 970000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in d3's read-only analysis for TASK-1895 (workflow wf_a2aa95ff-0f6, trace + independent verifier); line numbers at origin/dev 1c7757419. The banner branch (OTGW-Core.ino:6250-6270) sets settings.iBoardMode (:6269) when the board mode is auto (0), no PIC is available and OT-Direct is off, and it does not look at whether the line was replayed. A replayed OTGW banner on such a unit can therefore persist a PIC board mode. The banner is not a frame line, so OTdata.bReplayed is not valid there: the gate must use processOT's replayed parameter. satNotifyPicRestart() on a replayed banner is deliberate (bench scenario SB15, comment at :6253) and stays.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: a replayed banner line leaves settings.iBoardMode at 0; red on the current code, green on the fix
- [x] #2 A real banner still sets it, and satNotifyPicRestart() still runs for a replayed banner (regression checks)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (92, lane wt-92-webui). Fix: the PIC-recovery block after the processOT() banner branch gets && !replayed (processOT's own parameter; a banner is not a frame, so OTdata.bReplayed is not valid there); satNotifyPicRestart() untouched. test/host/test_replay_frame_guards.py slices the REAL processOT() blocks from OTGW-Core.ino (banner, PIC-recovery block after the banner, the T MsgID 16 feed) and compiles them with MSVC. --rev HEAD (5ac0a2f52, before the fix): FAIL 4 = replayed banner -> iBoardMode 1, state.pic.bAvailable 1, writeSettings 1; replayed T Write-Data 16 -> satOnThermostatSetpoint called once; every regression check PASS (real banner sets iBoardMode 1 + bAvailable + one write; satNotifyPicRestart still runs for a replayed banner on a PIC board; real T Write-Data 16 reaches SAT once with 20.50). Working tree (fix): 20/20 PASS. Evidence: comms scratchpad t1901/old.txt, new.txt.
<!-- SECTION:NOTES:END -->
