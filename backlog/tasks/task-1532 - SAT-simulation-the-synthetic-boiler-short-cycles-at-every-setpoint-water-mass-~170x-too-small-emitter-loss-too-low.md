---
id: TASK-1532
title: >-
  SAT simulation: the synthetic boiler short-cycles at every setpoint (water
  mass ~170x too small, emitter loss too low)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-31T18:55:00Z'
updated_date: '2026-10-31T20:16:00Z'
labels:
  - sat
  - simulation
  - beta.1
dependencies: []
priority: high
ordinal: 688000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while answering Robert (2026-10-31) on TASK-1525 AC7. satUpdateSimulation() (SATcontrol.ino:5148-5186, constants :171-175): flow gain 0.4 C/s per kW (24 kW full mod = 9.6 C/s, the comment says ~2 C/s; a real ~100 l system is ~0.0024 C/s per kW) and emitter loss 0.02 kW/K (0.1 kW at flow-room 5 K, 0.6 kW at 30 K) while the minimum output is 5 % of 24 kW = 1.2 kW. So the minimum output always exceeds the load and the flame cycles: recompute of the equations (scratchpad simflame.py) gives 37-54 starts/h with 5-12 s burns at setpoints 23/35/50. The room model also heats at a fixed 0.5 C/min whenever the flame is on, independent of flow or power. Consequence: every self-tune hour ends on reason=flame_off, so AC7 of TASK-1525 cannot be shown in simulation; the PWM/continuous logic is also exercised against an unrealistic boiler. Also: SATTICK flow/ret print the bus values (OTcurrentSystemState.Tboiler/Tret, 0.0 in simulation) instead of satGetFlowTemp()/satGetReturnTemp() (SATcontrol.ino:6783).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real satUpdateSimulation() (red on the old model): at a steady low-load setpoint the simulated boiler burns continuously at a modulation above minimum for at least one simulated hour, and still cycles when the load is below the minimum output
- [ ] #2 Room temperature follows the emitter heat output instead of a fixed heat rate
- [ ] #3 SATTICK flow and ret show the simulated values in SAT simulation
- [ ] #4 Existing SAT simulation and PWM tests stay green
- [ ] #5 Bench run in SAT simulation: at least one SATTUNEH hour counted=1 (unblocks TASK-1525 AC7)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Model change in satUpdateSimulation() only (SATcontrol.ino ~5113-5212); flame state machine, modulation P-law, scenario injection, DHW schedule unchanged.
1. Water node with real mass: SAT_SIM_WATER_KJ_PER_K 400 (~100 l incl. boiler): dFlow/dt = (P_in - Q_out) / 400 per s, P_in = mod% x fBoilerCapacity.
2. Emitter: Q_emit = SAT_SIM_EMITTER_KW_PER_K 0.15 x max(0, flow - room) (6 kW at 40 K). During a DHW draw the 3-way valve sends the water to the DHW coil: Q_emit = 0, Q_out = SAT_SIM_DHW_LOAD_KW 15.
3. Room: dRoom/dt (C/min) = fSimHeatRate x Q_emit / SAT_SIM_EMITTER_REF_KW (15 kW) - fSimCoolRate x (room - outdoor) / 10 K. Heats and cools continuously (no flame keying); solar/window injection as before (window multiplies the cool term). Equilibrium at outdoor 2..14 C: 5.4..1.8 kW, above the 1.2 kW minimum (5 % of 24 kW) -> continuous burn; outdoor near 17 C -> load below minimum -> it cycles, as a real boiler does.
4. Return temp formula unchanged; drop SAT_SIM_FLOW_LOSS_K / SAT_SIM_FLOW_THERMAL_GAIN (wrong comment ~2 C/s).
5. SATTICK flow/ret (both lines, :6403 and :6783) print satGetFlowTemp()/satGetReturnTemp().
6. Help text satsimheatrate/satsimcoolrate updated to the new meaning (heat rate at 15 kW emitter output; cool rate per 10 K indoor-outdoor difference). Multi-zone/area sim (:5293-5330) stays flame-keyed: out of scope, noted.
Verification: host test compiling the real satUpdateSimulation() (pattern of test_sat_curve_selftune / test_sat_hp_pwm extract): old model red (short-cycles: >20 starts/h at outdoor 8), new green (continuous burn >= 1 sim hour with mod > minimum at outdoor 8 and setpoint 40; cycles at outdoor 17 with low setpoint); SATTICK source check; full tests via run_tests.py; bench SAT simulation on .61: SATTUNEH counted=1 at least once. Not a SAT control change (simulation only), so no Sergeant D gate.
<!-- SECTION:PLAN:END -->
