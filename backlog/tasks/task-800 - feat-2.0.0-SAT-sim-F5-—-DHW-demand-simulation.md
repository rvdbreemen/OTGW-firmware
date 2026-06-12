---
id: TASK-800
title: 'feat-2.0.0: SAT sim F5 — DHW demand simulation'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-31 22:56'
updated_date: '2026-06-01 17:16'
labels:
  - sat
  - simulation
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up F5 from SAT simulation plan section 12. Model DHW demand (flame steal, CH-vs-DHW cycle fraction) under simulation. Depends on TASK-795. Milestone 2.0.0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Synthetic DHW demand drives the DHW status bit so the cycle classifier tags cycles SAT_CK_DHW / SAT_CK_MIXED under simulation
- [x] #2 Flame-steal: CH contribution suppressed while a synthetic DHW draw is active; flow targets DHW setpoint
- [x] #3 DHW draws can be triggered both on a light periodic schedule and via the TASK-797 /sim/event dhw_demand override
- [x] #4 No real bus traffic (commit-3 gate honoured); python build.py both targets SUCCESS; evaluate.py --quick clean
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Source: plan §12 F5 + §5.3 (modulation) + §4.1 (DHW must not actuate the real bus). Depends on TASK-795; overlaps TASK-797 F2 (dhw_demand event stub).

GOAL: model DHW (domestic hot water) demand under simulation — flame steal and CH-vs-DHW cycle fraction — so the cycle classifier sees mixed CH/DHW cycles (SATCycleKind SAT_CK_DHW / SAT_CK_MIXED already exist in SATtypes.h) and DHW-related health paths run.

DESIGN:
- Synthetic DHW demand source: either a periodic schedule (e.g. morning/evening draw) or triggered via the F2 /sim/event dhw_demand. When DHW active: force the synthetic boiler to DHW mode (flame on, flow ramps toward DHW setpoint not CH setpoint), suppress CH contribution (flame-steal) for the draw duration.
- Set the synthetic status bits / cycle kind so satCycleOnFlameChange + the classifier tag the cycle SAT_CK_DHW or SAT_CK_MIXED. Confirm how the real path distinguishes DHW (OT status bit 2 = DHW active, seen at SATcycles.ino:652) and mirror it: under sim, drive that synthetic status bit.
- DHW setpoint: reuse an existing DHW setpoint setting if present; else a sim const.
- Strictly no bus traffic (the commit-3 gate covers it); this only moves synthetic state.

OPEN QUESTION (morning): scheduled DHW draws (autonomous, exercises classifier passively) vs purely event-driven via F2. Default: support both — a periodic light schedule + F2 override.

VERIFY: build both; evaluate --quick; with bSimulation, trigger a DHW draw -> observe a cycle classified SAT_CK_DHW/MIXED and CH suppressed during the draw.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-01T18:44:38+02:00: F5 core shipped 373a6038 (alpha.131, pushed). Decision 3a+3c. DHW draw forces bDhwActive -> CH suppressed (flame-steal) -> classifier sees SAT_CK_DHW/MIXED; flame demanded regardless of CH. Light 30-min schedule + F2 dhw_demand event. AC#1/#3/#4 met. AC#2 PARTIAL (left unchecked): flame-steal done, but 'flow targets DHW setpoint' NOT implemented — synthetic flow still tracks CH setpoint during draw. Minor follow-up; F5 goal (DHW/MIXED classification) achieved without it. Maintainer: accept F5 with the flow-target as a noted follow-up, or want the DHW-flow-target added before closing 800?
<!-- SECTION:NOTES:END -->
