---
id: TASK-1359
title: >-
  SAT coefficient range: make REST metadata and the wizard slider match the
  0.1-5.0 setting clamp
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T20:21:00Z'
updated_date: '2026-11-01T08:41:00Z'
labels:
  - sat
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 494000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-24 (ADR-198 open question 1): keep 0.1-5.0 (settingStuff.ino:1001) and align everything: REST settings metadata (restAPI.ino:3888, now 0-5 as integers) becomes 0.1-5.0 with decimals; the wizard slider (data/v2.js:5547, 0.2-2.4) goes to 0.1-5.0. Whether 5 is high enough for SAT's design is asked of Sergeant D separately (queue Q21).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 REST metadata and the wizard slider both expose 0.1-5.0 with 0.1 steps (test red on HEAD, green); setting clamp unchanged
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-10-31: tests/test_sat_coefficient_range.py new 3/3, old 2 FAIL (REST metadata 0-5 int, wizard 0.2-2.4), setter clamp 0.1-5.0 unchanged; sat-wizard-tuning-text 11/11; evaluate 0 FAIL. Side effect found: addNum's int bounds also turned sattempstep's 0.1 into 0; now sent as 0.10. Pending: bench REST check after the next build.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: bench /api/v2/settings satcoefficient min 0.1 max 5; tests test_sat_coefficient_range 3/3 and test_sat_wizard_coefficient 4/4 OK. Build: esp32-combo 2.0.0-alpha.597+d458ac3 (build/…-flash.zip 08:51, built and posted by the coordinating session); the task commit is an ancestor of d458ac3 (git merge-base --is-ancestor). evaluate.py --quick 2026-11-01: 0 FAIL, Health 100%. Bench .61 (OT-Direct) runs 2.0.0-alpha.597+d458ac3 (/api/v2/device/info), up 25 min without reboot.
<!-- SECTION:FINAL_SUMMARY:END -->
