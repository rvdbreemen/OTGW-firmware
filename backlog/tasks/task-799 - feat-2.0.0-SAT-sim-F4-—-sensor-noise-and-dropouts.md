---
id: TASK-799
title: 'feat-2.0.0: SAT sim F4 — sensor noise and dropouts'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 22:55'
updated_date: '2026-06-01 09:41'
labels:
  - sat
  - simulation
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up F4 from SAT simulation plan section 12. Add configurable sensor noise + occasional dropouts to exercise stale-detection and EMA filters under simulation. Depends on TASK-795. Milestone 2.0.0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Configurable bounded noise added to synthetic flow/return/room values under bSimulation, no heavy RNG lib
- [ ] #2 Configurable dropout injection pushes staleness timestamps so stale-detection + fallback paths trigger
- [ ] #3 Noise/dropouts are opt-in (default off) so the clean simulation remains the default behaviour
- [ ] #4 Zero effect on the real-bus path; python build.py both targets SUCCESS; evaluate.py --quick clean
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Source: plan §12 F4 + §3 (sensor noise/dropouts deferred from MVP). Depends on TASK-795.

GOAL: inject configurable sensor noise + occasional dropouts into the synthetic sensor stream so the stale-detection timeouts and EMA filters (which never trip under the clean MVP sim) get exercised.

DESIGN:
- In satUpdateSimulation(), after computing clean synthetic values, optionally perturb the values the wrappers expose: add bounded Gaussian-ish noise (cheap: sum of a few rand() draws, or a triangular approximation — no <random> heavy lib) to fSimFlowTemp / fSimReturnTemp / fSimRoomTemp.
- Dropouts: with low probability per tick, skip updating a value and/or push its iLast*Ms timestamp back so the staleness guards (SAT_STALE_TEMP_BLE_MS, iSensorMaxAgeS, SAT_STALE_OUTDOOR_MS) fire and the fallback paths run.
- Determinism: vary the noise by tick index, not Math.random/Date (host-side determinism not required on-device, but document the seed source). On-device use micros()/an LCG seeded at boot.
- Tuning consts: SAT_SIM_NOISE_AMPLITUDE_C, SAT_SIM_DROPOUT_PROB (per-tick), SAT_SIM_DROPOUT_MS. Static const MVP.
- Gate strictly under bSimulation; zero effect on real-bus path.

OPEN QUESTION (morning): noise/dropouts always-on under sim, or opt-in via a sim sub-flag / the F2 scenario endpoint? Default: opt-in via an event (window into F2) so the clean sim stays the default and noise is a deliberate test.

VERIFY: build both; evaluate --quick; with noise enabled observe staleness fallback + EMA smoothing actually engage (telnet/REST).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-01T10:41:45+02:00: READY but HELD. F4 is now unblocked — its plan default 'opt-in via the F2 scenario endpoint' is satisfied (F2 shipped 43661b85). Clean-solo implementation: add a sensor_noise event clause to satSimInjectEvent + bounded LCG noise on flow/return/room in satUpdateSimulation + dropout timestamp-push, all static-const, sim-gated. NOT building unprompted: F1+F2 cores already shipped this session without a maintainer ack on the F-series direction, and 4 decisions are stacked unanswered (F2-close AC#2, F7 blocker, 743 jsonStuff, 687). Per advisor guidance (don't silently grind 796->802) holding for user read rather than auto-expanding deferred scope. Reverting to To Do; flip back + I'll implement on a 'continue F-series' nod.
<!-- SECTION:NOTES:END -->
