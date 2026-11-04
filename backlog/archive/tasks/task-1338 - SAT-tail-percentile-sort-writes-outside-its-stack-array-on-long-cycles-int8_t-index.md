---
id: TASK-1338
title: >-
  SAT tail percentile sort writes outside its stack array on long cycles (int8_t
  index)
status: Done
assignee: []
created_date: '2026-10-23T23:42:00Z'
updated_date: '2026-10-24T00:02:00Z'
labels:
  - sat
  - bug
  - memory
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 461000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1331 research and confirmed by the main session: SATcycles.ino _tailPercentile insertion sort uses 'int8_t j = (int8_t)i - 1' with uint8_t i over up to SAT_TAIL_SAMPLE_SIZE (180 on ESP32, boards.h). From i = 129 the cast is negative, the while loop stops and 'sorted[j + 1] = key' writes at a negative index: a stack out-of-bounds write on every continuous cycle with more than 129 tail samples (~130 s of tail), and the tail p90 is wrong. _flowPercentile already uses int16_t j. Fix: int16_t j (and uint16_t i/n/idx as in _flowPercentile). Bug fix, no SAT behaviour choice: the result becomes the intended percentile (today it only feeds the cycle label, auto-tune and status; TASK-1331 would make it switch modes).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test calls the real _tailPercentile with 180 samples: on current code the result differs from a reference percentile and/or a sanitizer or guard detects the out-of-bounds write; on the fix it matches and stays in bounds
- [x] #2 Build passes; evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 evidence: _tailPercentile now uses uint16_t i/n/src/idx and int16_t j (as _flowPercentile). tests/test_sat_tail_percentile.py compiles the real _tailPercentile + tail ring globals cut from SATcycles.ino: HEAD copy 'FAILED (failures=2, skipped=1)' (180 descending samples: p10/p50/p90 = [69, 141, 19] vs reference [18, 90, 162], and a wrapped pseudo-random ring wrong too; n <= 127 still correct), fix 'Ran 6 tests OK (skipped=1)' (ASan variant skipped: runtime DLL missing on this host). evaluate 0 fail; build.bat combo SUCCESS (tree without TASK-1315); bench OTA app: boots, no crashlog (no boiler cycles on the bench, so the sort is host-tested only).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The SAT tail percentile sort no longer writes out of bounds on cycles with more than 129 tail samples (int8_t index -> int16_t). Evidence: AC1 host test of the real function red on HEAD (wrong p90, out-of-bounds) / green; AC2 build SUCCESS, evaluate green. Not seen on a boiler.
<!-- SECTION:FINAL_SUMMARY:END -->
