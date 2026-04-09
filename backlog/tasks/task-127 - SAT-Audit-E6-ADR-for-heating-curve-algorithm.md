---
id: TASK-127
title: 'SAT Audit E6: ADR for heating curve algorithm'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:55'
updated_date: '2026-04-09 05:25'
labels:
  - audit
  - sat
  - phase-5
  - adr
milestone: m-0
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Write an ADR documenting the heating curve algorithm: linear interpolation vs polynomial, configuration parameters and the rationale for the chosen curve calculation method.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR describes alternative curve methods
- [x] #2 Chosen interpolation method documented
- [x] #3 ADR published in backlog/decisions/
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Created docs/adr/ADR-072-sat-heating-curve-algorithm.md documenting:
- Formula: setpoint = baseOffset + (coeff/4) * [4*(target-20) + 0.03*(outside-20)^2 - 0.4*(outside-20)]
- Quadratic term rationale: real heating curves are slightly concave upward at extreme cold
- Single-coefficient design: gains auto-scale without re-tuning
- Base offsets: 20.0C (underfloor), 27.2C (radiators/heat pump)
- System limits by type: heat pump 40C, underfloor 45C, radiators 62C
- Safety hard ceilings: 50C underfloor, 80C radiators
- Alternatives rejected: linear, lookup table, polynomial fit from manufacturer data
<!-- SECTION:FINAL_SUMMARY:END -->
