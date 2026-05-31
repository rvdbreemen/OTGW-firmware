---
id: TASK-786
title: Add jitter to timer5min to prevent 60s/5min timer collision
status: To Do
assignee: []
created_date: '2026-05-31 16:38'
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
- [ ] #1 timer5min_due is shifted by a random 30-59s offset on first loop() run
- [ ] #2 randomSeed called before jitter is applied (already guaranteed by setup() order)
- [ ] #3 build passes: python build.py --firmware exits 0
- [ ] #4 evaluate passes: python evaluate.py --quick shows no new failures
- [ ] #5 jitter only applied once (static bool guard)
<!-- AC:END -->
