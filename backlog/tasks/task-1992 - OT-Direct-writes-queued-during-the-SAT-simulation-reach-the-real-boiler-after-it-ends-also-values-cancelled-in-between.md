---
id: TASK-1992
title: >-
  OT-Direct: writes queued during the SAT simulation reach the real boiler after
  it ends, also values cancelled in between
status: Done
assignee:
  - '@claude'
created_date: '2026-11-16T00:19:00Z'
updated_date: '2026-11-16T02:58:00Z'
labels:
  - otdirect
  - bench-finding
  - bug
dependencies: []
priority: high
ordinal: 1015000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench finding 09, .61 OT-Direct master, beta.331+e4dbf70, 2026-11-16 (raw: 09 scratchpad b331/telnet.raw, b331/ttq/d3_lines.txt). With satsimulation=true no OT frames go out, but the command ring keeps accepting writes; at satsimulation=false (01:03:08) it flushed R90013D4D (CS 61.30) after CS=0, R900E6400 (MM 100) after MM=T and R90101480 (TT 20.5) after TT=0, 6 min old. Without the simulation TT=20.5 goes out in 1.2 s. To verify in code: clears do not remove a pending ring entry of the same MsgID; the ring keeps entries across the simulation's bus pause. Risk: stale CS 61.3 to a real boiler after a user tries the simulation. Scope (coordinator): drop pending writes on simulation exit; a clear or later write removes/replaces the pending entry.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real ring/command code, red on origin: writes queued while the simulation blocks TX and then cleared give no stale frame after the simulation ends
- [x] #2 A clear command removes a pending ring entry of the same MsgID also without the simulation
- [x] #3 Bench 09, the b331 sequence: no R frame with a cancelled value after satsimulation=false
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Verified in code (origin a2bfce174 + lane-1804b): (1) otCmdEnqueue() (OTDirect.ino) coalesces only a NEW frame into a pending entry of the same MsgID; clearWriteOverride(id) (CS=0 -> 1, MM=T -> 14, SW=0 -> 56, and TT=0 in master mode through clearRemoteOverride() -> 16 and 100) clears the override and the write cache but leaves a pending ring entry, so the old value still goes out. (2) satSimulationBlocksBusTx() (SATcontrol.ino ~:2976) makes sendMasterRequestAsync() return false while settings.sat.bSimulation or state.debug.bOTGWSimulation is on; scheduleMasterRequest() then keeps the ring head, the ring keeps filling and coalescing, and drains to the real bus when the block ends. Fix: (a) clearWriteOverride(id) marks pending WRITE entries of that MsgID as a tombstone (0xFFFFFFFF) that the consumer drops without sending; no compaction, because otdFlushCmdQueue() from another task snapshots otCmdHead and a moved head would break it; (b) loopOTDirect() keeps the previous simulation-block state and calls otdFlushCmdQueue() when it goes from blocked to free, so nothing queued during the simulation reaches the real boiler (also covers SAT left on with the simulation turned off: its simulated CS would otherwise go out). Tests first: a host harness on the real otCmdEnqueue/consumer/clearWriteOverride + the transition, red on origin (stale CS 61.3, MM 100, TT 20.5 frames after the simulation), green on the fix. Not SAT control behaviour (the simulation is a firmware test feature); recorded as such.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-16 d3: fix on lane-1992 (c9857d1b6, WIP until the gates). tests/test_otd_sim_exit_stale_writes.py on the real ring + otCmdEnqueue + scheduleMasterRequest + otdFlushCmdQueue + clearWriteOverride + otdSimExitFlush, with sendMasterRequestAsync refusing while settings.sat.bSimulation (as satSimulationBlocksBusTx): old code (origin e2688b437) 3 FAIL: the b331 sequence sends 10101480, 10013D4D, 100E6400 (09's three stale frames), a write queued under the simulation goes out after it, a cleared CS still goes out without a simulation; fix 4/4 (the after-simulation write goes out once on both). Harness stubs (otCmdDropPendingWrite / otCmdSkipTombstones / otdSimExitFlush) in test_otd_cool_ch2_defaults, test_otd_gateway_alt_slots (also fixes test_otd_ps1_summary_pic), test_otd_gateway_own_status, test_otd_rt_room_temp, test_otd_vs_msgid71, test_sat_no_return_startup, test/host/test_override_reply.cpp. Gates on lane-1992: test_otd_* all green (three parts), override_reply 41/41 + test_override_reply.py 4 OK, evaluate 88/0, ll018 10 passed, test_evaluate 80 OK. Still open before push: full test_sat_lowload_contract alone; build + bench (AC3, 09 re-runs the b331 sequence).

2026-11-16 03:2x d3: full tests/test_sat_lowload_contract.py alone on lane-1992 (85191c0b2): 34 passed, 1 skipped, 0 failed (682 s). All pre-build gates green. Next: rebase onto origin, build, bench (AC3: 09 re-runs the b331 sequence: satsimulation on, TT=20.5, CS/MM under SAT sim, TT=0, CS=0, MM=T, satsimulation off -> no R frame with a cancelled value).

2026-11-16 AC3 scope (coordinator, Robert's rule: simulation only in software, the 1979 enabler on hardware was a one-off): NO satsimulation toggle on the bench. The simulation-exit evidence is the host test tests/test_otd_sim_exit_stale_writes.py, which reproduces exactly 09's three captured b331 frames (10101480 TT 20.5, 10013D4D CS 61.30, 100E6400 MM 100) on the old code and none on the fix; 09's b331 capture is the field evidence. The bench on the stack build 2.0.0-beta.338+3e36217 checks: health, CS=/MM=/TT= go out once, clears leave 0 stale frames, a write + its clear in one burst send nothing with the cancelled value after the clear answer, settings identical. Built as a stack on lane-1804b (TASK-1804 A2, TASK-1985, TASK-1804 B3e; 09 PASS on b335) so both push together.

2026-11-16 bench 09 on .61, stack build 2.0.0-beta.338+3e36217 (evidence 09 scratchpad b338/, runner b1992/smoke1992b.py): PASS all 6 steps, no satsimulation toggle. CS=40/MM=100/TT=20.5 each out once after 1.4/1.8/2.1 s; clears leave 0 stale frames in a 5-min watch; bursts (two REST POSTs ~169 ms apart): CS=40+CS=0 -> 0 frames with 2800 before or after the clear answer, TT=20.5+TT=0 -> 0 frames with 1480; 1804b regression echoes; settings byte-identical. LIMIT, stated plainly: the burst check on hardware is green on the fix only (09 did not run it on the old code); the old-vs-fix proof lives in the host test tests/test_otd_sim_exit_stale_writes.py (old 3 FAIL with exactly 09's b331 frames 10101480/10013D4D/100E6400, fix 4/4); 09's b331 capture is the field evidence of the defect. Pushed origin/dev 1d7483fc1..3e362170f as beta.338 (fix commit f61307c38); zip posted in #alpha-testing, message 1571720879048495206.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: tests/test_otd_sim_exit_stale_writes.py on the real ring, otCmdEnqueue, scheduleMasterRequest, otdFlushCmdQueue, clearWriteOverride and otdSimExitFlush: the b331 sequence sends 09's three stale frames on origin and none on the fix; a write queued under the simulation is not sent after it. AC2: same test, case 'clear': a cleared CS no longer goes out without a simulation (tombstone in the ring). AC3: bench 09 .61 beta.338+3e36217 PASS (no simulation toggle on hardware by Robert's rule; burst check green on the fix only, old-vs-fix in the host test). Fix: clearWriteOverride() marks pending WRITEs of its MsgID as tombstones the two consumers skip (no compaction, safe with the cross-task flush snapshot); otdSimExitFlush() flushes the ring when a simulation stops blocking TX. Gates: test_otd_* 67/67, override_reply 41/41 + test_override_reply.py, test_sat_no_return_startup, full lowload 34 passed, evaluate 88/0, test_evaluate 80.
<!-- SECTION:FINAL_SUMMARY:END -->
