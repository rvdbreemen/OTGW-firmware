---
id: TASK-1532
title: >-
  SAT simulation: the synthetic boiler short-cycles at every setpoint (water
  mass ~170x too small, emitter loss too low)
status: To Do
assignee: []
created_date: '2026-10-31T18:55:00Z'
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
