---
id: TASK-1572
title: >-
  PIC command queue: PM= and TP= hold the priority-message slot until the boiler
  answers (rule 3)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T21:10:00Z'
updated_date: '2026-11-02T21:46:00Z'
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
- [ ] #1 Host test on the real queue code: PM=3 then PM=15 queued; PM=15 is not sent before a boiler response for ID 3 (or the 60 s timeout); old code sends PM=15 one tick later (red)
- [ ] #2 TP=11:12=80 and a user PM= do not cancel each other: the second waits for the boiler answer of the first (host test)
- [ ] #3 Other commands (CS=, MM=) keep draining while a PM=/TP= waits (host test)
- [ ] #4 pmChain behaviour kept: tests/test_pic_pm_chain.py green
- [ ] #5 Bench .63 with SAT sim: three REST PM= each get their boiler answer in the log, none cancelled
<!-- AC:END -->
