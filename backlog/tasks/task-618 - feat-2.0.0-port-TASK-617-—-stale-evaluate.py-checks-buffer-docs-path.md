---
id: TASK-618
title: 'feat-2.0.0: port TASK-617 — stale evaluate.py checks (buffer + docs path)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-17 10:23'
updated_date: '2026-05-17 10:24'
labels:
  - tooling
dependencies: []
ordinal: 36000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the applicable subset of the dev evaluate.py tooling fixes (TASK-617) to the 2.0.0 line. Investigation showed only 2 of the 4 dev fixes apply here: the ADR-resolver false-positive (TASK-608) does not manifest on 2.0.0 (independent ADR inventory resolves), and the #pragma-once header-guard fix is already present in 2.0.0 evaluate.py. The 2.0.0 PROGMEM FAIL is a separate pre-existing firmware issue, explicitly out of scope.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Buffer-arithmetic check returns PASS (not WARN) when sendMQTTheapdiag has no fixed char[N] buffer — 2.0.0 firmware confirmed refactored to per-stat publishStatU32()
- [x] #2 Documentation check finds BUILD.md and FLASH_GUIDE.md in docs/guides/ (present in 2.0.0 tree)
- [x] #3 python evaluate.py on 2.0.0: the two target checks report PASS; no new failures vs the 90.8% baseline (pre-existing PROGMEM FAIL unchanged, out of scope)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Buffer-arith no-buffer branch (evaluate.py ~1077): WARN -> PASS (per-stat publish, not applicable)
2. Doc-check (evaluate.py ~2261): add docs/guides/<doc> candidate alongside root and docs/
3. Run python evaluate.py on 2.0.0; confirm both target checks PASS and PROGMEM FAIL is the only remaining FAIL (pre-existing, out of scope)
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Investigation: of the 4 dev evaluate.py fixes (TASK-608 + TASK-617), only 2 apply to 2.0.0.
- ADR-resolver false positive (TASK-608): does NOT manifest on 2.0.0 — its References-resolve check passes (independent ADR inventory; ADR-097/099 exist as files here).
- #pragma-once header-guard fix: ALREADY present in 2.0.0 evaluate.py (line 606).
- Buffer-arith no-buffer branch: ported WARN->PASS. 2.0.0 sendMQTTheapdiag confirmed refactored to per-stat publishStatU32() (no char[N] buffer).
- Doc-check: ported docs/guides/ candidate. 2.0.0 has BUILD.md/FLASH_GUIDE.md under docs/guides/.
Result: Passed 68->70, target WARNs cleared. Remaining FAIL is the pre-existing [PROGMEM] Flash string compliance (2 violations) — separate firmware issue, explicitly out of scope.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported the applicable subset of dev evaluate.py tooling fixes (TASK-617) to the 2.0.0 line.

Only 2 of 4 dev fixes applied after per-branch verification (cross-worktree rule: confirm, do not assume parity):
- sendMQTTheapdiag buffer-arithmetic check: no-buffer branch now PASS (not applicable) instead of WARN — 2.0.0 firmware confirmed using per-stat publishStatU32() with no fixed buffer.
- Documentation check now also searches docs/guides/ for BUILD.md / FLASH_GUIDE.md (present there in the 2.0.0 tree).

Not ported: ADR-resolver hatch (does not manifest on 2.0.0 — independent ADR inventory resolves) and #pragma-once header-guard (already fixed in 2.0.0 evaluate.py).

Verification: python evaluate.py on 2.0.0 -> the 3 target checks PASS; Passed 68->70, no new failures. The single remaining FAIL ([PROGMEM] 2 violations) is pre-existing and unrelated — a separate firmware task, deliberately out of scope. Tooling-only change (top-level evaluate.py), no firmware touched, no version bump per policy.
<!-- SECTION:FINAL_SUMMARY:END -->
