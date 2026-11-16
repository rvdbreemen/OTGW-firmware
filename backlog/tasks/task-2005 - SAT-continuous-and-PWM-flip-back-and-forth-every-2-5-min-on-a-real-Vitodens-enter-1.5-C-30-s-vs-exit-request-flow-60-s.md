---
id: TASK-2005
title: >-
  SAT: continuous and PWM flip back and forth every 2-5 min on a real Vitodens
  (enter 1.5 C/30 s vs exit request>flow/60 s)
status: To Do
assignee:
  - '@ba'
created_date: '2026-11-16T16:35:00Z'
updated_date: '2026-11-16T16:39:00Z'
labels:
  - sat
  - field
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1026000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field finding from indigo_light's beta.350 capture (real Viessmann Vitodens, SAT on, simulation off; archived at RvdB/OTGW-field-data/indigo_light/20261116-144953/analysis/; analysis by comms 92, Sergeant D DM 1571924364050432032). Between 16:17 and 16:39 (tester local time) SAT switched modes four times while the flame burned without a break (16:18:36-16:51:12, 1955 s): 16:17:58 PWM (enter); 16:19:58 continuous ('flame on and request 38.1 above flow 26.1 for 60s'); 16:30:28 PWM (flow 42.4 >= 40.9 + 1.5 for 30 s); 16:32:28 continuous (request 41.4 above flow 35.2 for 60 s); 16:35:28 PWM (flow 41.9 >= 39.5 + 1.5); 16:37:28 continuous (request 39.0 above flow 37.1); 16:39:28 PWM (flow 40.7 >= 37.9 + 1.5) until PWM OFF 16:50:28. The control setpoint swings with the mode (e.g. 55.0 in a PWM ON phase, 38.1 in continuous). Rules involved: enter = TASK-1888 / entry 327 and 379 (flow >= request + 1.5 C for 30 s, SATcycles.ino:1393); exit = ADR-236 (Proposed; entries 339 and 342: flame on and request above flow for 60 s). Thermo-nova (pin 7beaaf5) uses a wide gap: enter flow >= request + 3.0 for 60 s (heating_control.py:288-296, const.py:21, cycles/const.py:10), exit flow <= request - 3.0 for 180 s (heating_control.py:335-343, const.py:22). Sergeant D decides whether this is acceptable and what the fix is (question Q-2005a with the scope line); no change before his answer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's answer recorded as an entry (with the release scope)
- [ ] #2 If a change: host test on the real satCycleCheckOvershootSwitch/exit path that replays the 16:17-16:39 sequence, red on the old code and green on the fix
- [ ] #3 Field check on a real boiler: no mode flip within one burn (or the accepted behaviour, per the entry)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-16 Q-2005a asked in #dev-sat-mqtt by 92: https://discord.com/channels/812969634638725140/1105556725714649128/1571927220631638081 (options A keep / B no PWM re-entry until the current burn ends / C thermo-nova exit; our advice B; scope: this release recommended).
<!-- SECTION:NOTES:END -->
