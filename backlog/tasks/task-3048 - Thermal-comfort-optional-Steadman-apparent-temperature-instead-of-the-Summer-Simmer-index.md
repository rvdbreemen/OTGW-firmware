---
id: TASK-3048
title: >-
  Thermal comfort: optional Steadman apparent temperature instead of the Summer
  Simmer index
status: To Do
assignee: []
created_date: '2026-11-25T06:24:00Z'
updated_date: '2026-11-26T20:03:00Z'
labels:
  - sat
  - feature
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Maintainer go 2026-11-26 (Robert, interactive session: 'Voer taak 3048 optioneel uit'): execute as an OPTIONAL mode. The default stays the Summer Simmer index, so there is no behaviour change unless the user switches the mode. The label needs-maintainer-ok is removed; milestone 2.1.0 stays unless the coordinator says otherwise.

Assignee: otgw-webui (SAT owner). Plan first, then implement.

AC1 (Sergeant D decides whether the optional Steadman mode is wanted) stays open as a parallel question. The implementation does not wait for it (standing rule: build the recommended option meanwhile). It stays a lane until his answer is recorded in the decisions repo: no push before AC1 is recorded, or Robert says otherwise.

Test values for the host test (AC2): 22 C at 70 % gives 24.1 C (description: e = 18.45 hPa, AT = 24.09 C). 20 C at 50 % is not stated in the description; the same formula (AT = T + 0.33 e - 4.00, e = rh/100 x 6.105 x exp(17.27 T/(237.7+T))) gives e = 11.66 hPa and AT = 19.85 C (arithmetic, checked with a script on 2026-11-26).
<!-- SECTION:NOTES:END -->
