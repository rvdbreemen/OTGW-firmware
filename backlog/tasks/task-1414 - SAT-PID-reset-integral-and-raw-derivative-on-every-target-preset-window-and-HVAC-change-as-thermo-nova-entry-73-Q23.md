---
id: TASK-1414
title: >-
  SAT PID: reset integral and raw derivative on every target, preset, window and
  HVAC change as thermo-nova (entry 73, Q23)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-28T06:44:00Z'
updated_date: '2026-10-30T20:03:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1270
  - TASK-1418
references:
  - docs/sat/sergeant-d-corrections.md
documentation:
  - docs/sat/sat-low-load-execution-backlog-2026-10-28.md
priority: medium
ordinal: 578000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-27 (entry 73, Q23: follow thermo-nova). Reset integral + raw derivative (main and zone PIDs) on every real target change, preset change/clear, window switch/restore, HVAC on/off and reset_integral, as thermo-nova reset_control_state (pid.py:134-137). New reset that zeros only integral and raw derivative; do NOT reuse satPidReset() (clears timers, the TASK-1270 failure). Coordinate with TASK-1270 and TASK-1412.10. Update ADR-197 Decision 8.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test per trigger (target, preset, window, HVAC, reset_integral): integral and raw derivative are 0 after it; red on HEAD for the triggers HEAD misses
- [x] #2 Timers and the initialised flag are not cleared by this reset
- [x] #3 ADR-197 Decision 8 / open question updated
- [ ] #4 build + evaluate green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Premise revalidated 2026-10-30 on wt-1416 (origin/dev a16a0ed67 + TASK-1417/1418): still open. Entry 73 (Q23 'follow thermo-nova'); thermo-nova at the pin: PID.reset() zeros _integral and _raw_derivative only (pid.py:134-137); reset_control_state() = main PID + all area PIDs (climate.py:461-464, area.py:335-338).

Triggers in thermo-nova and in the firmware today:
- Target change, only when it really changes (climate.py:643-644, reset :660). Firmware: SATtargettemp branch of updateSetting() (settingStuff.ino:997-1004; REST, MQTT and the adopted thermostat, SATcontrol.ino:1575, all end there): no reset.
- Preset change/clear via async_set_target_temperature (climate.py:604-624), so reset only when the target changes. Firmware satHandlePreset(): zeros only the published copy state.sat.fPidI (SATcontrol.ino:1532, :1540); _pid_integral and the raw derivative stay.
- Window open (ACTIVITY preset, climate.py:819) and close (:832): via the target-change reset. Firmware: satResetIntegral(), integral only, unconditional (SATcontrol.ino:1687, :1710).
- HVAC HEAT/OFF: reset on every call (climate.py:570-583). Firmware: disable = satPidReset() (broader, keeps as is), no zone reset; enable = no reset (settingStuff.ino:991-994).
- reset_integral service: main + areas (climate.py:217-220, services.py:31-39). Firmware: satResetIntegral() integral only, zones untouched (MQTTstuff.ino:619, restAPI.ino:1349, :1600).
- Room climate target change resets that area's PID (climate.py:785-795; not listed in entry 73, found now). Firmware: satHandleZoneSetpoint() no reset.

Change:
1. SATpid.ino: satResetIntegral() becomes satPidResetTerms(): _pid_integral = 0, _pid_rawDerivative = 0 and their published copies; last error, last temperature, timers, _pid_initialized and the start-up flags stay (AC#2, the TASK-1270 failure stays out).
2. SATcontrol.ino: satResetControlState() = satPidResetTerms() + every zone's fPidIntegral = 0 (zones have no D term; fPrevError is the held error, kept like _last_error).
3. Call it: SATtargettemp branch when the clamped target != before (exact, as :643); satHandlePreset() and both window paths when the target changed (replaces fPidI = 0 / satResetIntegral()); MQTT/REST reset_integral; SATenabled off->on transition; satDisable() before satPidReset() (zones). Zone setpoint: that zone's integral = 0 when a valid setpoint really changes (first setpoint: no reset, as :788 with the default).
4. Not changed: satPidReset() on disable/flush/init, satPidCurveOnly() (entries 38/46), stale-sensor path.
5. Best-control note: HVAC 'every call' read as the off->on transition, because a settings POST carries SATenabled=1 on every save; a reset there would wipe the integral on any unrelated save. Reported, not a behaviour beyond entry 73.

Tests: new tests/test_sat_pid_resets.py splicing the real satHandlePreset/satHandleWindow/_satCheckWindowTimer, the preset helpers, the SATtargettemp/SATenabled branches of updateSetting(), satHandleZoneSetpoint(), the MQTT reset_integral handler and the reset functions; per trigger: integral, raw derivative and zone integrals 0 after it, unchanged on an unchanged target. Red on origin/dev (old copies via env), green on the fix. AC#2 through the cadence harness: after satPidResetTerms() the next satPidStep() is a normal step (initialised, timers, last error kept). Full tests/ folder + evaluate --quick before the commit. ADR-197 Decision 8 + docs.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 dependency reconciliation (Robert's go: reconcile the graph alongside the wave-3 work; scout+verifier report): TASK-1412.3 dependency replaced by TASK-1270: entry 73 Open says the reset must not reintroduce the TASK-1270 first-tick behaviour; own host-test AC#1.

Execution-graph update requested by Robert: serialize pending SATpid.ino writers after TASK-1368 -> TASK-1270: TASK-1416 -> TASK-1417 -> TASK-1418 -> TASK-1414 -> TASK-1423 -> TASK-1415. TASK-1381 remains a parallel branch after TASK-1270, gated separately by Q53. Revalidate the task premise after the preceding fix; if already addressed, record evidence and re-scope/close through the normal review, rather than applying a duplicate change. Persistence write cadence remains Robert's decision.

Fix round: reset requests go through a volatile flag applied on the loop task (satApplyControlStateReset first in satControlLoopRun); docs/comments fixed; ADR-204 (replaces ADR-197 decision 8) written, open question answered (Robert), Accepted 2026-10-30, related to ADR-197. Integrated on alpha.562 as alpha.563: full suite 73/73 rc=0, evaluate EXIT 0; tests/test_sat_pid_resets.py 16 OK, base 10 F, mutant satPidReset 8 F. Interpretations in entry 157; Q-1414a queued.
<!-- SECTION:NOTES:END -->
