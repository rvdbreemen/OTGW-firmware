---
id: TASK-606
title: 'feat-2.0.0: port TASK-605 — fix broken ../ links in docs/guides'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-16 07:10'
updated_date: '2026-05-16 07:11'
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
- [ ] #1 All ../ links in 2.0.0 docs/guides/BUILD.md resolve to real targets (../../README.md, ../../Makefile, ../../scripts/autoinc-semver.py)
- [ ] #2 All ../ links in 2.0.0 docs/guides/browser-debug-console.md resolve (../../README.md anchors; .ino links point at real src/OTGW-firmware/ paths)
- [ ] #3 Link targets verified to exist on the 2.0.0 branch before editing (no assumed parity with dev)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add worktree off origin/feature-dev-2.0.0-...
2. Verify exact lines/targets in 2.0.0 BUILD.md + browser-debug-console.md
3. Fix ../ -> ../../ (and .ino -> ../../src/OTGW-firmware/)
4. Run link checker on 2.0.0 guides -> 0 broken; commit+push; draft PR into 2.0.0
<!-- SECTION:PLAN:END -->
