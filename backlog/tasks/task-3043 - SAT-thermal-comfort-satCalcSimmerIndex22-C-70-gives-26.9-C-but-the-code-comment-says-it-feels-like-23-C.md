---
id: TASK-3043
title: >-
  SAT thermal comfort: satCalcSimmerIndex(22 C, 70 %) gives 26.9 C but the code
  comment says it feels like 23 C
status: Done
assignee: []
created_date: '2026-11-25T05:17:00Z'
updated_date: '2026-11-26T20:32:00Z'
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
- [x] #1 Compare satCalcSimmerIndex with thermo-nova summer_simmer.py (pinned 7beaaf5, file:line cited, opened) and say whether the 26.9 C is the reference value
- [x] #2 Check docs/sat/sergeant-d-corrections.md for decisions on thermal comfort and name the entries
- [x] #3 Decide with Sergeant D if the formula or only the comment is wrong; fix the comment or the formula with a host test red then green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 (BlueFlame): checked against thermo-nova 7beaaf5, opened this session. The formula equals thermo-nova: summer_simmer.py 1.98 x (F - (0.55 - 0.0055 x H) x (F - 58)) - 56.83, below 58 F the index is F, then converted to C and rounded to 1 decimal (summer_simmer.py:38). Ours is SATcontrol.ino:5229-5235 (origin/dev), same formula, NOT rounded. So 22 C at 70 % gives 26.9 C in both; the code comment at SATcontrol.ino:2793 ('22C at 70% humidity feels like 23C') is wrong, not the formula: no behaviour bug. Fix: correct the comment, and add 1-decimal rounding if Sergeant D answers 6b A or B (decisions repo, Q6b, asked 2026-11-25 as reply to 1575026429773479977). otgw-dev does it in the rounding lane. Sergeant D recorded decisions on thermal comfort: not yet grepped for this note; check before closing.

Correction 2026-11-25 (BlueFlame): the wrong comment is at SATcontrol.ino:2875-2877 on origin/dev ('22C at 70% humidity feels like 23C'), not 2793. Behaviour checked against thermo-nova climate.py:270-277: with presets.thermal_comfort the room temperature IS SummerSimmer.index(current_temperature, humidity), so SAT seeing +4.9 C (22 C/70 %) and +2.4 C (20 C/50 %) above the sensor is the reference behaviour. Answered gruijter in #alpha-testing (message 1575027175814332548).

2026-11-26 otgw-webui AC1+AC2 (read only, thermo-nova pinned 7beaaf5, files opened this session). AC1: thermo-nova custom_components/sat/summer_simmer.py:31 index = 1.98*(F-(0.55-0.0055*H)*(F-58.0))-56.83; :34-35 below 58 F the index is F; :38 result converted to C and rounded to 0.1. Firmware satCalcSimmerIndex SATcontrol.ino:5069-5075 is the same formula and threshold; only differences: no rounding to 0.1 (published values are formatted anyway) and a guard that returns the raw temperature for humidity <0 or >100 (thermo-nova has none). Hand-checked with Python on the same formula: (22 C,70 %)=26.9, (20,50)=22.4, (22,50)=25.3, (22,30)=23.7. So 26.9 C IS the reference value; the formula is not wrong, the comment at SATcontrol.ino:2793 ('22C at 70% humidity feels like 23C') is. AC2: entries 32 (humidity only with the comfort option; Summer Simmer stays), 114 (no firmware-only comfort adjust; humidity only through Thermal comfort), 117 (indoor humidity only), 189 (published index stays as thermo-nova), 190 (summer simmer cutoff leaves PWM). All agree; no conflict; none states a feels-like value. AC3: only the comment is wrong, so no SAT behaviour changes and no Sergeant D decision is needed for the fix; proposed comment: e.g. 22 C at 70 % humidity gives an index of 26.9 C. Real effect worth knowing, but already settled by entries 114/117/189 (parity): with Thermal comfort on, a humid room reads warmer to the PID. Comment edit touches src, so it needs a prerelease bump and the normal gates; not done yet.

2026-11-26 otgw-webui AC3: only the comment was wrong (formula equals thermo-nova summer_simmer.py:31, no Sergeant D decision needed, entries 32/114/117/189/190 agree). The comment was corrected in 5534647d8 (TASK-3050), now at SATcontrol.ino:2920: '22C at 70% humidity ... 26.9C'. No behaviour change, so no red/green test applies; the value 26.9 for (22,70) is the one the harness of that commit rounds to 1 decimal. Closing with this reference.

Traceability (2026-11-26, otgw-webui): the comment fix at SATcontrol.ino:2920 ('26.9C (the index, 1 decimal)') is in commit 5534647d8 (otgw-dev, 25 Nov, under TASK-3050), contained in v2.0.0-beta.428, beta.432 and beta.438. Comment-only, no behaviour change, no CHANGELOG line needed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: thermo-nova summer_simmer.py:31/:34-35/:38 equals satCalcSimmerIndex SATcontrol.ino:5069-5075; (22 C, 70 %) = 26.9 C is the reference value (checked by hand and by tests/test_sat_simmer_rounding.py:62). AC2: entries 32, 114, 117, 189, 190 agree, no conflict. AC3: only the code comment was wrong; fixed in 5534647d8 (TASK-3050), pinned by tests/test_sat_simmer_rounding.py:135 ('feels like' 26.9C). No behaviour change; no Sergeant D decision needed. Not verified on a unit (comment only).
<!-- SECTION:FINAL_SUMMARY:END -->
