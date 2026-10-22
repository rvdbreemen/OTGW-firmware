---
id: TASK-1292
title: >-
  SAT specification from thermo-nova (ground truth), firmware SAT specification
  and a detailed comparison
status: Done
assignee:
  - '@claude'
created_date: '2026-10-22T05:18:00Z'
updated_date: '2026-10-22T18:22:00Z'
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
- [x] #1 docs/sat/thermo-nova-spec.md: the thermo-nova control design (start-up sequence, PID, heating curve, PWM incl. suppression, cycles, setpoint selection, safety/limits, DHW, areas, presets, solar/summer features, coordinator I/O) with formulas, constants, timings and file:line citations to the pinned commit
- [x] #2 docs/sat/firmware-sat-spec.md: the same structure for the firmware's SAT with file:line citations to the source at the commit it describes
- [x] #3 docs/sat/sat-comparison.md: per area match / differs / missing / firmware-only, each difference with both citations, impact and a proposed action; differences adversarially verified against the code
- [x] #4 Published on origin/dev and linked in #alpha-testing for Sergeant D to validate; the open SAT behaviour tasks reference the spec
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Ground truth pinned: Alexwijn/SAT releases/thermo-nova @ 7beaaf5 (local copy verified identical). Workflow (45 agents): 8 areas (a1 start-up + loop timing, a2 PID, a3 heating curve + setpoint, a4 PWM/modulation/suppression, a5 boiler state + cycles, a6 areas/presets/window/sync, a7 solar/summer simmer/temperature history/health, a8 boiler I/O + DHW + safety + defaults); per area: thermo-nova spec writer (opus) -> citation auditor (opus, opens every file:line) -> firmware spec writer with the same headings -> comparison table -> adversarial verifier of every differs/missing/firmware-only row. Then a completeness critic (uncited control code in thermo-nova) and three assemblers: docs/sat/thermo-nova-spec.md, docs/sat/firmware-sat-spec.md, docs/sat/sat-comparison.md. Main session then spot-checks citations itself, commits docs-only, posts GitHub links for Sergeant D, and links TASK-1270/1272/1273/1282 to the spec.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 workflow done (44 agents, 8 areas x writer/auditor/firmware writer/comparer/verifier + completeness critic + 3 assemblers): thermo-nova-spec.md 3378 lines / 1845 citations, firmware-sat-spec.md 3809 / 2037, sat-comparison.md 1002 lines, 443 rows: match 79, differs 281, missing in firmware 31, firmware-only 52; 434 rows (verified), 1 corrected, 8 match rows in area 8 unmarked. Main-session spot check: 16 random citations (10 py, 6 firmware) opened by hand, all hold. Firmware citations read at 2c194a329; SATcontrol/SATpid/SATcycles/safeTimers unchanged since 51df6b486. Not verified: the full citation set by hand (only the auditors' pass + the sample), the 'derived/computed' results (not run on a device).

2026-10-22 08:56 posted the three GitHub links in #alpha-testing for Sergeant D (message 1562696953316515853 thread); TASK-1270/1272/1273/1282 notes reference the spec. AC4 done; the task stays In Progress until his validation is recorded in docs/sat/ (his corrections rank above the code per the CLAUDE.md block).

2026-10-22 18:01 Sergeant D (#alpha-testing 1562888583004688629): 'I checked the files and everything looks good so far. Good job.' Recorded as entry 4 in docs/sat/sergeant-d-corrections.md. His earlier answers (duty formula, 180 s window, thermo-nova after-window behaviour, base temperature) are entries 1 and 3; the follow/push question he deferred to the maintainer is entry 2.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Three cited documents on dev: docs/sat/thermo-nova-spec.md (the design as Alexwijn/SAT releases/thermo-nova @ 7beaaf5 implements it, 1845 file:line citations, each reopened by an audit pass, 16 spot-checked by hand), docs/sat/firmware-sat-spec.md (what the firmware does today, same structure) and docs/sat/sat-comparison.md (443 points, 434 verified adversarially, executive summary). AC1-3: the documents; AC4: posted in #alpha-testing, and Sergeant D validated them on first reading (entry 4 of docs/sat/sergeant-d-corrections.md), with his decisions on the duty-cycle BT recorded as entry 1. CLAUDE.md/AGENTS.md carry the ground-truth block with the pinned commit, guarded by tests/test_sat_ground_truth_block.py. Not verified: the 'derived/computed' results were not run on a device; the pin check of the local copy is recorded in UPSTREAM_COMMIT and the diff -rq of 2026-10-22.
<!-- SECTION:FINAL_SUMMARY:END -->
