---
id: TASK-1602
title: >-
  SAT: MM/CH repair resend comes every 90 s instead of once a minute (entry
  239), the 60 s check just misses the 30 s tick
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-04T07:45:00Z'
updated_date: '2026-11-04T07:46:00Z'
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
- [ ] #1 Host test on the real satSendMM/satSendCH with a 30 s control tick and a few ms of send latency: repair every 60 s (red on old code: 90 s)
- [ ] #2 Fix keeps the on-change rule and the 60 s intent (e.g. compare with SAT_MMCH_REPAIR_MS minus a tick tolerance), same for CH and the Immergas TP
- [ ] #3 Bench .63 telnet: MM and CH repair gaps about 60 s, CS still 5 s
<!-- AC:END -->
