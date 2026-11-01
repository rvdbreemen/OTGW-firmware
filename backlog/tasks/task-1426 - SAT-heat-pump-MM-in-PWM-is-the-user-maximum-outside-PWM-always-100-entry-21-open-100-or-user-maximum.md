---
id: TASK-1426
title: >-
  SAT: heat pump MM in PWM is the user maximum, outside PWM always 100 (entry 21
  open: 100 or user maximum?)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T14:59:00Z'
updated_date: '2026-11-01T01:31:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 590000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Review finding on TASK-1419 (2026-10-28). satComputeMM (SATcontrol.ino:379-388): heat pump outside PWM -> MM=100; in PWM -> iMaxRelModulation (100 if 0). With entry 87 heat pumps now enter PWM automatically, so a user max below 100 makes MM drop when PWM starts. Entry 21 allows '100 % or the desired max modulation limit the user selects'; its Open item (which one is meant) is unresolved. settingsHelp.h:70 says heat pumps keep full modulation. Needs a question to Sergeant D (queue in docs/sat/open-questions.md) before any change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sergeant D's answer recorded in docs/sat/sergeant-d-corrections.md
- [x] #2 satComputeMM and settingsHelp.h follow the answer; host test red/green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 Q54 answered B (entry 113): heat pump MM = configured max (100 if 0) in and outside PWM. AC#1 done by entry 113. Hybrids unchanged. Implement after TASK-1368 (same file SATcontrol.ino).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
satComputeMM: a heat pump gets the configured max relative modulation in and outside PWM, 100 when that is 0 (Sergeant D entry 113, Q54 B; settles entry 21 Open); the old 'heat pump outside PWM = 100' branch is gone. Hybrids and gas unchanged. settingsHelp satsource text no longer says heat pumps keep full modulation.
Evidence: tests/test_sat_pwm_override.py (compiles the real satComputeMM): new 79/79 OK; old code fails test_heat_pump_and_hybrid_never_zero (outside PWM gave 100 instead of the configured 70). evaluate 0 FAIL. Not verified on a heat pump (bench .61 build pending).
<!-- SECTION:FINAL_SUMMARY:END -->
