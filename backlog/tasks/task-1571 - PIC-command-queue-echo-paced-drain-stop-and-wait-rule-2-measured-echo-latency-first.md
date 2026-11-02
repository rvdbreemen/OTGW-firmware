---
id: TASK-1571
title: >-
  PIC command queue: echo-paced drain (stop-and-wait, rule 2), measured echo
  latency first
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T21:06:00Z'
updated_date: '2026-11-02T21:38:00Z'
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
- [ ] #2 One command outstanding: host test on the real queue code shows the next command is sent only after the echo (+guard) or after the 1000 ms timeout; old code (1 s tick) red on the latency check
- [ ] #3 A 20-deep queue drains in a few seconds instead of ~20 s and a CS= behind it goes out within 2 s (host test)
- [ ] #4 Echo matched to the outstanding command, error codes too (NG/SE/BV/OR/NS/NF/OE); OE retried, not dropped (rule 4)
- [ ] #5 Bench .63: burst of 20 REST commands, no OE, every command echoed once, no reboot, settings kept
<!-- AC:END -->
