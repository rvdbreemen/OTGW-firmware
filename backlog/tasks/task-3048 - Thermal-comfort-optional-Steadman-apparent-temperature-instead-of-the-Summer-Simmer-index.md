---
id: TASK-3048
title: >-
  Thermal comfort: optional Steadman apparent temperature instead of the Summer
  Simmer index
status: To Do
assignee: []
created_date: '2026-11-25T06:24:00Z'
labels:
  - sat
  - feature
  - needs-maintainer-ok
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1151000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Idea from gruijter (#alpha-testing 2026-11-25, https://discord.com/channels/812969634638725140/1514720723980259460/1575033941981790219): the Summer Simmer index was built for outdoor summer heat and sees the room warmer than it feels (22 C at 70 % gives 26.9 C, +4.9 C; 20 C at 50 % gives 22.4 C, +2.4 C). He suggests Steadman's Apparent Temperature AT = T + 0.33 e - 4.00 for mild indoor climates without wind. Our check: e = rh/100 x 6.105 x exp(17.27 T/(237.7+T)) hPa; 22 C at 70 % gives e = 18.45 hPa and AT = 24.09 C (gruijter says about 23.3, which would need e of about 15.8 hPa; asked him which e he used). Both formulas on origin/dev: the SSI at SATcontrol.ino:5229-5235 equals thermo-nova summer_simmer.py and climate.py:276-277 uses it as the room temperature when thermal comfort is on. Using AT instead deviates from the reference and needs Sergeant D's decision (not decided, only an idea; Robert: all DECIDED items go to 2.0.0, this one is not decided). Related: TASK-3043 (SSI comment and rounding).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D decides whether an optional Steadman AT mode is wanted (decisions repo entry)
- [ ] #2 If yes: settings option, formula with the standard Magnus e, host test with 22 C/70 % = 24.1 C and 20 C/50 %, documented in the manual
<!-- AC:END -->
