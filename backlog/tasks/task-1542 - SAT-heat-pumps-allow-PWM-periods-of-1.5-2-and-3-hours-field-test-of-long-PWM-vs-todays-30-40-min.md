---
id: TASK-1542
title: >-
  SAT heat pumps: 600 s minimum ON for every heat pump, plus an experimental
  on/off heat-pump type with a 3-hour cycle
status: To Do
assignee: []
created_date: '2026-11-01T09:38:00Z'
updated_date: '2026-11-01T12:37:00Z'
labels:
  - sat
  - heat-pump
dependencies: []
priority: medium
ordinal: 698000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Replaces the earlier scope (1.5/2/3 h options on the ordinary heat-pump type, entry 193). Decided 2026-11-01:
- Entry 197 (Robert + Sergeant D): every heat pump in SAT PWM gets a minimum ON time of 600 s instead of 180 s (satGetMinOnTimeSec, SATcontrol.ino; today 180 s for every source, thermo-nova pwm.py:53). Gas keeps 180 s. Hybrid keeps 180 s until asked (it takes only the heat-pump cycle, entry 188).
- Entry 195 (Sergeant D's request) as designed in #dev-sat-mqtt: one extra entry in the appliance list, 'On/off heat pump, long cycle (experimental)', clearly marked experimental, meant for on/off heat pumps with large thermal mass. Stored as heat pump (satsource 2) plus iHpCycleSeconds 10800, so every heat-pump rule applies unchanged (40 C cap, no hourly limit, MM, entry 87 switch); ordinary heat pumps never get 10800 s. Minimum ON 600 s (entry 197 replaces the 3600 s of entry 195; round 18c: 3600 s costs 0.035-0.11 C comfort for about 1.4 fewer starts a day). Shown in hours, never cycles per hour: 'cycle 3 hours, minimum run 10 minutes'.
- Mapper bands at 10800 s / 600 s (dutyLower = 600/10800): no start below duty ~0.028, continuous ON from ~0.972. Validation of the mapper at 10800/3600 on the real code: entry 195.
- Who may choose the type: Q-1527b (A: only with heating system underfloor, advised; B: any system with a warning). Implement as decided there.
- Out of scope: minimum OFF / start-to-start limit (Q-1527d), ordinary heat-pump cycle options (Q-1527c, entry 194), degree minutes.
Evidence: docs/sat/sim-hp-lowload/round18b.md (rounds 18b/18c, PWM as the firmware runs it; on/off underfloor 180 min / 600 s: 7.7 starts/day at 0.318 C vs 34.6 at 0.314 C today). Field test: docs/sat/sim-hp-lowload/field-test-plan.md (arms: Heat pump 30 min vs the experimental type, switched by the appliance type).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 satGetMinOnTimeSec returns 600 s for a heat pump (both the ordinary and the experimental type), 180 s for gas and hybrid; host test against the real code red on HEAD, green after
- [ ] #2 Experimental type: setter/REST accept iHpCycleSeconds 10800 only together with the experimental appliance choice; the ordinary heat-pump type keeps 1800/2400; satGetPwmPeriodSec returns 10800 for it
- [ ] #3 Host test against the real PWM mapper at 10800 s / 600 s: on/off times correct in all 5 duty ranges, ON phase ends at the current ON time, 600 s minimum holds
- [ ] #4 Existing PWM, gas and hybrid tests stay green (tests/test_sat_hp_pwm.py and the other tests/test_sat_pwm_*.py)
- [ ] #5 v2 UI: the experimental type appears in the appliance list labelled experimental, shown in hours ('cycle 3 hours, minimum run 10 minutes'), availability as decided in Q-1527b
- [ ] #6 Bench run with satsimulation on the experimental type shows a full ON/OFF period with the expected times
- [ ] #7 Minimum PWM OFF time 600 s for heat pumps (entry 198), also when the duty asks for continuous ON (range 5 must not restart within 600 s of a stop); gas unchanged; host test red on HEAD, green after
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-01: entry 198 (Sergeant D): minimum PWM OFF 600 s, heat pumps only. With 600 s ON this gives >= 20 min start-to-start (NIBE F1345 p.18). Round 18d: no comfort or start effect in the model (safety net). Q-1527d answered.
<!-- SECTION:NOTES:END -->
