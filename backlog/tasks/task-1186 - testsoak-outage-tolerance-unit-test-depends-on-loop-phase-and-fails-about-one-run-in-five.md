---
id: TASK-1186
title: >-
  test(soak): outage-tolerance unit test depends on loop phase and fails about
  one run in five
status: Done
assignee:
  - '@claude'
created_date: '2026-09-30 18:19'
updated_date: '2026-09-30 19:14'
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
- [x] #1 The outage test makes at least one snapshot attempt fall inside the outage window by construction, not by loop phase
- [x] #2 20 consecutive runs of the test pass
- [x] #3 A mutation that makes the driver count a short outage as unreachable still fails the test
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Reproduce with a request trace under CPU load and name the mechanism. 2. Move the outage to device/info request 2, which run() always sends as the baseline snapshot (no load request precedes poll('baseline')), so the outage starts on a snapshot by construction. 3. Prove old vs fix on the same load: alternate the HEAD test and the fixed test 20 times each under 20 busy processes. 4. Mutant driver that counts any failed snapshot as unreachable: must fail the fixed test every time. 5. Full test module and evaluate --quick green; scripts/ only, no firmware bump.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Diagnosis: a first hypothesis (the run ends before device/info request 4) was refuted: failing runs had 9 and 11 requests. A request trace under CPU load then caught a failing run: request 4 was a load request (right after the previous snapshot's simulate call), the stub refused it, and the next snapshot arrived 0.47 s later, after the 0.3 s outage. Side note from the measurement: the scratchpad CPU burner's multiprocessing workers outlived a killed parent on Windows; 20 orphans kept burning until they were stopped, and the one module run made during that window was discarded and repeated.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The heap soak driver's outage-tolerance unit test no longer depends on loop phase.

Cause, reproduced with a request trace under CPU load (20 busy processes on a 16-thread host): the stub starts its 0.3 s outage on device/info request 4, and request 4 can be a load request (API_PATHS includes device/info). The dropped request is then not a snapshot. The next snapshot follows after the loop's 0.1 s sleep plus scheduling delay; in the traced failing run it arrived 0.47 s later, after the outage had ended, so no snapshot failed and snapshots_failed stayed 0.

Fix (scripts/tests/test_heap_soak_driver.py only): the outage now starts on device/info request 2. run() sends no load request between its startup check (request 1) and poll("baseline"), so request 2 is always the baseline snapshot, and the request that starts the outage is itself refused. A snapshot fails by construction.

Evidence
- AC#1: request 2 is the baseline snapshot by the driver's structure (heap_soak_driver.py run(): device/info, telnet z, simulate/start, then poll("baseline")); the request that fires the outage is the one the stub refuses (route() checks dark_until after fire_events()).
- Old vs fix, alternating on the same load (20 busy processes): the HEAD test passed 15 of 20 runs, the fixed test 20 of 20.
- AC#2: those 20 runs of the fixed test passed in a row, under load.
- AC#3: a mutant driver whose poll() calls check_gap(inf) on every failed snapshot fails the fixed test in 5 of 5 runs. With the old test it was also caught in 5 of 5; the old test misses it only in a run where no snapshot fails, which happened in 5 of its 20 runs above.
- Full module (19 tests): OK on a quiet host (101.8 s) and OK once more under the same load (153.5 s). One loaded run is an observation, not a robustness proof for the other 18 tests.
- python evaluate.py --quick: exit 0, health score 100%. scripts/ only, so no firmware bump or build is involved.
<!-- SECTION:FINAL_SUMMARY:END -->
