---
id: TASK-1673
title: >-
  Minimum free heap drops to ~2-3 KB during a two-tab page load (dev and PR #697
  alike)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-06T10:57:00Z'
updated_date: '2026-11-06T19:40:00Z'
labels:
  - heap
  - webui
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 796000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench .61 2026-11-06: hd_min_free_heap went to 2612 B (dev beta.106+069ae27, after three headless-Chrome page loads of the asset script) and to 3296/3172 B (PR #697 build beta.107+742f969, two-tab load test: 2 parallel tabs x 7 assets x 25 rounds, all 350 requests 200). maxfreeblock stays 31,732; free heap recovers to ~68-71 KB after. A dip this deep is within a few KB of an allocation failure. Not PR-specific (both builds). Scripts: session scratchpad b103/twotab_load.py, queue_burst.py.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Find what holds the memory at the minimum (per-connection AsyncTCP/lwIP buffers, response objects, JSON writers, WebSocket) with a heap trace or per-consumer counters during a two-tab load
- [ ] #2 Raise the minimum to a safe margin (target >= 8 KB, the HEAP_LOW level) under the same two-tab load, red/green on the bench
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-06 research workflow wf_6068a950-81e (4 readers + verifiers, code only, no device access; full output session tasks/w5svbqjue.output). Confirmed: (1) lwIP send queue per TX-holding port-80 connection up to TCP_SND_BUF 5744 B (4 x MSS 1436) of copied pbufs, ~6.2 KB each; the server closes before the last ACK (WebRequest.cpp:255-264), so closing tails also hold it: 2 streams + 0-2 tails = ~12-25 KB; compiled into prebuilt liblwip (sdkconfig.h:1279/1283), not changeable by -D. (2) newlib stdio buffer 4096 B per open LittleFS File (vfs_api.cpp:223-251, st_blksize = littlefs block 4096) = ~8 KB for 2 streams. (3) AsyncAbstractResponse _send_buffer 2872 B (WebResponseImpl.h:27, no #ifndef) during an ack. (4) TIME_WAIT pcbs: server closes first, 2*MSL 120 s, pool 16 x ~250 B = ~3-3.5 KB. Plausible: per-connection request/header objects ~1-2.5 KB each, WiFi dynamic RX buffers, /ws clients ~2 KB fixed + queue per tab (real browser only).

Planned fixes, app side, no library change: (A) in webSendFile open the File with setBufferSize(512) and pass it to beginResponse (saves ~3.5 KB per stream, ~7 KB at the peak; also drops the duplicate exists()/open); (B) webFileGateTryAdmit also caps at 1 stream when platformFreeHeap() < ~24 KB and answers 503 Retry-After 1 below ~16 KB (today it only looks at maxblock). Library/sdkconfig options (snd_buf per pcb, ASYNC_RESPONCE_BUFF_SIZE, CONFIG_LWIP_TCP_SND_BUF) need Robert's approval and an ADR. Sequencing: both fixes touch webSendFile and the file gate that PR #697 (TASK-1659) rewrites, so implement them after the #697 merge decision, on top of whichever code lands. Bench A/B: twotab_load.py on .61, hd_min_free_heap and hd_min_max_block, old vs fix.

2026-11-06 fix: (A) webServerCompat.h WebQueuedFileResponse opens the File itself with setBufferSize(512) and passes it to beginResponse(File, ...); (B) webFileGateTryAdmit caps at 1 stream when platformFreeHeap() < 24000 as well as maxblock < 16000. Host test tests/test_web_file_scheduling.py new case small_file_buffer_and_total_heap_cap: 3 FAIL on the old code (lastBuffer 0, two streams admitted at 20 KB free), PASS on the fix (0 failures).

2026-11-06 esp32-combo build of beta.111 failed: webServerCompat.h:505 'call of overloaded beginResponse(fs::File&, char[128], char[48]) is ambiguous' (fs::File's implicit operator bool reaches beginResponse(int, const char*, const char*)). The host mock had an explicit bool and no int overload, so it missed it. Mock made faithful (implicit bool + the int overload): host test then fails to compile with the same C2666; fix passes String(_path) explicitly, host test PASS.

2026-11-06 bench .61 beta.112 (contains #698 too, so combined effect): two-tab load 4 x 25 rounds, minimum free heap 8136 B over the four runs (first run 21788 B) vs 3356 B on beta.109 under the same two-tab load with separate assets; no 503s; bootcount steady. AC2 target is >= 8 KB (8192 B): 8136 B is 56 B short, so AC2 stays open. AC1 asks for a heap trace or per-consumer counters; the cause analysis so far is code reading (workflow wf_6068a950-81e), so AC1 stays open too. Next: per-consumer counters (lwIP TX-holding pcbs, open files, WS clients) sampled at the minimum, then decide whether the remaining gap needs more.
<!-- SECTION:NOTES:END -->
