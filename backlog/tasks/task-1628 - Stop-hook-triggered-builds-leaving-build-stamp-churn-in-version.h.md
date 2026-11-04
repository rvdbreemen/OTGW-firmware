---
id: TASK-1628
title: Stop hook-triggered builds leaving build-stamp churn in version.h
status: To Do
assignee: []
created_date: '2026-11-04T22:41:00Z'
labels:
  - process
  - agentic
dependencies: []
priority: medium
ordinal: 755000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Observed 2026-11-04 in a cloud session: the SessionStart toolchain hook (.claude/hooks/session-start-build-toolchain.sh -> build.py) rewrites _VERSION_BUILD and _VERSION_GITHASH in src/OTGW-firmware/version.h and data/version.hash. The Stop hook then reports uncommitted changes and asks to commit them. CLAUDE.md says build-stamp churn must be restored, not committed, so the agent reverts it every turn.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After a session start in a clean clone, git status is clean (the stamp is restored by the hook or not written), or the Stop hook ignores a stamp-only diff
- [ ] #2 A real _VERSION_PRERELEASE change is still reported by the Stop hook
- [ ] #3 Behaviour shown before and after in a cloud session
<!-- AC:END -->
