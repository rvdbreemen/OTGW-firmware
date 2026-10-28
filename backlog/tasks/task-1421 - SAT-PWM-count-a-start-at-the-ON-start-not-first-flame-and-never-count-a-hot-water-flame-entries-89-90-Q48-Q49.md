---
id: TASK-1421
title: >-
  SAT PWM: count a start at the ON start (not first flame) and never count a
  hot-water flame (entries 89, 90; Q48, Q49)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-28T06:47:00Z'
updated_date: '2026-10-28T09:52:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1382
references:
  - docs/sat/sergeant-d-corrections.md
priority: high
ordinal: 585000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-28: Q49 B (count at the OFF->ON start, every start, as pwm.py:160; entry 90) and Q48 A with 'do not count flame when DHW enable is triggered' (entry 89). Move counting to satPwmBeginOn, drop the first-flame count, exclude a flame lit while the boiler DHW-active bit was set (Q48b may refine). Hard dependency: TASK-1382 (Q35) in the same build or earlier, else slow ignition burns the hourly limit. Feeds TASK-1412.9.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: an unlit ON start counts (red on HEAD, green on fix)
- [ ] #2 build + evaluate green
- [x] #3 Characterization: no flame (CH or hot water) adds a count; only ON starts count (a CH or hot-water flame inside an ON phase leaves cycles_this_hour unchanged)
- [ ] #4 Bench old vs fix (Classic .63): an ON phase whose flame never lights does not move cycles_this_hour on alpha.516, adds 1 on the fix
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
One candidate with TASK-1422 (both change PWM under hot water). Decisions: entries 59, 89, 90 (Q49 B), 95 (Q48b: boiler DHW-active bit, MsgID 0 slave bit 2). thermo-nova counts every OFF/IDLE->ON start (pwm.py:159-160).
Change (SATcontrol.ino): count in satPwmBeginOn for every start, lit or not; remove the first-flame count in satPwmSchedule (if flame && bPwmFlameRequested -> satPwmCountStart). Exception (entry 89/95): do not count when the flame burning at the start was lit while the boiler DHW-active bit was set. Track that in the flame-edge handler (SATcycles.ino satCycleOnFlameChange ~724-741): record a flag 'flame lit during DHW' from the live slave status bit (OTcurrentSystemState.SlaveStatus & 0x04, because state.sat.bDhwActive is refreshed after the edge handler at SATcontrol.ino ~5056 vs ~5089); getter satCycleFlameLitDuringDhw(). Range 5 and PWM entry starts follow the same rule. Hour-limit check order unchanged (entry 59, Q48 A).
Proof: host tests in tests/test_sat_pwm_anchors.py / test_sat_pwm_start_limit.py: (a) an unlit ON start counts (red on HEAD 0acead3), (b) a DHW-lit flame carrying into an ON start does not count (red on HEAD), (c) a CH ignition inside an ON phase is counted once (only the start), regression of start_limit suite updated where it encodes first-flame counting (cite entry 90).

Self-review (advisor): plain reading of entry 95. Under Q49 B the counter counts ON starts, not flames, and no ON start happens while the boiler DHW bit is set (schedule frozen during a draw), so no 'lit during DHW' flag; carry-over flame after a draw counts as a normal ON start (Sergeant D expects the flame off then). Remove the first-flame count and _pwm_startCounted if no other reader. test_sat_pwm_start_limit 'slow' scenario flips (flameless starts now count, entry 90): update with citation.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 Q48b (entry 95): the bit is the boiler DHW-active bit (MsgID 0 slave bit 2): a flame while it is set is never counted.

2026-10-28: implemented (count in satPwmBeginOn at every start; first-flame count, satPwmCountStart, _pwm_startCounted removed). Host: tests/test_sat_pwm_anchors.py 14 OK; e1 (unlit ON start counts) FAILED on the alpha.516 source, OK on fix; e2/e3/e4 characterization (e4 = entry 101 reading). test_sat_pwm_start_limit 17 OK with 3 expectations flipped per entry 90 (range-5 start counted at start; slow scenario count 1->2, more 2->1; entry ON flame-off counted). Review (opus correctness + sonnet tests/decisions): no code defect; docs + comments + entry 101 added. Compile esp32-combo SUCCESS; evaluate --quick 0 failed.
<!-- SECTION:NOTES:END -->
