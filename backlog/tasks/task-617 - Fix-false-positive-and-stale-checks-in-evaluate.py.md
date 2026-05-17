---
id: TASK-617
title: Fix false-positive and stale checks in evaluate.py
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-17 10:03'
updated_date: '2026-05-17 10:06'
labels:
  - tooling
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
evaluate.py reports 3 WARN-level checks that are bugs in the evaluator itself, not firmware defects (pragma-once header guard not recognized; stale sendMQTTheapdiag buffer-arithmetic check after refactor; BUILD.md/FLASH_GUIDE.md searched in wrong dir). The related ADR-reference FAIL is owned by TASK-608. Fix these 3 evaluator checks so the run reflects reality.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Header-guard check accepts '#pragma once' as a valid guard (mqtt_discovery_verify.h passes)
- [x] #2 JSON buffer-arithmetic check returns PASS (not WARN) when sendMQTTheapdiag has no fixed char[N] buffer
- [x] #3 Documentation check finds BUILD.md and FLASH_GUIDE.md in docs/guides/
- [x] #4 python evaluate.py: header-guard, buffer-arithmetic, and BUILD/FLASH doc checks all pass; no new failures introduced
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Header-guard check (evaluate.py:148): also accept "#pragma once"
2. Buffer-arithmetic check (evaluate.py:618): PASS when no char[N] buffer in body (refactored to per-stat publishStatU32)
3. Doc check (evaluate.py:1271): add docs/guides/<doc> to search paths
4. Run python evaluate.py to confirm all three pass, no regressions
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Three evaluator-self bugs fixed in evaluate.py:
1. Header-guard check (line ~148): now accepts "#pragma once" OR #ifndef/#define. mqtt_discovery_verify.h (uses #pragma once) -> PASS.
2. Buffer-arithmetic check (sendMQTTheapdiag): function was refactored to per-stat publishStatU32() calls with no fixed char[N] buffer, so no overflow surface. "no buffer" branch changed WARN -> PASS ("not applicable").
3. Documentation check: BUILD.md/FLASH_GUIDE.md live in docs/guides/ per project layout; added docs/guides/ to candidate paths (root and docs/ kept for back-compat).
All three now PASS; full evaluate.py Health 96.1% (was 88.2%), 0 FAIL.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed 3 self-bugs in evaluate.py that produced false-positive/stale WARNs unrelated to firmware quality.

- Header-guard check now recognizes "#pragma once" (modern guard) in addition to #ifndef/#define; mqtt_discovery_verify.h passes.
- sendMQTTheapdiag buffer-arithmetic check: after the per-stat publishStatU32() refactor there is no fixed char[N] buffer and thus no overflow surface; the "no buffer found" branch now reports PASS (concern not applicable) instead of WARN.
- Documentation check searches docs/guides/ (the documented home of operational guides) in addition to repo root and docs/; BUILD.md and FLASH_GUIDE.md now resolve.

Verification: python evaluate.py -> 0 FAIL, Health 96.1% (was 88.2%); the three target checks report PASS. Tooling-only change (top-level evaluate.py), no firmware touched, no version bump per policy. The ADR-reference FAIL is owned by TASK-608 (fixed in the same commit).
<!-- SECTION:FINAL_SUMMARY:END -->
