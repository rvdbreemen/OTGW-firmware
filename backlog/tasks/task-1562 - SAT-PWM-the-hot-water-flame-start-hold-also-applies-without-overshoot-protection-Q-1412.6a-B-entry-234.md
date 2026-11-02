---
id: TASK-1562
title: >-
  SAT PWM: the hot-water-flame start hold also applies without overshoot
  protection (Q-1412.6a B, entry 234)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T16:39:00Z'
updated_date: '2026-11-02T17:00:00Z'
labels:
  - sat
  - pwm
dependencies: []
priority: high
ordinal: 714000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-02 answered Q-1412.6a with B (entry 234, changes entry 133): the PWM ON start is held while a flame lit during hot water still burns (entries 105, 106) also with overshoot protection off, because entry 230 (beta.11, TASK-1412.6) makes every OFF phase stop the boiler without protection too. Firmware beta.11: satPwmHoldForDhwFlame gates on settings.sat.bOvershootProtection explicitly (SATcontrol.ino ~1159). Change: drop that explicit gate; satPwmDrivesSetpoint already covers no DHW and requested setpoint above the cold setpoint. Test: tests/test_sat_pwm_anchors.py test_k3 flips to 'held until the flame is out' (start at 1350, as e4).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real SATcontrol.ino: protection off, hot-water flame still burning at the OFF->ON start: the start waits until the flame is out (red on beta.11, green on the fix)
- [x] #2 Protection on unchanged (e4, h1-h4, k1, k2, k4 green); full tests folder green
- [x] #3 firmware-sat-spec.md and entry 231 references updated
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Drop the explicit settings.sat.bOvershootProtection gate in satPwmHoldForDhwFlame (SATcontrol.ino ~1159); satPwmDrivesSetpoint keeps no DHW and requested setpoint above the cold setpoint, and is true in the OFF phase whatever the option is (entry 230). Test k3 flips to start at 1350 (red on beta.12). Docs: comment, firmware-sat-spec.md, entry 231 is superseded on this point by 234 (no rewrite). Alpha track: beta.13 bump, shares the beta.12 build/bench.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence: k3 red on beta.12 (start 1230 s), green on fix (1350 s); anchors 53/53, override/hp/start-limit/no-return OK; full tests/ 94/94 (test_adr_governance fixed: the ADR-206 bullet was missing in docs/adr/README.md, added in this commit); evaluate --quick 0 failed. Spec + CHANGELOG updated; entry 231 left as written (entry 234 supersedes its hold point).
<!-- SECTION:NOTES:END -->
