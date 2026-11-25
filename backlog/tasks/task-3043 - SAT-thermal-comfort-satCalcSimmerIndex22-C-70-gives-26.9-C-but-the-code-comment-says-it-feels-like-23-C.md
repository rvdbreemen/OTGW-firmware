---
id: TASK-3043
title: >-
  SAT thermal comfort: satCalcSimmerIndex(22 C, 70 %) gives 26.9 C but the code
  comment says it feels like 23 C
status: To Do
assignee: []
created_date: '2026-11-25T05:17:00Z'
updated_date: '2026-11-25T05:55:00Z'
labels:
  - sat
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1150000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by gruijter in #alpha-testing 2026-11-23 (https://discord.com/channels/812969634638725140/1514720723980259460/1574350309323710545). SATcontrol.ino:2793 comment says 22 C at 70 % humidity feels like 23 C, but satCalcSimmerIndex() (SATcontrol.ino:5069) returns 26.9 C for (22, 70) and 22.4 C for (20, 50), checked by hand: F=71.6, idx=1.98*(71.6-(0.55-0.0055*70)*(71.6-58))-56.83=80.5 F=26.9 C. With thermal comfort on, SAT therefore sees the room 2-3 C warmer than measured. Not yet checked: the reference summer_simmer.py in thermo-nova (the code comment says it matches exactly) and Sergeant D's recorded decisions on thermal comfort. No claim about SAT intent until both are checked.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Compare satCalcSimmerIndex with thermo-nova summer_simmer.py (pinned 7beaaf5, file:line cited, opened) and say whether the 26.9 C is the reference value
- [ ] #2 Check docs/sat/sergeant-d-corrections.md for decisions on thermal comfort and name the entries
- [ ] #3 Decide with Sergeant D if the formula or only the comment is wrong; fix the comment or the formula with a host test red then green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 (BlueFlame): checked against thermo-nova 7beaaf5, opened this session. The formula equals thermo-nova: summer_simmer.py 1.98 x (F - (0.55 - 0.0055 x H) x (F - 58)) - 56.83, below 58 F the index is F, then converted to C and rounded to 1 decimal (summer_simmer.py:38). Ours is SATcontrol.ino:5229-5235 (origin/dev), same formula, NOT rounded. So 22 C at 70 % gives 26.9 C in both; the code comment at SATcontrol.ino:2793 ('22C at 70% humidity feels like 23C') is wrong, not the formula: no behaviour bug. Fix: correct the comment, and add 1-decimal rounding if Sergeant D answers 6b A or B (decisions repo, Q6b, asked 2026-11-25 as reply to 1575026429773479977). otgw-dev does it in the rounding lane. Sergeant D recorded decisions on thermal comfort: not yet grepped for this note; check before closing.

Correction 2026-11-25 (BlueFlame): the wrong comment is at SATcontrol.ino:2875-2877 on origin/dev ('22C at 70% humidity feels like 23C'), not 2793. Behaviour checked against thermo-nova climate.py:270-277: with presets.thermal_comfort the room temperature IS SummerSimmer.index(current_temperature, humidity), so SAT seeing +4.9 C (22 C/70 %) and +2.4 C (20 C/50 %) above the sensor is the reference behaviour. Answered gruijter in #alpha-testing (message 1575027175814332548).
<!-- SECTION:NOTES:END -->
