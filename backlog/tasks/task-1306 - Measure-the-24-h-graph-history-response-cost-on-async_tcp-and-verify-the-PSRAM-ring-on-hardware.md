---
id: TASK-1306
title: >-
  Measure the 24 h graph history response cost on async_tcp and verify the PSRAM
  ring on hardware
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T06:27:00Z'
updated_date: '2026-11-14T15:31:00Z'
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
- [x] #1 Temporary uncommitted build with the 726-slot ring forced static and a 1 s interval on the no-PSRAM bench: full 24 KB body served, curl time_total recorded, hd_max_loop_gap_ms before/after 5 requests recorded, no TWDT reset
- [x] #2 On a PSRAM board (LOLIN S3 Mini/Pro): /api/v2/graph/history reports interval_s 120, PSRAM free size before/after boot recorded, 24 h window filled after the run time allows
- [x] #3 Decision on the reboot-while-page-open history drop recorded (accept or fix)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 cycle: skipped for now, needs a firmware build and the PC is low on memory (a build was stopped by Claude Code); resume after the maintainer's go.

2026-10-23 cycle: skipped, AC2 needs a PSRAM board (LOLIN S3 Mini/Pro); USB shows only COM4 (OTGW32, no PSRAM) and COM3 (1.x ESP8266). AC1/AC3 could be done on the OTGW32 but the task cannot close without AC2.

Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: measurement only, needs a PSRAM board.

2026-11-04 22:1x (otgw-firmware-6c) AC2 part 1 on .63 (Classic+S3 combo, ESP32-S3FH4R2, beta.79+a9dd048, uptime 6 min after the last TASK-1395 reboot at ~22:06): device/info psram_found 1, psram_size 2097152, psram_free 2057988 (39164 B in use after boot, ring included); GET /api/v2/graph/history: HTTP 200, interval_s 120, count 3, 277 B, 0.195 s. .63 left untouched (no reboot) so the ring fills; recheck around 06:15 (~8 h = ~240 slots; a full 24 h window needs 24 h uptime, not available tonight because the bench is needed at 06:30).

2026-11-04 AC3 decided by Robert (interactive, this session): fix. mergeGraphHistory (data/v2.js) dropped every earlier history record on a reload (live = hist.filter(!r.h)); after a device reboot the ring only holds post-reboot slots, so history from before page load vanished on the next 10-minute reload. Fix: history records whose span ends before the oldest slot of the new response (tolerance one 5 s GSLOT for request timing) are kept; the overlap is still replaced by the response. tests/webui/graph-history-merge.test.mjs: new test 'history from before a device reboot survives the next reload' plus a stricter reload test; on HEAD b1fe73d2e both FAIL ('pre-reboot history slot 1799998770000 was dropped', 'history older than the new response is kept'); on the fix node --test tests/webui/graph-*.test.mjs: 58 pass, 0 fail. Needs a filesystem build + bump by the lead; bench: open Graph, reboot the unit, wait for the 10-min reload, earlier history still drawn.

2026-11-05 lead: (a) 1 s image (GRAPH_HIST_LARGE_INTERVAL_S=1, beta.79 e8fb50b) app-only on .63 (Classic, PSRAM 2 MB) 14:24; ring full at count 720 after 30 min. 14:55: 5x GET /api/v2/graph/history: 28237 B in 0.249-0.358 s, HTTP 200 each; bootcount 146 before/after, uptime continuous, lastreset Software reset (the OTA), hd_max_loop_gap_ms 476 before/after (baseline 435-459), crashlog empty. This is PSRAM-board evidence; AC1 asks the no-PSRAM bench (.61, psram_found 0): to run there after the TASK-1450 bench frees .61. (b) AC2: .63 back on beta.95+1fbd5cf at ~14:58 (interval_s 120 reported); psram_free 2057884 of 2097152 after boot; the 24 h window needs the unit untouched until ~2026-11-06 15:00.

