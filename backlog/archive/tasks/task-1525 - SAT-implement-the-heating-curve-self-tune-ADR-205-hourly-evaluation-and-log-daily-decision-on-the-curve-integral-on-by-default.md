---
id: TASK-1525
title: >-
  SAT: implement the heating curve self-tune (ADR-205): hourly evaluation and
  log, daily decision on the curve integral, on by default
status: Done
assignee:
  - '@claude'
created_date: '2026-10-31T13:45:00Z'
updated_date: '2026-11-02T00:48:00Z'
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
- [x] #7 Bench: SAT simulation on a bench unit over at least 4 simulated days shows hourly and daily records and one logged change; telnet and SAT log captured as evidence
- [x] #8 python evaluate.py --quick green; combo build green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert 2026-10-31 13:47 UTC #dev-sat-mqtt (msg 1566086252254601217): 'we have a go. Please add this feature to backlog to build before beta.1. Priority: high'. Sergeant D accepted ADR-205 13:46 (entry 180). Prerequisite TASK-1410 raised to high with it.

2026-10-31 evidence (alpha.575+081ee4d): tests/test_sat_curve_selftune.py HEAD 55/55 OK (calls the extracted firmware blocks + SATcurveTune.h); pre-change source: 'no satCurveTune pure block' (red). ExclusionMutation OK; manual mutants dropping the PWM or SUN exclusion -> test_each_exclusion FAILED (AC2 red-when-removed). AC5: REST debug settings.sat.curve_self_tune=true default (bench_check PASS), MQTT sat/curve_self_tune in ch09/h09/MANUAL.md (test_manual_sat_commands OK), settings-help 28/28. AC6: bench .61 SATTUNEH hour=0/1/2 mean=2.74/2.83/2.93 ticks=6 counted=0 reason=flame_off. AC8: evaluate --quick 80/0/0, combo build OK. AC7 OPEN: built-in SAT simulation toggles the flame every tick (flow 37 > setpoint 23.6), so every simulated hour ends flame_off and no day counts; the OT frame replay has CH on with flame off. A 4-day bench run needs a scenario with a steady flame (boiler emulator) or field data. Found on the way: the tick fed RelModLevel instead of satGetActualModulation() (simulation only); fixed in the working tree, ships with the next build.

Correction 2026-10-31 (alpha session, after Robert questioned it): the earlier note that the built-in simulation 'toggles the flame every tick' was wrong. Measured in the bench logs: SATTICK flame=0 on every 30 s tick and SATTUNEH reason=flame_off every (60 s sim) hour. Cause from the model equations: the synthetic boiler short-cycles, 37-54 starts/h with 5-12 s burns, because its minimum output (1.2 kW) always exceeds its emitter loss (0.1-0.6 kW) and its water mass is ~170x too small. Filed as TASK-1532; AC7 waits on it.

2026-11-01 Robert: AC7 bench run deferred while 2.0.1 development is ongoing; do not tie up a bench unit for it now. Field logs meanwhile show the hourly records and exclusions working (gruijter alpha.597 capture: SATTUNEH hour=0 not_sat, hour=1/2 sun). Note: with the simulation at 60 s per hour the 4 simulated days take ~1.5-2 h wall time, not 4 days.

AC#7 bench run 2026-11-02 00:01-01:46 (lead session, Robert: 'met spoed', bench frozen for it): .63 Classic+S3 Mini, 2.0.0-beta.4+2905bd1 (contains the 1525 code and the TASK-1532 sim boiler), satsimulation on, gas, target 19.8. Tuner hour 60 s via POST /api/v2/sat/sim/event {event:tune_hour,value:60}, SATinterval 10 s (docs/sat/sat-log-format.md), SAT telnet debug captured. Scenario: the stock sim (24 kW, min 20 % = 4.8 kW) cannot modulate above its minimum against the sim radiators, so every hour was excluded as min_mod (attempts 1 and 2, logs kept); run 3 used boilercapacity 10 kW, simcoolrate 0.2 (house needs ~6.6 kW) and coefficient 1.0 (curve too low on purpose). Result over 104 tuner hours = 4.3 simulated days: 104 SATTUNEH records, 86 counted (hours 0-1 min_mod while the house cooled down); SATTUNED day=0 clean=19 hold few_days; day=1 clean=19 hold few_days; day=2 clean=20 days_in_window=3 mean3=10.08 decision=raise reason=outside_band; 'SATTUNE old=1.0 new=1.1 mean3=10.08' (01:13:29); day=3 clean=21 days_in_window=1 hold few_days (window emptied by the change, as designed). bootcount 46 before and after, freeheap ~57-58.5 KB, maxblock 31732. Settings restored afterwards (SATinterval 30, boilercapacity 24, coefficient 1.5, simcoolrate 0.1). Finding: the stock simulation scenario never produces a counted tuner hour (sim house load < sim boiler minimum); a bench run needs the scenario above. Logs: scratchpad run1525/ (telnet.log, status.log, summary.txt).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Heating curve self-tune (ADR-205): hourly evaluation and log, daily decision on the 3-day mean of the curve signal, fixed 0.1 step, on by default. Code on dev (0fd940bf7, fb02f8431, 081ee4d9f, 9398cec94). AC1-AC6, AC8: host tests (tests/test_sat_curve_selftune.py 55/55, exclusion mutants red), REST/MQTT/doc checks, bench .61 hourly records, evaluate 80/0 (notes). AC7: bench .63 2026-11-02, 4.3 simulated days with 60 s tuner hours: 104 hourly records (86 counted), 4 daily records, one logged change on day 2 (mean3 10.08 C outside the 1 C band -> raise, SATTUNE old=1.0 new=1.1), window emptied after the change as designed, no reboot. The run needed a heavier sim scenario (10 kW boiler, simcoolrate 0.2, coefficient 1.0): the stock sim never yields a counted hour (house load below the sim boiler minimum), recorded in the notes.
<!-- SECTION:FINAL_SUMMARY:END -->
