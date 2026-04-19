---
id: TASK-309
title: >-
  [TEST-M2] Make fix_satcycles.py idempotent or delete it (one-shot migration
  done)
status: To Do
assignee: []
created_date: '2026-04-18 19:21'
labels:
  - testing
  - review-2026-04-18
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
scripts/fix_satcycles.py change #4 has no already-applied guard; second run duplicates ~30 lines causing redefinition errors. Migration is already applied in tree.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Either add early-exit guard at the top (assert satFlushCycleWindow not in content)
- [ ] #2 Or delete the script and document one-shot migration in backlog
- [ ] #3 No CI job or dev workflow depends on the script silently
<!-- AC:END -->
