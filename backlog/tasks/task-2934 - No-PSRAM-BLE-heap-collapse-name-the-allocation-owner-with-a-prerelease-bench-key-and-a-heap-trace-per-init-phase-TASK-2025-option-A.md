---
id: TASK-2934
title: >-
  No-PSRAM BLE heap collapse: name the allocation owner with a prerelease bench
  key and a heap trace per init phase (TASK-2025 option A)
status: To Do
assignee: []
created_date: '2026-11-19T22:32:00Z'
labels:
  - ble
  - heap
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1067000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-2025 AC2, waived for 2.0.0 by Robert ('C', DM 1573102947980542135, 2026-11-19): the ADR-222 guard keeps BLE off without PSRAM, so the collapse (free 9 KB, maxblk 2 KB, minFree 520 B within ~2 min on .61, beta.370) is unreachable in 2.0.0. Only needed if BLE without PSRAM should ever be supported. Plan (option A of the TASK-2025 proposal): a prerelease-only telnet key that starts NimBLE despite the guard, with heap_caps_get_info(MALLOC_CAP_INTERNAL) before init, after the controller init, after the host init, after the first scan start and every 10 s, plus the largest-block owner when the IDF build allows heap tracing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Prerelease-only key (compiled out of stable, checked with tests/test_prerelease_macro_guarded.py) starts NimBLE on a no-PSRAM unit and prints the heap per init phase
- [ ] #2 Bench on .61 (otgw-bench-tester): the phase that takes the heap and its allocation owner named with file:line
- [ ] #3 Fix or decision recorded with the numbers
<!-- AC:END -->
