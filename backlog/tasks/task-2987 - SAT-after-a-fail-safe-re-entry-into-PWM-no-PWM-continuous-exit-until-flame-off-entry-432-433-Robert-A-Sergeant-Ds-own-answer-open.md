---
id: TASK-2987
title: >-
  SAT: after a fail-safe re-entry into PWM no PWM->continuous exit until
  flame-off (entry 432/433, Robert A; Sergeant D's own answer open)
status: Done
assignee:
  - '@otgw-dev'
created_date: '2026-11-21T01:44:00Z'
updated_date: '2026-11-25T13:18:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1116000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert (maintainer) answered Q-2005d with A (entry 432, confirmed in entry 433, #dev-sat-mqtt 2026-11-21). Once the 3.0 C fail-safe (SATcycles.ino:1434-1439, entry 429) has switched a burning flame back to PWM, neither the underheat exit (SATcycles.ino:1494-1500, entry 342) nor the saturation exit (:1509-1517) may fire again until the flame goes out. Today both exits ignore the entry-429 flag, so the loop repeats. Field evidence: indigo_light beta.392 19 Nov 21:23-21:49, one flame with 4 switches (21:32:30 saturation exit, 21:33:30 fail-safe, 21:36:30 underheat exit, 21:39:00 fail-safe); gruijter beta.401 20 Nov, 3 flames each PWM -> continuous -> fail-safe PWM. No thermo-nova counterpart (heating_control.py:262-305, :307-376). Sergeant D's own answer to Q-2005d is still open; the wider minimal-switching proposal (margin on the entry-342 exit) is separate and goes to him first.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A host test reproduces the indigo_light loop on the real satCycleCheckOvershootSwitch()/satCycleCheckPwmExits() code: after a fail-safe re-entry the underheat or saturation exit fires again (red on the current code)
- [ ] #2 With the fix the same test shows no PWM->continuous exit after a fail-safe re-entry until flame-off, and the first PWM->continuous exit of a flame (before any fail-safe) still works
- [ ] #3 Flame-off clears the state: the next flame can leave PWM again
- [ ] #4 Bench via the NAS (replay or simulation, per the NAS recipe) shows at most one fail-safe switch per flame
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 03:34 UTC: ON HOLD. Sergeant D (entry 439) chose to build only the duty fix (TASK-2993) and drop Q-2005d/e/f, which conflicts with Robert's decision behind this task. Put to Robert in #dev-sat-mqtt (msg 1573542401606426685). Do not build until Robert answers.

Parked by Robert's A 2026-11-21 (DM 1573595175458963567, reply to 1573542823507140729) after entry 439: for 2.0.0 only the duty fix (TASK-2993), these guards move to 2.1.0. Built + tested on lane/d3-sat2987 (6e1eacf30), kept local as archive branch, not pushed. Reconsider after field data.
2026-11-21 (92): Robert chose A (entry 442, DM 1573595175458963567): follow Sergeant D, the duty fix only for 2.0.0; this guard moves to the 2.1.0 backlog. The built code (beta.406+6e1eacf) is not pushed for 2.0.0.

2026-11-21 (92): entry 445 (Robert, #dev-sat-mqtt 07:23 UTC): one change at a time. First the duty fix (TASK-2993/2992) goes out in a public release, then these guards are the next change, in their own build. Release line (2.0.0 or 2.1.0) still open.

2026-11-21 (otgw-dev): see TASK-2989 notes/ACs of today (architect's second reading: on >= beta.410 the saturation exit is a real-shortfall exit; ADR-236 amendment two-step; stale comments). Both guards ride the same lane lane/d3-sat2987 (6e1eacf30), rebase onto dev before implementing for 2.1.0.

2026-11-25 otgw-docs (95): milestone moved to 2.0.0 on Robert's DM A (via the coordinator). Overlap warning for otgw-dev, who owns this code area (SAT PWM exits): TASK-2987 (no PWM -> continuous exit after the fail-safe re-entry until the flame goes out) overlaps with TASK-3039 / ADR-296 (Proposed: one PWM ON clock per flame, frozen while continuous, resumed on the PWM re-entry, entries 475-479, which are about exactly these fail-safe flips) and with TASK-3008 / ADR-236 and ADR-297 (the rewritten underheat exit). Re-read this task after the freeze (TASK-3039) lands; do not implement it against the old exit code. Note: ADR-236 (Accepted) still says TASK-2987 and TASK-2989 'stay 2.1.0 per entry 448' and ADR-236 option C 'not chosen'; with this milestone change the ADR text and the backlog disagree, so the ADR needs one line or the milestone a reason.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as superseded, nothing landed. The lane lane/d3-sat2987 (6e1eacf30 'fail-safe latch and underheat margin', 9247c2252 build evidence, beta.406+6e1eacf) was never pushed to dev. Why dropped (architect verdict, relayed by the coordinator 2026-11-25): the latch after a fail-safe re-entry contradicts entry 448 (PWM to continuous only on the underheat exit, 180 s, flow <= request - 3.0) and entry 478/475 (one PWM ON clock per flame, TASK-3039 freeze, which landed in beta.428); the 1.5 C margin of TASK-2989 is replaced by entries 448, 456, 463 and 492. Landed instead: TASK-3008 (448 exit with the 456 flame gate), TASK-3039 (freeze), entry 429 flag _cycle_burnLeftPwm. Nothing of this task is in firmware. Preserved, not landed: the replay test tests/test_sat_switch_replay_2987_2989.py and its fixtures (tests/fixtures/sat_replay_2987/) are in OTGW-worktree-leftovers/d3-bundles/sat2987.bundle (also sat2987-presquash.bundle), verified with git bundle verify, SHA256SUMS present. Evidence this session: none new (no code change); the decision is the architect's, recorded here.
<!-- SECTION:FINAL_SUMMARY:END -->
