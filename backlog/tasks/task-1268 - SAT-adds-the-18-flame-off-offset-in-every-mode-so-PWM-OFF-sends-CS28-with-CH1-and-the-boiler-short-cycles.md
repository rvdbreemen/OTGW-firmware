---
id: TASK-1268
title: >-
  SAT adds the +18 flame-off offset in every mode, so PWM-OFF sends CS=28 with
  CH=1 and the boiler short-cycles
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-20 18:15'
updated_date: '2026-10-21 05:35'
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
- [x] #1 No flame-off offset outside the PWM ON flame-off hold: continuous mode sends the PID/clamp setpoint while the flame is off (bench/sim run, red on alpha.441: pid+18)
- [x] #2 New setting overshoot_protection (default off), end to end: settings.ini, REST GET/POST + whitelist, SAT REST route, MQTT sat/overshoot_protection, status JSON, MQTT state, HA switch, v2 UI, docs
- [ ] #3 PWM with overshoot_protection off: no CS override (CS keeps its previous value) and MM=0 in both PWM phases, identical to thermo-nova (sim run)
- [x] #4 PWM with overshoot_protection on: OFF phase sends CS=10 and CH=0 (no relight at 28), ON phase keeps the return+offset hold then flow-1 (sim run, red on alpha.441: CS=28 CH=1 in OFF)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Approved by Sergeant D 2026-10-20 (20:27 'This looks ok'), consolidated:
1. Delete the generic flame-off offset block (SATcontrol.ino, '--- Flame-off setpoint offset' before the COLD_SETPOINT cutoff). The +18 stays only in the PWM ON wait-for-flame hold.
2. New setting SATovershootprotection / settings.sat.bOvershootProtection, default TRUE (all heating systems; user can switch it off), same layers as SATforcepwm: SATtypes.h, settingStuff write+updateSetting, restAPI GET addBool + knownSettings + SAT REST route 'overshoot_protection', MQTTstuff kSatMqttCmds passthrough, status JSON, publishIfChangedB sat/overshoot_protection_enable, HA switch next to force_pwm, v2.js (PID / PWM group), docs.
3. satApplyPWM: CS override only when overshoot protection is on, DHW is not active and the PID setpoint is above the cold setpoint (thermo-nova heating_control.py:385); otherwise return the previous control setpoint (no override, state.sat.fFinalSetpoint at entry). With it on: OFF -> 10 (CH off); ON waiting for flame -> return + fFlameOffOffset clamped cold..max (hold); ON after fModSupDelay -> flow - fModSupOffset.
4. Suppression refresh ~every 3 s during PWM ON after the hold (OpenTherm text 2.1.1.2.5 from Sergeant D: update CS with the latest boiler temperature every 2-3 s), independent of the 30 s control tick, through the normal SAT CS enqueue path (ADR-179 ownership).
5. MM: 0 in both PWM phases while PWM is enabled (thermo-nova RelativeModulationState.OFF); heat pump and hybrid (iHeatingSource) never 0, always iMaxRelModulation; Geminox floor max(10, value) including MM=0 (today only 1..9 are lifted).
6. Continuous -> PWM auto-switch compares flow with the requested/PID setpoint, not fFinalSetpoint (SATcycles.ino ~949), as thermo-nova (flow >= requested + margin for 60 s); overshoot margin default 3.0 -> 2.0 (SATtypes.h:442; also the classifier threshold). Stored values stay.
Out of scope (TASK-1273): continuous previous-CS term, PWM ON timing/start gate/EMA seeding, cycle-end mode switch, CH threshold 22, PM=25, PM/MI on runtime enable, curve coefficient default 2.0.
Verification: SAT simulation + force PWM on the OTGW32 bench, sample /api/v2/sat/status (final_setpoint, pwm_flame_req, control_mode, modulation/MM, flow) every 2 s over several PWM cycles: old (alpha.443) vs new, with overshoot_protection on and off, heating source gas vs heat pump; continuous with flame off (no +18). Plus telnet CS= lines for the 3 s refresh.
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

