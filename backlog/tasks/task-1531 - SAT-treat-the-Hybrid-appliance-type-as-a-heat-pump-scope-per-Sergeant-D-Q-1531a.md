---
id: TASK-1531
title: >-
  SAT: treat the Hybrid appliance type as a heat pump (scope per Sergeant D
  Q-1531a)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-31T18:51:00Z'
updated_date: '2026-10-31T22:29:00Z'
labels:
  - sat
  - beta.1
dependencies: []
priority: medium
ordinal: 687000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, #alpha-testing 2026-10-31 15:27 UTC (msg 1566111477927116892), entry 186: 'Hybrid Appliance type should be considered as heat pump'. Today hybrid follows the gas path except satComputeMM (SATcontrol.ino:471-482): PWM period 3600/cycles per hour (SATcontrol.ino:440-446), hourly start limit, max flow defaults 62/50 instead of the heat-pump cap 40, and the PWM card shows cycles per hour (TASK-1529). thermo-nova has no hybrid type (not found in the code; only HeatingSystem.HEAT_PUMP, config_flow.py:686). Scope asked as Q-1531a: A) PWM cycle and the PWM card only, B) every heat-pump rule. Blocked until his answer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sergeant D's answer to Q-1531a recorded in sergeant-d-corrections.md
- [x] #2 Host tests against the real SAT code: hybrid gets the agreed heat-pump behaviour (red on the old code, green on the fix)
- [x] #3 PWM card shows the heat-pump cycle for Hybrid (browser test old vs new)
- [x] #4 Bench run in SAT simulation with source Hybrid shows the agreed PWM period
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-10-31: AC1 entry 188 recorded (Q-1531a A, msg 1566167931191304233). AC2 tests/test_sat_hp_pwm.py (real getter + PWM mapper compiled): new 27/27 OK; old code 10 FAIL (hybrid period 1200 s from cycles per hour instead of 1800/2400 s). AC3 tests/webui/v2-settings-sat-followups.test.mjs 34/34 on the new v2.js; old fails 'hybrid selected: heat-pump cycle shown'. AC4 bench .61 alpha.583 (contains alpha.580), normal 24 kW boiler, source Hybrid, sathpcycle 1800: SAT PWM pwm_on=668 s pwm_off=1132 s = 1800 s (C:/Users/rvdbr/AppData/Local/Temp/claude/D--Users-Robert-Documents-GitHub-RvdB-OTGW-firmware/7e8a9b2c-8154-4fbc-badd-02c669e54188/scratchpad/bench-check-192.168.88.61-20261031-232115.txt). Earlier oversized-boiler attempts did not reach PWM and were dropped on Robert's request (normal boiler and house).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Hybrid (satsource 3) takes the heat-pump PWM cycle (iHpCycleSeconds) in satGetPwmPeriodSec(); hourly start limit and max-flow defaults stay the gas ones (Sergeant D entry 188). The SAT page's PWM card shows the heat-pump cycle for Hybrid; help texts updated. Evidence per AC in the notes: host test 27/27 (old 10 FAIL), browser test 34/34 (old FAIL on the hybrid check), bench PWM period 668+1132 = 1800 s with source Hybrid on a normal simulated boiler. Shipped in alpha.580 (zip alpha.582).
<!-- SECTION:FINAL_SUMMARY:END -->
