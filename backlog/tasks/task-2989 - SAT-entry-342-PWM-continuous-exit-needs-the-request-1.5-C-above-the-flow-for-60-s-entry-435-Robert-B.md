---
id: TASK-2989
title: >-
  SAT: entry-342 PWM->continuous exit needs the request 1.5 C above the flow for
  60 s (entry 435, Robert B)
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-21T01:55:00Z'
updated_date: '2026-11-21T07:08:00Z'
labels:
  - sat
milestone: 2.1.0
dependencies: []
priority: high
ordinal: 1117000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert (maintainer) answered Q-2005e with B (entry 435, #dev-sat-mqtt 2026-11-21 01:51 UTC); Sergeant D's own answer is still open. The underheat exit (SATcycles.ino:1494-1500, entry 342: PWM ON, flame on, request above flow for 60 s) gets a margin: request - flow >= 1.5 C for 60 s (value read from the question). Reason: during PWM ON the CS is flow - fModSupOffset (1.0 C, SATcontrol.ino:1300-1306, as thermo-nova heating_control.py:438-441), which holds the flow just under the request, so the 0 C exit fired in 4 of 4 long field flames at request - flow 0.1-1.0 C (gruijter beta.401 20 Nov x3, indigo_light beta.392 19 Nov), then overshoot and the fail-safe back to PWM. Analysis in #dev-sat-mqtt msgs 1573515307388244040 and 1573515310277984268. Beside TASK-2987. Open beside it: duty 1.00 soon after ignition (saturation exit), otgw-dev is checking.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A host test on the real satCycleCheckPwmExits() shows the exit firing at request - flow 0.5 C for 60 s on the current code (red) and not with the fix
- [ ] #2 With the fix the exit still fires at request - flow >= 1.5 C held for 60 s, and the timer resets when the difference drops below 1.5 C
- [ ] #3 The margin stays above fModSupOffset; constant or setting (like fPwmSwitchMargin) decided with the coordinator and documented
- [ ] #4 Replay of the gruijter 20 Nov and indigo_light 19 Nov sattick data through the new rule shows no underheat exit in the four flames
- [ ] #5 Bench via the NAS (replay or simulation) before the push
- [ ] #6 The replay with A+B (TASK-2987 + this task) reports how many saturation exits (off time 0 for 300 s) remain in the four flames, because duty reaches 1.00 when BT (seeded at ignition, EMA for 180 s, then held; SATcontrol.ino:1648/1669-1686, as thermo-nova pwm.py:144-149, 186-193, 229-239) stays at or below the request
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 coordinator decision (relayed by 92): the margin is a CONSTANT 1.5 C for 2.0.0, one named constant next to the existing exit constants (SATtypes.h / SATcycles.ino), no new setting. Reasons: 8 days to stable; a setting adds UI, REST, HA discovery, settings migration and tests; YAGNI until a field case needs another value; Sergeant D can still change the value. Deliberately not symmetric with fPwmSwitchMargin (which is a setting). This settles AC #3.

2026-11-21 03:34 UTC: ON HOLD. Sergeant D (entry 439) chose to build only the duty fix (TASK-2993) and drop Q-2005d/e/f, which conflicts with Robert's decision behind this task. Put to Robert in #dev-sat-mqtt (msg 1573542401606426685). Do not build until Robert answers.

Parked by Robert's A 2026-11-21 (DM 1573595175458963567, reply to 1573542823507140729) after entry 439: for 2.0.0 only the duty fix (TASK-2993), these guards move to 2.1.0. Built + tested on lane/d3-sat2987 (6e1eacf30), kept local as archive branch, not pushed. Reconsider after field data.
2026-11-21 (92): Robert chose A (entry 442, DM 1573595175458963567): follow Sergeant D, the duty fix only for 2.0.0; this guard moves to the 2.1.0 backlog. The built code (beta.406+6e1eacf) is not pushed for 2.0.0.
<!-- SECTION:NOTES:END -->
