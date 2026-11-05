---
id: TASK-1655
title: >-
  SAT: simulate KD -10% and -20% for the warm-up (Sergeant D proposal, entry
  278)
status: To Do
assignee: []
created_date: '2026-11-05T08:29:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 779000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-05 (entry 277/278, #dev-sat-mqtt 1567789991315120178 and 1567814060550918146): the slow warm-up comes from KD (D about -2.5 C while the room warms 0.5 C/h in our example), not from the integral, which must stay as is. Simulate KD x0.9 and x0.8 against today (kd = 0.07*8400*kp, pid.py:85-93, SATpid.ino:221) in the TASK-1412.16/1412.19 harness; bring the numbers to him (queued question Q-1412.16b) before any firmware change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Simulation baseline vs KD x0.9 vs x0.8 per plant class: time to within 0.1/0.3 C, overshoot, starts per day, comfort error; thresholds pre-registered before the run
- [ ] #2 Report posted to Sergeant D with the numbers and a recommendation; his answer recorded as a numbered entry
- [ ] #3 Only if he agrees: firmware change + host test red/green + ADR or spec update
<!-- AC:END -->
