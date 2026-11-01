---
id: TASK-1358
title: 'v2 UI: offer all 16 GPIO output trigger bits (0-15) with their OpenTherm names'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T19:14:00Z'
updated_date: '2026-11-01T08:42:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 493000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-24 (ADR-200 open question 2): the v2 UI lists only bits 0-3 (data/v2.js:3036-3037) while the firmware accepts 0-15. Offer 0-15 with names per ADR-200: bits 0-7 boiler (slave) status (0 fault, 1 CH active, 2 DHW active, 3 flame on, 4 cooling, 5 CH2 active, 6 diagnostic, 7 electricity production per OT v4.2), bits 8-15 thermostat (master) status (8 CH enable, 9 DHW enable, 10 cooling enable, 11 OTC active, 12 CH2 enable, 13 summer/winter, 14 DHW blocking). Default stays 0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The trigger-bit selector shows 16 named options, saving bit 8 stores 8 (browser test red on HEAD, green; screenshot)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-10-31: tests/webui/v2-settings-sat-followups.test.mjs new 37/37, old v2.js fails 'select offers 16 named bits' (n=4) and 'saving bit 8 posts 8'; trigger-bit-labels.test.mjs new 5/5, old 4 FAIL. Screenshot scratchpad shots1358/new/trigger-bit-8.png. Pending: bench (build alpha.588 was blocked by low memory).

2026-11-01 evidence on alpha.597: browser test v2-settings-sat-followups.test.mjs PASS 'GPIO trigger-bit select offers 16 named bits' and 'saving bit 8 posts gpiooutputstriggerbit = 8'; trigger-bit-labels.test.mjs 5/5; served bench v2.js carries TRIGGER_BITS with 16 named entries. Open: the screenshot the AC asks for (Chrome extension not connected this session).
<!-- SECTION:NOTES:END -->
