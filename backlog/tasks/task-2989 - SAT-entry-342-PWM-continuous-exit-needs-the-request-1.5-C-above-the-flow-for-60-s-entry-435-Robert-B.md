---
id: TASK-2989
title: >-
  SAT: entry-342 PWM->continuous exit needs the request 1.5 C above the flow for
  60 s (entry 435, Robert B)
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-21T01:55:00Z'
updated_date: '2026-11-25T06:42:00Z'
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
- [ ] #7 Replay or field check on a build >= beta.410 (live-flow duty, TASK-2993): the saturation exit (off time 0 for 300 s) does not fire while the flow is above the request; a firing there is a new finding (otgw-architect 2026-11-21)
- [ ] #8 The ADR-236 amendment states the two steps: shortfall 0-1.5 C -> only the saturation exit (300 s at off time 0); shortfall >= 1.5 C -> the underheat exit (60 s), so the 0-1.5 C band is not read as a gap
- [ ] #9 Stale comments fixed in the implementation commit: SATcycles.ino fail-safe comment 3.5 C -> 3.0 C (entry 429) and SATcontrol.ino exit comment 180 s -> 60 s
- [ ] #10 Two more stale comments in SATcontrol.ino (otgw-docs reviewer, dev 8c929eee2): :1284-1285 PWM_IGNITION_TIMEOUT_MS (180 s) is no longer 'the start-up EMA window' (that is SAT_PWM_BT_FILTER_MS 30 s + 1 s slack); :1296-1297 MM=T flip wording: state it as in entry 440 (on 21:25:00 at 0.93, off 21:26:00 at 0.86, on 21:26:30-21:28:00; the 0.94 is a TASK-2993 replay tick at 21:25:30, not in the entry). Comment-only, so in the same commit as the next firmware change of this lane (bump)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 coordinator decision (relayed by 92): the margin is a CONSTANT 1.5 C for 2.0.0, one named constant next to the existing exit constants (SATtypes.h / SATcycles.ino), no new setting. Reasons: 8 days to stable; a setting adds UI, REST, HA discovery, settings migration and tests; YAGNI until a field case needs another value; Sergeant D can still change the value. Deliberately not symmetric with fPwmSwitchMargin (which is a setting). This settles AC #3.

2026-11-21 03:34 UTC: ON HOLD. Sergeant D (entry 439) chose to build only the duty fix (TASK-2993) and drop Q-2005d/e/f, which conflicts with Robert's decision behind this task. Put to Robert in #dev-sat-mqtt (msg 1573542401606426685). Do not build until Robert answers.

Parked by Robert's A 2026-11-21 (DM 1573595175458963567, reply to 1573542823507140729) after entry 439: for 2.0.0 only the duty fix (TASK-2993), these guards move to 2.1.0. Built + tested on lane/d3-sat2987 (6e1eacf30), kept local as archive branch, not pushed. Reconsider after field data.
2026-11-21 (92): Robert chose A (entry 442, DM 1573595175458963567): follow Sergeant D, the duty fix only for 2.0.0; this guard moves to the 2.1.0 backlog. The built code (beta.406+6e1eacf) is not pushed for 2.0.0.

2026-11-21 (92): entry 445 (Robert, #dev-sat-mqtt 07:23 UTC): one change at a time. First the duty fix (TASK-2993/2992) goes out in a public release, then these guards are the next change, in their own build. Release line (2.0.0 or 2.1.0) still open.

2026-11-21 (otgw-dev): three ACs added at otgw-architect's request (second reading of the duty-saturation finding): on dev since bce9496a5 (beta.410) the ON-phase duty uses the live flow, so duty 1.00 = request >= live flow and the saturation exit means a real shortfall; the earlier finding 'the margin alone does not prevent the saturation exit' applies to the beta.392/401 captures. Lane lane/d3-sat2987 (6e1eacf30) must be rebased onto current dev first.

2026-11-21 (otgw-dev): coordinator relayed two stale comments found by otgw-docs' reviewer; checked on dev 8c929eee2: the 180 s 'start-up EMA window' wording is stale (TASK-2993); the 0.86-0.94 range is from the TASK-2993 replay (21:25:30 gave 0.94) while entry 440 names 0.93 on / 0.86 off, so the comment is reworded to the entry's numbers.

2026-11-25 otgw-docs (95), assessed for the coordinator, NOT closed. (1) The rule this task changes is superseded as a rule: ADR-236 (Accepted 2026-11-25, rewritten to Sergeant D's entry 448) replaces the entry-342 exit (flame on, request above flow for 60 s) by thermo-nova's underheat exit (flow at least 3.0 C under the request for 180 s); ADR-297 (Accepted) adds the 180 s flame gate (entry 456) and keeps the at-max exit on the 60 s start gate (entry 463). ADR-236 lists this task as option B 'Entry 342 with a 1.5 C margin (TASK-2989). Not chosen (entry 448): 2-4 exits remain in the replay, and it stays 2.1.0' and under Must Not: 'Bring back the entry-342 60 s rule, or a margin or latch variant of it (TASK-2987 and TASK-2989 stay 2.1.0 per entry 448)'. Entry 484 keeps the 448 rules for 2.0.0. (2) But the code on dev (3f0b98f42 and later, SATcycles.ino:1494-1500) still has the old rule: TASK-3008 (the rewrite) is not on dev. So the task cannot be shown moot by code yet. (3) ACs 1 to 8 describe a margin on a rule that goes away with TASK-3008; ACs 9 and 10 are stale-comment fixes that do not: carry them into the TASK-3008 lane (the exit comment 180 s -> 60 s of AC9 becomes wrong again once the 180 s rule is back; check against the landed code first). Recommendation: leave it in 2.1.0; when TASK-3008 is on dev, close it as superseded by ADR-236 and ADR-297 (entries 448, 456) with the final summary naming them, after AC9 and AC10 are moved.
<!-- SECTION:NOTES:END -->
