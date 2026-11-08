---
id: TASK-1572
title: >-
  PIC command queue: PM= and TP= hold the priority-message slot until the boiler
  answers (rule 3)
status: Done
assignee:
  - '@system32-78'
created_date: '2026-11-02T21:10:00Z'
updated_date: '2026-11-08T18:35:00Z'
labels:
  - pic
  - command-queue
milestone: 2.0.0
dependencies:
  - TASK-1571
priority: high
ordinal: 720000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-02, queue design rule 3. The PIC has one priority-message slot (prioritymsgid, otgw-6.6 gateway.asm SetPrioMessage ~4984, SlaveParameter ~4454); the request goes to the boiler only in an alternative slot (SendPriorityMsg ~3436, CreateRequest ~2396-2408) and the slot frees only when the boiler answers that ID (HandleResponse ~2432). Manual: 'If a new command requiring an available slot is executed before the previous one completed, the new command will cancel the older command' (otgw.tclcode.com/firmware.html). The PIC echo of PM=/TP= is only a receipt. Today only SAT's boot chain (pmChain, OTGW-Core.ino) waits for the boiler; user PM=/TP= from REST/MQTT and the Immergas TP=11:12 (TASK-1537/1570) only get the TASK-1567 drip. Change: a PM=/TP= stays in flight after its echo until a boiler frame with its DataID (TP: the TSP/FHB MsgID 11/13/89/91/106/108) or a 60 s timeout; no other PM=/TP= goes out meanwhile; other commands keep draining. Generalises pmChain (keep its contract: SAT's PM=3/15/48 still asked once each, missed ones re-asked once).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real queue code: PM=3 then PM=15 queued; PM=15 is not sent before a boiler response for ID 3 (or the 60 s timeout); old code sends PM=15 one tick later (red)
- [x] #2 TP=11:12=80 and a user PM= do not cancel each other: the second waits for the boiler answer of the first (host test)
- [x] #3 Other commands (CS=, MM=) keep draining while a PM=/TP= waits (host test)
- [x] #4 pmChain behaviour kept: tests/test_pic_pm_chain.py green
- [x] #5 Bench .63 (PIC, no boiler): three REST PM= go out one after another; each waits for the 60 s priority-slot timeout before the next is sent; none is cancelled (telnet log)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-02 handover to the lead (lane-q2 ../wt-q2, no bump, series 537fd5a63..HEAD, one commit per task). Host: tests/test_pic_pm_chain.py 28/28 at the top; each task's new tests red on the code before it (see commit/test names), green at its own commit; evaluate --quick 0 failed. Open: bench on .63 after the 08:00 release (lead tags, builds, benches each task), firmware compile check of the top running at handover.

2026-11-08 AC5 reworded, approved by Robert (relayed by the coordinator): the old AC5 (three REST PM= each get their boiler answer on .63 with SAT sim) cannot pass on .63, which has no boiler (health: boilerconnected false, otgwconnected false) and whose simulation is a frame replay that does not answer PM=. The new AC5 checks the 60 s priority-slot timeout path on .63; the boiler-answer path stays covered by the host tests of AC1-AC2. Taken by system32-78; bench during 54's .63 time tonight.

AC5 bench 2026-11-08 on .63 (Classic + S3 Mini, PIC 6.8, no boiler, beta.147+d63a128) by system32-78, with Robert's OK for the temporary setting change: the first run (19:16) was INVALID because .63 had satsimulation=true, and sendPICSerial() then holds back every PIC command (satSimulationBlocksBusTx, TASK-1580; the queue consumes it). So the old AC5 'with SAT sim' could never have passed: no 'Sending to Serial' in 3.5 min, only MsgID 0 R-frames. Valid run: settings saved, satsimulation=false, three REST PM= at 19:31:03 (PM=3, PM=15, PM=48, each 202 queued). Telnet: Sending to Serial [PM=3] 19:31:04.054 + Confirmed; [PM=15] 19:32:04.056 (+60.0 s, the priority-slot timeout) + Confirmed; [PM=48] 19:33:04.057 (+60.0 s) + Confirmed; 0 dropped/deleted lines. Then satsimulation=true, settings diff vs the saved copy: 150 keys, no difference; bootcount 176 unchanged. Logs: scratchpad t1572/63-pm-bench.log (invalid run) and 63-pm-bench2.log.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
PM= and TP= hold the PIC's priority-message slot until the boiler answers their DataID or the 60 s timeout (commit 9aa8b6c69, beta.22). AC1-AC4 by host tests on the real queue code (pic_pm_chain 28/28 at handover; red on the code before). AC5 reworded with Robert's OK (2026-11-08) because .63 has no boiler: on .63 with SAT simulation off, three REST PM= went out 60.0 s apart (19:31:04.054, 19:32:04.056, 19:33:04.057), each confirmed by the PIC echo, none cancelled or dropped; the boiler-answer path stays host-tested. Note: with satsimulation=true the TASK-1580 gate holds back all PIC commands, so a bench of the PIC queue needs SAT simulation off.
<!-- SECTION:FINAL_SUMMARY:END -->
