---
id: TASK-1567
title: >-
  PIC command queue: send at most one command per 1 s tick, so the PIC never
  gets commands faster than it processes them
status: Done
assignee:
  - '@claude'
created_date: '2026-11-02T20:37:00Z'
updated_date: '2026-11-04T10:14:00Z'
labels:
  - pic
  - command-queue
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 717000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-02: never send queue messages too fast; after one message always wait for the next tick. Today handleCommandQueue() (OTGW-Core.ino, called 1x/s from doTaskEvery1s) sends every due slot in one burst; only PM=/TP= are spaced 1 s (TASK-1315). The PIC (otgw-6.6 gateway.asm) has one 16-byte rxbuffer and handles one command per CR; bytes arriving while a command waits are dropped (SerialReceive 'Not ready to accept a command now', line ~1466) and a busy processor answers OE (otgw.tclcode.com/firmware.html). Change: handleCommandQueue sends at most one due command per call, then returns; the next due command goes out on the next 1 s tick. The PM=/TP= spacing becomes implied and is folded in. Not in scope: holding PM=/TP= until the boiler answers (separate task).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 handleCommandQueue sends at most one command per call; host test on the real queue code: three due commands are sent on three successive ticks (old code: all three in one tick, red)
- [x] #2 PM=/TP= ordering and TASK-1315/1555 behaviour kept: tests/test_pic_pm_chain.py and the queue tests green (adapted only where they assumed a same-tick burst)
- [x] #3 SAT keeps its 5 s CS/MM/CH resend and 30 s PIC keepalive within the slower drain: host check that a full SAT resend set (CS, MM, CH) drains within 5 s
- [x] #4 Bench .63 (PIC): telnet shows 'Sending to Serial' lines at least ~1 s apart under a burst of 5 REST commands; settings kept, no reboot
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. handleCommandQueue(): after one sendPICSerial() return (one command per call; the loop calls it 1x/s). The PM=/TP= 1 s spacing is then implied and its static state removed. 2. Keep everything else (retry 5 x 5 s, dedup, eviction, ser2net quiet, pmChainTick). 3. Tests: extend tests/test_pic_pm_chain.py harness (real OTGW-Core.ino queue code) with a burst scenario: three different commands due in one tick -> sent on three successive ticks; red on origin/dev (all three in tick 1). Adapt existing scenarios only if they assumed a same-tick burst. 4. Full tests folder + evaluate --quick. 5. Commit without bump, hand to the lead for the next build tag; bench .63: 5 REST commands, telnet 'Sending to Serial' ~1 s apart.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-02 evidence (537fd5a63): tests/test_pic_pm_chain.py test_queue_drips_one_command_per_tick: origin/dev FAILED ([0,0,0] ms at 1000 and 250 ms loops), fix OK (CS/MM/CH sent 1 s apart, drained within 5 s); new test_cs_behind_a_full_queue_still_well_within_the_pic_minute: my first lowest-slot drip FAILED (CS never sent in 60 s: retrying slots took every tick), the oldest-due-first drain sends it after 19000 ms. tests/test_pic_pm_chain.py 13/13; full tests folder 95/95 OK; evaluate --quick 0 failed. Boot PR= readout is self-paced (3 s), PIC detection and version do not use the queue, so no boot timeout depends on the drip. AC4 (bench .63) by the lead after the 08:00 release, as beta.18.

2026-11-04 bench .63 beta.68+38426d5 (SAT on, simulation off for the run, restored): 130 s SAT traffic + a 5-command PR burst + two concurrent REST clients x 15 commands; every distinct command enqueued was sent and confirmed (sends == confirms per command, e.g. PR=M 5/5, PR=A/B/S/V 3/3 each, CS/CH/MM 1/1), cmdq_sent 20 == cmdq_echo_count 20, echo_timeouts 0, cmd_dropped 0, depth 0 at the end, bootcount 106 unchanged, settings unchanged. Enqueues above sends are the replace rule (TASK-1571). Transcript: scratchpad queue-bench-b68.txt. Sends are echo-paced (0.07-0.1 s apart: each waits for the PIC echo + 50 ms guard), not ~1 s: the 1 s literal of AC4 was superseded by TASK-1571 (echo-paced stop-and-wait, Robert's queue rules 2026-11-03), which keeps the AC's intent (never send before the previous command is answered).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The PIC queue sends one command at a time; since TASK-1571 the next one waits for the PIC echo of the previous (not a fixed 1 s). Evidence: tests/test_pic_pm_chain.py (40 OK), bench .63 beta.68 burst + concurrent clients: 20 sent = 20 echoed, 0 dropped, no reboot, settings kept.
<!-- SECTION:FINAL_SUMMARY:END -->
