---
id: TASK-1529
title: >-
  SAT page: the PWM card shows only the cycle setting of the selected appliance
  type and is renamed from 'PID/PWM' to 'PWM'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-31T15:25:00Z'
updated_date: '2026-10-31T16:11:00Z'
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
- [ ] #1 The card is titled 'PWM'
- [ ] #2 With appliance type gas boiler only the boiler cycles setting is shown; with heat pump only the heat-pump cycle setting; switching the type updates the card without a reload
- [ ] #3 Webui test or browser screenshot before/after; all webui tests green
<!-- AC:END -->
