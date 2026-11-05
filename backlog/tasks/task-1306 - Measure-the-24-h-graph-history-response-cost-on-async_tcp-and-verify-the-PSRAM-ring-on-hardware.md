---
id: TASK-1306
title: >-
  Measure the 24 h graph history response cost on async_tcp and verify the PSRAM
  ring on hardware
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-23T06:27:00Z'
updated_date: '2026-11-04T21:15:00Z'
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
- [x] #3 Decision on the reboot-while-page-open history drop recorded (accept or fix)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 cycle: skipped for now, needs a firmware build and the PC is low on memory (a build was stopped by Claude Code); resume after the maintainer's go.

2026-10-23 cycle: skipped, AC2 needs a PSRAM board (LOLIN S3 Mini/Pro); USB shows only COM4 (OTGW32, no PSRAM) and COM3 (1.x ESP8266). AC1/AC3 could be done on the OTGW32 but the task cannot close without AC2.

Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: measurement only, needs a PSRAM board.

2026-11-04 22:1x (otgw-firmware-6c) AC2 part 1 on .63 (Classic+S3 combo, ESP32-S3FH4R2, beta.79+a9dd048, uptime 6 min after the last TASK-1395 reboot at ~22:06): device/info psram_found 1, psram_size 2097152, psram_free 2057988 (39164 B in use after boot, ring included); GET /api/v2/graph/history: HTTP 200, interval_s 120, count 3, 277 B, 0.195 s. .63 left untouched (no reboot) so the ring fills; recheck around 06:15 (~8 h = ~240 slots; a full 24 h window needs 24 h uptime, not available tonight because the bench is needed at 06:30).

2026-11-04 AC3 decided by Robert (interactive, this session): fix. mergeGraphHistory (data/v2.js) dropped every earlier history record on a reload (live = hist.filter(!r.h)); after a device reboot the ring only holds post-reboot slots, so history from before page load vanished on the next 10-minute reload. Fix: history records whose span ends before the oldest slot of the new response (tolerance one 5 s GSLOT for request timing) are kept; the overlap is still replaced by the response. tests/webui/graph-history-merge.test.mjs: new test 'history from before a device reboot survives the next reload' plus a stricter reload test; on HEAD b1fe73d2e both FAIL ('pre-reboot history slot 1799998770000 was dropped', 'history older than the new response is kept'); on the fix node --test tests/webui/graph-*.test.mjs: 58 pass, 0 fail. Needs a filesystem build + bump by the lead; bench: open Graph, reboot the unit, wait for the 10-min reload, earlier history still drawn.
<!-- SECTION:NOTES:END -->
