---
id: TASK-1796
title: >-
  SAT self-tune: phase-based signal (warm-up vs floating), Sergeant D proposal
  (Q-1787a)
status: To Do
assignee: []
created_date: '2026-11-11T06:21:00Z'
updated_date: '2026-11-11T06:28:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 890000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-11 06:18:39 UTC, #dev-sat-mqtt msg 1569959525715542148 (entry 302): use the warm-up overdamping data for the heating curve self-tune by comparing the warm-up phase with the floating phase inside the deadband, then decide the coefficient change. Conflicts with ADR-205 Must (curve integral as the only signal) and entries 163/171, so adopting it needs a superseding ADR. Waiting for his answer to Q-1787a (asked msg 1569959952854945914: A split clean hours into warm-up and floating and decide on the difference, recommended; B warm-up only; C other). Do not implement before the answer and the follow-up threshold questions are recorded as entries.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's answer to Q-1787a recorded as a numbered entry, and any follow-up question (thresholds, step) asked and recorded one at a time
- [ ] #2 Design written with thermo-nova and firmware citations, and a Proposed ADR that supersedes ADR-205 for the changed signal
- [ ] #3 Implementation with host tests that are red on the old tuner and green on the new one, and a simulation run per house type
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-11 06:28 UTC: on Robert's order all open Sergeant D questions were asked in one combined message (#dev-sat-mqtt msg 1569961879022084211): Q-1787a (repeated, phase-based signal), Q-1787b (ADR-205 tuner until then: keep / clamp 1.6-2.4 / off), Q-1771a (preset restore after reboot, thermo-nova climate.py:175-188), Q-1777g (last Greek labels). Rows in docs/sat/open-questions.md.
<!-- SECTION:NOTES:END -->
