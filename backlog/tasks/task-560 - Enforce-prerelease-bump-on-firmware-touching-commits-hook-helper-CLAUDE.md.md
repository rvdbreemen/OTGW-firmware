---
id: TASK-560
title: >-
  Enforce prerelease bump on firmware-touching commits (hook + helper +
  CLAUDE.md)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 12:18'
updated_date: '2026-05-07 12:35'
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
- [x] #1 bin/bump-prerelease.sh exists, is executable, parses _VERSION_PRERELEASE matching ^[a-zA-Z]+\.[0-9]+$, increments the trailing integer, calls scripts/autoinc-semver.py with --prerelease, and prints old→new to stdout. Refuses with a clear error on non-matching tags or when not run from project root.
- [x] #2 .githooks/pre-commit extends the existing adr-kit hook: adr-judge runs first (gated by ADR_KIT_HOOK_DISABLE), then a bump-check runs (gated by OTGW_BUMP_HOOK_DISABLE). Either gate may block; both must pass for the commit to proceed.
- [x] #3 Bump-check trigger: any staged path under src/OTGW-firmware/ (excluding src/OTGW-firmware/version.h) or src/libraries/. If triggered, requires that git diff --cached for src/OTGW-firmware/version.h shows both a + and a - line containing _VERSION_PRERELEASE.
- [x] #4 Bump-check non-trigger paths (no bump required): *.md, docs/**, backlog/**, .claude/**, scripts/**, bin/**, .githooks/**, top-level .py/.sh/.bat. Verified by smoke test.
- [x] #5 Five smoke tests pass on the dev worktree: (1) bin/bump-prerelease.sh parses+increments+reverts cleanly; (2) hook BLOCKS a firmware-only synthetic commit; (3) hook PASSES a docs-only commit; (4) hook PASSES a firmware+bump commit; (5) ./build.sh exits 0. All synthetic state cleaned up after testing.
- [x] #6 CLAUDE.md gains a '## Versioning policy' section covering: what requires a bump, what does not, how to bump (bin/bump-prerelease.sh), and the OTGW_BUMP_HOOK_DISABLE bypass env var.
- [x] #7 Commit message is 'chore(hooks): enforce prerelease bump on firmware-touching commits' or similar; commit touches only .githooks/, bin/, CLAUDE.md (and is therefore exempt from the new bump check itself).
- [x] #8 After build green, the commit is pushed to origin/dev per CLAUDE.md push policy.
<!-- AC:END -->



## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verify pre-flight (hooksPath, version.h shape, autoinc script, build.sh, bin/ absent).
2. Create bin/ + bin/bump-prerelease.sh; chmod +x.
3. Extend .githooks/pre-commit with the bump-check stanza after the existing adr-judge logic.
4. Append "## Versioning policy" to CLAUDE.md near "## Git push policy".
5. Run five smoke tests in order, cleaning up between each:
   - T1: bin/bump-prerelease.sh prints old→new and updates files; revert.
   - T2: hook BLOCKS firmware-only synthetic commit.
   - T3: hook PASSES docs-only synthetic commit (use throwaway .md, NOT real CLAUDE.md edit).
   - T4: hook PASSES firmware+bump synthetic commit.
   - T5: ./build.sh exits 0; revert version.h+hash drift.
6. Stage explicit paths only (.githooks/pre-commit, bin/bump-prerelease.sh, CLAUDE.md).
7. Commit with chore(hooks): ... message and Co-Authored-By trailer.
8. Push to origin/dev per push policy.
9. Wrap up TASK-560: append-notes, check ACs 1-8, final-summary, status Done.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented:
- bin/bump-prerelease.sh (executable; parses ^[a-zA-Z]+\.[0-9]+$, increments trailing N, calls scripts/autoinc-semver.py --prerelease).
- .githooks/pre-commit extended with bump-check stanza after the adr-judge gate; gated by OTGW_BUMP_HOOK_DISABLE=1; preserves adr-kit hook behaviour exactly.
- CLAUDE.md "## Versioning policy" section appended near "## Git push policy".

Five smoke tests, all PASS:
T1: ./bin/bump-prerelease.sh printed "beta.23 → beta.24" and updated version.h + data/version.hash + cascaded source/asset files. Reverted clean.
T2: hook BLOCKED a synthetic firmware-only commit (touched MQTTstuff.ino, no bump) — exit 1 with [bump] error. Reset clean.
T3: hook PASSED a synthetic docs-only commit (docs/SMOKE-T3.md). Used throwaway path to keep real CLAUDE.md edit intact. Reset clean.
T4: hook PASSED a synthetic firmware+bump commit (MQTTstuff.ino + version.h + data/version.hash). Reset clean.
T5: ./build.sh exit 0; firmware + filesystem built (OTGW-firmware-1.5.0-beta.23+6616c85.ino.bin + .littlefs.bin). Build-time autoinc drift reverted clean.

Commit 932be9d6 chore(hooks): enforce prerelease bump on firmware-touching commits — three files (.githooks/pre-commit, bin/bump-prerelease.sh, CLAUDE.md), no firmware paths so the new bump-check did not fire on its own commit. Pushed to origin/dev (c0e5bb5e..932be9d6).
<!-- SECTION:NOTES:END -->
