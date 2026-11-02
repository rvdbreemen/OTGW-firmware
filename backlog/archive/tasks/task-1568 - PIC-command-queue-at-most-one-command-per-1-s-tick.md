---
id: TASK-1568
title: 'PIC command queue: at most one command per 1 s tick'
status: To Do
assignee: []
created_date: '2026-11-02T20:39:00Z'
labels:
  - pic
  - command-queue
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 718000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-02: never send queue messages too fast; after one message always wait for the next tick. handleCommandQueue() (OTGW-Core.ino, 1x/s from doTaskEvery1s) sends every due slot in one burst today; only PM=/TP= are spaced (TASK-1315). The PIC (otgw-6.6 gateway.asm) has one 16-byte rxbuffer, handles one command per CR, drops bytes while a command waits (SerialReceive) and answers OE when busy (otgw.tclcode.com/firmware.html). Change: send at most one due command per call. Phase 1 of the queue redesign (see design notes).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 handleCommandQueue sends at most one command per call; host test on the real queue code: three due commands go out on three successive ticks (old code: one tick, red)
- [ ] #2 tests/test_pic_pm_chain.py and the other queue tests stay green, adapted only where they assumed a same-tick burst
- [ ] #3 SAT resend set (CS, MM, CH) drains within its 5 s resend period in a host check
- [ ] #4 Bench .63: under a burst of 5 REST commands the telnet 'Sending to Serial' lines are about 1 s apart; settings kept, no reboot
<!-- AC:END -->
