---
id: TASK-1562
title: >-
  SAT PWM: the hot-water-flame start hold also applies without overshoot
  protection (Q-1412.6a B, entry 234)
status: To Do
assignee: []
created_date: '2026-11-02T16:39:00Z'
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
- [ ] #1 Host test on the real SATcontrol.ino: protection off, hot-water flame still burning at the OFF->ON start: the start waits until the flame is out (red on beta.11, green on the fix)
- [ ] #2 Protection on unchanged (e4, h1-h4, k1, k2, k4 green); full tests folder green
- [ ] #3 firmware-sat-spec.md and entry 231 references updated
<!-- AC:END -->
