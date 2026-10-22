---
id: TASK-1292
title: >-
  SAT specification from thermo-nova (ground truth), firmware SAT specification
  and a detailed comparison
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-22T05:18:00Z'
updated_date: '2026-10-22T05:21:00Z'
labels:
  - sat
  - docs
  - audit
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 417000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer request (#alpha-testing 2026-10-22 05:07, confirmed in session): deep audit of the SAT Python implementation and a detailed specification from it that Sergeant D can read and validate; the same for this firmware's SAT; then a detailed comparison. Ground truth: Alexwijn/SAT branch releases/thermo-nova @ 7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb (2026-03-18); the local copy other-projects/SAT-releases-thermo-nova is identical to it (diff -rq 2026-10-22: only __pycache__). There is no written SAT design document, so this code is leading. Context: Sergeant D reported that the agent repeatedly misunderstood the start-up sequence and the suppression mechanism. Every statement in the documents must cite file:line in the pinned commit (Python) or the firmware source, so it can be checked instead of believed; anything not found in the code is marked as such, never filled in. Open SAT behaviour tasks (TASK-1270, TASK-1282, TASK-1273, TASK-1272) wait for this spec.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 docs/sat/thermo-nova-spec.md: the thermo-nova control design (start-up sequence, PID, heating curve, PWM incl. suppression, cycles, setpoint selection, safety/limits, DHW, areas, presets, solar/summer features, coordinator I/O) with formulas, constants, timings and file:line citations to the pinned commit
- [ ] #2 docs/sat/firmware-sat-spec.md: the same structure for the firmware's SAT with file:line citations to the source at the commit it describes
- [ ] #3 docs/sat/sat-comparison.md: per area match / differs / missing / firmware-only, each difference with both citations, impact and a proposed action; differences adversarially verified against the code
- [ ] #4 Published on origin/dev and linked in #alpha-testing for Sergeant D to validate; the open SAT behaviour tasks reference the spec
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Ground truth pinned: Alexwijn/SAT releases/thermo-nova @ 7beaaf5 (local copy verified identical). Workflow (45 agents): 8 areas (a1 start-up + loop timing, a2 PID, a3 heating curve + setpoint, a4 PWM/modulation/suppression, a5 boiler state + cycles, a6 areas/presets/window/sync, a7 solar/summer simmer/temperature history/health, a8 boiler I/O + DHW + safety + defaults); per area: thermo-nova spec writer (opus) -> citation auditor (opus, opens every file:line) -> firmware spec writer with the same headings -> comparison table -> adversarial verifier of every differs/missing/firmware-only row. Then a completeness critic (uncited control code in thermo-nova) and three assemblers: docs/sat/thermo-nova-spec.md, docs/sat/firmware-sat-spec.md, docs/sat/sat-comparison.md. Main session then spot-checks citations itself, commits docs-only, posts GitHub links for Sergeant D, and links TASK-1270/1272/1273/1282 to the spec.
<!-- SECTION:PLAN:END -->
