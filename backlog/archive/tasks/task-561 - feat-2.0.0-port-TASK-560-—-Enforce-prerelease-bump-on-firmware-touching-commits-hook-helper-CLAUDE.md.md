---
id: TASK-561
title: >-
  feat-2.0.0: port TASK-560 — Enforce prerelease bump on firmware-touching
  commits (hook + helper + CLAUDE.md)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-07 12:20'
updated_date: '2026-05-07 12:37'
labels:
  - hooks
  - tooling
  - release
  - 2.0.0
dependencies: []
ordinal: 24000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sibling of dev TASK-560. Same change shape, applied to the 2.0.0 ESP32+SAT branch.

Field-test users on Discord identify issues by version string ("on alpha.6 I see..."), so each material firmware change must ship under its own prerelease tag. Today nothing enforces this. Adds three pieces:
1. Pre-commit hook (.githooks/pre-commit) — created from scratch in this worktree (currently has only a commit-msg hook, no pre-commit). Hook detects staged firmware paths and requires _VERSION_PRERELEASE change in same commit. Bypass: OTGW_BUMP_HOOK_DISABLE=1.
2. Helper bin/bump-prerelease.sh — identical content to dev: parses current tag, increments trailing integer, calls scripts/autoinc-semver.py with --prerelease.
3. CLAUDE.md "## Versioning policy" section — same text as dev (the 2.0.0 CLAUDE.md is separate per the worktree-layout section but kept in sync deliberately for cross-cutting policies).

Dev sibling TASK-560 lives in the dev worktree. The 2.0.0 worktree already has a commit-msg hook (TASK-NNN guard) which must remain untouched.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 bin/bump-prerelease.sh exists, is executable, parses _VERSION_PRERELEASE matching ^[a-zA-Z]+\.[0-9]+$, increments the trailing integer, calls scripts/autoinc-semver.py with --prerelease, and prints old→new to stdout. Refuses with a clear error on non-matching tags or when not run from project root.
- [x] #2 .githooks/pre-commit is created from scratch (this worktree currently has no pre-commit hook). Implements only the bump-check (no adr-kit composition needed here — adr-kit is not currently wired into this worktree's hooks). Existing .githooks/commit-msg is untouched.
- [x] #3 Bump-check trigger: any staged path under src/OTGW-firmware/ (excluding src/OTGW-firmware/version.h) or src/libraries/. If triggered, requires git diff --cached for src/OTGW-firmware/version.h to show both + and - lines containing _VERSION_PRERELEASE. Bypass: OTGW_BUMP_HOOK_DISABLE=1.
- [x] #4 Bump-check non-trigger paths (no bump required): *.md, docs/**, backlog/**, .claude/**, scripts/**, bin/**, .githooks/**, top-level .py/.sh/.bat. Verified by smoke test.
- [x] #5 Five smoke tests pass on the 2.0.0 worktree: (1) bin/bump-prerelease.sh parses+increments+reverts cleanly; (2) hook BLOCKS a firmware-only synthetic commit; (3) hook PASSES a docs-only commit; (4) hook PASSES a firmware+bump commit; (5) ./build.sh exits 0. All synthetic state cleaned up.
- [x] #6 CLAUDE.md gains a '## Versioning policy' section covering: what requires a bump, what does not, how to bump (bin/bump-prerelease.sh), and the OTGW_BUMP_HOOK_DISABLE bypass env var. Same text as dev sibling (kept in sync deliberately).
- [x] #7 Commit message references both the dev sibling and notes that this worktree gets a fresh pre-commit hook (vs dev's extension of an existing one). Touches only .githooks/, bin/, CLAUDE.md (and is therefore exempt from the new bump check itself).
- [x] #8 After build green, the commit is pushed to origin/feature-dev-2.0.0-otgw32-esp32-sat-support per CLAUDE.md push policy.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verify pre-flight: branch, hooksPath=.githooks, .githooks/commit-msg present, .githooks/pre-commit and bin/ absent, scripts/autoinc-semver.py available with --prerelease.
2. Create bin/bump-prerelease.sh (POSIX-safe bash, identical to dev sibling), chmod +x.
3. Create .githooks/pre-commit from scratch with only the bump-check (no adr-kit composition). Leave existing commit-msg untouched. chmod +x.
4. Append "## Versioning policy" section to CLAUDE.md (same text as dev sibling).
5. Smoke test 1: bin/bump-prerelease.sh increments alpha.6 -> alpha.7, version.h + data/version.hash updated; revert.
6. Smoke test 2: stage a trivial firmware-only edit, attempt commit, expect BLOCK; reset.
7. Smoke test 3: stage docs-only edit, commit succeeds, soft-reset and revert.
8. Smoke test 4: stage firmware edit + run bump + stage version.h/hash, commit succeeds, soft-reset and revert.
9. Smoke test 5: ./build.sh exit 0; revert any autoinc dirty state.
10. Stage ONLY .githooks/pre-commit, bin/bump-prerelease.sh, CLAUDE.md; verify nothing under src/.
11. Commit with the documented message (closes 561, sibling 560).
12. Push to origin/feature-dev-2.0.0-otgw32-esp32-sat-support.
13. Wrap up TASK-561: append-notes, check-ac 1..8, final-summary, status Done.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Created .githooks/pre-commit from scratch (bump-check only; no adr-kit composition).
- Created bin/bump-prerelease.sh; chmod +x; verified alpha.6 -> alpha.7 round-trip.
- Appended "## Versioning policy" to CLAUDE.md before "## Worktree layout".
- Smoke tests:
  - T1 (helper): alpha.6 -> alpha.7 printed, version.h + data/version.hash updated by autoinc-semver.py (also touched 42 other source files for consistency); reverted cleanly.
  - T2 (firmware-only commit): BLOCKED with [bump] error citing MQTTstuff.ino. EXIT=1.
  - T3 (docs-only): docs/guides/BUILD.md committed, EXIT=0; soft-reset and reverted.
  - T4 (firmware+bump): MQTTstuff.ino + version.h + data/version.hash staged together, committed, EXIT=0; soft-reset and reverted full src/.
  - T5 (./build.sh): completed successfully, produced ESP32 + ESP8266 artifacts; reverted version.h+hash drift from build-time autoinc.
