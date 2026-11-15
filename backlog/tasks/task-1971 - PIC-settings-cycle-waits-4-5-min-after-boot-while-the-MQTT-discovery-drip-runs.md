---
id: TASK-1971
title: PIC settings cycle waits 4-5 min after boot while the MQTT discovery drip runs
status: To Do
assignee: []
created_date: '2026-11-15T11:48:00Z'
labels:
  - pic
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1008000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by 09 on .63 (beta.306+041400e, 2026-11-15): right after a reboot the PIC panel stays empty (0/15 after 80 s). queryNextPICsetting() skips while dripDueWithinMs(500) (OTGW-Core.ino ~1300) and the MQTT discovery drip ticks every 1 s, so the PR= cycle barely gets a slot until the drip is done, about 4-5 min after boot.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After a boot the PIC settings fill within about 60 s also while the discovery drip runs
- [ ] #2 The ADR-088 status-burst window and cooldown stay intact (evaluate gates green); host test or trace on the real scheduling shows the cycle advancing during the drip, red on the old code
- [ ] #3 Bench: reboot .63, /api/v2/pic/settings 15/15 within 60 s
<!-- AC:END -->
