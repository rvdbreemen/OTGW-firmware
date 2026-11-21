---
id: TASK-2994
title: >-
  SAT docs: firmware-sat-spec and sat-comparison still describe the 180 s held
  BT and MM=T across range 5 (entries 437/438/440)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-21T04:26:00Z'
updated_date: '2026-11-21T14:06:00Z'
labels:
  - docs
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1121000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After TASK-2993/2992 the PWM duty uses the live flow after a 30 s start-up EMA (entries 437/438) and MM=T needs a raw duty above 1.03 (entries 436/440). Stale text found by the TASK-2993 review: docs/sat/firmware-sat-spec.md:1636, :1655, :1711, :1900; docs/sat/sat-comparison.md:515, :547, :568.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 firmware-sat-spec.md describes the 30 s EMA then live flow, and MM=T only above raw duty 1.03, with entry citations
- [x] #2 sat-comparison.md lists both as deliberate deviations from thermo-nova 7beaaf5 (pwm.py:144-149, 229-239) with entries 437/438/440
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 otgw-docs (95). Checked first: sergeant-d-corrections entries 436, 437, 438, 440, 441, 443, 444 and the later 445, 446, 447 (446/447: in PWM OFF only the flow is frozen, the request stays live; included). Code read at dev 8c929eee2 (SATcontrol.ino:1230-1298, :1640-1729, :1936-1951, :8545); thermo-nova 7beaaf5 opened: const.py:18, pwm.py:144-149, :229-239, heating_control.py:134-140, :517-518.

firmware-sat-spec.md: step list items 1 and 3, the 'Effective temperature, as coded' paragraph, MM=T item 6 (range 5 AND raw duty > 1.03), duty and seed pseudocode, constants table (30 000 ms window, 1 000 ms slack, 1.03), state table (_pwm_effectiveBoilerTemp, new rows _pwm_lastOnFlowTemp and _pwm_mmRelease), _pwm_flameOnMs sentence, open question 1 marked superseded, and a citation-base note (new cites marked 'dev 8c929eee2'). sat-comparison.md: new 'Update 2026-11-21' section listing both as deliberate deviations from 7beaaf5 with entries 437/438/440/441/443/444/446/447, area-4 row and plain-language items marked, firmware-only addition bullet for MM=T.

Second reader (independent sonnet reviewer, told to refute): no wrong firmware citation; findings fixed after I re-opened each: wrong thermo-nova cite for 'PWM on -> state OFF' (now heating_control.py:134-140 + :517-518, outside a hot-water draw), overstated entry-440 flip wording (now 'on at 0.93, off at 0.86, on again within about three minutes'), stale 180 s sentences in comparison item 3 and row point (c), stale _pwm_flameOnMs sentence, (verified) mark kept so the area-4 counts stay 67/67, pseudocode conditions completed, citation base note. Not changed (src, not mine): the comment at SATcontrol.ino:1284-1285 still calls the 180 s constant the start-up EMA window; reported to the coordinator. Gates: evaluate --quick Failed 0; sat_corrections_index --check current; no em dashes added.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: docs/sat/firmware-sat-spec.md describes the 30 s (plus 1 s slack) start-up EMA, then the live flow in the ON phase and the frozen last-ON flow with the current request in the OFF phase, and MM=T only in range 5 with the raw duty above 1.03, citing entries 436-447 and SATcontrol.ino at dev 8c929eee2 (each line re-opened). AC2: docs/sat/sat-comparison.md has an 'Update 2026-11-21' section listing both as deliberate deviations from thermo-nova 7beaaf5 (const.py:18, pwm.py:144-149, pwm.py:229-239; heating_control.py:134-140, :517-518) with entries 437/438/440 (and 441/443/444/446/447); area-4 row and items marked. Second-reader review done, its findings fixed. evaluate --quick Failed 0. Not changed: the stale src comment at SATcontrol.ino:1284-1285 (reported).
<!-- SECTION:FINAL_SUMMARY:END -->
