---
id: TASK-480
title: Promote design-system drift gate from WARN to FAIL after grace release
status: Done
assignee:
  - '@claude'
created_date: '2026-04-28 21:46'
updated_date: '2026-05-08 21:18'
labels:
  - tooling design-system ci-gate
dependencies: []
ordinal: 10000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up for TASK-470 and ADR-091. The first release of evaluate.py check_design_system_drift reports actionable class drift as WARN to absorb false positives. After one release cycle with no accepted false-positive reports, promote the check to FAIL so CI blocks new design-system class drift.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 check_design_system_drift uses FAIL severity for actionable non-allowlisted missing CSS classes
- [x] #2 ADR-091 and any task notes are updated with the promotion date or release boundary
- [x] #3 python evaluate.py --quick exits cleanly on the promoted gate with a green baseline
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Identify the 3 drifted classes (sat-marker-empty/item/del from TASK-586 gap)\n2. Add CSS selectors in components.css (sat-marker panel block)\n3. Add missing HTML container (ul#sat-marker-list) in index.html heating curve section\n4. Promote evaluate.py gate from WARN to FAIL\n5. Update ADR-091 status line with promotion date\n6. Build and evaluate.py verify\n7. Bump prerelease, commit, push
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Promoted the design-system drift gate from WARN to FAIL in evaluate.py (TASK-480 grace release complete).

Root cause: three classes from TASK-586 (sat-marker-empty, sat-marker-item, sat-marker-del) had no CSS selector, keeping the gate in WARN state. Also discovered the #sat-marker-list HTML container was missing from index.html, making the calibration marker list silently non-functional.

Changes:
- components.css: added .sat-marker-panel, .sat-marker-heading, .sat-marker-list, .sat-marker-empty, .sat-marker-item, .sat-marker-del selectors using design tokens
- index.html: added <ul id='sat-marker-list'> inside #sat-curve-section (TASK-586 gap fix)
- evaluate.py: check_design_system_drift severity changed WARN -> FAIL; dispatch comment updated
- ADR-091 status line: promotion date 2026-05-08 appended
- CLAUDE.md (both worktrees): auto-advance-to-next-task policy documented

evaluate.py --quick: 58 passed (was 57 PASS + 1 WARN), 2 pre-existing warnings, 1 pre-existing failure. Build: exit 0. Shipped as alpha.33 (0b4c36b3).
<!-- SECTION:FINAL_SUMMARY:END -->
