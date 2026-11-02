---
id: TASK-1567
title: >-
  PIC command queue: send at most one command per 1 s tick, so the PIC never
  gets commands faster than it processes them
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T20:37:00Z'
updated_date: '2026-11-02T20:41:00Z'
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
- [ ] #1 handleCommandQueue sends at most one command per call; host test on the real queue code: three due commands are sent on three successive ticks (old code: all three in one tick, red)
- [ ] #2 PM=/TP= ordering and TASK-1315/1555 behaviour kept: tests/test_pic_pm_chain.py and the queue tests green (adapted only where they assumed a same-tick burst)
- [ ] #3 SAT keeps its 5 s CS/MM/CH resend and 30 s PIC keepalive within the slower drain: host check that a full SAT resend set (CS, MM, CH) drains within 5 s
- [ ] #4 Bench .63 (PIC): telnet shows 'Sending to Serial' lines at least ~1 s apart under a burst of 5 REST commands; settings kept, no reboot
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. handleCommandQueue(): after one sendPICSerial() return (one command per call; the loop calls it 1x/s). The PM=/TP= 1 s spacing is then implied and its static state removed. 2. Keep everything else (retry 5 x 5 s, dedup, eviction, ser2net quiet, pmChainTick). 3. Tests: extend tests/test_pic_pm_chain.py harness (real OTGW-Core.ino queue code) with a burst scenario: three different commands due in one tick -> sent on three successive ticks; red on origin/dev (all three in tick 1). Adapt existing scenarios only if they assumed a same-tick burst. 4. Full tests folder + evaluate --quick. 5. Commit without bump, hand to the lead for the next build tag; bench .63: 5 REST commands, telnet 'Sending to Serial' ~1 s apart.
<!-- SECTION:PLAN:END -->
