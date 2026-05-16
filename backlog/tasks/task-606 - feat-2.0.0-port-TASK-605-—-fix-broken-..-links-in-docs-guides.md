---
id: TASK-606
title: 'feat-2.0.0: port TASK-605 — fix broken ../ links in docs/guides'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-16 07:10'
updated_date: '2026-05-16 07:20'
labels:
  - documentation
dependencies:
  - TASK-605
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port Finding 4 only from the dev documentation review to the 2.0.0 feature line. Findings 1,2,3,5 do not apply on 2.0.0 (no dev/stable README banner, no DOCUMENTATION_LINKS_POLICY.md, no docs/releases/README.md, no link-check workflow block, no EN/NL guide split). Only the broken relative links in docs/guides/BUILD.md and docs/guides/browser-debug-console.md exist there.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All ../ links in 2.0.0 docs/guides/BUILD.md resolve to real targets (../../README.md, ../../Makefile, ../../scripts/autoinc-semver.py)
- [x] #2 All ../ links in 2.0.0 docs/guides/browser-debug-console.md resolve (../../README.md anchors; .ino links point at real src/OTGW-firmware/ paths)
- [x] #3 Link targets verified to exist on the 2.0.0 branch before editing (no assumed parity with dev)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add worktree off origin/feature-dev-2.0.0-...
2. Verify exact lines/targets in 2.0.0 BUILD.md + browser-debug-console.md
3. Fix ../ -> ../../ (and .ino -> ../../src/OTGW-firmware/)
4. Run link checker on 2.0.0 guides -> 0 broken; commit+push; draft PR into 2.0.0
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported dev review Finding 4 to the 2.0.0 line (docs-only).

Fixed broken ../ relative links in docs/guides/BUILD.md and docs/guides/browser-debug-console.md (../ -> ../../; .ino links -> ../../src/OTGW-firmware/). Targets verified to exist on the 2.0.0 branch (no assumed parity). Findings 1/2/3/5 do not exist on 2.0.0 (different README, no policy doc/releases index, no link-check workflow block, no EN/NL guides).

Landed via GitHub API (local commit signing is bound to the primary checkout and 400s in a linked worktree). Commit 509d3d7 on branch claude/review-documentation-2.0.0-8L1cy; draft PR #582 into feature-dev-2.0.0-otgw32-esp32-sat-support.

CI note: #582 red checks (evaluate.py --quick, Spec-driven OT v4.2 audit, pio run -e esp8266) are PRE-EXISTING on the 2.0.0 alpha base 201b301b (verified: base evaluate.py --quick already Failed:1, Health 95.6%). A 14-line markdown link change cannot affect firmware compilation or the OT spec audit — not regressions from this task.
<!-- SECTION:FINAL_SUMMARY:END -->
