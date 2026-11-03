---
id: TASK-1579
title: >-
  SAT pressure health: residual false alarm on the pressure step when the pump
  stops (thermo-nova parity leaves 2 per 3.9 h)
status: To Do
assignee: []
created_date: '2026-11-03T04:03:00Z'
updated_date: '2026-11-03T12:09:00Z'
labels:
  - sat
dependencies: []
priority: low
ordinal: 725000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1511 (maintainer decision 2026-11-03: close 1511 on thermo-nova parity). Real capture 20261030-092718 (gruijter, alpha.549): current code 2 alarm episodes in 3.91 h, the same as the thermo-nova oracle with the CH-mode bit (entry 201). The residual comes from the pressure step when the pump stops, which thermo-nova does not filter either. Any filter deviates from thermo-nova, so it waits for Sergeant D (Q-1511a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's answer to Q-1511a is recorded in sergeant-d-corrections.md
- [ ] #2 If he chooses a filter: the capture replay (OTGW_PRESSURE_CAPTURE) gives 0 episodes and the slow-leak test still alarms
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 maintainer: Q-1511a not asked (entry 201), task parked at low priority.
<!-- SECTION:NOTES:END -->
