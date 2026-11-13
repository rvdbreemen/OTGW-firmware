---
id: TASK-1889
title: 'SAT faster curve self-tune: hours not days (entry 329)'
status: To Do
assignee: []
created_date: '2026-11-13T08:07:00Z'
updated_date: '2026-11-13T16:06:00Z'
labels:
  - sat
  - adr
milestone: 2.0.0
dependencies: []
ordinal: 962000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-13 (https://discord.com/channels/812969634638725140/1105556725714649128/1570710410427109397) accepted Robert's proposal (https://discord.com/channels/812969634638725140/1105556725714649128/1570705537383800905, https://discord.com/channels/812969634638725140/1105556725714649128/1570707806607245357): keep the current control and self-tune structure but let the coefficient tune react in hours instead of days. Entry 329 in docs/sat/sergeant-d-corrections.md (replaces entry 325 / TASK-1886). Today: at most 0.1 per step, at least 3 counted days between steps (ADR-205, ADR-228, ADR-230, ADR-231). Interpretation to confirm: the ADR-228 two-phase signal stays, with a much shorter window and step interval. Open: window, step size, minimum time between steps, which exclusions stay; Q-1882a and the TASK-1883 tuner observation apply. The confirmation question goes with the PID research report the coordinator posts.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Plan with thermo-nova citations (pid.py:185-189, const.py:16) and the ADR-228/ADR-230 timing, plus Sergeant D's confirmation of the interpretation, before code
- [ ] #2 ADR that supersedes the ADR-205/ADR-228/ADR-230 timing
- [ ] #3 Host test on the real tuner: red on the old timing, green on the new (tests/test_sat_tuner_convergence.py as base)
- [ ] #4 Bench test by the bench-tester
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 hard constraint from gruijter beta.218 capture (12 Nov 21:39 - 13 Nov 16:07; Sergeant D asked 1570830517580992553, answered 1570832004922998918): the coefficient stayed 1.80 (121 published heating_curve_coeff values, all 1.8) and the self-tune counted 0 of 17 evaluated hours (SATTUNEH counted=0 every hour): 7x min_mod (gas boiler outside PWM at or below minimum modulation, SATcontrol.ino:6031), 6x sun (sun elevation >= 12 deg, :6027, a position rule not measured sunshine), 4x not_sat (capture start and the first hour after each of the 3 restarts 04:00/08:38/11:59, SAT not active, :6024). Each restart also reset the hour count. Consequence: a faster self-tune (entry 329) gets no signal on this house unless the exclusions change; changing the exclusions is a Sergeant D question. Coordinator puts this into the Fable PID report as a hard constraint.
<!-- SECTION:NOTES:END -->
