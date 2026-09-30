---
id: TASK-1186
title: >-
  test(soak): outage-tolerance unit test depends on loop phase and fails about
  one run in five
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-30 18:19'
updated_date: '2026-09-30 18:33'
labels:
  - 2.0.0
  - test
dependencies: []
priority: low
ordinal: 309000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
scripts/tests/test_heap_soak_driver.py DriverTests.test_outage_shorter_than_threshold_is_tolerated fails intermittently with 'AssertionError: 0 not greater than or equal to 1' on snapshots_failed. Measured on 2026-09-30: 7 of 8 runs pass on dev plus the TASK-1123 change, and 6 of 8 on the unchanged 7f636ff42 sources (test, driver and the four firmware files it reads copied from git HEAD), so the flake predates TASK-1123. Likely cause, from the test's own comment in run_soak(): one driver iteration with a 4-asset reload takes about 1.5 s, the stub outage lasts 0.3 s at t=4 s in a 5 s run, so whether any snapshot attempt lands inside the outage depends on the loop phase. The host suite reports this as a FAIL about one run in five, which hides real regressions behind a known-flaky red.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The outage test makes at least one snapshot attempt fall inside the outage window by construction, not by loop phase
- [ ] #2 20 consecutive runs of the test pass
- [ ] #3 A mutation that makes the driver count a short outage as unreachable still fails the test
<!-- AC:END -->
