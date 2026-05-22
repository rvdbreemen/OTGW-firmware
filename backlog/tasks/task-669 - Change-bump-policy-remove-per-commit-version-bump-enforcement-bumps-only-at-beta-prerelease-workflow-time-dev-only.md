---
id: TASK-669
title: >-
  Change bump policy: remove per-commit version bump enforcement; bumps only at
  beta-prerelease workflow time (dev only)
status: To Do
assignee: []
created_date: '2026-05-22 07:06'
labels:
  - ci
  - policy
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Current dev policy: every firmware-touching commit must bump _VERSION_PRERELEASE via bin/bump-prerelease.sh, enforced by .githooks/pre-commit. User instruction: bumps should happen ONLY when running the beta-prerelease workflow (via the /beta-prerelease skill which already calls bump-prerelease.sh). Remove the per-commit enforcement. Keeps 2.0.0 branch policy unchanged.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 CLAUDE.md Versioning policy section rewritten: bumps happen at release-prep time only, not per commit
- [ ] #2 .githooks/pre-commit bump-check block removed (both _VERSION_PRERELEASE and the data/version.hash companion check from TASK-660)
- [ ] #3 .githooks/README.md updated to reflect that pre-commit no longer runs the bump-check
- [ ] #4 .claude/skills/beta-prerelease/SKILL.md confirmed to call bin/bump-prerelease.sh at the right phase (no change needed if already does)
- [ ] #5 OTGW_BUMP_HOOK_DISABLE env-var documentation removed from CLAUDE.md (no longer needed)
- [ ] #6 Change applies to dev worktree ONLY — 2.0.0 worktree retains its own bump policy unchanged
<!-- AC:END -->
