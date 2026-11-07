---
id: TASK-1679
title: Find the async_tcp transient below the loop-sampled heap dip at two-tab load
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-07T04:05:00Z'
updated_date: '2026-11-07T04:45:00Z'
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
- [ ] #1 Measure the transient on the async_tcp task (a heap low-water hook or sampling inside the response path) during the two-tab load
- [ ] #2 Decide with Robert whether a reduction is worth its cost, with the numbers
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 instrumentation prepared (not committed: the public-beta freeze holds builds, and beta.117/118 belong to the release). Patch: scratchpad b103/task1679-ack-low.patch (127 lines). It attributes the heap-window minimum to the file response path: WebQueuedFileResponse::_ack reads platformHeapWindowMin() before and after _inner->_ack; a drop within one window generation (new platformHeapWinGen(), bumped on every window start, because MQTT restarts the window) is recorded by webAckLowObserve() as hd_ack_low_min / hd_ack_low_free_before / hd_ack_low_hits / hd_ack_low_files. If hd_ack_low_min is close to hd_min_free_heap after a two-tab load, the transient sits in the library ack (send-buffer copy + tcp_write pbufs); if hd_min_free_heap is much lower, it is elsewhere. Host fixture test_web_file_scheduling PASS and evaluate --quick 85/0 with the patch applied. Needs an MQTT heap window to be open (MQTT connected) during the run. Next: after 'beta.118 tag out', apply, bump, build, two-tab A/B on .61.
<!-- SECTION:NOTES:END -->
