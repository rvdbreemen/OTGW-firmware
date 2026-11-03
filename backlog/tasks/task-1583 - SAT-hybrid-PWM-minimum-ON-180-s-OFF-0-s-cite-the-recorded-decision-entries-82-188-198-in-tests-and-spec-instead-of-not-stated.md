---
id: TASK-1583
title: >-
  SAT hybrid PWM minimum ON 180 s / OFF 0 s: cite the recorded decision (entries
  82, 188, 198) in tests and spec instead of 'not stated'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-03T06:34:00Z'
updated_date: '2026-11-03T06:35:00Z'
labels:
  - sat
dependencies: []
priority: low
ordinal: 728000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Q-1412.9b (found by TASK-1412.9) was withdrawn 2026-11-03 before asking: entry 82 (Q37 A, hybrid uses gas timing), entry 188 ('everything else stays as for a gas boiler', only the PWM cycle from the heat pump) and entry 198 (600 s OFF 'only heat pump') already decide it. The firmware matches, but tests n5/n8 in tests/test_sat_pwm_anchors.py and docs/sat/firmware-sat-spec.md still call it 'not stated / our reading'. No behaviour change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Tests n5/n8 cite entries 82/188/198 as the decision (comments and assertion messages), still green
- [ ] #2 firmware-sat-spec.md states hybrid minimum ON 180 s and OFF 0 s with the entry citations
<!-- AC:END -->
