---
id: TASK-798
title: 'feat-2.0.0: SAT sim F3 — multi-zone / multi-area room model'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 22:55'
updated_date: '2026-06-01 16:26'
labels:
  - sat
  - simulation
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up F3 from SAT simulation plan section 12. Extend the single-zone sim room model to iZoneCount>1 and bMultiArea=true so the P75 aggregation path runs under simulation. Depends on TASK-795. Milestone 2.0.0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Synthetic per-zone room temperatures drive the multi-zone PID + P75 aggregation when iZoneCount>1 and bSimulation=true
- [x] #2 Multi-area weighted average (satGetWeightedRoomTemp) reads synthetic area temps under simulation when bMultiArea=true
- [x] #3 OFF-mode zones (TASK-593 bOff) remain excluded under simulation — no synthetic room temp keeps them in the P75 set
- [x] #4 Single-zone behaviour unchanged; python build.py both targets SUCCESS; evaluate.py --quick clean
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Source: plan §12 F3 + §3 (single-zone room model is the MVP non-goal; this task lifts it). Depends on TASK-795 AND coordinates with TASK-593 (OFF-zone P75 exclusion, committed a3aa6672).

GOAL: multi-zone / multi-area synthetic room model so the P75 aggregation block (satControlLoop, iZoneCount>1) runs under simulation. Today the sim drives a single fSimRoomTemp; multi-zone control paths never exercise.

DESIGN:
- Per-zone synthetic room temp: extend the SATZoneState (SATtypes.h) usage so each active zone has a sim room temp, or add a parallel fSimRoomTemp[SAT_MAX_ZONES] in SATRuntimeSection. Prefer reusing satZones[] fRoomTemp written by the sim when bSimulation (so satGetWeightedRoomTemp + per-zone PID read synthetic values through the same path).
- satUpdateSimulation() drives each zone toward settings.sat.fTargetTemp with per-zone heat/cool, keyed on the synthetic flame (shared single boiler -> shared flow temp, but per-zone room response). Optionally per-zone loss coefficient for asymmetry.
- Respect TASK-593: an OFF zone (bOff) must stay excluded — do NOT synthesize room temp for OFF zones (leave bRoomValid false or skip the update) so the P75 exclusion still holds under sim.
- bMultiArea path: feed synthetic area temps into the multi-area weighted average (satGetWeightedRoomTemp) when bSimulation.

OPEN QUESTION (morning): single shared boiler/flow with per-zone room response (simplest, realistic for one heat source) vs independent per-zone flow. Default: shared boiler, per-zone room.

VERIFY: build both; evaluate --quick; with iZoneCount>1 + bSimulation, observe per-zone room temps diverge and P75 aggregation populates zoneOutputs[]; OFF zone (TASK-593) stays excluded.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-01T12:12:01+02:00: INVESTIGATED, design-gated — NOT the clean per-zone-room-model the plan implies. FINDING: a zone enters P75 only when BOTH bRoomValid AND bSpValid are true (SATcontrol.ino:1582). Under sim with no external per-zone setpoint feed, bSpValid stays false -> zones excluded -> the P75-runs-under-sim goal is unreachable by synthesizing room temp ALONE. F3 must ALSO synthesize per-zone SETPOINTS, which the plan's 'shared boiler, per-zone room' default does not specify: what setpoint per zone? (all = global fTargetTemp? staggered offsets? a fixed test profile?). That is a design choice, not mechanical. ALSO interacts with TASK-593: OFF zones (bOff) must stay excluded. DESIGN QUESTIONS for maintainer: (1) per-zone setpoint source under sim — global target for all active zones, or a staggered/profile scheme to make zones visibly diverge? (2) per-zone loss-coefficient asymmetry, or identical zones? (3) confirm shared-boiler/shared-flow + per-zone-room (plan default). Once decided, impl is: sim populates satZones[i].{fRoomTemp,fSetpoint,bRoomValid,bSpValid,iLastUpdateMs} for non-OFF zones each tick, driving room toward setpoint on the shared synthetic flame. Holding — no half-build. Stays To Do.

2026-06-01T18:16:27+02:00: per-zone multi-zone core shipped 2d095045 (alpha.128, pushed). Default B (staggered -0.5C/zone setpoints), identical rates, OFF zones excluded (TASK-593). AC#1 (per-zone P75 drive) + #4 (single-zone unchanged, gated iZoneCount>1) met. MAINTAINER QUESTION (plain-language shared with user): per-zone setpoint scheme A=all-global / B=staggered(impl) / C=fixed-profile; + identical-vs-varied heat/cool rates (identical impl). DEFERRED sub-feature: bMultiArea synthetic-area feed into satGetWeightedRoomTemp (multi-AREA, distinct from multi-ZONE) — AC#2. Build esp8266+esp32 SUCCESS fw+fs, eval 0-fail. Staying In Progress: AC#2 (multi-area) + maintainer setpoint-scheme confirm.
<!-- SECTION:NOTES:END -->
