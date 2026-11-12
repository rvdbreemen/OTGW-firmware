---
id: TASK-1855
title: >-
  OT-Direct: a write command (CS=, C2=, MM=, SW=, SH=, CC=/CL=, VS=) puts its
  WRITE on the bus twice
status: Done
assignee:
  - '@claude'
created_date: '2026-11-12T02:35:00Z'
updated_date: '2026-11-12T09:06:00Z'
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
- [x] #3 Bench: VS=15 and CS=50 each give one R frame in a capture
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented (lane-1846, local; release window, no push). OTDirect.ino otdNoteCmdSent(): a one-shot WRITE from the command ring that carries the value of a write schedule entry sets that entry's lastSentMs; called where scheduleMasterRequest() (master/loopback) and otdAltSlotTaken() (gateway alternative slot) consume a ring frame after a successful send. Master: the schedule refresh follows after its interval. Gateway: every alternative slot takes the most overdue entry (otdAltScheduleIndex, no interval wait, as the PIC cycles its alternatives), so the fix makes the next slot go to another entry instead of the same WRITE. Evidence: new tests/test_otd_write_once.py on the real ring, otCmdEnqueue, updateWriteCache, enqueueWriteCommand, scheduleMasterRequest, otdAltScheduleIndex, otdAltSlotTaken: fix 2 OK; pre-1855 OTDirect.ino 2 failures (master: 2 WRITE 71 in the first 10 s after VS=15; gateway: WRITE 71 twice in the first two slots). The refresh still comes once within 40 s. Harness upkeep: test_otd_gateway_own_status, test_otd_mode_switch_queue_reset and test_sat_no_return_startup cut the real otdNoteCmdSent() when present. Regression: 29 OT-Direct/SAT files incl. test_sat_lowload_contract (35, 1 skip, 2 expected failures) pass, test/host/test_override_reply OK, evaluate --quick 86/0. AC3 bench (VS=15 and CS=50 give one R frame each in a capture) open.

Shipped 2026-11-12: origin/dev a4b53fde9..89a37c700 (beta.209 TASK-1846, beta.210 TASK-1855, beta.211 comment), zip 2.0.0-beta.211+89a37c7 posted #alpha-testing msg 1570364042554376216 (rebuilt after the rebase onto ba's docs; src identical to the benched 211+cf7c808). Before the build: 36/36 targeted test files, test_override_reply OK, evaluate --quick 86/0. Bench by 09 on .61, 211+cf7c808: OTA app+fs, bootcount 457->459, health 200, MQTT, crashlog none, 164 settings unchanged; restored afterwards (settings diff 0, reboot, bootcount 464). Logs: 09 scratchpad b211/ (bench12-A.log, bench12.log). TASK-1855 in loopback (otdmode 4, port 25238 on, temporarily): VS=15 at 09:49:41.015 -> one 'R9047000F' at 09:49:41.433 in the first 10 s, refreshes +16.1/+15.0/+23.5/+15.1/+15.1/+15.1 s; CS=50 at 09:50:31.006 -> one 'R90013200' at 09:50:31.054, refreshes every 15.1 s (OT_WRITE_INTERVAL_MS 15000). The one 23.5 s VS interval (09:50:12.583 -> 09:50:36.062) lies around the CS=50 send. Reading (from the code, not traced line by line in the log): scheduleMasterRequest() sends one request per call, queued command frames first, then the round-robin of due entries (OTDirect.ino scheduleMasterRequest loop); the interval is a minimum, so the CS one-shot and then the new MsgID 1 write entry took turns and VS went out later. Expected, harmless (same value, never shorter than the interval); no task. Side effect noted by 09: VS=15 also stores otdventsetpoint (TASK-584, as designed); VS=0 restores it.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A write command puts exactly one WRITE on the bus, as the PIC: otdNoteCmdSent() counts a sent one-shot WRITE from the command ring as the matching write schedule entry's send, at both consumers (scheduleMasterRequest master/loopback, otdAltSlotTaken gateway). AC1/AC2: tests/test_otd_write_once.py on the real ring, write cache and both consumers: red on the old code (2 WRITEs; on the integrated pre-1855 tree too), green on the fix; three harnesses cut the new hook (and the write-once harness the TASK-1840 clock block). AC3: bench 09 on .61 211+cf7c808 loopback: VS=15 and CS=50 each one R frame in the first 10 s, refreshes after 15 s (one 23.5 s VS interval explained by the one-request-per-call round-robin, expected). Shipped beta.210 (89a37c700, zip msg 1570364042554376216).
<!-- SECTION:FINAL_SUMMARY:END -->
