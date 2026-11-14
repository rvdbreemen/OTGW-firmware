---
id: TASK-1932
title: >-
  SAT self-tune: a 60 min hour counts at >=90 % clean ticks, excluded ticks left
  out of the mean (entry 340)
status: To Do
assignee:
  - '@95'
created_date: '2026-11-14T10:22:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 988000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer + Sergeant D decision, entry 340: Robert 'Drop it to 90%?' (https://discord.com/channels/812969634638725140/1105556725714649128/1571102509303332867), 'Yes put it to Sergeant D' (1571103627807039599); Sergeant D 'I agree' (1571105706982121514) and Q-TUNE2 'The answer s B' (1571105869733564439). Today ADR-205 decision 2 drops a whole hour on any excluded tick (satTuneTickReason(), SATcontrol.ino ~6290-6305). Evidence: TASK-1927 (5 gruijter runs, 67 h, 8 -> 10 counted hours, no vote changes, <=0.5 C shift). Non-per-tick markers (SAT not in control at boot, coefficient changed, too few ticks) stay whole-hour. Shorter windows are not adopted. Assigned to lane 95.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Open point of entry 340 settled in the plan with Sergeant D before the build: is 90 % counted against all ticks of the hour or against the expected 120
- [ ] #2 The tuner counts a 60 min hour when at least 90 % of its ticks are clean; excluded ticks do not enter the hour mean or the warm-up/floating counts; boot, coeff_changed and few_ticks markers stay whole-hour
- [ ] #3 An ADR amending or superseding ADR-205 decision 2, via adr-kit
- [ ] #4 Host test on the real tuner code: red on the old code (hour with 1 excluded tick of 120 not counted), green on the fix; and an hour below 90 % clean stays excluded
- [ ] #5 Replay of the 5 gruijter field runs of TASK-1927 on the fix gives 10 counted hours with the same votes as the replay variant
<!-- AC:END -->
