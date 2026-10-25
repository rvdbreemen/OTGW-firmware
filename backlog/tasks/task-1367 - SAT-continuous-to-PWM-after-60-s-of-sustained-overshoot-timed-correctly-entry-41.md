---
id: TASK-1367
title: >-
  SAT: continuous to PWM after 60 s of sustained overshoot, timed correctly
  (entry 41)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T21:42:00Z'
updated_date: '2026-10-25T21:42:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 502000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 (Q9, entry 41): 60 s. Settles conflict C1. Audit #12: the sustain timers count the first tick as 30 s, so the switch fires after ~30 s; fix the timing to a real 60 s (thermo-nova heating_control.py:280-304).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The overshoot switch fires after 60 s, not ~30 s (host test red/green)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25 implemented (wf_5b20725f: plan, implement, two refuting reviews, fix). Overshoot sustain now one start timestamp from the first qualifying sample, switch at >= 60000 ms (thermo-nova heating_control.py:293-304, helpers.py:51-56); cleared when not qualifying, on bypass paths (DHW, fault, thermal-safe, summer simmer, heat-pump gate else), on every mode change (satSetControlMode) and in satDisable; the switch resets all sustain guards (heating_control.py:304-305). Underheat/saturation counting unchanged (Q44). Fixes the dead post-DHW guard only for the overshoot start. Known timing: CATCH_UP_MISSED_TICKS grid means about half of switches slip one tick (90 s at a 30 s tick), computed from code, not measured. Queued Q43 (threshold >= vs >), Q44 (underheat/saturation counting), Q45 (heat-pump exclusion), Q46-Q47. Evidence this session: tests/test_sat_overshoot_sustain.py OK on fix, HEAD FAILED (failures=16); all tests/test_sat_*.py + test_otd_ch_override OK; evaluate --quick exit 0.

2026-10-25 22:37-22:41 bench (OTGW32 .61, alpha.508+c7ee107): replay with flame on, flow 40.0, curve-only sp 21.9, margin 3.0, SAT continuous. Run 1: first qualifying sample 22:37:57.502 (os=0s), 22:38:27.505 os=30s NO switch, 22:38:57.505 'sustained overshoot for 60s ... switching to PWM mode' = 60.003 s. Run 2 (after control_mode auto): 22:39:57.508 -> 22:40:57.512 = 60.004 s, again no switch at os=30s. Telnet transcript scratchpad run1/telnet.log lines 540/1131/1586 and 2499/2946/3399. Settings unchanged (diff empty), simulation stopped, replay file removed. No one-tick slip observed in 2 runs. Old firmware not run on the bench (old-vs-fix is the host test).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The sustained-overshoot switch (continuous to PWM) fires after a real 60 s (Sergeant D entry 41), not ~30 s. Commit 4f82625bc: one start timestamp from the first qualifying sample, switch at >= 60000 ms (thermo-nova heating_control.py:293-304, helpers.py:51-56); cleared on non-qualifying samples, bypass paths, mode changes and disable; the switch resets all sustain guards. AC#1 evidence: tests/test_sat_overshoot_sustain.py on HEAD 17 OK, on 4f82625bc^ FAILED (failures=16); all tests/test_sat_*.py OK. Bench (OTGW32, alpha.508, replay): switch at 60.003 s and 60.004 s in two runs, no switch at the 30 s tick. Not verified: old firmware on the bench; the predicted one-tick slip to 90 s was not seen. Follow-ups for Sergeant D: Q43-Q47.
<!-- SECTION:FINAL_SUMMARY:END -->
