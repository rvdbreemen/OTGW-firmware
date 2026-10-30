---
id: TASK-1370
title: 'SAT: send CS, MM and CH on every control update as thermo-nova (entry 44)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T21:44:00Z'
updated_date: '2026-10-30T09:13:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.3
  - TASK-1412.5
references:
  - TASK-1412
priority: medium
ordinal: 505000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 (Q13, entry 44): follow thermo-nova, send CS/MM/CH every update instead of on change + 300 s (audit #24). TC= stays on change only (entry 16). Check PIC queue load and OT-Direct write cadence. Blocks ADR-199.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 CS/MM/CH are sent on every control update (host test red/green); PIC queue and OT-Direct load checked on the bench
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-30: Sergeant D entry 146: must be in beta 1 (TASK-1504, target Sun 2026-11-01). Priority raised to high.

Robert 2026-10-30: ADR-199 decision 4 ('CH on change + every 300 s') conflicts with entry 44; resolve with a superseding ADR via adr-kit (Proposed, Robert accepts). Plan drafted (coordinator): always-send helpers for the four tick paths, satPwmSuppressionRefresh keeps the change-checked helper (entry 23), keepalive kept; open question to Sergeant D: cadence A every tick vs B extra 5 s resend.
<!-- SECTION:NOTES:END -->
