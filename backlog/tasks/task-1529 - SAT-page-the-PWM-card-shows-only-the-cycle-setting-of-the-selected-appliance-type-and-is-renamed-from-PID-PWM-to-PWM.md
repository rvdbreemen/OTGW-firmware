---
id: TASK-1529
title: >-
  SAT page: the PWM card shows only the cycle setting of the selected appliance
  type and is renamed from 'PID/PWM' to 'PWM'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-31T15:25:00Z'
updated_date: '2026-10-31T16:33:00Z'
labels:
  - sat
  - ui
  - alpha
dependencies: []
priority: medium
ordinal: 685000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-31 15:24 UTC #alpha-testing msg 1566110604618629279: 'currently in the PID / PWM card we display the cycles of both systems radiators and heat pumps. It is confusing for the user. So when an Appliance Type is selected keep only the cycles for this specific appliance type. More over rename the card name from PID/PWM to PWM.' UI only (data/v2.html / v2.js); no change in what SAT does.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The card is titled 'PWM'
- [x] #2 With appliance type gas boiler only the boiler cycles setting is shown; with heat pump only the heat-pump cycle setting; switching the type updates the card without a reload
- [x] #3 Webui test or browser screenshot before/after; all webui tests green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-31 evidence: tests/webui/v2-settings-sat-followups.test.mjs (real v2.html/v2.js, headless Chrome, fake device) 30/30 on the new v2.js; on the alpha.575 v2.js (OTGW_DATA_DIR) the 3 new checks FAIL (no 'PWM' card). Checks: title 'PWM' and no 'PID / PWM'; satsource 1 shows satcyclesperhour not sathpcycle; select Heat Pump -> sathpcycle shown, satcyclesperhour hidden at once, neither lands in 'Other', unsaved type stays dirty. Screenshots (synthetic data): C:\Users\rvdbr\AppData\Local\Temp\claude\D--Users-Robert-Documents-GitHub-RvdB-OTGW-firmware\7e8a9b2c-8154-4fbc-badd-02c669e54188\scratchpad\shots\new\pwm-card-gas.png, C:\Users\rvdbr\AppData\Local\Temp\claude\D--Users-Robert-Documents-GitHub-RvdB-OTGW-firmware\7e8a9b2c-8154-4fbc-badd-02c669e54188\scratchpad\shots\new\pwm-card-heatpump.png. Full suite run_tests.py -j4 --webui earlier this cycle 107/108 (ADR README, fixed by 9c). Bench .61 alpha.577 serves v2.js with satFieldShown (curl). Rule matches satGetPwmPeriodSec/satGetEffectiveHeatingSource (Auto = gas) and thermo-nova config_flow.py:686-700 (9c). Sergeant D entry 185.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT page card 'PID / PWM' renamed 'PWM'; it shows sathpcycle only for appliance type Heat Pump and satcyclesperhour otherwise (Auto/gas/hybrid), re-rendering when the type changes (unsaved value kept). Evidence: browser test 30/30, new checks red on the old v2.js; screenshots in the task notes; bench unit serves the new page. Shipped alpha.577+dbb6e37 (zip msg 1566127640237318315).
<!-- SECTION:FINAL_SUMMARY:END -->
