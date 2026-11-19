---
id: TASK-2912
title: >-
  Capture-tool host tests are flaky on Windows under load (stub server resets,
  threads not stopped, file locks)
status: To Do
assignee: []
created_date: '2026-11-19T12:27:00Z'
updated_date: '2026-11-19T15:58:00Z'
labels:
  - tests
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1054000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen 2026-11-19 in the full sweep for beta.389 (931d1cdef) and in solo reruns, while builds and other sessions loaded the machine; the tests drive scripts' capture tools against a local stub device and do not read firmware source. tests/test_capture_device_data.py: different test each run (436 s: 2 failures + 2 errors, WinError 32 file lock, threads still alive after join; 110 s: 1 failure; 9.5 s: OK; 60 s: 1 failure in test_full_sweep_is_get_only_and_redacted after ConnectionResetError WinError 10054 from the stub). tests/test_capture_discovery.py: TimeoutError against the stub in the sweep, OK alone. tests/test_capture_tool.py: test_l_toggles_restored_at_stop_simulator_untouched failed alone once (5 s wait), OK alone later.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each of the three files passes 5 solo runs in a row under a parallel synthetic CPU load, or its remaining failure has a named cause
- [ ] #2 Waits in the stub-driven tests scale with load or retry, as TASK-1509 did for test_refresh_storm
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 also tests/test_ota_flash.py: in the full sweep on 0f9d85af2 (beta.392) and a solo rerun 10x TimeoutError / URLError against the local stub (64 s), a third run OK 26/26 in 16.7 s; scripts/ota_flash.py and the test unchanged since 931d1cdef, passed twice earlier that day (17 s). Same pattern: stub-driven waits that time out intermittently on Windows.
<!-- SECTION:NOTES:END -->
