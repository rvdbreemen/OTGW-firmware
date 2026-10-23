---
id: TASK-1315
title: >-
  SAT queues PM=3, PM=15 and PM=48 back to back but the PIC keeps only one
  pending priority message
status: To Do
assignee: []
created_date: '2026-10-23T09:37:00Z'
updated_date: '2026-10-23T13:14:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 cycle: gruijter's 1 h capture (20261023-094059.zip, alpha.461, SAT active) contains no PM= command at all, so it cannot show the effect; needs a PIC board on the bench (Classic-S3 not connected) or a capture from an enable/refresh moment that issues the PM= burst. Skipped for now.

2026-10-23 data point (TASK-1314 investigation): on a real PIC 6.8 (1.x unit, port 25238) three PR= reads sent back to back in one TCP write were all answered (3/3 runs). So the PIC does not drop back-to-back PR= commands; whether PM= behaves differently (it queues a request for a later OT slot) is still open.
<!-- SECTION:NOTES:END -->
