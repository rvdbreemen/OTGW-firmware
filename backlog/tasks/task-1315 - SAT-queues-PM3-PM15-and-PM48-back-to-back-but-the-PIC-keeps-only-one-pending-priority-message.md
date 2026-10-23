---
id: TASK-1315
title: >-
  SAT queues PM=3, PM=15 and PM=48 back to back but the PIC keeps only one
  pending priority message
status: To Do
assignee: []
created_date: '2026-10-23T09:37:00Z'
labels:
  - pic
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 438000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Side finding of the TASK-1314 research (2026-10-23): src/OTGW-firmware/SATcontrol.ino ~3765-3767 queues PM=3, PM=15 and PM=48 in a row. In the PIC gateway firmware 6.x a single prioritymsgid is stored and each PM= overwrites the pending one until a matching response arrives (other-projects/otgw-6.6 gateway.asm SetPrioMessage ~4983-4996, cleared at ~2430-2434); the priority message only goes out in a slot the thermostat gives up. Unless the ESP queue spaces these commands past a served slot, only the last PM= is likely to take effect. Check the ESP queue pacing and the bus (capture), then either space them (send the next PM= after the previous ID's response) or drop the ones that are not needed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Bench or capture evidence of which of MsgID 3/15/48 are actually requested after the three PM= commands (old)
- [ ] #2 All needed IDs are requested (spacing on response) with evidence, or the unneeded PM= removed with the reason recorded
<!-- AC:END -->