2026-11-05 AC1 on the no-PSRAM bench .61 (psram_found 0): temporary uncommitted build (graphHistoryStuff.ino: static ring forced to GRAPH_HIST_LARGE_SLOTS 726, interval 1 s; beta.98+df9ea31-dirty, RAM 39.8%), app-only OTA 16:09; ring full 16:22 (count 720). 5x GET /api/v2/graph/history: 28753-28756 B in 0.245-0.324 s, HTTP 200 each; hd_max_loop_gap_ms 416 before and after; bootcount 66 before/after, uptime continuous, no TWDT, crashlog empty. .61 restored to beta.95+1fbd5cf (app+fs). The temp image is not kept in the repo. AC2 (PSRAM .63, 24 h window) runs until ~2026-11-06 15:00.

2026-11-11 08:49 otgw-firmware-09, AC2 read-only on .63 (beta.174+e6b4037, uptime 2:16, bootcount 11, so it restarted ~06:33 with the public beta.174): psram_found 1, psram_size 2097152, psram_free 2057544; GET /api/v2/graph/history HTTP 200, 2776 B in 0.059 s, interval_s 120, count 67, newest_age_ms 115005. 67 slots x 120 s = 134 min matches the uptime: the ring fills at one slot per 2 min. The full 24 h window (720 slots) needs uptime >= 24 h without a reboot: earliest check 2026-11-12 ~06:40. Series flow/ret/mod are null because .63 has no boiler on the bus (outside has values).

2026-11-12 06:08 AC2 capture on .63 (otgw-bench-tester), PSRAM board (ESP32-S3FH4R2), 2.0.0-beta.174+e6b4037, uptime 23:35, bootcount 11: psram_found 1, psram_size 2097152, psram_free 2057576; GET /api/v2/graph/history x3: HTTP 200, 27736 B each, time_total 0.368 / 0.282 / 0.421 s; interval_s 120, count 707; hd_max_loop_gap_ms 399 before and after; no reboot. AC2 NOT ticked: 24 h = 720 slots and the window was 26 min (13 slots) short, because .63 had to be flashed with the public-beta candidate at 06:09 (Robert wanted the beta earlier). Ticking needs either Robert accepting 707/720, or a fresh 24 h run on .63 (now on beta.203, boot 06:09:23; full at about 06:10 on 2026-11-13).

AC2 accepted by Robert, DM 1571184575894397039 (2026-11-14, relayed by the coordinator): option A, accept 707/720. Basis: the .63 capture of 2026-11-12 06:08 (otgw-bench-tester, beta.174+e6b4037, PSRAM ESP32-S3FH4R2, psram_found 1, psram_size 2097152, psram_free 2057576; interval_s 120, count 707, 3x HTTP 200 27736 B in 0.282-0.421 s, hd_max_loop_gap_ms 399 before/after, bootcount 11, no reboot) stopped at uptime 23:35, 13 slots (26 min) short of 720, because .63 had to be flashed with the public-beta candidate at 06:09; earlier 67 slots x 120 s matched the uptime exactly, so no interval drift. AC2 ticked on that maintainer decision.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: no-PSRAM bench .61 (psram_found 0), temporary uncommitted build with the 726-slot static ring at 1 s: ring full at count 720, 5x GET /api/v2/graph/history 28753-28756 B in 0.245-0.324 s, HTTP 200, hd_max_loop_gap_ms 416 before/after, no TWDT, crashlog empty (2026-11-05). AC2: PSRAM .63, interval_s 120, psram_free recorded, count 707 of 720 at uptime 23:35 with no reboot and no drift; the missing 13 slots are the beta flash at 06:09; accepted by Robert (DM 1571184575894397039, option A). AC3: decision on the reboot-while-page-open history drop recorded earlier in the notes. Not verified: a full 720-slot window on a PSRAM board.
<!-- SECTION:FINAL_SUMMARY:END -->
