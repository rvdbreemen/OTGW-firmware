---
id: TASK-1512
title: >-
  SAT cycle statistics: DHW flame cycles counted in the 4 h / 24 h windows
  (field capture gruijter, alpha.549)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-30T14:13:00Z'
updated_date: '2026-11-04T20:53:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 668000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field evidence: gruijter's capture 20261030-092718 (alpha.549). 14 DHW flame cycles (CS 10, flow ~62 C, 3-52 s, cycle_attributes kind DOMESTIC_HOT_WATER, class 5 UNCERTAIN / 7 INSUFFICIENT) were logged as SAT cycles #7-#20 and counted in 'SAT 4h': n=7 avgOn 489 s duty 0.34 flow 42.0 (12:33) -> n=20 avgOn 181 s duty 0.29 flow 54.6 (13:23). Question: does thermo-nova exclude DHW cycles from its cycle windows / statistics? Must be checked at the pinned commit (cycles/ or classifier.py) with file:line before any change; if thermo-nova also counts them, the choice goes to Sergeant D. Related: TASK-1387 (classifier parity).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 thermo-nova behaviour for DHW cycles in the cycle windows cited (file:line, second reader)
- [x] #2 Firmware matches the decided behaviour; host test with a DHW cycle shows the window counts
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: SATcycles.ino:939-956 pushes every cycle into the 4 h window, DHW not filtered. AC1 thermo-nova citation first.

2026-11-04 (otgw-firmware-6c, from lead 6b): AC1 thermo-nova at the pinned commit (other-projects submodule c5211afe = mirror of 7beaaf5, no SAT-releases-thermo-nova commits since 2026-10-22). Every flame ON->OFF edge calls record_cycle (cycles/tracker.py:74-80); _build_cycle_state drops a cycle only without a start time or with fewer than 3 samples (tracker.py:90-100), never on kind. record_cycle appends every cycle to the duration, delta and classification windows without a kind check (cycles/history.py:102-127); _window_snapshot builds n, duty and median-on from them (history.py:221-232). CycleKind is only used for UNKNOWN -> UNCERTAIN (classifier.py:22) and as a label (history.py:133, sensor.py:451). So thermo-nova counts DHW cycles in its 4 h / 24 h windows. Second reader: advisor review in this session, read the same excerpts and agreed. Settled by entry 268 (every flame counts in the duty, hot-water burns included, as thermo-nova); the firmware does the same: no kind filter in the 4 h push (SATcycles.ino window path), 3-sample minimum already present (SAT_CYCLE_MIN_SAMPLES, SATcycles.ino:30, TASK-1588). Interpretation (not a Sergeant D decision): sat/4h_avg_flow_temp (f4hAvgFlow, SATcycles.ino:471) has no thermo-nova counterpart (thermo-nova keeps p50 flow-return delta and setpoint errors, history.py:113-118) and is telemetry only (MQTT SATcontrol.ino:4633, HA discovery, trace); it follows the same rule as entry 268 (every flame counts). No control path reads it, so no open SAT question. AC2: tests/test_sat_telemetry_wire.py::test_s3b_a_hot_water_only_burn_is_a_cycle_of_its_own_and_counts_in_the_window (commit dad586338, TASK-1412.15) feeds a 150 s DHW-only burn plus a CH burn and checks n, on, gap and duty against the frame oracle; run 2026-11-04 21:5x in this worktree: 'Ran 1 test in 7.521s OK'. No code change.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
No code change. thermo-nova records every flame cycle, DHW included, in its 4 h / 24 h windows (cycles/tracker.py:74-80, history.py:102-127, :221-232; kind only labels and maps UNKNOWN->UNCERTAIN, classifier.py:22). Entry 268 already decided 'every flame counts, as thermo-nova'; the firmware matches (no kind filter, 3-sample minimum per TASK-1588). The gruijter field numbers (n 7->20 with DHW burns) are therefore the intended behaviour. The firmware-only 4 h flow average is telemetry only and follows the same rule (interpretation, recorded in notes). Evidence: test_s3b in tests/test_sat_telemetry_wire.py (DHW-only burn counts in the window), fresh run OK; second reader = advisor review in session.
<!-- SECTION:FINAL_SUMMARY:END -->
