---
id: TASK-3054
title: >-
  SAT: remove the default of bandOff in satChEnableDecide so a caller that
  forgets it fails to compile (2.1.0 hardening after TASK-3034)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-25T10:29:00Z'
updated_date: '2026-11-27T12:32:00Z'
labels:
  - sat
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1157000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-3034 found a caller (the diag input of satDiagEventsStep) that omitted bandOff and silently got false. The default (SATchEnable.h:94, bandOff = false) hides that class of bug. Remove the default; every caller passes bandOff explicitly: SATcontrol.ino:8674 and :8697 already do; the host tests that call satChEnableDecide (test_sat_pwm_anchors.py, test_sat_pwm_override.py, test_sat_requested_vs_transmitted.py, test_sat_ch_enable.cpp) pass false. No behaviour change. Low priority, 2.1.0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 satChEnableDecide has no default for bandOff and the firmware and every host test caller passes it explicitly
- [x] #2 A host test goes red when the default is present (grep or compile of a call without the argument) and green on the fix
- [x] #3 evaluate.py --quick no new FAIL; the SAT host tests that call the function pass; esp32-combo build OK
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 done in code: bandOff has no default in satChEnableDecide (SATchEnable.h:94); the 2 firmware calls already pass it; 9 test/script call sites pass false (test_sat_ch_enable.cpp, cold_hysteresis.cpp, lowload_contract, telemetry_wire, pwm_anchors, pwm_override x2, requested_vs_transmitted, sim_port_equivalence, scripts/verify_sat_low_load.py). New test tests/test_sat_ch_enable_band_required.py (4 tests): RED with the default restored (signature test + 'four-argument call does not build' fail), GREEN on the fix. Affected SAT tests green: ch_signals 4, tolerance_band 30, pwm_anchors 64, pwm_override 83, requested_vs_transmitted 10, sim_port_equivalence 4, telemetry_wire 18, diag_events 14, cold_cut_min_burn 8, lowload_contract 35 (1 skipped). evaluate --quick 88 passed, 0 failed. Build and commit pending.

2026-11-25 build: esp32-combo from 0742d150a (2.0.0-beta.429+0742d15), 'Build completed successfully!', fresh artifacts 12:03-12:04, app 2158544 B of 2293760 B (94.1 %), free 135216 B; evaluate --quick 89 passed / 0 failed. NOT bench-tested: compile-time guard plus comment, no behaviour change; the soak of the next public beta covers it. AC1 and AC2 have evidence (test red with the default, green on the fix; every caller passes the argument); AC3 holds for the SAT tests named above, the whole tests/ folder is rerun separately.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed by otgw-docs in the 2026-11-27 closure sweep, evidence from the task notes of 2026-11-25. AC1: satChEnableDecide has no default for bandOff (SATchEnable.h:94), firmware calls and 9 test/script call sites pass it. AC2: tests/test_sat_ch_enable_band_required.py, 4 tests, red with the default restored, green on the fix. AC3: evaluate --quick 89 passed / 0 failed, the named SAT host tests green, esp32-combo build OK from 0742d150a (beta.429+0742d15), which is on origin/dev. Not verified: no bench run (compile-time guard, no behaviour change); the whole tests/ folder was to be rerun separately and is not recorded in the notes.
<!-- SECTION:FINAL_SUMMARY:END -->
