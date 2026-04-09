---
id: TASK-230
title: 'SAT fix: radiators max setpoint default mismatch (C++ 62C vs Python 55C)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:32'
updated_date: '2026-04-09 06:15'
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
- [x] #1 Verify whether 62C or 55C is correct for typical radiator systems
- [x] #2 Confirm alignment with OT spec and typical boiler MaxTSet values
- [x] #3 Either align to Python default or document the explicit choice in code
- [x] #4 Check if heat pump max 40C aligns with Python 55C default (big gap)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Investigate the correct radiator max setpoint: 62C vs 55C
2. 62C is the correct value for radiator systems (EN 12831, typical European hydronic); Python 55C is conservative default for modern condensing boilers
3. Decision: keep 62C for C++ (correct for traditional radiator systems) and add an explicit comment explaining why this differs from Python 55C default
4. Also document the heat pump gap: C++ 40C vs Python 55C -- these are actually different: Python 55C is for radiators; Python for heat pump is 40C, which matches C++
5. Verify that comment is clear and no code change needed beyond documentation
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
No code change. 62C is correct for traditional high-temperature European radiator systems (EN 12831 design conditions). Python 55C is a conservative default for modern condensing installs, not an OT spec requirement. In practice the boiler's own MaxTSet OT parameter (read in satControlLoop) caps the value, so the difference only matters when MaxTSet is unavailable or high. Heat pump: both C++ and Python agree on 40C -- no discrepancy. Added a detailed explanatory comment to satGetMaxSetpoint() documenting the rationale and explicitly closing the audit finding.
<!-- SECTION:FINAL_SUMMARY:END -->
