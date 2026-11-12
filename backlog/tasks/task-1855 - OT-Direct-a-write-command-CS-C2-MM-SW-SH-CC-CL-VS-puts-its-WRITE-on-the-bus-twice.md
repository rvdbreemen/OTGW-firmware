---
id: TASK-1855
title: >-
  OT-Direct: a write command (CS=, C2=, MM=, SW=, SH=, CC=/CL=, VS=) puts its
  WRITE on the bus twice
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T02:35:00Z'
updated_date: '2026-11-12T04:56:00Z'
labels:
  - otdirect
dependencies: []
priority: low
ordinal: 933000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
09's bench of beta.197 on .61 (loopback): VS=15 and CL=0 each put their WRITE frame on the socket twice. Cause from the code (OTDirect.ino): enqueueWriteCommand() enqueues the one-shot frame and updateWriteCache() sets valueSet on the matching W_ENTRY; a write entry that was never sent has lastSentMs 0, so scheduleMasterRequest() finds it due at once and sends the same WRITE again. Existing pattern for every write command with a write entry (1, 7, 8, 14, 16, 24, 27, 56, 57; 71 since TASK-1831). Harmless (same value) but one extra bus frame per command; the PIC sends one.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A write command puts exactly one WRITE frame on the bus; the scheduler refresh follows after its interval (e.g. set lastSentMs when the one-shot frame is enqueued, or let the schedule send the first frame)
- [x] #2 Host test on the real enqueueWriteCommand/updateWriteCache/scheduler: red on the current code, green on the fix
- [ ] #3 Bench: VS=15 and CS=50 each give one R frame in a capture
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented (lane-1846, local; release window, no push). OTDirect.ino otdNoteCmdSent(): a one-shot WRITE from the command ring that carries the value of a write schedule entry sets that entry's lastSentMs; called where scheduleMasterRequest() (master/loopback) and otdAltSlotTaken() (gateway alternative slot) consume a ring frame after a successful send. Master: the schedule refresh follows after its interval. Gateway: every alternative slot takes the most overdue entry (otdAltScheduleIndex, no interval wait, as the PIC cycles its alternatives), so the fix makes the next slot go to another entry instead of the same WRITE. Evidence: new tests/test_otd_write_once.py on the real ring, otCmdEnqueue, updateWriteCache, enqueueWriteCommand, scheduleMasterRequest, otdAltScheduleIndex, otdAltSlotTaken: fix 2 OK; pre-1855 OTDirect.ino 2 failures (master: 2 WRITE 71 in the first 10 s after VS=15; gateway: WRITE 71 twice in the first two slots). The refresh still comes once within 40 s. Harness upkeep: test_otd_gateway_own_status, test_otd_mode_switch_queue_reset and test_sat_no_return_startup cut the real otdNoteCmdSent() when present. Regression: 29 OT-Direct/SAT files incl. test_sat_lowload_contract (35, 1 skip, 2 expected failures) pass, test/host/test_override_reply OK, evaluate --quick 86/0. AC3 bench (VS=15 and CS=50 give one R frame each in a capture) open.
<!-- SECTION:NOTES:END -->
