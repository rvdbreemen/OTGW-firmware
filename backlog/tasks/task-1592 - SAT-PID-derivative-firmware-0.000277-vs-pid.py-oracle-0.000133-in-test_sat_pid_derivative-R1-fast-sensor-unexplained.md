---
id: TASK-1592
title: >-
  SAT PID derivative: firmware -0.000277 vs pid.py oracle -0.000133 in
  test_sat_pid_derivative R1 (fast sensor), unexplained
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-03T14:26:00Z'
updated_date: '2026-11-03T17:57:00Z'
labels:
  - sat
  - test
dependencies: []
priority: medium
ordinal: 737000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by TASK-1582 (2026-11-03): after the harness repair, check R1 (fast sensor) of test/host/test_sat_pid_derivative gives firmware -0.000277 and the pid.py oracle -0.000133 (true slope -0.000333). R1/E1 were turned into non-judged NOTE lines because they asserted TASK-1195's separate derivative reference that TASK-1418 removed on purpose (entry 77 Q27). But if the firmware now follows pid.py literally, these two numbers should match; tests/test_sat_pid_semantics.py (TASK-1412.10, line-by-line pid.py oracle) found no deviation, so the difference may be the harness's oracle, the room EMA input, or a real gap. Investigate which; second reader before any claim.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Cause identified with file:line (harness oracle, input path such as the room EMA, or firmware); if firmware: a host test red on the old code and a recorded decision before any change
- [ ] #2 R1/E1 either judged again with the correct oracle or documented why not
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 investigation (opus): INPUT PATH DIFFERS, firmware follows pid.py. The R1 oracle feeds pid.py the sensor's exact change times on a 30 s grid, so dt sits exactly on pid.py's delta_time <= 60 boundary (pid.py:217; SATpid.ino:303 identical) and steps are skipped; the firmware stamps when it first sees a value (loop jitter 60.004-60.99 s), so every step passes. pid.py fed the firmware's own stamps equals the firmware within 1e-6 in 185/185 harness runs. Fix in the test only: judge R1/E1 against pid.py fed the firmware's (room, stamp) per step, compare steady-state means, keep the raw-time comparison as a NOTE.
<!-- SECTION:NOTES:END -->
