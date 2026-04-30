---
id: TASK-480
title: Promote design-system drift gate from WARN to FAIL after grace release
status: To Do
assignee: []
created_date: '2026-04-28 21:46'
updated_date: '2026-04-30 00:37'
labels:
  - tooling design-system ci-gate
dependencies: []
ordinal: 10000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up for TASK-470 and ADR-091. The first release of evaluate.py check_design_system_drift reports actionable class drift as WARN to absorb false positives. After one release cycle with no accepted false-positive reports, promote the check to FAIL so CI blocks new design-system class drift.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 check_design_system_drift uses FAIL severity for actionable non-allowlisted missing CSS classes
- [ ] #2 ADR-091 and any task notes are updated with the promotion date or release boundary
- [ ] #3 python evaluate.py --quick exits cleanly on the promoted gate with a green baseline
<!-- AC:END -->
