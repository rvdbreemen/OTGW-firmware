---
id: TASK-1582
title: >-
  Repair the stale test/host harnesses that no longer compile
  (boiler_unsupported_origin, local_frame_consumers) and run test/host in the
  full test sweep
status: Done
assignee:
  - '@claude'
created_date: '2026-11-03T06:11:00Z'
updated_date: '2026-11-03T14:28:00Z'
labels:
  - test
dependencies: []
priority: medium
ordinal: 727000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-11-03 while integrating TASK-1389: test/host/test_boiler_unsupported_origin.py and test_local_frame_consumers.py exit 2 on clean origin/dev 429fd8c (COMPILATION FAILED): their stubs miss platformRecursiveMutexLock/Unlock, state.otBus.iBoilerLastSeenMs, satOnThermostatSetpoint, pmChainOnBoilerResponse (and after TASK-1412.5 also feedTret/feedEvent/satFeedNote). They live outside tests/, so the full sweep (tests/test_*.py) never ran them and they rotted silently.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Both harnesses compile and pass on current dev (RESULT PASS), with their mutants still red
- [x] #2 Every test/host/test_*.py runs in the full sweep (documented command), and a sweep over test/host on dev is green or each failure has its own task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 (agent, verified by lead): all 16 test/host harnesses compile and pass on dev; 7 fixed (boiler_unsupported_origin, local_frame_consumers, cmd_queue_drop, pic_banner_dispatch, pic_banner_recovery, pic_settings_value, sat_pid_derivative), dhw_water_meter was never broken (timeout artefact). Declarations now sliced from source where possible (OTBusState from OTBustypes.h, SATboilerFeed.h included, CmdQueue types). Mutants still red (local_frame_consumers 8/8, cmd_queue_drop, pid_derivative 6/6). R1/E1 asserted behaviour TASK-1418 removed (entry 77): now NOTE lines; the unexplained R1 number is TASK-1592. Full sweep: python scripts/run_tests.py now runs tests/ + test/host (124/124 in the agent's run); documented in docs/guides/test-runner.md and one CLAUDE.md test-section bullet.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The stale test/host harnesses compile and pass again (16/16, mutants still red), declarations are read from source where possible, and python scripts/run_tests.py runs tests/ plus test/host as one documented full sweep (124/124). One open number in the PID derivative harness moved to TASK-1592.
<!-- SECTION:FINAL_SUMMARY:END -->
