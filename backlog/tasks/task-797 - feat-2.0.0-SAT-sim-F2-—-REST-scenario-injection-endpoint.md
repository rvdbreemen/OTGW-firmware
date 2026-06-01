---
id: TASK-797
title: 'feat-2.0.0: SAT sim F2 — REST scenario-injection endpoint'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 22:55'
updated_date: '2026-06-01 08:19'
labels:
  - sat
  - simulation
  - rest-api
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up F2 from SAT simulation plan section 12. Add /api/v2/sat/sim/event for window_open, solar_gain, dhw_demand, pressure_drop, pv_surplus so bench testers can drive scenarios. Depends on TASK-795. Milestone 2.0.0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 POST /api/v2/sat/sim/event added to kV2Routes[] in restAPI.ino, parsed without ArduinoJson
- [x] #2 Accepts window_open, solar_gain, dhw_demand, pressure_drop, pv_surplus with optional value + duration_s
- [x] #3 Rejected with HTTP 409 when simulation is not active (no perturbation of real hardware)
- [x] #4 Each event produces an observable change in the synthetic model consumed by satUpdateSimulation()
- [ ] #5 Endpoint documented in the OpenAPI spec; python build.py both targets SUCCESS; evaluate.py --quick clean
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Source: plan §12 F2 + §4.1 (note: scenario injection must NOT bypass the bus-tx isolation contract). Depends on TASK-795.

GOAL: REST endpoint POST /api/v2/sat/sim/event to inject bench scenarios that perturb the synthetic model, so testers exercise SAT responses without physical hardware.

EVENTS (from plan §12 F2): window_open, solar_gain, dhw_demand, pressure_drop, pv_surplus.

DESIGN:
- Add route to kV2Routes[] dispatch table in restAPI.ino (POST). Body: {"event":"<name>","value":<num>,"duration_s":<int optional>}. Parse with existing parseJsonKVLine pattern (NO ArduinoJson per ADR).
- Guard: only honoured when satEffectiveSimulationActive() (the commit-3 helper) is true; otherwise HTTP 409 (no real-hardware perturbation). If TASK-795 commit-3 not yet landed, gate on settings.sat.bSimulation directly and add a TODO to switch to the helper.
- Each event sets a transient state.sat.sim* perturbation consumed by satUpdateSimulation():
  - window_open: temporary step increase in heat-loss coefficient (room cools faster) for duration_s.
  - solar_gain: additive room-temp gain rate for duration_s.
  - dhw_demand: forces flame-steal (CH suppressed / flow diverted) — overlaps F5, keep minimal here or defer the deep DHW model to F5.
  - pressure_drop: drive synthetic CH pressure down to exercise SATpressure health.
  - pv_surplus: reuse existing satHandlePvSurplus() path (PV-boost already implemented) — just a sim-time trigger.
- New transient fields in SATRuntimeSection for active perturbations + their expiry millis.

OPEN QUESTION (morning): overlap with F5 (dhw_demand) — keep a stub here and do the full DHW model in F5, or fold F5 into this endpoint? Default: stub here, full model in F5.

VERIFY: build both targets; evaluate --quick; curl each event with bSimulation on -> observable model change; with sim off -> HTTP 409. Add the endpoint to the OpenAPI spec (docs/api).
<!-- SECTION:PLAN:END -->
