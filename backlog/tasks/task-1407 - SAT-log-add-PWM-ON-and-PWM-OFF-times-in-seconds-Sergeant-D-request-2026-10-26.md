---
id: TASK-1407
title: >-
  SAT log: add PWM ON and PWM OFF times in seconds (Sergeant D request
  2026-10-26)
status: To Do
assignee: []
created_date: '2026-10-26T18:24:00Z'
updated_date: '2026-10-27T23:46:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.3
references:
  - TASK-1412
ordinal: 545000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-26 13:35 (#alpha-testing msg 1564271354390257867): 'Also in the SAT log add PWM ON and PWM OFF times in seconds'. Logging only, no control change. Add the computed PWM on-time and off-time (seconds) for the current period to the SAT debug/narration log line where the PWM schedule is computed (SATcontrol.ino satPwmSchedule / PWM state publish), and to the v2 SAT log view if that view is fed from the same narration ring.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each PWM schedule decision logs the ON and OFF time in seconds (telnet SAT debug and the SAT event log)
- [ ] #2 Host or bench evidence of the new line; no change to what is sent to the boiler
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.
<!-- SECTION:NOTES:END -->
