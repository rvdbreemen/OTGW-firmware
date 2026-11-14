---
id: TASK-1932
title: >-
  SAT self-tune: a 60 min hour counts at >=90 % clean ticks, excluded ticks left
  out of the mean (entry 340)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T10:22:00Z'
updated_date: '2026-11-14T10:26:00Z'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. AC1 first (hard stop): one question to Sergeant D via 92/coordinator, 90 % of the ticks the hour had (A) or of the expected 120 (B). Facts: the replay of TASK-1927 (scripts/replay.py G_CLOSE_FILTER) counted against the ticks the hour had; on the 5 gruijter runs every 60 min hour has exactly 120 ticks (59 of 59 in followup2_windows.txt), so A and B give the same 10 hours there. They differ only for an hour with missing ticks (90-119; under 90 the few_ticks marker at SAT_CT_MIN_TICK_FRACTION 0.75 already drops it). Recommendation B (at least 108 clean 30 s ticks = 54 min of clean data; stricter, no credit for a gap, same field result). Record his answer as a new entry in docs/sat/sergeant-d-corrections.md before code.
2. ADR via adr-kit amending ADR-205 decision 2 (and entry 174's clean-hour rule): per-tick exclusions counted, hour counts at >= 90 % clean (per the AC1 answer); excluded ticks out of the hour mean and the phase counts; coeff_changed, not_sat at boot, few_ticks/no_ticks stay whole-hour; first-in-time reason kept for telemetry.
3. Code (SATcontrol.ino satCurveTune block only): satTuneAddTick counts excluded ticks (exclTicks + first reason) and adds signal/phase only for clean ticks; satTuneCloseHour marks the first reason when the clean share is below 90 %; SAT_CT_MIN_CLEAN_TICK_FRACTION 0.90 in SATcurveTune.h. Check not_sat: the boot marker is per tick today (satActive false) - confirm with the code which path is 'boot' and keep it whole-hour.
4. Host test (tests/test_sat_curve_selftune.py compiles the real block): 1 dhw tick of 120 -> counted, mean excludes it (red on old); 13 of 120 excluded -> not counted; coeff_changed and few_ticks stay whole-hour; mean equals the clean-tick mean. Old-vs-fix via the env override of the source.
5. Replay of the 5 gruijter runs with 92's harness on the fix: 10 counted hours, same votes as the share10 variant.
6. evaluate --quick, bump, build on GO, bench brief (no-regression; the tune telemetry on device/info), push, post.
<!-- SECTION:PLAN:END -->
