---
id: TASK-1306
title: >-
  Measure the 24 h graph history response cost on async_tcp and verify the PSRAM
  ring on hardware
status: To Do
assignee: []
created_date: '2026-10-23T06:27:00Z'
labels:
  - webui
  - graph
  - heap
  - bench
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 429000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1296. Boards with PSRAM keep a 726 x 120 s ring and serve ~24 KB through restSendChunked, which re-runs the whole emit per ~1460 B window (~17 passes). Only the 4 KB no-PSRAM body was measured on a device (0.067 s). The 24 KB case is the TWDT/async_tcp starvation class (TASK-879/883) and lands on the boards where BLE runs by default. Also: the 10-minute reload in loadGraphHistory() drops earlier history records and keeps only what the device returns, so history from before a device reboot disappears from an open page on the next reload (accepted or fix, decide).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Temporary uncommitted build with the 726-slot ring forced static and a 1 s interval on the no-PSRAM bench: full 24 KB body served, curl time_total recorded, hd_max_loop_gap_ms before/after 5 requests recorded, no TWDT reset
- [ ] #2 On a PSRAM board (LOLIN S3 Mini/Pro): /api/v2/graph/history reports interval_s 120, PSRAM free size before/after boot recorded, 24 h window filled after the run time allows
- [ ] #3 Decision on the reboot-while-page-open history drop recorded (accept or fix)
<!-- AC:END -->
