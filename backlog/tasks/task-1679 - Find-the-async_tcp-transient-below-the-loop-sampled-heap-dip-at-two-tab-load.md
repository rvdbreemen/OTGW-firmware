---
id: TASK-1679
title: Find the async_tcp transient below the loop-sampled heap dip at two-tab load
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-07T04:05:00Z'
updated_date: '2026-11-07T12:16:00Z'
labels:
  - heap
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 801000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1673 (ADR-216). With the per-connection send cap, the native minimum free heap is still about 10 KB below what the loop task samples at the dip (one run 3252 B). That transient sits on the async_tcp task outside the lwIP send queues (candidates from the TASK-1673 research: the 2872 B ESPAsyncWebServer response copy buffer plus pbuf copies during tcp_write, WiFi TX/RX buffers, request/header objects, 13 TIME_WAIT pcbs ~3.2 KB).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Measure the transient on the async_tcp task (a heap low-water hook or sampling inside the response path) during the two-tab load
- [ ] #2 Decide with Robert whether a reduction is worth its cost, with the numbers
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 instrumentation prepared (not committed: the public-beta freeze holds builds, and beta.117/118 belong to the release). Patch: scratchpad b103/task1679-ack-low.patch (127 lines). It attributes the heap-window minimum to the file response path: WebQueuedFileResponse::_ack reads platformHeapWindowMin() before and after _inner->_ack; a drop within one window generation (new platformHeapWinGen(), bumped on every window start, because MQTT restarts the window) is recorded by webAckLowObserve() as hd_ack_low_min / hd_ack_low_free_before / hd_ack_low_hits / hd_ack_low_files. If hd_ack_low_min is close to hd_min_free_heap after a two-tab load, the transient sits in the library ack (send-buffer copy + tcp_write pbufs); if hd_min_free_heap is much lower, it is elsewhere. Host fixture test_web_file_scheduling PASS and evaluate --quick 85/0 with the patch applied. Needs an MQTT heap window to be open (MQTT connected) during the run. Next: after 'beta.118 tag out', apply, bump, build, two-tab A/B on .61.

2026-11-07 07:5x pre-freeze-exit prep (no commit/build/push, freeze respected): patch applied in isolated worktree scratchpad/wt1679 at local HEAD d5431d4bf; git apply clean (6 files, +39/-1). Fixture tests/test_web_file_scheduling.py PASS (0 failures) compiled with MSVC through a g++ shim (no host g++/clang++ on this machine; MSVC does not enforce -Werror, so -Wall -Wextra -Werror is NOT verified). evaluate.py --quick on that tree: 91 checks, 84 passed, 0 failed, 0 warnings. Next after beta.118 tag out: copy patch to dev, bump, commit, build, OTA .61, A/B.

2026-11-07 bench .61, beta.119+35f2cfd, MQTT connected, 5 runs (reboot each, twotab_gz 25 rounds, 100/100 200 each): r1 min_free 5068 / ack_low_min 8004 (free before that _ack 22568, files 2); r2 19772 / 26320; r3 18576 / 51412; r4 19452 / 46972; r5 14240 / 28768. Reading: the one deep dip (r1, 5 KB) lies within ~3 KB of an _ack low, and that _ack started at 22.6 KB, so about 14.5 KB went during one file _ack with 2 file responses in flight. The usual 14-20 KB minimum is NOT inside an _ack (ack lows 26-51 KB); it happens elsewhere. Caveat: the heap window is process-wide, so the _ack low includes whatever another task allocated during that call. Log: scratchpad/b103/ack_b119.log.

2026-11-07 40 KB admission threshold tried as beta.120+86f6716 (local only): bench 5 runs min_free 13.9-21.0 KB (no 5 KB dip, but n=5), 3 timeouts of 12 s (r3 x2, r4 x1; beta.119: 0), p50 1.6-3.0 s vs 1.6-1.8 s, p95 up to 4.1 s vs 2.6 s; two streams still overlapped in an _ack at 32 KB free (admitted above 40 KB). Robert chose: revert (commit dropped, never pushed), keep monitoring. Log scratchpad/b103/ack_b120.log. Next: root-cause research of the dips with 5 options (Robert 2026-11-07).

2026-11-07 root-cause research (read-only workflow, 5 lenses + synthesis; journal subagents/workflows/wf_74bfb91d-58f): heap budget of the two-tab peak explains ~21-31 KB of the observed ~52-58 KB drop (lwIP TX pbufs full-MSS copy ~6.3 KB, _send_buffer 2.9 KB per _ack, WiFi dynamic TX copies 0-6.4 KB, LittleFS chain ~1.45 KB per open file, request/response/client objects ~4 KB, 13 TIME_WAIT pcbs ~3.3 KB); ~22-37 KB unexplained, hypothesis: concurrent non-web consumers (MQTT/telnet pcbs uncapped, post-boot HA burst) and WiFi dynamic RX occupancy (runtime 32 dyn RX/32 dyn TX, WiFiGeneric.cpp:274-279). Bench .61 has psram_found=0. Five options put to Robert: 1 zero-copy gz from flash, 2 own file response with one shared static buffer, 3 size-aware admission + favicon from PROGMEM, 4 TIME_WAIT reaper, 5 send cap/burst pause for non-web pcbs; prerequisite measurement: extend sampleHeapDip (heap_caps_get_info internal, MQTT queue, TX bytes per port, heap just before the load) + A/B with MQTT off.

2026-11-07 cause A/B on .61 (beta.120+8fad93a, no build; reboot per run, twotab_gz 25 rounds, 100/100 OK and 0 timeouts in every run; log scratchpad/b103/mqtt_ab.log). hd_min_free_heap per arm: A MQTT on, load 10 s after boot: 23580/14596/14308/15568/12748 (pre-load free 72.7-72.9 KB). B MQTT on, load 150 s after boot: 24876/19956/26284/20860/14364 (pre 72.8-73.3 KB). C MQTT off, load 10 s after boot: 44932/42068/45200/46544/46720 (pre 75.0-75.2 KB). Reading: with MQTT off the two-tab drop is about 30 KB (75 -> 45), which matches the web-path budget (21-31 KB); with MQTT on the minimum is 25-30 KB lower although MQTT costs only ~2.3 KB at rest. So the unexplained part is MQTT activity during the web load, and it is not only the post-boot burst (arm B still 14-26 KB). Settings identical before/after, MQTT reconnected. Side note: a capture-discovery test (TASK-1690) sent a few GETs to .61 during the run.
<!-- SECTION:NOTES:END -->
