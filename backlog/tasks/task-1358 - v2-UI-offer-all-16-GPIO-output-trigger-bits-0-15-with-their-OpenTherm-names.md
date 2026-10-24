---
id: TASK-1358
title: 'v2 UI: offer all 16 GPIO output trigger bits (0-15) with their OpenTherm names'
status: To Do
assignee: []
created_date: '2026-10-24T19:14:00Z'
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
