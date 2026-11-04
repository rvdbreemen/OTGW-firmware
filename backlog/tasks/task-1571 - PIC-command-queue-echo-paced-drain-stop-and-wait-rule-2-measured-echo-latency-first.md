---
id: TASK-1571
title: >-
  PIC command queue: echo-paced drain (stop-and-wait, rule 2), measured echo
  latency first
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T21:06:00Z'
updated_date: '2026-11-04T05:10:00Z'
labels:
  - pic
  - command-queue
milestone: 2.0.0
dependencies:
  - TASK-1567
priority: high
ordinal: 719000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-02 (queue design rule 2 + timing analysis). Today (TASK-1567) the queue drips one command per 1 s tick, oldest-due first. The PIC is ready for the next command when its echo or error code arrives: SerialCommand runs before PrintString (otgw-6.6 gateway.asm ~1424-1431); while a command is processed CommandComplete is set and SerialReceive drops new bytes (~1466), also during EEPROM writes (EepromWait ~1679). otmonitor (sercmd, 2000 ms timeout) and pyotgw (issue_cmd under a lock) both wait for the response. Design: one command outstanding; the next goes out after the echo or error of the outstanding one plus a guard (proposed 50 ms), or after a 1000 ms timeout (counts as a failed attempt); never two within the guard. handleCommandQueue runs every loop pass, decisions stay time/echo based. The echo then belongs to the one outstanding command (no 2-letter matching ambiguity). Calculated echo latency ~20 ms (9600 baud, 4 MHz PIC), ~30 ms with an EEPROM write: NOT measured yet. Depends on TASK-1567.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Bench measurement first: 100+ commands on .63, micros() at send and at the matching echo, distribution (min/p50/p99/max) recorded; the guard is set to p99 plus margin (expected ~50 ms) and the value with its evidence goes in the notes
- [x] #2 One command outstanding: host test on the real queue code shows the next command is sent only after the echo (+guard) or after the 1000 ms timeout; old code (1 s tick) red on the latency check
- [x] #3 A 20-deep queue drains in a few seconds instead of ~20 s and a CS= behind it goes out within 2 s (host test)
- [x] #4 Echo matched to the outstanding command, error codes too (NG/SE/BV/OR/NS/NF/OE); OE retried, not dropped (rule 4)
- [ ] #5 Bench .63: burst of 20 REST commands, no OE, every command echoed once, no reboot, settings kept
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-02 handover to the lead (lane-q2 ../wt-q2, no bump, series 537fd5a63..HEAD, one commit per task). Host: tests/test_pic_pm_chain.py 28/28 at the top; each task's new tests red on the code before it (see commit/test names), green at its own commit; evaluate --quick 0 failed. Open: bench on .63 after the 08:00 release (lead tags, builds, benches each task), firmware compile check of the top running at handover.

2026-11-04 ~00:15 bench .63 (beta.60+1c34c39, satsimulation temporarily OFF, restored ON after): with simulation ON every command is consumed without sending (TASK-1580 path: sendPICSerial false), so cmdq_* stay 0; measurements need simulation off. Echo latency 9 samples: min 15, avg 25, max 44 ms, hist [0,6,3,0,0,0,0,0], 0 echo timeouts (QUEUE_ECHO_GUARD_MS 50 is above the observed max). FINDING: throughput far below the echo pacing: 110 x PR=A back-to-back -> 3 sends; 120 x PR=A at 200 ms spacing -> 5 sends; burst of 20 PR=x -> 1 send; no reboot (boot 96), settings kept, cmd_dropped only the ZZ=1 NG. Coalescing of identical pending commands explains part; ~1 send per 3-4 s does not follow from the 50 ms gaps alone. AC1 (100+ samples) not reached yet; analysis lane started.

2026-11-04 Robert decisions on the queue analysis: (1) on a replace keep the earliest due (min(old due, now+delay), wrap-safe) so a fast repeater is not starved; (2) PR= unique per register (add PR to the per-value unique list). Implement with host tests, one tag.
<!-- SECTION:NOTES:END -->
