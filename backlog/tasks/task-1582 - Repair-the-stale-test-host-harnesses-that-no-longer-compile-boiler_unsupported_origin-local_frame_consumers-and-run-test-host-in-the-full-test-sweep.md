---
id: TASK-1582
title: >-
  Repair the stale test/host harnesses that no longer compile
  (boiler_unsupported_origin, local_frame_consumers) and run test/host in the
  full test sweep
status: To Do
assignee: []
created_date: '2026-11-03T06:11:00Z'
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
- [ ] #1 Both harnesses compile and pass on current dev (RESULT PASS), with their mutants still red
- [ ] #2 Every test/host/test_*.py runs in the full sweep (documented command), and a sweep over test/host on dev is green or each failure has its own task
<!-- AC:END -->
