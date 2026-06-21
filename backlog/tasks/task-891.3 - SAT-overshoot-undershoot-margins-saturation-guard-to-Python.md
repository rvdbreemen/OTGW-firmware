---
id: TASK-891.3
title: 'SAT: overshoot/undershoot margins + saturation guard to Python'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-20 19:20'
updated_date: '2026-06-20 20:17'
labels: []
milestone: 2.0.0
dependencies: []
references:
  - src/OTGW-firmware/SATcycles.ino
  - src/OTGW-firmware/SATtypes.h
parent_task_id: TASK-891
priority: high
ordinal: 110000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Cycle auto-switch guards + classification margins to thermo-nova Python (cycles/const.py, pwm.py). Audit 2026-06-20; George: 'match Python, it is tested and works reliably'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Overshoot margin default 2.0->3.0 (Python OVERSHOOT_MARGIN_CELSIUS=3.0; fOvershootMargin SATtypes.h:422) in both auto-switch and classification
- [x] #2 Undershoot margin effective -2.0 -> -3.0 (SAT_UNDERSHOOT_MARGIN_C SATcycles.ino:30; underheat disable guard :730; classifier :494; Python UNDERSHOOT_MARGIN_CELSIUS=-3.0)
- [x] #3 Saturation disable guard switches from the continuous-flame-ON>300s proxy (SATcycles.ino:744) to the real PWM off_time==0 sustained 300s mechanism (Python pwm.py; SATURATION_SUSTAIN_SECONDS=300)
- [x] #4 python build.py --target esp32 exits 0; evaluate.py --quick clean; prerelease bumped
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Overshoot margin 2.0->3.0 and undershoot 2.0->3.0 (feed both the auto-switch guards and cycle classification); saturation guard switched from the flame-ON-duration proxy to the real PWM off_time==0 sustained 300s (satApplyPWM records _pwm_lastOffTimeMs, exposed via satPwmLastOffTimeMs()). alpha.229, esp32 build SUCCESS, evaluate 0-fail, pushed origin/dev 475b046e.
<!-- SECTION:FINAL_SUMMARY:END -->
