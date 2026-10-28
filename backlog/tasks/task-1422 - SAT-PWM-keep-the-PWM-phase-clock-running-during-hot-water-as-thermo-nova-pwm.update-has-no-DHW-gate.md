---
id: TASK-1422
title: >-
  SAT PWM: keep the PWM phase clock running during hot water, as thermo-nova
  (pwm.update has no DHW gate)
status: To Do
assignee: []
created_date: '2026-10-28T07:36:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1421
references:
  - docs/sat/sergeant-d-corrections.md
priority: medium
ordinal: 586000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Review finding on the TASK-1382/1380/1383 candidate (correctness + SAT-decision lenses, 2026-10-28). The SAT control loop returns early while the boiler DHW bit is set (SATcontrol.ino ~5096-5113), so satPwmSchedule() is frozen during a draw. An ON phase whose ON time runs out during a draw only switches to OFF at the first tick after the draw, so _pwm_lastUpdateMs gets the end of the draw and the next ON comes up to a full off time late (example: duty 0.22 at 1200 s, ON from T, 20-min draw from T+10: thermo-nova ON again ~T+1210, firmware ~T+2146). thermo-nova calls pwm.update() whenever heater_state is ON (heating_control.py:202-215); heater_state has no DHW term (heating_control.py:64-72). The draws in gruijter's 2026-10-27 capture all fell in OFF phases, which the candidate fixes. Running the schedule during DHW interacts with start counting: an ON start during a draw with the DHW flame lit (entry 89, Q48 'do not count flame when DHW enable is triggered', Q48b open), so plan it with TASK-1421.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: an ON phase whose ON time ends during a hot-water draw switches to OFF at its own expiry and the next ON comes at expiry + off time (red on the TASK-1382 candidate, green on the fix)
- [ ] #2 No hot-water flame is counted as a PWM start (entry 89) with the schedule running during DHW
- [ ] #3 No PWM setpoint override during DHW (heating_control.py:385, unchanged)
- [ ] #4 build + evaluate green
<!-- AC:END -->