- Note: existing .githooks/commit-msg is non-executable in this worktree (pre-existing condition, not introduced by this task; left untouched per instructions).
- Commit 17cf05e5 pushed to origin/feature-dev-2.0.0-otgw32-esp32-sat-support.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Enforced prerelease-bump policy on firmware-touching commits in the 2.0.0 worktree, mirroring dev TASK-560.

This worktree previously had no pre-commit hook at all (only a commit-msg TASK-NNN guard), so the hook is created from scratch rather than extending an existing one. No adr-kit composition is needed here since adr-kit is not currently wired into this worktree's hooks.

Changes:
- .githooks/pre-commit (NEW, +x): Detects staged paths under src/OTGW-firmware/ (excluding version.h) or src/libraries/. If triggered, requires git diff --cached for src/OTGW-firmware/version.h to show both + and - lines containing _VERSION_PRERELEASE. Bypass via OTGW_BUMP_HOOK_DISABLE=1.
- bin/bump-prerelease.sh (NEW, +x): Parses _VERSION_PRERELEASE matching ^[a-zA-Z]+\.[0-9]+$, increments the trailing integer, and delegates the rewrite to scripts/autoinc-semver.py --prerelease. Prints "old → new". Refuses with a clear error on non-matching tags or when run outside the project root.
- CLAUDE.md gains a "## Versioning policy" section (placed before "## Worktree layout") documenting trigger paths, non-trigger paths, the bump command, the bypass env var, and the motivation (Discord field-test users identifying builds by tag).
- The existing .githooks/commit-msg hook (TASK-NNN guard) is intentionally untouched.

User impact:
- Committers who change firmware code (.ino/.h/.cpp under src/OTGW-firmware/, or src/libraries/) without bumping _VERSION_PRERELEASE in the same commit are blocked with a clear remediation message.
- Documentation/tooling-only commits remain unaffected.

Tests run on this worktree:
- T1 helper: bin/bump-prerelease.sh prints "alpha.6 → alpha.7" and updates version.h + data/version.hash (and 42 other source files for consistency); reverted cleanly with git checkout -- src/OTGW-firmware/.
- T2 firmware-only commit BLOCKED (exit 1) with the expected [bump] error citing MQTTstuff.ino.
- T3 docs-only commit (docs/guides/BUILD.md) PASSES (exit 0).
- T4 firmware+bump commit (MQTTstuff.ino + version.h + data/version.hash staged together) PASSES (exit 0).
- T5 ./build.sh completed successfully, producing ESP32 and ESP8266 artifacts; build-time autoinc dirty on version.h+hash reverted.

Risks/follow-ups:
- The existing commit-msg hook in this worktree is not marked executable (git surfaces a warning on each commit). This is a pre-existing condition, NOT introduced by this task, and was explicitly left untouched per the task brief. Worth tracking separately if the maintainer wants commit-msg's TASK-NNN guard to be active here.

Sibling: dev TASK-560. Pushed: commit 17cf05e5 → origin/feature-dev-2.0.0-otgw32-esp32-sat-support.
<!-- SECTION:FINAL_SUMMARY:END -->
