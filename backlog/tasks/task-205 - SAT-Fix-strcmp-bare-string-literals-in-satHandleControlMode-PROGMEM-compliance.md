---
id: TASK-205
title: >-
  SAT: Fix strcmp bare string literals in satHandleControlMode (PROGMEM
  compliance)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-09 05:23'
updated_date: '2026-04-09 06:12'
labels:
  - audit-fix
  - sat
  - progmem
dependencies: []
priority: medium
---

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 satHandleControlMode() uses strcmp(value, "1"), strcmp(value, "2"), strcmp(value, "0") -- bare RAM literals
- [ ] #2 Replace with strcmp_P(value, PSTR("1")) etc. or compare numeric value using atoi() to avoid the string comparison entirely
- [ ] #3 evaluate.py PROGMEM check reports 0 SAT-related violations after fix
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
