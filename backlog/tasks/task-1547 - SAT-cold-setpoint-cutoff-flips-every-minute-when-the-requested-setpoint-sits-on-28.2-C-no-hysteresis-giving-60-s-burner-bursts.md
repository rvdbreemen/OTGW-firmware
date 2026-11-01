---
id: TASK-1547
title: >-
  SAT: cold-setpoint cutoff flips every minute when the requested setpoint sits
  on 28.2 C (no hysteresis), giving 60 s burner bursts
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-01T16:30:00Z'
updated_date: '2026-11-01T22:31:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 702000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by gruijter (tester), #alpha-testing msg 1566487960164110377 (alpha.597, 2026-11-01 16:23, graph otgw-graph-20261101-172152), answered in 1566489261685866619. Evidence: his morning capture (20261101-100131) narrates 12:27:33 'Heating off: requested 28.1 is at or below the cold setpoint 28.2', 12:28:33 'Heating on: requested 28.8 ...', 12:29:33 off again; the afternoon graph shows CS 28.2 for ~60 s then CS 10/CH 0, four times (16:34:35, 17:14:35, 17:16:35, 17:20:30), each lighting the boiler for ~60 s. Cause: satColdCutoff is requestedSetpoint <= coldSetpoint with no hysteresis (SATchEnable.h:26-29, used at SATcontrol.ino:6751 and in satChEnableDecide), evaluated every 30 s tick; with the room on target the PID output is ~the curve, which at ~10 C outside is 28.2-28.9 C, right on SAT_COLD_SETPOINT_RAD = 28.2 (SATcontrol.ino:133). thermo-nova has the same rule without hysteresis (heating_control.py:72 'requested_setpoint > COLD_SETPOINT', :385), but COLD_SETPOINT = 22 (const.py:34), so it rarely sits on it; 28.2 is the firmware's own radiator value (TASK-891.2, George). Options to put to Sergeant D: A) hysteresis band around the cold setpoint (e.g. on above 28.2+0.5, off at/below 28.2); B) minimum on/off time for the cutoff state; C) keep as is. SAT behaviour change: needs Sergeant D's OK (CLAUDE.md SAT rules) and Robert's OK (tester-originated).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sergeant D's choice recorded in docs/sat/sergeant-d-corrections.md before implementation
- [x] #2 Host test on the real satColdCutoff/satChEnableDecide code: a requested setpoint oscillating 28.1/28.8 every tick toggles CH every tick on the old code (red) and not on the fix (green)
- [ ] #3 Replay or simulation of a mild day shows fewer starts with the fix and no loss of heat delivery
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-01 Robert: ask Sergeant D first, with priority, options hysteresis / lower to 22 C as thermo-nova / both (he first proposed 28.2 -> 22; entry 24 shows Sergeant D chose 28.2 over 22). Asked as Q-1547a in #alpha-testing msg 1566504412351041578, advice A (hysteresis, keep 28.2). No code until his answer is recorded.

2026-11-01 17:35 Sergeant D answered Q-1547a: A (hysteresis 0.5 C, keep 28.2/21; 1 C if issues remain). Recorded as entry 199. Ready for implementation by the dev session (Robert's OK: he asked to put it to Sergeant D with priority and to execute).

2026-11-01 Robert: the main (lead) agent builds TASK-1547 once the beta release is done. Spec: entry 199 (0.5 C latched hysteresis on the cold cutoff, 28.2 rad / 21 floor kept; all cutoff users follow the same latched state).

2026-11-01 17:55 Sergeant D: thermo-nova's 22 C was a mistaken request, 28.2 intended (entry 200); confirms option A.

2026-11-01 shipped as 2.0.1-alpha.1 (bf57355dd), posted with alpha.2 (Discord msg 1566551398550737136). Evidence: host tests tests/test_sat_cold_hysteresis.cpp (gruijter's request sequence: bare rule 17 CH changes, latched 3; does not compile on the old header) and tests/test_sat_cold_hysteresis_source.py (4 failures on old, 4/4 on fix); PWM harnesses green (pwm_override 79, hp_pwm 27, overshoot_sustain 43, no_return_ignition 19, pwm_start_limit 22); full tests folder: only the 3 failures that also fail on clean origin/dev (manual_sat_commands, sat_mqtt_contract_1485, sat_preset_persist). evaluate --quick 80/0. Bench .63 Classic+S3 PIC 6.8, OTA 2.0.1-alpha.2+6f44f9d, SAT simulation: narration 21:19:11 'Heating on: requested 29.8 is above the cold setpoint 28.2 + 0.5 hysteresis'; 41 control ticks, CS=10 exactly when CH=0 (0 mismatches); stable 9 min after reboot. NOT shown on the bench: a request held inside 28.2-28.7 staying off (the simulation's D term jumps past the band on every target change); that case rests on the host replay. AC#3 (mild-day replay with fewer starts and no heat loss) stays open: the field capture from a tester on alpha.2 is the evidence; In Progress until then.

2026-11-01: needs-maintainer-ok dropped (Robert gave the OK; built as 2.0.1-alpha.1, contained in 2.0.0-beta.2). AC#3 closes with gruijter's next capture or a mild-day replay through the TASK-1532 simulation boiler comparing heat delivered.
<!-- SECTION:NOTES:END -->
