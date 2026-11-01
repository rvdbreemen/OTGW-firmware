---
id: TASK-1425
title: >-
  SAT: cycles_this_hour saturates at 6 (rolling-hour ring
  SAT_MAX_CYCLES_PER_HOUR); heat pumps now have no start bound
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T14:58:00Z'
updated_date: '2026-11-01T08:41:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 589000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Review finding on TASK-1419 (correctness lens, 2026-10-28). The rolling-hour ring in SATcycles.ino:37-40/:286-291 holds SAT_MAX_CYCLES_PER_HOUR=6 entries and the count caps at 6. Since entry 81 a heat pump has no hourly start limit, and range-5 / PWM-entry starts were never held for any source, so more than 6 starts in an hour are possible; REST/MQTT/SAT page then show 6 and older starts are overwritten. Control is not affected (nothing blocks heat pumps; gas blocks at cph <= 6 before the ring overflows for OFF->ON starts).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: 7 PWM starts in one rolling hour report cycles_this_hour 7 (red on HEAD, green on fix), or the cap is documented as intended telemetry ceiling
- [x] #2 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Correction: AC2 (build green) unticked and status back to In Progress; the esp32-combo build of alpha.596 has not run yet (blocked since the low-memory stop). AC1 stands.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 checked earlier (test_sat_pwm_start_limit, now 22/22 OK). Build: esp32-combo 2.0.0-alpha.597+d458ac3 (build/…-flash.zip 08:51, built and posted by the coordinating session); the task commit is an ancestor of d458ac3 (git merge-base --is-ancestor). evaluate.py --quick 2026-11-01: 0 FAIL, Health 100%. Bench .61 (OT-Direct) runs 2.0.0-alpha.597+d458ac3 (/api/v2/device/info), up 25 min without reboot.
<!-- SECTION:FINAL_SUMMARY:END -->
