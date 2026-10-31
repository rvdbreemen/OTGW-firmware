---
id: TASK-1530
title: >-
  SAT page: remove the Max modulation field from the PWM card, keep it only in
  Central Heating Settings
status: Done
assignee:
  - '@claude'
created_date: '2026-10-31T18:50:00Z'
updated_date: '2026-10-31T20:26:00Z'
labels:
  - sat
  - webui
  - beta.1
dependencies: []
priority: medium
ordinal: 686000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, #alpha-testing 2026-10-31 18:24/18:26 UTC (msgs 1566155941357944883, 1566156455160062037), entry 187: the PWM card's Max modulation number field showed a different value than the Central Heating Settings card's Max relative modulation slider. Both edit the same setting (settings.sat.iMaxRelModulation: settings key satmaxmodulation, REST sat status max_rel_modulation, SATcontrol.ino:3790, restAPI.ino:3903). He asks to remove the field from the PWM card and keep only the slider. UI only, no change in what SAT sends.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The PWM card on the SAT settings page no longer shows satmaxmodulation, and the field does not reappear in another settings card
- [x] #2 The Central Heating Settings slider still reads and writes the setting (browser test)
- [x] #3 Browser test against the real v2.js: the new check fails on the old page and passes on the new one; screenshots with synthetic data
- [x] #4 Bench unit serves the new page
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT settings page: satmaxmodulation added to SAT_NOT_IN_FORMS and removed from the PWM card list (v2.js), so no settings card shows it; the Central Heating Settings slider (TASK-1514, POST sat/settings/max_modulation) is the only editor. Cause of the different values (entry 187): both edit settings.sat.iMaxRelModulation, but the slider posts through the sat endpoint and the settings form kept its stale copy.
Evidence: AC1/AC2/AC3 tests/webui/v2-settings-sat-followups.test.mjs against the real v2.js: new 33/33 PASS; old v2.js fails exactly 'no settings card shows satmaxmodulation (1 rows)'; slider shows the live 80 % and moving it POSTs sat/settings/max_modulation 65. Screenshots (synthetic data) scratchpad shots1530/old|new/pwm-card-gas.png. AC4 bench .61 OTA app+fs alpha.578+5df5429 (settings kept, 174 keys): served v2.js contains SAT_NOT_IN_FORMS={satenabled:1,satmaxmodulation:1}; POST sat/settings/max_modulation 100 -> HTTP 200. evaluate --quick 0 FAIL. Pushed 5df542942, zip posted.
Not verified: a screenshot of the device-served page itself (the browser test runs the same v2.js against a fake device).
<!-- SECTION:FINAL_SUMMARY:END -->
