---
id: TASK-1425
title: >-
  SAT: cycles_this_hour saturates at 6 (rolling-hour ring
  SAT_MAX_CYCLES_PER_HOUR); heat pumps now have no start bound
status: To Do
assignee: []
created_date: '2026-10-28T14:58:00Z'
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
- [ ] #1 Host test: 7 PWM starts in one rolling hour report cycles_this_hour 7 (red on HEAD, green on fix), or the cap is documented as intended telemetry ceiling
- [ ] #2 build + evaluate green
<!-- AC:END -->
