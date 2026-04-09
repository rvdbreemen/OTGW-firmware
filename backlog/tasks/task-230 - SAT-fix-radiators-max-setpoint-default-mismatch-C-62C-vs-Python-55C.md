---
id: TASK-230
title: 'SAT fix: radiators max setpoint default mismatch (C++ 62C vs Python 55C)'
status: To Do
assignee: []
created_date: '2026-04-09 05:32'
labels:
  - audit-fix
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python SAT calculate_default_maximum_setpoint returns 55C for radiator systems. C++ satGetMaxSetpoint() returns 62C for SAT_HSYS_RADIATORS. This means C++ allows 7C higher flow temperatures before the system-level cap kicks in. If the OT boiler's MaxTSet is low (e.g. 60C), C++ uses the boiler's value; but if MaxTSet is high or unavailable, C++ uses 62C vs Python's 55C. This may be intentional (62C is normal for radiator systems) but represents a behavioral difference that should be explicitly documented and confirmed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Verify whether 62C or 55C is correct for typical radiator systems
- [ ] #2 Confirm alignment with OT spec and typical boiler MaxTSet values
- [ ] #3 Either align to Python default or document the explicit choice in code
- [ ] #4 Check if heat pump max 40C aligns with Python 55C default (big gap)
<!-- AC:END -->