AC #2 wording 'default off' is superseded by Sergeant D's decision 2026-10-20 (default ON for all heating systems, switchable); implementation follows the decision. Red bench run alpha.442 (OTGW32 .88.61, SAT simulation + force PWM, 6 min, 2 s samples): OFF phase pwm_flame_req=false with final_setpoint 28 (= 10 + 18 flame-off offset), CH enable on throughout, MM 100. Evidence %TEMP%\bench-1268\red-alpha442.json. Review round 1 (opus correctness + sonnet rules/tests): underheat in OFF phase, range-5 tick vs refresh sawtooth, solar ratchet, thermal-safe vs refresh, stale PWM state, hold on lit boiler, refresh vs simulation, doc counts 13->14; sent back for fixes.

Round 1 fixes done (underheat ON-only, range-5 tick==refresh, solar ratchet via _sat_lastControlCS, thermal-safe guard, satPwmReset/satSetControlMode, hold only while flame off with pending-until-delay, refresh writes fFinalSetpoint first; docs counts 13->14, ch04/ch09/gen_sat_props, TASK-1271 changelog). Host test tests/test_sat_pwm_override.py 25 OK, against HEAD 19 FAIL. Round 2 (opus): all 7 fixes verified real, nothing blocking. Accepted as-is: protection off freezes CS (thermo-nova parity, documented); keep-previous-CS falls back to the PID setpoint when the previous CS was 10 (at most fModSupDelay, avoids CH off in the ON phase); pre-existing ON-phase timing flap and force-PWM-at-max reset moved to TASK-1273.

2026-10-21 bench OTGW32 .88.61, fw+fs alpha.443+e07a33c (this diff) via safeboot/OTA, SAT simulation. AC4 red/green: alpha.442 OFF phase final_setpoint 28 (10+18) while PID 21-25 was below cold (relight); new OFF phase final 10, CH off. ON phase with flame off: hold return+18 (37.3, rising to the 45 max); the 20 s hold -> flow-1 step and the 3 s refresh are not reachable in the SAT simulation (DHW draws fire the flame for <20 s), covered by tests/test_sat_pwm_override.py (25 OK, 19 FAIL on HEAD). AC1 green: continuous, flame off, radiators (max 62): final_setpoint == pid_output (55.3), not pid+18 (73.3 -> 62). AC2: setting defaults true after OTA (stored margin 3.0 kept); REST sat/settings/overshoot_protection 0 -> status false; MQTT OTGW/set/<id>/sat/overshoot_protection 1 -> status true and persisted in settings; HA discovery homeassistant/switch/<id>/sat_overshoot_protection_enable/config present (14 switch configs) with stat_t .../sat/overshoot_protection_enable and cmd_t .../sat/overshoot_protection. NOT yet observed: the state topic payload itself (two captures were killed by host memory pressure). AC3 (protection off, sim run) only by host test + REST toggle. Settings restored, diff vs baseline only the new key. Evidence %TEMP%\bench-1268\red-alpha442.json, green-1268*.json, mqtt-op.txt.

2026-10-21 AC2 state topic verified on alpha.444 (a7bbaccb6): with SAT active (simulation on, room temp pushed) OTGW/value/<id>/sat/overshoot_protection_enable gave 'true' (retained) and, after MQTT OTGW/set/<id>/sat/overshoot_protection 0, a live 'false' (REST status false too). Earlier empty captures: SAT was safety-tripped (no boiler, simulation off), and an inactive SAT publishes no sat/ topics at all. Bench restored (protection on, simulation off, external temp off). Open: AC3 and the flow-1 suppression + 3 s refresh outside the host tests.

2026-10-21 suppression + 3 s refresh now bench-proven via TASK-1277 replay: new firmware CS = flow-1 refreshed with every flow frame (~6 s), old firmware PID / 10 / 45 on the 30 s tick only. Still open: AC3 (protection off, sim or field run).
<!-- SECTION:NOTES:END -->
