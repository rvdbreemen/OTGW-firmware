---
id: TASK-1380
title: >-
  SAT PWM: time the OFF phase from PWM's own ON->OFF switch, not from the last
  boiler flame-off (audit #59)
status: To Do
assignee: []
created_date: '2026-10-25T10:14:00Z'
updated_date: '2026-10-27T23:45:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.3
  - TASK-1382
references:
  - TASK-1412
ordinal: 519000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
thermo-nova times the OFF phase from its own ON->OFF transition (pwm.py:208); hot-water and relight flame edges do not move it. Firmware reads satCycleGetFlameOffStartMs() (SATcontrol.ino:974 at HEAD 143609173), so a hot-water flame-off re-anchors the OFF timer. Evidence: gruijter alpha.500 run 2026-10-25 (msg 1563846779928055839), OFF periods ending at tap/start boundaries. Changes how SAT controls the boiler: plan quotes pwm.py and waits for Sergeant D's OK; scope together with Q35 (ON anchor). Blocked on Q35.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OFF phase timed from PWM's own ON->OFF switch; a DHW flame edge during OFF does not move it (host test red on HEAD, green on fix)
- [ ] #2 Sergeant D's OK recorded (Q35 or a follow-up) before implementation
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.
<!-- SECTION:NOTES:END -->
