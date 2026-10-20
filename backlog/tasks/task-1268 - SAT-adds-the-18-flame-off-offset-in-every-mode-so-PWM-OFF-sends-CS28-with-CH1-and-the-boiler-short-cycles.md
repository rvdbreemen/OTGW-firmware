---
id: TASK-1268
title: >-
  SAT adds the +18 flame-off offset in every mode, so PWM-OFF sends CS=28 with
  CH=1 and the boiler short-cycles
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-20 18:15'
updated_date: '2026-10-20 19:28'
labels:
  - bug
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 398000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
First real SAT field run (gruijter, Intergas HReco 30 + Honeywell Round + PIC Classic, alpha.439/441, #alpha-testing 2026-10-20), captures in %TEMP%\discord-1020. SATcontrol.ino ~4679-4687 adds fFlameOffOffset (18) to finalSetpoint whenever the flame is off, in any mode. thermo-nova (heating_control.py:385-408) uses return+offset only as a REPLACEMENT in the PWM-ON flame-off phase, and only when overshoot_protection is on (default off). Effects: PWM-OFF minimum 10 becomes 28 and wantHeating (finalSetpoint > SAT_MIN_SETPOINT) sends CH=1, so the boiler relights at flow 28; graph CSVs show chSet only at 10 / 28.0 / 45 and ~20 CH bursts per hour against cycles_per_hour=3; continuous startup jumps to 40.3+18 clamped to 45 (telnet 11:39:49 'pid=40.3 final=45.0'); the PWM wait step gets the offset twice. Found independently by the field-log analysis and the thermo-nova comparison.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 No flame-off offset outside the PWM ON flame-off hold: continuous mode sends the PID/clamp setpoint while the flame is off (bench/sim run, red on alpha.441: pid+18)
- [ ] #2 New setting overshoot_protection (default off), end to end: settings.ini, REST GET/POST + whitelist, SAT REST route, MQTT sat/overshoot_protection, status JSON, MQTT state, HA switch, v2 UI, docs
- [ ] #3 PWM with overshoot_protection off: no CS override (CS keeps its previous value) and MM=0 in both PWM phases, identical to thermo-nova (sim run)
- [ ] #4 PWM with overshoot_protection on: OFF phase sends CS=10 and CH=0 (no relight at 28), ON phase keeps the return+offset hold then flow-1 (sim run, red on alpha.441: CS=28 CH=1 in OFF)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Goal: PWM setpoint behaviour identical to thermo-nova (heating_control.py update() 197-240, _compute_pwm_control_setpoint 380-448, relative_modulation_state 134-145), per Sergeant D 2026-10-20.
1. SATcontrol.ino ~4679-4687: delete the generic flame-off offset block (adds fFlameOffOffset whenever the flame is off, any mode). The PWM ON flame-off hold at ~770 already uses return+offset as a replacement value.
2. satApplyPWM(): keep the PWM timing/state machine as is (bPwmFlameRequested, ignition wait, cycle limit), but gate the CS override like Python: when !bOvershootProtection (or DHW active, or pidOutput <= cold setpoint) return the previous control setpoint (state.sat.fFinalSetpoint at entry, i.e. no override) instead of 10 / hold / flow-1. With the flag on, unchanged sequence.
3. MM in PWM: 0 in both phases while PWM is enabled (Python RelativeModulationState.OFF), max otherwise (DHW keeps max). Today ours is 0 in ON and max in OFF (~4662-4664).
4. New setting SATovershootprotection / settings.sat.bOvershootProtection (default false), same layers as SATforcepwm: SATtypes.h, settingStuff.ino write+updateSetting, restAPI.ino GET addBool + knownSettings + SAT REST route 'overshoot_protection', MQTTstuff.ino kSatMqttCmds passthrough, SATcontrol status JSON + publishIfChangedB sat/overshoot_protection_enable, MQTTHaDiscovery.cpp switch next to force_pwm, v2.js label in the PID / PWM group, docs (MANUAL SAT command + passthrough tables, ch05/h05 settings table, docs/api/MQTT.md if it lists SAT switches).
Out of scope (TASK-1273): continuous previous-CS term, PWM timing/start gate/EMA, auto-switch input, CH threshold 22, PM=25, 5 s loop; coefficient default 2.0.
Verification: SAT simulation mode + Force PWM on the OTGW32 bench (OT-Direct), poll /api/v2/sat/status (final_setpoint, pwm_flame_req, control_mode, modulation) every 5 s plus telnet SAT lines; red run on alpha.441 (expect CS=28 with CH on in PWM OFF, pid+18 in continuous with flame off), green runs on the fix with the flag off and on. Settings saved before, restored after. tests/test_settings_whitelist.py must stay green (it enforces the whitelist for the new key). Cross-line: SAT is 2.0.0-only, no port.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-20 Sergeant D: PWM override behaviour must match thermo-nova exactly (gated by overshoot_protection, default off); see TASK-1273.

2026-10-20 maintainer: present the plan to Sergeant D for advice and approval before implementing. Posted in #alpha-testing (two open points: force_pwm fallback to the PID setpoint when no valid previous CS; MM-ignoring boilers such as Intergas with overshoot_protection off). Waiting for his reply; no code written yet.
<!-- SECTION:NOTES:END -->
