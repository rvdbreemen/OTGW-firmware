---
id: TASK-797
title: 'feat-2.0.0: SAT sim F2 — REST scenario-injection endpoint'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-31 22:55'
updated_date: '2026-06-01 16:25'
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
- [ ] #2 Accepts window_open, solar_gain, dhw_demand, pressure_drop, pv_surplus with optional value + duration_s
- [x] #3 Rejected with HTTP 409 when simulation is not active (no perturbation of real hardware)
- [x] #4 Each event produces an observable change in the synthetic model consumed by satUpdateSimulation()
- [x] #5 Endpoint documented in the OpenAPI spec; python build.py both targets SUCCESS; evaluate.py --quick clean
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-01T10:19:41+02:00: F2 core shipped 43661b85 (alpha.125, pushed). POST /api/v2/sat/sim/event with window_open + solar_gain. satSimInjectEvent injector + room-model consumption + 409-when-sim-off. Per plan defaults: dhw_demand->F5, pressure_drop deferred (SATpressure coupling), pv_surplus reuses existing endpoint. AC#1-4 (endpoint, events, 409-gate, build) met. OPEN (morning, non-blocking): (a) OpenAPI spec entry not yet added (docs/api) — small doc follow-up; (b) F4 noise/dropouts can hook this same endpoint as its opt-in trigger per F4 plan; (c) extend to dhw_demand/pressure_drop when F5 + SATpressure-coupling decided.

2026-06-01T10:21:05+02:00: OpenAPI documented d283911a (pushed). AC #1/#3/#4/#5 met. AC#2 deliberately LEFT UNCHECKED — endpoint accepts window_open + solar_gain (the additive room-model perturbations), NOT the full 5 the AC lists. dhw_demand needs the F5 DHW model (TASK-800), pressure_drop needs SATpressure coupling, pv_surplus already routes via /v2/sat/pvsurplus. Honest partial: the F2 *framework* is complete and extensible (add an event = one clause in satSimInjectEvent); the remaining 3 events are downstream-task-gated. MAINTAINER DECISION: either (a) accept F2-core as Done and split the 3 remaining events into their owning tasks (F5 etc.), or (b) keep 797 open until all 5 land. Recommend (a) — the others are genuinely different subsystems. Leaving In Progress pending your call.

2026-06-01T18:25:47+02:00: MAINTAINER DECISION 5a — accept F2 core as Done. AC#2 (all 5 events) NOT fully met by design: window_open+solar_gain+sensor_noise implemented; remaining events ROUTED to owning subsystems — dhw_demand -> TASK-800 (F5), pressure_drop -> needs SATpressure coupling (new follow-up if pursued), pv_surplus -> already served by /api/v2/sat/pvsurplus. F2 framework complete + extensible (new event = one clause). Closing Done.
<!-- SECTION:NOTES:END -->
