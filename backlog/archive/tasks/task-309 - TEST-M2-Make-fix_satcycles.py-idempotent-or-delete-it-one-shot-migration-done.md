---
id: TASK-309
title: >-
  [TEST-M2] Make fix_satcycles.py idempotent or delete it (one-shot migration
  done)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:21'
updated_date: '2026-04-19 07:05'
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
- [x] #1 Either add early-exit guard at the top (assert satFlushCycleWindow not in content)
- [ ] #2 Or delete the script and document one-shot migration in backlog
- [x] #3 No CI job or dev workflow depends on the script silently
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
scripts/fix_satcycles.py: added idempotency guard at module top that checks for 'void satSaveCycleWindow()' or 'void satFlushCycleWindow()' in the target file. If either exists, the script exits cleanly with a clear message instead of appending the functions a second time (which previously caused redefinition errors). Verified by running the script against the post-migration tree: produces 'already applied' message and exits 0. Script kept rather than deleted to preserve the migration audit trail.
<!-- SECTION:FINAL_SUMMARY:END -->
