---
id: TASK-1675
title: Rare /favicon.ico request gets no answer within 12 s under two-tab load
status: To Do
assignee: []
created_date: '2026-11-06T19:39:00Z'
labels:
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 798000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen on .61 (OT-Direct S3 bench unit) beta.112: 4 of 400 requests in the two-tab load (scratchpad twotab_gz.py, '/' + '/favicon.ico' per tab, fresh connections) got no answer within 12 s; the one with a logged path was /favicon.ico (20:34:20). hd_webfile_queued stayed 0, so the request never entered the ADR-212 queue. Not reproduced in isolation: fav_probe.py 2 threads x 100 favicon only, and 2 x 50 page+favicon, 0 of 400. Same symptom seen earlier on PR #697 builds (asset_loading.py run 1, curl favicon 15 s once). beta.109 two-tab load: 0 of 350 timeouts. Possibly the known AsyncTCP/connection wedge (memory: HTTP 000 vs 503), not shown.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce the hang with a capture that shows where the request stops (telnet web debug + client timing), on dev
- [ ] #2 Same reproduction compared with beta.109 to decide whether it is a regression
- [ ] #3 Fix with red/green evidence, or a recorded explanation why it cannot be fixed in firmware
<!-- AC:END -->
