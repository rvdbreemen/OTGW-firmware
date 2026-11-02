---
id: TASK-1550
title: >-
  SAT page: one 'PWM cycle length' field in minutes for every appliance type,
  with the recommended range per type in the tooltip
status: To Do
assignee: []
created_date: '2026-11-01T18:56:00Z'
updated_date: '2026-11-02T04:37:00Z'
labels:
  - web-ui
  - sat
dependencies: []
priority: medium
ordinal: 704000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-01 adopts maarten's proposal (#alpha-testing msg 1566524283092603057): you can only have one appliance type at a time, so two differently named fields for the same idea (the PWM cycle) are odd. Today the PWM card already shows only one of the two settings per type (TASK-1529, v2.js:2212-2217), but they look different: gas 'Cycles per hour' (3/4, v2.js:2117, :3084) and heat pump 'Heat-pump PWM cycle' in seconds (1800/2400, v2.js:2050, :3083), and the heat-pump hint 'the minimum on time is 180 s' made maarten read it as a minimum-on-time field. Wanted: one field labelled 'PWM cycle length', shown in minutes for every type: gas/Auto 20 min (= 3 per hour, default) or 15 min (= 4 per hour); heat pump and hybrid 30 min (default) or 40 min, plus the longer options once TASK-1542 / entry 195 land. The tooltip names the recommended range per type and that the field follows the chosen appliance type. Storage and REST stay as they are (satcyclesperhour, sathpcycle), so no settings migration and no change in SAT behaviour; the UI maps minutes to the right setting. A free numeric entry (maarten's 30-180 minutes) is out of scope: the allowed values are Sergeant D's (entries 30, 40, 50, 52, 194-198).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The PWM card shows exactly one field 'PWM cycle length' in minutes for every appliance type; switching the type swaps its choices without a reload
- [ ] #2 Saving writes the existing setting for that type (satcyclesperhour for gas/Auto, sathpcycle for heat pump/hybrid) with the same values as today; REST and settings.ini unchanged
- [ ] #3 Tooltip states the recommended range per type; the misleading 'minimum on time' hint is gone from the cycle field
- [ ] #4 Browser test on the real page: red on the old page, green on the new one, screenshots before/after with synthetic data
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: v2.js:2050/:2117 still two fields (seconds, cycles per hour). UI only, no Sergeant D gate.
<!-- SECTION:NOTES:END -->
