---
id: TASK-669
title: >-
  Change bump policy: remove per-commit version bump enforcement; bumps only at
  beta-prerelease workflow time (dev only)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-22 07:06'
updated_date: '2026-05-22 07:08'
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
- [x] #1 CLAUDE.md Versioning policy section rewritten: bumps happen at release-prep time only, not per commit
- [x] #2 .githooks/pre-commit bump-check block removed (both _VERSION_PRERELEASE and the data/version.hash companion check from TASK-660)
- [x] #3 .githooks/README.md updated to reflect that pre-commit no longer runs the bump-check
- [x] #4 .claude/skills/beta-prerelease/SKILL.md confirmed to call bin/bump-prerelease.sh at the right phase (no change needed if already does)
- [x] #5 OTGW_BUMP_HOOK_DISABLE env-var documentation removed from CLAUDE.md (no longer needed)
- [x] #6 Change applies to dev worktree ONLY — 2.0.0 worktree retains its own bump policy unchanged
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Rewrite CLAUDE.md 'Versioning policy' section: drop the per-commit bump requirement and the enforcement / bypass subsections. Replace with: bumps happen only at /beta-prerelease invocation, which runs bin/bump-prerelease.sh as part of its phase 2.
2. Edit .githooks/pre-commit: remove the entire bump-check block (lines ~50-100 after the adr-kit gate). Keep only the adr-kit gate.
3. Edit .githooks/README.md: remove the pre-commit "bump-prerelease check" subsection; note that bumps happen via the /beta-prerelease skill.
4. Self-test: a firmware commit that does NOT change version.h should pass pre-commit without the bypass env-var.
5. Commit + push to a fresh branch off origin/dev; open draft PR.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed per-commit bump enforcement on dev. Commit 392ccc2f. (a) .githooks/pre-commit: bump-check block removed (TASK-560 + the data/version.hash companion from TASK-660). adr-kit gate at the top unchanged. (b) CLAUDE.md 'Versioning policy' rewritten: bumps happen only as Phase 2 of /beta-prerelease (which calls bin/bump-prerelease.sh). Field-tester A/B traceability is preserved by release cadence, not per-commit. (c) .githooks/README.md updated with a one-paragraph note + pointer to CLAUDE.md. (d) Skill .claude/skills/beta-prerelease/SKILL.md already calls bin/bump-prerelease.sh at the right phase — no change needed. (e) OTGW_BUMP_HOOK_DISABLE env-var no longer needed; removed from CLAUDE.md. (f) 2.0.0 worktree intentionally NOT changed — its per-commit bump-check remains active per user instruction (branch-local policy). Self-tested: firmware-touching commit without version.h change now passes pre-commit; only the commit-msg TASK-NNN-ref hook (TASK-659) remains as an unrelated guard.
<!-- SECTION:FINAL_SUMMARY:END -->
