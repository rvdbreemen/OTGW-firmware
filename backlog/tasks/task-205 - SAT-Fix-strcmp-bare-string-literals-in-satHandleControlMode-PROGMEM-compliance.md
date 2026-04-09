---
id: TASK-205
title: >-
  SAT: Fix strcmp bare string literals in satHandleControlMode (PROGMEM
  compliance)
status: To Do
assignee: []
created_date: '2026-04-09 05:23'
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
