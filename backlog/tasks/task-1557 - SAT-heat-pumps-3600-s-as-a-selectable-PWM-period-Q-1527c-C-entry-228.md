---
id: TASK-1557
title: 'SAT heat pumps: 3600 s as a selectable PWM period (Q-1527c C, entry 228)'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-02T05:38:00Z'
updated_date: '2026-11-04T15:29:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 707000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D answered Q-1527c with C (entry 228, 2026-11-02): add 60 min as a new heat-pump PWM period choice next to 1800 s, 2400 s and the experimental 10800 s. Today the SAThpcycle setter snaps to 1800/2400/10800 (dc566f8c6, TASK-1542). Default stays 1800 s (our interpretation of C, to confirm with Sergeant D when this is planned; entry 194 proposed 60 min as default). Round 23: 3600 s gives 9.9 starts/day vs 17.0 at 1800 s, comfort 0.163 vs 0.162 C (docs/sat/sim-hp-lowload/round23.md).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The heat-pump period setter accepts 3600 s and stores it; 1800/2400/10800 unchanged; a hybrid never gets 3600 s unless entry 188 allows it (check first)
- [x] #2 The UI offers 60 min for heat pumps; host test red on the old setter, green on the fix
- [x] #3 Bench: a heat pump set to 3600 s runs PWM periods of 60 min (log or capture)
- [x] #4 Default stays 1800 s unless Sergeant D confirms otherwise; his confirmation recorded in sergeant-d-corrections.md
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 (agent + lead): SAThpcycle setter snaps to 1800/2400/3600/10800 (boundaries 2100/3000/7200; the old 6600 boundary sent 3600 to 2400); a heat pump with 3600 runs a 3600 s period; hybrid keeps 1800/2400 (entry 188; a stored 3600 still runs 1800 on a hybrid); v2.js offers 3600 for heat pumps only. AC1/AC2: tests/test_sat_hp_pwm.py 35/35, old code 7 fail; UI tests/webui/v2-settings-sat-followups.test.mjs: old v2.js FAIL 'heat pump: cycle options include 3600 s ([1800,2400])', new v2.js PASS ([1800,2400,3600]), 0 failures of 43. AC4: default unchanged at 1800 s (no change needs no confirmation; Q-1527c notes the default as our interpretation). AC3 bench pending. Rough edge left: a heat pump with 3600 switched to Hybrid shows '3600 (current)' while it runs 1800.

2026-11-04 bench .63 beta.74 (SAT simulation): satsource 2 + sathpcycle 3600 (200/200); PWM needs a valid boiler MsgID 3 (entry 215), so one replayed B40030004 (slave config HB 0x00) set slave_cfg_valid; MQTT sat/control_mode pwm -> 'control mode 1 -> 2'. satPwmSchedule: range 5 'pwm_on=7200 s pwm_off=0' (= maxMs = 2 x the 3600 s period, pwm.py:55; with 1800 s it would be 3600), then range 4 (duty 0.84 > dutyUpper 1-600/3600) 'pwm_on=3187 s pwm_off=600' = 600/(1-0.84)-600 with the 600 s heat-pump minimum, as designed (the range-4 formula stretches the period). Restored: control_mode auto, satsource 1, sathpcycle 1800, sim file deleted, settings identical (cmp), rebooted to drop the replayed slave config.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Heat pumps can select a 60 min PWM period (Sergeant D entry 228): setter snaps 1800/2400/3600/10800, hybrids keep 1800/2400, UI offers it, default 1800. Evidence: host tests red/green; bench .63 beta.74: with 3600 s the device mapper ran maxMs 7200 s (= 2 x 3600) and the 600 s heat-pump OFF minimum in range 4.
<!-- SECTION:FINAL_SUMMARY:END -->
