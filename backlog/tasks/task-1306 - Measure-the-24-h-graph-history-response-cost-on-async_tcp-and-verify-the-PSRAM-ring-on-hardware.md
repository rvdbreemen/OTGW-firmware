---
id: TASK-1306
title: >-
  Measure the 24 h graph history response cost on async_tcp and verify the PSRAM
  ring on hardware
status: To Do
assignee: []
created_date: '2026-10-23T06:27:00Z'
updated_date: '2026-11-05T04:50:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 cycle: skipped for now, needs a firmware build and the PC is low on memory (a build was stopped by Claude Code); resume after the maintainer's go.

2026-10-23 cycle: skipped, AC2 needs a PSRAM board (LOLIN S3 Mini/Pro); USB shows only COM4 (OTGW32, no PSRAM) and COM3 (1.x ESP8266). AC1/AC3 could be done on the OTGW32 but the task cannot close without AC2.

Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: measurement only, needs a PSRAM board.

6c 2026-11-05 bench window .63. AC2 reading on beta.79+a9dd048 at 05:37 (uptime 7:33, lastreset Software reset): GET /api/v2/graph/history interval_s 120, count 226 (ring not yet full, 726 slots = 24 h), 8977 B in 0.069 s and 0.098 s (2 runs, HTTP 200); device/info psram_free 2057516, hd_max_loop_gap_ms 435. AC1: temporary image beta.79+e8fb50b (GRAPH_HIST_LARGE_INTERVAL_S=1) OTA-flashed 05:38 via safeboot, app only, settings 175 keys unchanged; at 05:46 count 515, hd_max_loop_gap_ms 459, psram_free 2057512, no reset. The measurement script (wait for 726, then gap / 5 curls / gap) was stopped by Claude Code at about 05:49 because the PC was low on memory (commit charge 87 %); it recorded nothing and was not restarted (session rule). Last reading before restore: count 658. AC1 is therefore NOT measured; it needs a new window. .63 restored to beta.79+a9dd048 at 05:49:45 (ota_flash.py, settings unchanged).
<!-- SECTION:NOTES:END -->
