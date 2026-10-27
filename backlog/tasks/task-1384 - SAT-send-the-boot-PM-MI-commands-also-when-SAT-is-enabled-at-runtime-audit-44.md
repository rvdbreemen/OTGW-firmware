---
id: TASK-1384
title: >-
  SAT: send the boot PM/MI commands also when SAT is enabled at runtime (audit
  #44)
status: To Do
assignee: []
created_date: '2026-10-25T10:39:00Z'
updated_date: '2026-10-27T23:46:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.3
  - TASK-1412.5
references:
  - TASK-1412
ordinal: 523000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From TASK-1273 and sat-port-audit-2026-10-24.md #44: the PM/MI commands sent at boot are not sent when SAT is switched on later. Relates to entry 45 (MI=500 for boilers without return temperature) and TASK-1371.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Enabling SAT at runtime sends the same PM/MI set as boot (host test or telnet transcript, red on HEAD)
- [ ] #2 Sergeant D's OK recorded in docs/sat/sergeant-d-corrections.md before implementation (CLAUDE.md SAT rules)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.
<!-- SECTION:NOTES:END -->
