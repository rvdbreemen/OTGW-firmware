---
id: TASK-1071
title: >-
  Make the OT frame-replay simulator work on OTDirect boards, not only the PIC
  path
status: To Do
assignee: []
created_date: '2026-08-08 18:17'
updated_date: '2026-08-08 19:13'
labels:
  - bug
  - tooling
  - otdirect
dependencies:
  - TASK-1073
priority: high
ordinal: 258000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The /api/v2/simulate file replay is bound to the PIC serial path and is silently inert on any OTDirect board, which today means it cannot be used for testing at all on the OTGW32 hardware. Evidence, gathered 2026-08-08 on the bench S3 (otgw.local, 192.168.88.52, MAC 10:20:BA:21:B4:F8, alpha.354+a7e06f8, otdirectavailable=true, no PIC): the fixture uploaded correctly (readable at /otgw_simulation.log, 4230 bytes), POST /api/v2/simulate/start returned active:true, and a 400s telnet capture then contained 3997 lines of satSimulatio OTGW-SIM traces and ZERO processOT lines. Not one fixture frame was decoded. Mechanism: on 1.x the replay pump is handleOTGWSimulation, called from the main loop (OTGW-Core.ino:4586). On 2.0.0 the equivalent is handlePICSerialSimulation, which lives in the PIC serial task, and picSerialTaskShouldPark() (OTGW-Core.ino:733) returns true whenever isOTDirectEnabled(), so on an OTDirect board that task parks permanently and the replay never runs. The endpoint still flips state.debug.bOTGWSimulation and reports active, so the failure is silent and looks like success. Consequence: the TASK-1070 coverage gate cannot be exercised on OTDirect hardware, and the TASK-1068/1069 ports remain build-and-inspection-verified only. Fix direction: feed replayed frames into the OTDirect RX path (the same entry point real OTDirect frames take) rather than into the PIC UART drain, so replay is transport-independent.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With simulation enabled on an OTDirect board and no PIC attached, replayed fixture lines reach processOT and appear as decoded OT frames in the debug log
- [ ] #2 The replay drives the same decode, state and MQTT publish path as real frames, so the coverage gate produces the same shape of output as on 1.x
- [ ] #3 Replay still works unchanged on a board that does have a PIC; the PIC path is not regressed
- [ ] #4 /api/v2/simulate reports a state that reflects reality: enabling it on a board where replay cannot run must not report active, or must report why
- [ ] #5 The TASK-1070 coverage gate runs end to end against an OTDirect board: upload, start, capture, stop, compare
- [ ] #6 Build green for the relevant esp32 targets and python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-08 feasibility analysis (code read on dev @ a7e06f8). Verdict: possible, and cheaper than the task description assumes, but the description names the wrong blocker.

CORRECTION to the mechanism in the description. handlePICSerialSimulation does NOT live in the PIC serial task and picSerialTaskShouldPark() is not what kills it. The pump is called from handlePICSerial() (OTGW-Core.ino:5079), which is loop-side: the comment at 5058-5065 states the UART drain moved into the dedicated task and handlePICSerial() keeps only the non-UART loop work, the replay among it. picSerialTaskShouldPark() returning true under isOTDirectEnabled() (line 733) is deliberate and correct, it stops the task draining the UART underneath the replay.

The two ACTUAL blockers:
1. OTGW-Core.ino:5069, 'if (isOTDirectEnabled()) return;' sits ABOVE the simulation call at 5079. On a combo in OT-Direct mode the pump is never reached. Runtime gate, same binary simulates fine in PIC mode.
2. The whole handlePICSerial() body is inside '#if HAS_PIC'. On a PIC-less OTGW32 build it is compiled out entirely, so there is nothing to reach.

ARCHITECTURAL FINDING that resizes this task. There are not two decode paths. There is one decoder with two producers. Both enqueueOTFrame() callers converge on processOT() in drainOTFrameQueue (OTGW-Core.ino:510). The replay enqueues with OTFRAME_SRC_PIC (3659); OTDirect enqueues with OTFRAME_SRC_OTDIRECT from bridgeFrameToParser (OTDirect.ino:717) after formatting the 32-bit frame as PSTR("%c%08lX") = the SAME 9-char text the fixture holds. msg.source only gates the LED blink and the port-25238 mirror (499-506). So replaying the fixture with source=OTDIRECT would exercise byte-identical decode and publish logic: near-tautological, it proves nothing the PIC-path gate does not already prove.

Three layers, only one of which is worth building:
- Below the queue (processOT onward): already covered by the TASK-1070 gate. Zero new coverage.
- bridgeFrameToParser and up: 32-bit frame word -> snprintf format -> otHideReports/PS=1 suppression -> source-tag side-effect gating. Genuinely untested today. Needs an injection point above bridgeFrameToParser and a fixture of frame WORDS, not text lines.
- PHY/ISR/parity/frame assembly: not simulatable in software at all.

Fixture portability checked: otgw_simulation_coverage.log is 423 lines, 100% matching ^[TBRAE][0-9A-F]{8}$, zero PIC-only banner or PS= lines, so no filtered variant is needed. Prefix histogram B=189 T=163 R=43 A=27 E=1. OTDirect emits only T/R/B/A (bridgeFrameToParser call sites), never E, so exactly one fixture line (421: E10000000) is unreachable on an OTDirect board.

SEPARATE DEFECT, split out: setOTGWSimulationEnabled() (restAPI.ino:345) sets state.debug.bOTGWSimulation with no mode or capability check, and sendSimulationStatus() (333) reports active straight from that flag. On an OTDirect board the API reports active:true while nothing is ever replayed. Silent no-op plus a lying status. Fixable in a few lines independently of the replay work; covers AC #4 on its own.
<!-- SECTION:NOTES:END -->
