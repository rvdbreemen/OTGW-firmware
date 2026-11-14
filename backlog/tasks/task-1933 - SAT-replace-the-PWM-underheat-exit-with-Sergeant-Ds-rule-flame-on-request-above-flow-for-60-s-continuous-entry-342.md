---
id: TASK-1933
title: >-
  SAT: replace the PWM underheat exit with Sergeant D's rule (flame on, request
  above flow for 60 s -> continuous), entry 342
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T10:36:00Z'
updated_date: '2026-11-14T13:24:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1924
priority: medium
ordinal: 989000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer + Sergeant D decision, entry 342: Q-338a A (https://discord.com/channels/812969634638725140/1514720723980259460/1571110480997523527), his rule from entry 339 (1571097503636652094). Today the PWM-to-continuous underheat exit is flow <= request - 3.0 C for 180 s while PWM is ON (thermo-nova heating_control.py:321-356, const.py:22, cycles/const.py:11; firmware SATcycles.ino:39-40). The saturation exit (300 s) stays. Deviates from thermo-nova. For lane d3 after TASK-1924 (same area, SATcontrol.ino / SATcycles.ino).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Simulation check FIRST, before any firmware change: the new exit against today's exit on the gruijter-calibrated sim houses; toggling against the entry-327 entry rule (+1.5 C for 30 s) and burner starts per day; numbers labelled MODEL RESULT
- [ ] #2 The simulation result is reported to Sergeant D and his OK is recorded as an entry before the firmware is changed
- [ ] #3 Open points of entry 342 settled with Sergeant D: (2) whether the rule runs only while PWM is ON (thermo-nova heating_control.py:321-323), (3) boiler temperature = flow temperature
- [ ] #4 ADR via adr-kit recording the deviation from thermo-nova's underheat exit
- [ ] #5 Host test on the real exit code: red on the old code (flow 1 C below request for 60 s does not exit), green on the fix; the saturation exit unchanged
- [ ] #6 Bench run by the bench-tester (otgw-bench-tester): no reboot, the exit fires as specified in a simulated warm-up
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Step 1, FIELD DATA (EXPLORATORY: one house, gruijter, 5 telnet captures beta.68/167/171/214/218, 04-12 Nov, 8420 SATTICK ticks, about 70 h; script scratchpad t1933/rise_after_ignition.py, data stays local). Per CH ignition inside a PWM ON phase (pwm=1, ch=1; pon= is the ON duration, not a phase flag): time until flow >= the requested setpoint (pid=). Result: 91 ignitions; flow reached the request within one 30 s tick in 90 of them (median 0 s, max 30 s), never in 1. The entry-342 rule (flame on, request > flow for >= 60 s) would have fired in 1 of 91 ON phases (beta.167, 90 s after an ignition, request 41.1 vs flow 37.3: a real underheat). Today's underheat exit (flow <= request - 3.0 for 180 s, PWM ON) was due once, at another moment (beta.218, request 36.7 vs flow 30.1). Reading: on this house the risk named in option A (an exit about 60 s after every ignition, then toggling with the entry-327 rule) does not show, because PWM starts only when the flow is 1.5 C above the request and the ON-phase CS (flow - 1) keeps it there. Limits: one house, 30 s tick resolution, no heat pump, no low-power boiler; the simulation check (AC1) is still to do.

Simulation (MODEL RESULT), script tools/research/sat/sat_pwm_exit_sim_1933.py, thresholds docs/sat/sim-1933/thresholds.md committed 898e62736 before any run (local, push pending the freeze). V1 validity: gruijter-like baseline enters PWM in 25 of 25 runs: PASS (thermal_lag and minimum_power only once in 48 h). Grid 800 runs per variant, 30 s interval: the e342 variant passes very_low_load, thermal_lag, minimum_power; FAILS dhw (toggles 1.141/day vs today 0.081 + 1.0; worst 12.5/day > 4) and ignition (worst 12.5/day > 4; mean 1.056 within). Starts per day and comfort S1/S3 within the limits in every class. 60 s interval sensitivity: same picture (dhw worst 17.0, ignition worst 12.0). Per the pre-registered rule: no recommendation to ship. POST-HOC (after the run): the worst toggling is on the gruijter-like plants (1983-92 radiators, fixed or variable flow, Pmin 5.5) in the dhw and ignition classes, 11-12.5 toggles/day against 0-1 for today, with the same burner starts; this conflicts with the field measurement on that house (1 firing in 91 PWM ON ignitions). Next: one trace run of a worst case to see whether the model's delayed ignition / post-draw flow makes the flow sit below the request for 60 s with the flame on, compared with the field rise times (median 0 s, max 30 s).
<!-- SECTION:NOTES:END -->
