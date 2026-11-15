---
id: TASK-1903
title: >-
  RAM-only installation knowledge picks up replayed or loopback frames: MsgID 3
  slave config, boiler outside-temperature freshness, OT-Direct boiler cache
status: Done
assignee:
  - '@claude'
created_date: '2026-11-13T16:24:00Z'
updated_date: '2026-11-15T02:23:00Z'
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
- [x] #1 Host test per item: a replayed or loopback frame does not set bSlaveCfgValid / iToutsideBoilerMs; red on the current code, green on the fix
- [x] #2 Leaving loopback clears the OT-Direct boiler cache (test or bench evidence)
- [x] #3 Real boiler frames still set all three (regression checks)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 (comms): item 2 (iToutsideBoilerMs has no bLocalAnswer check) is taken along in build 225 by lane 95, as a separate commit refs TASK-1903. Facts: otFrameIsFreshOutsideTemp() (TASK-1896, OTGW-Core.ino:5038-5044 in 484d129d4 / lane-95 1b9ef660c) excluded skipthis, bReplayed and A frames but not bLocalAnswer, so an OT-Direct loopback B Read-Ack 27 (bLocalAnswer set at OTGW-Core.ino:5892) stamped the outside temperature as fresh. Pre-existing: the old inline block on origin/dev (OTGW-Core.ino:6139-6140) had the same gap. Fix: add f.bLocalAnswer to the first return, plus a case in tests/test_outside_stamp_gateway_frames.py and test/host/test_replay_frame_guards.cpp. The OT-Direct own R Write-Data 27 (OT=) is a bus frame (bLocalAnswer 0) and keeps stamping (entry 330). Side effect: loopback bench runs no longer get a valid outside temperature from the simulated boiler MsgID 27.

2026-11-14 (92, lane wt-92-webui): item 1 done together with TASK-1857 (agreed with d3, who does not touch OTGW-Core.ino for 1903): print_slavememberid() now skips satNoteSlaveConfig() for bLocalAnswer/bReplayed. New host test test/host/test_slave_config_evidence.py slices the REAL print_slavememberid() from OTGW-Core.ino (MSVC). --rev HEAD (e74bb6ed0, before the fix): FAIL 6 = an OT-Direct loopback B Read-Ack MsgID 3 (bLocalAnswer) and a replayed one (bReplayed) each call satNoteSlaveConfig() once, satDetectManufacturer() once and set iDetectedHeatingSource 2 (heat pump from HB bit 2). Fix (boilerEvidence = !bLocalAnswer && !bReplayed in front of all three): 9/9 PASS, a real boiler Read-Ack still sets all three. Evidence: comms scratchpad t1857/old.txt, new.txt. Items 2 and 3 open; d3: his TASK-1807 commit clears the OT-Direct boiler cache on mode switches (likely item 3), he checks it after this push.

Closing check (d3, 2026-11-15, on origin/dev 2f4818aa1). Item 1: 92's bdaadc5f9 (TASK-1857); test/host/test_slave_config_evidence.py PASS now (a real boiler Read-Ack still calls satDetectManufacturer once and sets iDetectedHeatingSource 2; loopback/replayed do not), FAIL 6 on the code before it per 92's note. Item 2: otFrameIsFreshOutsideTemp() rejects f.bLocalAnswer (OTGW-Core.ino:5107 on origin/dev); tests/test_outside_stamp_gateway_frames.py 10/10 OK incl. test_local_answer_boiler_read_ack_does_not_stamp and the regression test_boiler_read_ack_stamps. Item 3: TASK-1807 (beta.278) resetTransientState() clears otBoilerCache/Valid and the MsgID 3 marker on any switch into or out of loopback; tests/test_otd_mode_change_keeps_state.py cases loopback and loopback->gateway red on the old code, green now; regression: a gateway<->monitor switch keeps it and host suite K5 (a genuine READ-ACK(3) drives presence) OK; bench 09 on .61 beta.281: after loopback via GW=S, GW=1, GW=R+GW=S sim_available stays true (PASS).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
All three loopback/replay leaks closed: item 1 by TASK-1857 (92), item 2 by lane 95 (otFrameIsFreshOutsideTemp + bLocalAnswer), item 3 by TASK-1807 (cache cleared on leaving loopback). Evidence per item in the notes: host tests red/green, real-boiler regression cases, bench 09 beta.281 item 8.
<!-- SECTION:FINAL_SUMMARY:END -->
