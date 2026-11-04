---
id: TASK-1602
title: >-
  SAT: MM/CH repair resend comes every 90 s instead of once a minute (entry
  239), the 60 s check just misses the 30 s tick
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T07:45:00Z'
updated_date: '2026-11-04T10:09:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 743000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench .63 beta.68 (2026-11-04, simulation on, 5.5 min telnet): MM and CH enqueued every 90.0 s (4 samples), CS every 5.0 s, SATTICK every 30.0 s. satSendMM/satSendCH (SATcontrol.ino ~688) skip while (millis() - iLastSentMMMs) < SAT_MMCH_REPAIR_MS (60000); the send stamp is taken a few ms after the tick, so at the 60 s tick the elapsed time is just under 60000 and the repair waits for the 90 s tick. Entry 239 says once a minute.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real satSendMM/satSendCH with a 30 s control tick and a few ms of send latency: repair every 60 s (red on old code: 90 s)
- [x] #2 Fix keeps the on-change rule and the 60 s intent (e.g. compare with SAT_MMCH_REPAIR_MS minus a tick tolerance), same for CH and the Immergas TP
- [x] #3 Bench .63 telnet: MM and CH repair gaps about 60 s, CS still 5 s
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-04 (sonnet agent; lead reran in wt-int4): SAT_MMCH_REPAIR_SLACK_MS 1000; repair due from 59 s. tests/test_sat_cmd_cache.py old: repairs at 90/180/270 s (red), fix: 60/120/180/240/300 s, changed MM still immediate; 28 OK; command_delivery 16, immergas_tp 5 OK; evaluate 0 failed. Tag beta.69; bench pending.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The unchanged MM/CH (and Immergas TP) repair resend comes every 60 s again (entry 239); it slipped to 90 s because the 60 s check just missed the 30 s control tick. Evidence: tests/test_sat_cmd_cache.py old repairs at 90/180/270 s, fix 60..300 s; bench .63 beta.69: MM and CH gaps 60.0 s (before 90.0 s on beta.68), CS 5.0 s.
<!-- SECTION:FINAL_SUMMARY:END -->
