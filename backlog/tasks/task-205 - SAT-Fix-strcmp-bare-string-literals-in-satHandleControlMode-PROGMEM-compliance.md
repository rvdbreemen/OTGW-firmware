---
id: TASK-205
title: >-
  SAT: Fix strcmp bare string literals in satHandleControlMode (PROGMEM
  compliance)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:23'
updated_date: '2026-04-09 06:17'
labels:
  - audit-fix
  - sat
  - progmem
dependencies: []
priority: medium
---

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 satHandleControlMode() uses strcmp(value, "1"), strcmp(value, "2"), strcmp(value, "0") -- bare RAM literals
- [x] #2 Replace with strcmp_P(value, PSTR("1")) etc. or compare numeric value using atoi() to avoid the string comparison entirely
- [x] #3 evaluate.py PROGMEM check reports 0 SAT-related violations after fix
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Find all strcmp() calls in satHandleControlMode() in SATcontrol.ino
2. Currently: strcmp(value, "1"), strcmp(value, "2"), strcmp(value, "0") -- bare string literals compared against RAM string
3. Replace with atoi(value) comparison to int -- avoids string comparison entirely, is faster, no PROGMEM needed
4. Alternative: strcmp_P(value, PSTR("1")) -- also valid but atoi is cleaner
5. atoi approach: int v = atoi(value); if (v == 1 || ...) -- cleaner and no flash/RAM concerns
6. Verify evaluate.py would not flag this pattern
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed bare strcmp(value, "1/2/0") calls in satHandleControlMode() in SATcontrol.ino.
Replaced with int numericVal = atoi(value) and integer comparisons (numericVal == 1, etc.),
eliminating RAM-resident string literals from comparisons entirely. Named string forms
("continuous", "pwm", "auto") already used strcmp_P with PSTR() and were left unchanged.
evaluate.py PROGMEM check no longer flags any violations in satHandleControlMode().
<!-- SECTION:FINAL_SUMMARY:END -->
