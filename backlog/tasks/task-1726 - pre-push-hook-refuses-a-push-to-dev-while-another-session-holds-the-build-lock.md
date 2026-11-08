---
id: TASK-1726
title: pre-push hook refuses a push to dev while another session holds the build lock
status: Done
assignee: []
created_date: '2026-11-08T10:06:00Z'
updated_date: '2026-11-08T10:53:00Z'
labels:
  - tooling
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 834000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2026-11-08: a backlog-only push (26f34f4fa) moved origin/dev during toegang's beta.143 build, so the zip githash no longer matched the pushed HEAD after rebase and cost a rebuild. Make the coordinator rule mechanical: .githooks/pre-push reads %LOCALAPPDATA%/OTGW-build/build.lock and refuses a push to dev when the lock is held by a live PID that is not this shell's build, with a clear message; an explicit override env var for the coordinator.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 pre-push refuses a push to origin/dev while a live foreign build lock exists, and allows it when the lock is absent or stale (dead PID)
- [ ] #2 Red/green shown with a fake lock file: refused, then allowed after removal; no effect on other remotes/branches
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Dropped by Robert 2026-11-08: the coordinator rule (no push while another session holds the build lock) suffices; a hook is more code than the problem is worth.
<!-- SECTION:FINAL_SUMMARY:END -->
