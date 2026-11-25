---
id: TASK-3053
title: >-
  TASK-3008 follow-up: entry 456 flame gate vs 300 s saturation exit (the 448
  exit may be unreachable)
status: To Do
assignee: []
created_date: '2026-11-25T10:24:00Z'
labels:
  - sat-question
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1156000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
CLAIM (reading of the code on origin/dev 173b39345, NOT MEASURED; second reader pending: otgw-architect). Since entry 456 the PWM -> continuous underheat exit (entry 448) needs 180 s of flame plus 180 s of flow <= request - 3.0 C, so it cannot fire earlier than about 360 s after ignition (SATcycles.ino:47, :51, :1498-1509). The saturation exit (off time 0 for 300 s, SATcycles.ino:52, :1520-1528) can fire at about 300 s. In PWM the duty is (request - base) / (flow - base) capped at 1 (SATcontrol.ino:1716); whenever flow < request the duty is >= 1, which is range 5: continuous ON with off time 0 (SATcontrol.ino:1768-1786, _pwm_lastOffTimeMs = 0). So in a steady underheat the saturation timer starts at the first ON tick and fires first; the 448 exit would then never be the one that fires. Thermo-nova 7beaaf5 has no flame gate, so there underheat (180 s) wins over saturation (300 s). The at-max rule (SATcontrol.ino:8543-8546) can pre-empt as well (bench-63/2026-11-25_8401c72-448exit run 4, 240 s after ignition). Both exits end in the same continuous mode and set the same flag (entry 429), so control behaviour is similar; the difference is which rule fires and when (about 300 s versus 360 s) and that TASK-3008 AC4's positive exit may be unreachable. Entries: 448, 456, 463, 429.
OPEN: confirm or refute (otgw-architect second reader, then the bench timing from the flip test: OTGW-field-data/bench-63/flip-test-brief/BRIEF.md). If confirmed, one question to Sergeant D by the SAT rules (release 2.0.0 per Robert, entry 491): keep as is (saturation covers the exit), let the saturation exit not fire while the underheat condition holds, or shorten the 456 gate. No code until decided. TASK-3008 AC4 stays open.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Second reader (otgw-architect) confirms or refutes the claim with the file:line cites
- [ ] #2 Bench timing from the flip test (which exit fires, at what second) is recorded in the task
- [ ] #3 If confirmed: one question to Sergeant D with the options and the 2.0.0 scope, answer recorded in the decisions register before any change
<!-- AC:END -->
