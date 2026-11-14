---
id: TASK-1903
title: >-
  RAM-only installation knowledge picks up replayed or loopback frames: MsgID 3
  slave config, boiler outside-temperature freshness, OT-Direct boiler cache
status: To Do
assignee: []
created_date: '2026-11-13T16:24:00Z'
updated_date: '2026-11-14T22:15:00Z'
labels:
  - sat
  - otgw
  - otdirect
  - simulation
dependencies: []
priority: low
ordinal: 971000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in d3's read-only analysis for TASK-1895 (workflow wf_a2aa95ff-0f6, trace + independent verifier); line numbers at origin/dev 1c7757419. Cleared by a reboot, so lower impact than the persisted cases. (1) print_slavememberid calls satNoteSlaveConfig() for any OTGW_BOILER frame (OTGW-Core.ino:3179, SATcycles.ino:1302) without bLocalAnswer/bReplayed, so a replay fixture or OT-Direct loopback (both bit 4 = 0) sets bSlaveCfgValid and lets satPwmPermitted() allow PWM. (2) iToutsideBoilerMs is gated on !bReplayed but not on bLocalAnswer, so loopback MsgID 27 (8.5 C) marks the boiler outside temperature fresh. (3) otBoilerCache / otBoilerCacheValid keep loopback answers (OTDirect.ino:1761) with no clear on a mode switch found. Fix: gate (1) on !OTdata.bLocalAnswer && !OTdata.bReplayed, (2) also on !bLocalAnswer, (3) clear the cache when loopback ends; check each against the bench scenarios that rely on loopback.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test per item: a replayed or loopback frame does not set bSlaveCfgValid / iToutsideBoilerMs; red on the current code, green on the fix
- [ ] #2 Leaving loopback clears the OT-Direct boiler cache (test or bench evidence)
- [ ] #3 Real boiler frames still set all three (regression checks)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 (comms): item 2 (iToutsideBoilerMs has no bLocalAnswer check) is taken along in build 225 by lane 95, as a separate commit refs TASK-1903. Facts: otFrameIsFreshOutsideTemp() (TASK-1896, OTGW-Core.ino:5038-5044 in 484d129d4 / lane-95 1b9ef660c) excluded skipthis, bReplayed and A frames but not bLocalAnswer, so an OT-Direct loopback B Read-Ack 27 (bLocalAnswer set at OTGW-Core.ino:5892) stamped the outside temperature as fresh. Pre-existing: the old inline block on origin/dev (OTGW-Core.ino:6139-6140) had the same gap. Fix: add f.bLocalAnswer to the first return, plus a case in tests/test_outside_stamp_gateway_frames.py and test/host/test_replay_frame_guards.cpp. The OT-Direct own R Write-Data 27 (OT=) is a bus frame (bLocalAnswer 0) and keeps stamping (entry 330). Side effect: loopback bench runs no longer get a valid outside temperature from the simulated boiler MsgID 27.

2026-11-14 (92, lane wt-92-webui): item 1 done together with TASK-1857 (agreed with d3, who does not touch OTGW-Core.ino for 1903): print_slavememberid() now skips satNoteSlaveConfig() for bLocalAnswer/bReplayed. New host test test/host/test_slave_config_evidence.py slices the REAL print_slavememberid() from OTGW-Core.ino (MSVC). --rev HEAD (e74bb6ed0, before the fix): FAIL 6 = an OT-Direct loopback B Read-Ack MsgID 3 (bLocalAnswer) and a replayed one (bReplayed) each call satNoteSlaveConfig() once, satDetectManufacturer() once and set iDetectedHeatingSource 2 (heat pump from HB bit 2). Fix (boilerEvidence = !bLocalAnswer && !bReplayed in front of all three): 9/9 PASS, a real boiler Read-Ack still sets all three. Evidence: comms scratchpad t1857/old.txt, new.txt. Items 2 and 3 open; d3: his TASK-1807 commit clears the OT-Direct boiler cache on mode switches (likely item 3), he checks it after this push.
<!-- SECTION:NOTES:END -->
