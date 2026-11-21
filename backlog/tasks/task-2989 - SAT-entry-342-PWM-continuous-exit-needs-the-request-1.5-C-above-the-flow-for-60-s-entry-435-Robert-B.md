---
id: TASK-2989
title: >-
  SAT: entry-342 PWM->continuous exit needs the request 1.5 C above the flow for
  60 s (entry 435, Robert B)
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-21T01:55:00Z'
labels:
  - sat
milestone: 2.0.0
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
<!-- AC:END -->
