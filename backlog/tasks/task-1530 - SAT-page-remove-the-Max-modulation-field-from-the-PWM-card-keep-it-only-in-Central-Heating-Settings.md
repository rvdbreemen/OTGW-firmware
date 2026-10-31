---
id: TASK-1530
title: >-
  SAT page: remove the Max modulation field from the PWM card, keep it only in
  Central Heating Settings
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-31T18:50:00Z'
updated_date: '2026-10-31T19:17:00Z'
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
- [ ] #1 The PWM card on the SAT settings page no longer shows satmaxmodulation, and the field does not reappear in another settings card
- [ ] #2 The Central Heating Settings slider still reads and writes the setting (browser test)
- [ ] #3 Browser test against the real v2.js: the new check fails on the old page and passes on the new one; screenshots with synthetic data
- [ ] #4 Bench unit serves the new page
<!-- AC:END -->
