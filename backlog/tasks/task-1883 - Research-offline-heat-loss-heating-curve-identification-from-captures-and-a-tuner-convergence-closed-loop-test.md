---
id: TASK-1883
title: >-
  Research: offline heat-loss / heating-curve identification from captures and a
  tuner convergence closed-loop test
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T06:13:00Z'
updated_date: '2026-11-13T06:33:00Z'
labels:
  - sat
  - research
  - tests
dependencies:
  - TASK-1879
priority: low
ordinal: 956000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Lessons 4 and 5 of docs/sat/vtherm-comparison-2026-11-13.md (TASK-1879). (4) VTherm fits dT = g(Tout-Tin) + B u + q per cycle bin with a 70/30 chronological split, physical guards, condition number < 1000, >= 40 cycles, block bootstrap, labelled exploratory and never auto-applied (tpi_identification.py:98-198, 597, 630-650). For us: estimate heat loss and the heating-curve coefficient from 24 h captures in the analyze-capture pipeline, as evidence for the coefficient question (entries 298-312). Compare at least two open-source system-identification candidates first (directive 2026-10-14). (5) Add a closed-loop test that runs the curve self-tune for weeks against a house model with a known correct coefficient and checks convergence without oscillation (counterpart of VTherm tests/test_auto_tpi_simulation.py:58-98); first check what tests/test_sat_sim_boiler_model.py already covers.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Candidate comparison (>= 2 open-source options, licence, activity, fit) recorded in the task notes with a recommendation
- [ ] #2 Prototype estimates the coefficient with a confidence interval on at least one existing long-run capture, labelled exploratory
- [ ] #3 Tuner convergence test: fails on a mutant tuner that oscillates or diverges, passes on the real tuner
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 picked up by the comms session on Robert's request via the coordinator (no rush). Research runs in a background agent in wt-comms; output docs/sat/research/ + a test under tests/.
<!-- SECTION:NOTES:END -->
