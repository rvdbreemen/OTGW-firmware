---
id: TASK-1367
title: >-
  SAT: continuous to PWM after 60 s of sustained overshoot, timed correctly
  (entry 41)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T21:42:00Z'
updated_date: '2026-10-25T17:34:00Z'
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
<!-- SECTION:NOTES:END -->
