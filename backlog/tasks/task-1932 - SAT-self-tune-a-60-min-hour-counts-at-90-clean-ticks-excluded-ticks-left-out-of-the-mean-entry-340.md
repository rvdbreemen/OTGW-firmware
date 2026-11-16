---
id: TASK-1932
title: >-
  SAT self-tune: a 60 min hour counts at >=90 % clean ticks, excluded ticks left
  out of the mean (entry 340)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T10:22:00Z'
updated_date: '2026-11-16T04:26:00Z'
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
- [x] #1 Open point of entry 340 settled in the plan with Sergeant D before the build: is 90 % counted against all ticks of the hour or against the expected 120
- [x] #2 The tuner counts a 60 min hour when at least 90 % of its ticks are clean; excluded ticks do not enter the hour mean or the warm-up/floating counts; boot, coeff_changed and few_ticks markers stay whole-hour
- [ ] #3 An ADR amending or superseding ADR-205 decision 2, via adr-kit
- [x] #4 Host test on the real tuner code: red on the old code (hour with 1 excluded tick of 120 not counted), green on the fix; and an hour below 90 % clean stays excluded
- [x] #5 Replay of the 5 gruijter field runs of TASK-1927 on the fix gives 10 counted hours with the same votes as the replay variant
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Q-343a decided 2026-11-15 by the coordinator under Robert's directive (CLI) 'Do not wait for Sergeant D. Finish everything that can be finished.'; Sergeant D may overrule, his answer later becomes a follow-up task; asked in the 18-question batch in #dev-sat-mqtt (item 16, https://discord.com/channels/812969634638725140/1105556725714649128/1571633470696980583): option A, 90 % of the ticks the hour logged (109 of 121), meets entries 340 and 343. Unblocked (build was held for this answer).

Sergeant D answered Q-343a with B (entry 369, https://discord.com/channels/812969634638725140/1105556725714649128/1571637957230862506): an hour counts at 108 clean ticks whatever it logged (90 % of the expected 120), NOT 90 % of the ticks it logged (the directive choice A is replaced). Assignee per coordinator: 95.

Session 95, 2026-11-16 ~03:00, lane-95-1932 (worktree wt-95-1932 on origin/dev): the old lane commit 66eff3998 (ADR-234 + tuner + CleanShare) cherry-picked onto dev; conflicts: task file and ADR index files taken from dev (index regenerated with adr-doctor --fix-index), the one test hunk resolved per hunk (a first whole-file --ours lost the CleanShare tests; caught by grep, conflict recreated with checkout -m and fixed). Changed for entry 369 (Q-343a B): satTuneCloseHour() measures the clean share against the EXPECTED ticks only ((ticks - exclTicks) * 100 >= 90 * expected), not max(logged, expected); SATcurveTune.h comment; ADR-234 Decision/Contract/test list/Open Questions updated (adr_lint: ADR-234 PASS). AC1 settled: entries 343 (expected 120) and 369 (108 clean whatever it logged). Tests: CleanShare 'an hour with more ticks than expected needs 108 clean whatever it logged' (121 logged: 108 clean counts, 107 not). 15 older tuner tests assumed the old whole-hour rule; their stub default interval 3600 s made an hour expect 1 tick, so any mixed hour counted under the new rule: they now run at int:900 (4 ticks/hour) so their mixed hours are really below 90 % (sonnet agent, each change commented 'TASK-1932 (entry 369)', no assertion removed). Main-session run: tests/test_sat_curve_selftune.py 101 passed, 1 skipped (compiler-skip). Old code (origin/dev SATcontrol.ino + SATcurveTune.h via OTGW_SAT_INO/OTGW_SAT_HDR): CleanShare 6 failed, 1 passed (the whole-hour marker test, unchanged behaviour). AC3: ADR-234 stays Proposed (acceptance needs the human yes). AC5 open: the TASK-1927 replay harness lives in 92's scratchpad (tune-replay); asked 92 to run it on this tree. On those runs every 60 min hour has exactly 120 ticks (followup2_windows.txt, 59 of 59), so entry 369 changes nothing there; still to be shown.

AC5 (replay, 92, 2026-11-16 ~05:00): real tuner of 1f796d256 (satTune*/satCurveTune blocks + SATcurveTune.h, read-only via git show) on the 5 gruijter runs counts 10 hours: raise 6, band 4, lower 0 = TASK-1927 followup2 baseline; no hour counted in only one of the two. r1 2/20 (#18 +1.54 raise, 118 clean 2 dhw; #19 +0.40 band), r2 0/17, g167 5/11 (all raise), g214 0/7, g68 3/12 (all band). Every counted hour has 120 ticks, so entry 369 changes nothing on these runs; logged mean = clean-only mean (entry 343). 95 read summary.txt itself: TOTAL counted 10; votes raise 6 band 4 -> MATCH. Output: 92 scratchpad tune-replay/r1932/out/ (r1/r2/g167/g214/g68_1932.json, runlog.txt, summary.txt). Rebase onto origin/dev (92fc8fec7, beta.339): git diff 1f796d256 HEAD on SATcurveTune.h + tuner test = 0 lines, no tune lines in SATcontrol.ino diff, so the replay holds for the rebased commit. Re-run after rebase: test_sat_curve_selftune 102 OK (1 skip), test_sat_loop_integration 15 OK, test_sat_flow_avg OK, evaluate --quick 87 pass 0 fail.

Pushed 3423baa83..92fc8fec7 (beta.339) after 09 smoke PASS on .61 (2026-11-16 05:13-05:22): device 2.0.0-beta.339+92fc8fe, bootcount 809->811 (2 OTA restarts), freeheap 52.3-56.7 KB flat, maxfreeblock 31732, MQTT connected, telnet 2664 lines 0 panic/WDT, settings byte-identical, /api/v2/sat curve_tune block identical to beta.338 with SAT off. Not tested on hardware: the 90 % hour counting itself (needs SAT active); it rests on test CleanShare (red on old code) and the 92 replay (AC5). Zip posted #alpha-testing msg 1571743145467707473. Open: AC3 (ADR-234 acceptance, Robert's yes; coordinator asked by DM).
<!-- SECTION:NOTES:END -->
