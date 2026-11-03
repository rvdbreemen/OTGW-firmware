---
id: TASK-1581
title: >-
  SAT: send MM=T at the maximum PWM duty (range 5) as the threshold to
  continuous mode (entry 242)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-03T06:08:00Z'
updated_date: '2026-11-03T06:53:00Z'
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
- [x] #1 Q-1412.9c answer recorded in sergeant-d-corrections.md
- [ ] #2 Host test on the real SATcontrol.ino: entering range 5 sends MM=T once (on change), unchanged it repeats once a minute, leaving range 5 sends what Q-1412.9c decided; red on the old code
- [ ] #3 SATTICK/log shows the MM value sent; docs/sat/firmware-sat-spec.md updated
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03: Q-1412.9c answered A (entry 243): below the maximum duty SAT sends its own MM= again (on change, entry 237). Unblocked.

2026-11-03: agent stopped before editing on an uncovered interaction: Immergas TP=11:12=min(MM,80) precedes every MM= (entry 233, SATcontrol.ino:678-694) but MM=T has no number. Queued Q-1581a for Sergeant D (A: TP=11:12=80 before MM=T, advice). HP/hybrid: MM=T applies to every source at range 5, independent of the PWM phase (plain reading of entry 242). Design ready: range-5 flag in satPwmSchedule (duty >= dutyMax branch), satSendMM target T|number, SATTICK shows mm=T.

2026-11-03: Q-1581a answered A (entry 246): on an Immergas TP=11:12=80 right before MM=T at the maximum duty; below it TP and MM back to the low-load values. Unblocked.
<!-- SECTION:NOTES:END -->
