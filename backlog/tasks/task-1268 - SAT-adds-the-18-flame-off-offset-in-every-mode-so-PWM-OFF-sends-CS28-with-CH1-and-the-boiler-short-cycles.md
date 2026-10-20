---
id: TASK-1268
title: >-
  SAT adds the +18 flame-off offset in every mode, so PWM-OFF sends CS=28 with
  CH=1 and the boiler short-cycles
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-20 18:15'
updated_date: '2026-10-20 20:31'
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

2026-10-20 19:27 UTC Sergeant D: 'when SAT detects the flame ON it keeps return+18 for 18 seconds and then falls back slowly by reducing the CS gradually and clamps to boiler temperature - 1'. Conflicts with our thermo-nova snapshot (SAT 5.0.0, 2026-06-11): modulation_suppression_delay_seconds default 20 (const.py:152), then CS = flow - offset(1.0) immediately, recomputed each 5 s tick (heating_control.py:435-448); no step-down found. Our fModSupDelay=20 / fModSupOffset=1 already match the snapshot. Asked him whether he describes a newer version (file/lines/release) or observed behaviour. Still waiting on points a) force_pwm fallback and b) MM-ignoring boilers.

2026-10-20 19:36-19:38 UTC Sergeant D: hold time is a slider in the original (ours: satmodsupdelay setting, default 20 s, already exposed); after the hold flow-1 straight away is fine; a) never used force PWM, follow thermo-nova (no special fallback); b) Intergas and Ideal ignore MM so they should always use the suppression; a brand that stops on MM=0 uses a floor (he thought 9). Code check (5.0.0): no manufacturer forcing of overshoot_protection in thermo-nova; Ideal/Intergas/Geminox/Nefit only have supports_relative_modulation=False (coordinator/__init__.py:310); the MM floor brand is Geminox with max(10, value) (heating_control.py:522), already in our quirk table (SAT_QUIRK_MIN_MOD_10). Proposed to him: overshoot_protection default ON for Intergas and Ideal (our own rule on top of Python), OFF otherwise; asked whether Nefit belongs on that list. Waiting for that confirmation.

2026-10-20 19:45 UTC Sergeant D: Nefit also on the list. 19:46 maintainer (number3nl): 'Ideal, Intergas, Geminox, Nefit' = exactly the brands with SAT_QUIRK_NO_REL_MOD in satManufacturerTable. Decision: the PWM CS override runs when overshoot_protection is on OR the manufacturer has SAT_QUIRK_NO_REL_MOD (always on for those four, our own rule on top of thermo-nova); default off for other brands. Plan approved; resume after TASK-1271.

2026-10-20 19:56-19:58 UTC Sergeant D: (1) heat pumps must never get MM=0 (Elga Ace stops the heat pump on MM=0): for any heat pump keep MM at 100% or the user's max modulation limit, generic rule because behaviour at MM=0 is unknown; (2) Geminox must get MM=10 instead of MM=0 (existing SAT_QUIRK_MIN_MOD_10, check it covers both PWM phases of the new MM=0 path); (3) Geminox on the always-suppress list confirmed ('otherwise Robert is correct'). Asked whether 'hybrid' counts as heat pump for the MM rule.

2026-10-20 20:01 UTC Sergeant D: 'So in this list add all heat pumps also' -> heat pumps (iHeatingSource = heat pump) join the always-suppress list (PWM CS override always on), with MM never 0 for them (kept at the user's max modulation). Hybrid still unanswered; proposal: treat as heat pump.

2026-10-20 20:04 UTC Sergeant D: thinks thermo-nova suppresses on all boilers. Re-checked 5.0.0: gate default False (const.py:122, heating_control.py:385); config_flow system_configuration (config_flow.py:678+) no longer exposes overshoot_protection (translation leftover only); True only for entries migrated from version < 5 that had it on (__init__.py:160-162). So long-running installs have it on, fresh 5.0.0 installs off. Asked him: A) default on for all boilers with an off switch (recommended), or B) always on, no setting. Heat-pump MM rule and Geminox MM=10 apply either way.

2026-10-20 20:09-20:14 UTC Sergeant D: (1) watch graphs for a CS that follows the start-up + suppression logic; apart from start-up RelMod should be as close to 0 as possible (OpenTherm workaround). Screenshot: OpenTherm application text 2.1.1.2.5 'Control during the ON period (without ID14)': raise CS until the flame is on (or start at max CS), then CS = actual boiler temp or dT 1 K lower to keep the burner on lowest level, updated every 2-3 s. (2) TRVs can change the flow temperature, so start-up + suppression should be the option for all heating systems -> option A (overshoot_protection default ON for all, user can switch off). (3) hybrid treated as heat pump. Consequence: the flow-1 step must refresh every ~3 s during the PWM ON phase, not on the 30 s control tick (pulled from TASK-1273).

2026-10-20 20:27 UTC Sergeant D approved the final plan ('This looks ok' after the point-4 detail). He also described the thermo-nova cold start: continuous first (CH=1, CS = curve+PID, MM=100), when BT - CS >= ~2 for ~60 s switch to PWM. Python: flow >= REQUESTED + 3.0 (cycles/const.py OVERSHOOT_MARGIN_CELSIUS) sustained 60 s (const.py OVERSHOOT_SUSTAIN_SECONDS) -> enable PWM (heating_control.py ~262-305). Ours compares flow with fFinalSetpoint (SATcycles.ino ~949), which the continuous clamp keeps within 2 C of flow, so the switch practically never fires (TASK-1273 item). Pulled into TASK-1268: compare with the requested (PID) setpoint, so the cold start actually reaches the PWM suppression.

2026-10-20 20:30 UTC Sergeant D: overshoot margin default 3.0 -> 2.0 (settings.sat.fOvershootMargin, SATtypes.h:442; used by the continuous->PWM switch and the cycle classifier SATcycles.ino:671). Stored values on existing units stay as they are (his rule 'leave existing settings alone'). In scope of TASK-1268.
<!-- SECTION:NOTES:END -->
