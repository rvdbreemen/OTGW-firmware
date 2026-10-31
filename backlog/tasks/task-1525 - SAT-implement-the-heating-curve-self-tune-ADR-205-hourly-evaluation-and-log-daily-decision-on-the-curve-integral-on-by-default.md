---
id: TASK-1525
title: >-
  SAT: implement the heating curve self-tune (ADR-205): hourly evaluation and
  log, daily decision on the curve integral, on by default
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-31T13:45:00Z'
updated_date: '2026-10-31T16:10:00Z'
labels:
  - sat
dependencies:
  - TASK-1410
priority: high
ordinal: 681000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implements ADR-205 (Accepted 2026-10-31) from TASK-1519. Design source: Sergeant D entries 160-178 in docs/sat/sergeant-d-corrections.md; study docs/sat/sim-1519/ (round5b-round7.md, evaluation-2026-10-31.md). Summary: signal = hourly mean of (PID output clamped to the setpoint range, before the PWM override) minus the curve; an hour counts only with SAT active, no DHW, valid sun elevation < 12 deg, not PWM, flame not off while CH enabled, MsgID 17 above the MsgID 15 minimum (5 % if absent), no coefficient change that hour; a day counts with >= 6 clean hours; once a day on the last 3 counted days: +-1.0 C threshold, fixed 0.1 step, clamp 0.1-5.0; window empties after a change and on a manual coefficient change; PID terms untouched (ADR-204); settings.ini written only on change; one boolean setting, default on (entries 176/177); hourly record, daily record and a change line (old, new, mean) in the SAT log (docs/sat/sat-log-format.md) and telnet. Depends on TASK-1410 (local sun elevation, entry 178). Per CLAUDE.md: before changing how SAT controls the boiler the plan cites ADR-205 and the entries; the tuner itself changes only the coefficient.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test calling the firmware tuner hour/day functions (not a re-implementation): a winter with a too-low curve (mean +1.5 C) raises the coefficient by exactly 0.1 after 3 counted days; red on a build without the decision, green after
- [x] #2 Host test: hours with PWM, flame off while CH on, modulation at the MsgID 15 minimum, DHW, sun elevation >= 12 or missing each do not count; red when any one exclusion is removed, green with all
- [x] #3 Host test: inside +-1.0 C no change; a change is 0.1, at most once per day, clamped to 0.1-5.0; a manual coefficient change empties the window; the PID integral and derivative are unchanged by a tuner step (ADR-204)
- [x] #4 Host test: settings.ini is written only when the coefficient changes
- [x] #5 Setting exposed in settings/REST/MQTT/web UI like the other SAT settings, default on; switching it off keeps the coefficient and stops evaluation
- [x] #6 Log records: hourly (mean, counted or first reason), daily (clean-hour count, 3-day mean, raise/lower/hold and reason) and a change line with old value, new value and mean; documented in docs/sat/sat-log-format.md; parsed by scripts/sat_log_to_csv.py
- [ ] #7 Bench: SAT simulation on a bench unit over at least 4 simulated days shows hourly and daily records and one logged change; telnet and SAT log captured as evidence
- [x] #8 python evaluate.py --quick green; combo build green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert 2026-10-31 13:47 UTC #dev-sat-mqtt (msg 1566086252254601217): 'we have a go. Please add this feature to backlog to build before beta.1. Priority: high'. Sergeant D accepted ADR-205 13:46 (entry 180). Prerequisite TASK-1410 raised to high with it.

2026-10-31 evidence (alpha.575+081ee4d): tests/test_sat_curve_selftune.py HEAD 55/55 OK (calls the extracted firmware blocks + SATcurveTune.h); pre-change source: 'no satCurveTune pure block' (red). ExclusionMutation OK; manual mutants dropping the PWM or SUN exclusion -> test_each_exclusion FAILED (AC2 red-when-removed). AC5: REST debug settings.sat.curve_self_tune=true default (bench_check PASS), MQTT sat/curve_self_tune in ch09/h09/MANUAL.md (test_manual_sat_commands OK), settings-help 28/28. AC6: bench .61 SATTUNEH hour=0/1/2 mean=2.74/2.83/2.93 ticks=6 counted=0 reason=flame_off. AC8: evaluate --quick 80/0/0, combo build OK. AC7 OPEN: built-in SAT simulation toggles the flame every tick (flow 37 > setpoint 23.6), so every simulated hour ends flame_off and no day counts; the OT frame replay has CH on with flame off. A 4-day bench run needs a scenario with a steady flame (boiler emulator) or field data. Found on the way: the tick fed RelModLevel instead of satGetActualModulation() (simulation only); fixed in the working tree, ships with the next build.
<!-- SECTION:NOTES:END -->
