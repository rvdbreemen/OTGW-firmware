---
id: TASK-1992
title: >-
  OT-Direct: writes queued during the SAT simulation reach the real boiler after
  it ends, also values cancelled in between
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-16T00:19:00Z'
updated_date: '2026-11-16T01:42:00Z'
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
- [ ] #3 Bench 09, the b331 sequence: no R frame with a cancelled value after satsimulation=false
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Verified in code (origin a2bfce174 + lane-1804b): (1) otCmdEnqueue() (OTDirect.ino) coalesces only a NEW frame into a pending entry of the same MsgID; clearWriteOverride(id) (CS=0 -> 1, MM=T -> 14, SW=0 -> 56, and TT=0 in master mode through clearRemoteOverride() -> 16 and 100) clears the override and the write cache but leaves a pending ring entry, so the old value still goes out. (2) satSimulationBlocksBusTx() (SATcontrol.ino ~:2976) makes sendMasterRequestAsync() return false while settings.sat.bSimulation or state.debug.bOTGWSimulation is on; scheduleMasterRequest() then keeps the ring head, the ring keeps filling and coalescing, and drains to the real bus when the block ends. Fix: (a) clearWriteOverride(id) marks pending WRITE entries of that MsgID as a tombstone (0xFFFFFFFF) that the consumer drops without sending; no compaction, because otdFlushCmdQueue() from another task snapshots otCmdHead and a moved head would break it; (b) loopOTDirect() keeps the previous simulation-block state and calls otdFlushCmdQueue() when it goes from blocked to free, so nothing queued during the simulation reaches the real boiler (also covers SAT left on with the simulation turned off: its simulated CS would otherwise go out). Tests first: a host harness on the real otCmdEnqueue/consumer/clearWriteOverride + the transition, red on origin (stale CS 61.3, MM 100, TT 20.5 frames after the simulation), green on the fix. Not SAT control behaviour (the simulation is a firmware test feature); recorded as such.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-16 d3: fix on lane-1992 (c9857d1b6, WIP until the gates). tests/test_otd_sim_exit_stale_writes.py on the real ring + otCmdEnqueue + scheduleMasterRequest + otdFlushCmdQueue + clearWriteOverride + otdSimExitFlush, with sendMasterRequestAsync refusing while settings.sat.bSimulation (as satSimulationBlocksBusTx): old code (origin e2688b437) 3 FAIL: the b331 sequence sends 10101480, 10013D4D, 100E6400 (09's three stale frames), a write queued under the simulation goes out after it, a cleared CS still goes out without a simulation; fix 4/4 (the after-simulation write goes out once on both). Harness stubs (otCmdDropPendingWrite / otCmdSkipTombstones / otdSimExitFlush) in test_otd_cool_ch2_defaults, test_otd_gateway_alt_slots (also fixes test_otd_ps1_summary_pic), test_otd_gateway_own_status, test_otd_rt_room_temp, test_otd_vs_msgid71, test_sat_no_return_startup, test/host/test_override_reply.cpp. Gates on lane-1992: test_otd_* all green (three parts), override_reply 41/41 + test_override_reply.py 4 OK, evaluate 88/0, ll018 10 passed, test_evaluate 80 OK. Still open before push: full test_sat_lowload_contract alone; build + bench (AC3, 09 re-runs the b331 sequence).
<!-- SECTION:NOTES:END -->
