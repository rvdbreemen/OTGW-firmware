---
id: TASK-1581
title: >-
  SAT: send MM=T at the maximum PWM duty (range 5) as the threshold to
  continuous mode (entry 242)
status: To Do
assignee: []
created_date: '2026-11-03T06:08:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 727000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 242 (2026-11-03, msg 1567055574548353047): when PWM is at the maximum cycle length (duty range 5) SAT sends MM=T instead of its own MM= value, as the step into continuous mode. Part 1 of the same answer (no dip every 2 periods in range 5) matches the current firmware. Our reading: MM=T follows the entry 237/239 sending rules (on change, once a minute). Waits on Q-1412.9c for the way back below range 5.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Q-1412.9c answer recorded in sergeant-d-corrections.md
- [ ] #2 Host test on the real SATcontrol.ino: entering range 5 sends MM=T once (on change), unchanged it repeats once a minute, leaving range 5 sends what Q-1412.9c decided; red on the old code
- [ ] #3 SATTICK/log shows the MM value sent; docs/sat/firmware-sat-spec.md updated
<!-- AC:END -->
