---
id: TASK-1384
title: >-
  SAT: send the boot PM/MI commands also when SAT is enabled at runtime (audit
  #44)
status: To Do
assignee: []
created_date: '2026-10-25T10:39:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
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
