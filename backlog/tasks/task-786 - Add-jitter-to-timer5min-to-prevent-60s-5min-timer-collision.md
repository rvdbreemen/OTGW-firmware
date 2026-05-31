---
id: TASK-786
title: Add jitter to timer5min to prevent 60s/5min timer collision
status: Done
assignee:
  - '@claude'
created_date: '2026-05-31 16:38'
updated_date: '2026-05-31 16:43'
labels:
  - mqtt
  - timers
  - heap
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
timer5min and timer60s always fire together every 5 minutes (300s = 5x60s). This causes a burst of ~20 MQTT publishes from do5minevent() coinciding with PR=M response traffic, resulting in canPublishMQ dropping 26 messages at once. Fix: apply a one-shot random 30-59s offset to timer5min_due on first loop() run so the timers are permanently desynchronised.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 timer5min_due is shifted by a random 30-59s offset on first loop() run
- [x] #2 randomSeed called before jitter is applied (already guaranteed by setup() order)
- [x] #3 build passes: python build.py --firmware exits 0
- [x] #4 evaluate passes: python evaluate.py --quick shows no new failures
- [x] #5 jitter only applied once (static bool guard)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add __JitterOffset__(min, max) helper to safeTimers.h
2. Update DECLARE_TIMER_MIN/SEC/MS macros to call __JitterOffset__ with params 2+3
3. Update DECLARE_TIMER_MIN(timer5min, ...) in loop() with 30000, 60000 jitter
4. Build + evaluate
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added optional jitter_min_ms / jitter_max_ms parameters to all three DECLARE_TIMER_* macros (MIN, SEC, MS) in safeTimers.h. A new __JitterOffset__(min, max) helper returns random(min, max) when max > min, else 0. Applied to timer5min with 30000-60000ms so it can never align with timer60s (300s = 5×60s) again. All existing callers unaffected — default 0 = no jitter. Build clean, evaluator 100%. Pushed to origin/dev.
<!-- SECTION:FINAL_SUMMARY:END -->
