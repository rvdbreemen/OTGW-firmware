---
id: TASK-1360
title: 'SAT heating curve: round the curve value to 0.1 C as thermo-nova'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T20:21:00Z'
updated_date: '2026-11-04T05:57:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 495000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-24 (ADR-198 open question 3): round like thermo-nova heating_curve.py:39; the firmware does not (SATcontrol.ino:602-605). Effect at most 0.05 C on the computed flow. It touches control, so the plan goes to Sergeant D as a notice before implementation (ADR-196, skill step 4).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 satCalcHeatingCurve rounds to 0.1 like heating_curve.py:39 (host test red on HEAD, green); plan posted to Sergeant D first
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no rounding in satCalcHeatingCurve (SATcontrol.ino:746-761). Decided (ADR-198), notice only.

2026-11-04 Robert decided (maintainer decision, entries 257-262; Sergeant D informed msg 1567417361798144021, may object). See docs/sat/sergeant-d-corrections.md.
<!-- SECTION:NOTES:END -->
