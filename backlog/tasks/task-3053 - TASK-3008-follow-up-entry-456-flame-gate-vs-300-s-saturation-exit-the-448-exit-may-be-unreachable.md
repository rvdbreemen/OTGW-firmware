---
id: TASK-3053
title: >-
  TASK-3008 follow-up: entry 456 flame gate vs 300 s saturation exit (the 448
  exit may be unreachable)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-25T10:24:00Z'
updated_date: '2026-11-25T17:11:00Z'
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
- [x] #2 Bench timing from the flip test (which exit fires, at what second) is recorded in the task
- [x] #3 If confirmed: one question to Sergeant D with the options and the 2.0.0 scope, answer recorded in the decisions register before any change
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 entry 492 (Sergeant D: A, keep as built). Only code touch: a comment at both exits in SATcycles.ino ('entry 492: saturation is not flame-gated by design', cold start ends PWM at about 300 s, underheat reached on a mid-burn request step), committed with TASK-3054's bump. Docs side: otgw-docs 0656038fd.

2026-11-25 comment-only code touch built with TASK-3054: 0742d150a, beta.429, build OK, evaluate --quick 89/0/0. Not bench-tested, no behaviour change (comment). ACs 1 and 2 (architect confirm, bench timing) are still open; the Sergeant D answer is entry 492 (A).

2026-11-25 (BlueFlame): indigo_light beta.410 capture 24 Nov 21:44 to 25 Nov 12:04 analysed (OTGW-field-data/indigo_light/2026-11-25_beta.410_capture/analysis/facts.md + compare_428.md). Finding for this task: in beta.410 the underheat exit is 'request above flow for 60 s'; it fired 10 times in the 27 PWM/continuous switches (7 seen in telnet, 3 inferred from MQTT), the 5 seen at night 77-96 s after ignition; no 'PWM saturation' and no at-max line at all in that run. beta.428 changes this (flow 3.0 C under the request for 180 s after 180 s of flame, entries 456/463; cold-start order per entry 492: saturation at about 300 s first). Posted to the tester as measured facts only (message 1575114646929539125).

2026-11-25 otgw-dev: AC2 ticked: bench timing recorded from the flip test (bench-tester, RESULTS_E6_E7.md, 2.0.0-beta.432 on .63, simulator): A1 cold start: no exit in the first 60 s, no underheat line, saturation 300.0 s after the first ON tick; A2 mid-burn step: underheat exit 180.0 s after the step, saturation would have come 120 s later; negative control: saturation at 300.0 s; W3 observation: after a DHW tap both timers count from 300 s after the tap (SATcycles.ino satDhwGuardActive, entry 108). AC3 ticked: the question was answered by Sergeant D (entry 492, A: keep as built) and recorded before the only code touch (comments, 0742d150a). Documentation by otgw-docs 0656038fd and 34cce5132. AC1 (otgw-architect second reader) is still open: no confirmation is recorded in this task. The task stays In Progress for that AC only.
<!-- SECTION:NOTES:END -->
