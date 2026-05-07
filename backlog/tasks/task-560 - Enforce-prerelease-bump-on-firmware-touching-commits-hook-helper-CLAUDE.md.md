---
id: TASK-560
title: >-
  Enforce prerelease bump on firmware-touching commits (hook + helper +
  CLAUDE.md)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 12:18'
updated_date: '2026-05-07 12:22'
labels:
  - hooks
  - tooling
  - release
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field-test users on Discord identify issues by version string ("on beta.23 I see..."), so each material firmware change must ship under its own prerelease tag. Today nothing enforces this — the bumps batch implicitly per beta-cycle. Two recent commits (TASK-558 refactor + the prior discovery flip) both shipped as beta.23 with no way for testers to A/B them.

Adds three pieces:
1. Pre-commit hook (`.githooks/pre-commit`) extension: detect staged firmware paths and require a `_VERSION_PRERELEASE` change in the same commit. Composes with the existing adr-kit hook.
2. Helper `bin/bump-prerelease.sh`: thin wrapper around `scripts/autoinc-semver.py --prerelease ...` that parses the current tag, increments the trailing integer, and rewrites version.h + data/version.hash. Does NOT git-add — caller stages.
3. CLAUDE.md "## Versioning policy" section documenting what triggers a bump, what does not, how to bump, and the bypass env var.

This is one of two paired tasks. The 2.0.0 worktree carries the sibling task.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 bin/bump-prerelease.sh exists, is executable, parses _VERSION_PRERELEASE matching ^[a-zA-Z]+\.[0-9]+$, increments the trailing integer, calls scripts/autoinc-semver.py with --prerelease, and prints old→new to stdout. Refuses with a clear error on non-matching tags or when not run from project root.
- [ ] #2 .githooks/pre-commit extends the existing adr-kit hook: adr-judge runs first (gated by ADR_KIT_HOOK_DISABLE), then a bump-check runs (gated by OTGW_BUMP_HOOK_DISABLE). Either gate may block; both must pass for the commit to proceed.
- [ ] #3 Bump-check trigger: any staged path under src/OTGW-firmware/ (excluding src/OTGW-firmware/version.h) or src/libraries/. If triggered, requires that git diff --cached for src/OTGW-firmware/version.h shows both a + and a - line containing _VERSION_PRERELEASE.
- [ ] #4 Bump-check non-trigger paths (no bump required): *.md, docs/**, backlog/**, .claude/**, scripts/**, bin/**, .githooks/**, top-level .py/.sh/.bat. Verified by smoke test.
- [ ] #5 Five smoke tests pass on the dev worktree: (1) bin/bump-prerelease.sh parses+increments+reverts cleanly; (2) hook BLOCKS a firmware-only synthetic commit; (3) hook PASSES a docs-only commit; (4) hook PASSES a firmware+bump commit; (5) ./build.sh exits 0. All synthetic state cleaned up after testing.
- [ ] #6 CLAUDE.md gains a '## Versioning policy' section covering: what requires a bump, what does not, how to bump (bin/bump-prerelease.sh), and the OTGW_BUMP_HOOK_DISABLE bypass env var.
- [ ] #7 Commit message is 'chore(hooks): enforce prerelease bump on firmware-touching commits' or similar; commit touches only .githooks/, bin/, CLAUDE.md (and is therefore exempt from the new bump check itself).
- [ ] #8 After build green, the commit is pushed to origin/dev per CLAUDE.md push policy.
<!-- AC:END -->
