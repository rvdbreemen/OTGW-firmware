---
id: TASK-1714
title: Host test test_local_frame_consumers AC5/AC6 fail
status: Done
assignee: []
created_date: '2026-11-07T21:53:00Z'
updated_date: '2026-11-07T23:04:00Z'
labels:
  - test
  - host-harness
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 822000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
test/host/test_local_frame_consumers.py reports CASE AC5 FAIL and CASE AC6 FAIL (2 failures) on dev. Red since before 834902ac8 (TASK-1684): verified 2026-11-07 in a temp worktree at 834902ac8~1, same two failures. Not caused by TASK-1684/1695. Reported by session 54 while working on TASK-1707.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Root cause named: firmware regression (commit) or stale harness expectation
- [x] #2 Fix applied; the same harness run is red before and green after
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08. Root cause: stale harness expectation, not a firmware regression. Commit 8442aaa1c (TASK-1391, 2026-11-05) deliberately made only boiler B frames boiler evidence in the six OT support bitmaps (a PIC proxy A without a B, ADR-103, no longer sets an unsupported verdict or an acked bit; gruijter capture: 24W/115R branded from A frames only). It added tests/test_boiler_support_bitmap_b_only.py but did not update the two older harnesses. Proof: both harnesses at 8442aaa1c~1: PASS (P1, AC5, AC6 pass); at 8442aaa1c: exactly P1 / AC5 / AC6 FAIL, nothing else. Fix (test only): P1 wants read/write 0/0, AC5/AC6 want acked=false, header comments name TASK-1391. After: test_boiler_unsupported_origin.py RESULT PASS, test_local_frame_consumers.py RESULT PASS. Mutant (OTGW-Core.ino line 5939 also accepting OTGW_ANSWER_THERMOSTAT): exactly P1 FAIL and AC5+AC6 FAIL, so the cases guard the TASK-1391 rule. tests/test_boiler_support_bitmap_b_only.py OK, evaluate --quick 0 failed. Review (CLAUDE.md compliance + 2 independent bug reviews of the diff): no issues. Not done: running the new harness against the pre-1391 tree directly (it needs headers that tree lacks, SATpm25Poll.h), hence the mutant.
<!-- SECTION:NOTES:END -->
