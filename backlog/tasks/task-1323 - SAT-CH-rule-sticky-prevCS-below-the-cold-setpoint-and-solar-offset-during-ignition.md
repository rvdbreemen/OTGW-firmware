---
id: TASK-1323
title: >-
  SAT CH rule: sticky prevCS below the cold setpoint and solar offset during
  ignition
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-23T15:56:00Z'
updated_date: '2026-10-23T17:18:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 446000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-1316 review (opus), SAT behaviour, needs Sergeant D: (1) PWM mode with overshoot protection off (or pidOutput == cold): satApplyPWM returns prevCS (_sat_lastControlCS, written back every tick), e.g. a suppression CS of 24 on radiators stays; pwmDrivesSetpoint is false, so CH follows the cold rule (24 < 28.2) -> CS=24 held with CH=0 until a mode change (HEAD sent CH=1). Options: decide CH on pidOutput vs the cold setpoint on that path, or reset/floor prevCS. (2) Solar during PWM ON: hold minus fSolarSetpointOffset can clamp to CS=10 while CH=1 (SATcontrol.ino ~5000-5003): the boiler never lights and the 180 s ignition timeout fires; the CS side predates TASK-1316.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's choice for both cases recorded with citations
- [ ] #2 Implemented with a host test per case (red before, green after)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 16:16Z Sergeant D: 'Follow the thermo-nova about how it is done' (reply to the TASK-1323/1322 questions). Finding: thermo-nova applies the solar offset to requested_setpoint (climate.py:370-393: pid output, areas, overshoot cap, minus solar_gain.setpoint_offset_celsius at :391-392, then clamp MINIMUM..maximum), i.e. BEFORE the PWM/continuous step; our firmware subtracts it from the CS after the PWM step (SATcontrol.ino main block) -> deviation that causes case 2. Next: trace thermo-nova for case 1 (PWM with overshoot protection off) with citations, then a plan.

2026-10-23 16:45 thermo-nova trace (opus agent, all citations opened; second reader = main session re-opened climate.py:370-394, heating_control.py:68-74/383-399/439-442 and firmware SATcontrol.ino:958/:5001): A) thermo-nova subtracts solar from requested_setpoint before the mode step (climate.py:391-392), hold clamp(return+offset, 22, max) (heating_control.py:398) and suppression flow-offset (:441) have no solar term; firmware subtracts solar from the CS after the mode step (:5001) and in the 3 s refresh (:958). B) PWM with protection off: thermo-nova returns early (:385-386) and resends the last CS, CH = CS > 22 (:238); firmware prevCS is sticky the same way, differences: fallback to pidOutput when last CS<=10 (:913), pre-solar storage, 28.2/21 instead of 22, cutoff '<' vs gate '<='. C) case 2 disappears if solar moves upstream; case 1 stays (CH=0 at 24 comes from the 28.2 threshold). Freeze deviations: bSolarFreezeIntegral never read (SATpid.ino:163), firmware resets the integral while frozen (thermo-nova skips, pid.py:167-168). Proposal + 3 open points sent to Sergeant D (msg 1563229693308833825). Waiting; no implementation before his OK.

2026-10-23 16:47Z Sergeant D (msg 1563232315969249376): '1. The switch thresholds are 28.2/ 21. If the requested setpoint is lower than the thresholds then we send CH=0 CS=10  2. Yes we use 28.2/21  3. yes, follow thermo-nova'. Proposal A (solar on the requested setpoint before the mode step) not corrected -> accepted (he asked earlier to follow thermo-nova). Resulting rule: requested = pidOutput after the solar offset; requested < cold (28.2/21) -> CS=10 + CH=0 (cutoff; thermo-nova uses <= at heating_control.py:385 / > at :72, keep the firmware's existing comparison semantics but make cutoff and PWM gate consistent); otherwise CH=1, except the PWM OFF phase while PWM drives the CS (CS=10 + CH=0). Case 1 (protection off, held CS 24, requested >= 28.2) -> CH=1. Freeze: honour bSolarFreezeIntegral and skip the integral reset while frozen (climate.py:501, pid.py:167-168).
<!-- SECTION:NOTES:END -->
