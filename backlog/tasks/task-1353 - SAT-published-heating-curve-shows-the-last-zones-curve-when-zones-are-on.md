---
id: TASK-1353
title: 'SAT: published heating curve shows the last zone''s curve when zones are on'
status: To Do
assignee: []
created_date: '2026-10-24T16:28:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 488000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md) finding #66: SATcontrol.ino:598, 1996 publish the last zone's curve, misleading curve tuning. Display only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Published curve is the main (non-zone) curve with zones on (host test red/green)
<!-- AC:END -->
