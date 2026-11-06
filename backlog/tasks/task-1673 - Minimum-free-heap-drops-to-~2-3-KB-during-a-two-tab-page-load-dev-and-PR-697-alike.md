---
id: TASK-1673
title: >-
  Minimum free heap drops to ~2-3 KB during a two-tab page load (dev and PR #697
  alike)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-06T10:57:00Z'
updated_date: '2026-11-06T21:11:00Z'
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
- [x] #1 Find what holds the memory at the minimum (per-connection AsyncTCP/lwIP buffers, response objects, JSON writers, WebSocket) with a heap trace or per-consumer counters during a two-tab load
- [ ] #2 Raise the minimum to a safe margin (target >= 8 KB, the HEAP_LOW level) under the same two-tab load, red/green on the bench
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-06 research workflow wf_6068a950-81e (4 readers + verifiers, code only, no device access; full output session tasks/w5svbqjue.output). Confirmed: (1) lwIP send queue per TX-holding port-80 connection up to TCP_SND_BUF 5744 B (4 x MSS 1436) of copied pbufs, ~6.2 KB each; the server closes before the last ACK (WebRequest.cpp:255-264), so closing tails also hold it: 2 streams + 0-2 tails = ~12-25 KB; compiled into prebuilt liblwip (sdkconfig.h:1279/1283), not changeable by -D. (2) newlib stdio buffer 4096 B per open LittleFS File (vfs_api.cpp:223-251, st_blksize = littlefs block 4096) = ~8 KB for 2 streams. (3) AsyncAbstractResponse _send_buffer 2872 B (WebResponseImpl.h:27, no #ifndef) during an ack. (4) TIME_WAIT pcbs: server closes first, 2*MSL 120 s, pool 16 x ~250 B = ~3-3.5 KB. Plausible: per-connection request/header objects ~1-2.5 KB each, WiFi dynamic RX buffers, /ws clients ~2 KB fixed + queue per tab (real browser only).

Planned fixes, app side, no library change: (A) in webSendFile open the File with setBufferSize(512) and pass it to beginResponse (saves ~3.5 KB per stream, ~7 KB at the peak; also drops the duplicate exists()/open); (B) webFileGateTryAdmit also caps at 1 stream when platformFreeHeap() < ~24 KB and answers 503 Retry-After 1 below ~16 KB (today it only looks at maxblock). Library/sdkconfig options (snd_buf per pcb, ASYNC_RESPONCE_BUFF_SIZE, CONFIG_LWIP_TCP_SND_BUF) need Robert's approval and an ADR. Sequencing: both fixes touch webSendFile and the file gate that PR #697 (TASK-1659) rewrites, so implement them after the #697 merge decision, on top of whichever code lands. Bench A/B: twotab_load.py on .61, hd_min_free_heap and hd_min_max_block, old vs fix.

2026-11-06 fix: (A) webServerCompat.h WebQueuedFileResponse opens the File itself with setBufferSize(512) and passes it to beginResponse(File, ...); (B) webFileGateTryAdmit caps at 1 stream when platformFreeHeap() < 24000 as well as maxblock < 16000. Host test tests/test_web_file_scheduling.py new case small_file_buffer_and_total_heap_cap: 3 FAIL on the old code (lastBuffer 0, two streams admitted at 20 KB free), PASS on the fix (0 failures).

2026-11-06 esp32-combo build of beta.111 failed: webServerCompat.h:505 'call of overloaded beginResponse(fs::File&, char[128], char[48]) is ambiguous' (fs::File's implicit operator bool reaches beginResponse(int, const char*, const char*)). The host mock had an explicit bool and no int overload, so it missed it. Mock made faithful (implicit bool + the int overload): host test then fails to compile with the same C2666; fix passes String(_path) explicitly, host test PASS.

2026-11-06 bench .61 beta.112 (contains #698 too, so combined effect): two-tab load 4 x 25 rounds, minimum free heap 8136 B over the four runs (first run 21788 B) vs 3356 B on beta.109 under the same two-tab load with separate assets; no 503s; bootcount steady. AC2 target is >= 8 KB (8192 B): 8136 B is 56 B short, so AC2 stays open. AC1 asks for a heap trace or per-consumer counters; the cause analysis so far is code reading (workflow wf_6068a950-81e), so AC1 stays open too. Next: per-consumer counters (lwIP TX-holding pcbs, open files, WS clients) sampled at the minimum, then decide whether the remaining gap needs more.

2026-11-06 instrumentation for AC1: loop() calls sampleHeapDip() every iteration; on a new free-heap minimum it snapshots port-80 TX pcbs and the bytes they hold (TCP_SND_BUF - snd_buf), active and TIME_WAIT pcbs, admitted file streams, REST in flight and WS clients (platformTcpHoldSnapshot under LOCK_TCPIP_CORE). Exposed as hd_dip_* in /api/v2/device/info.

2026-11-06 AC1 evidence, .61 beta.113, two-tab load 2 x 25 rounds (page + favicon, gzip pages): loop-sampled dip 13592 B free with 2 port-80 pcbs holding 11488 B in their send queues (2 x TCP_SND_BUF 5744, both full), 2 admitted file streams, 13 TIME_WAIT pcbs (~3.2 KB), 3 active pcbs, REST 0, WS 0. The native watermark hd_min_free_heap was 3168 B in this run, about 10 KB below the loop-sampled dip: the extra is transient on the async_tcp task (the 2872 B response send buffer plus pbuf copies during tcp_write). CORRECTION: the 8136 B minimum reported for beta.112 was one favourable run; the minimum varies strongly run to run (3168 B here, same code path). It is not a stable improvement figure.

2026-11-06 A/B on .61, reboot before every run (hd_min_free_heap is since boot), two-tab load 25 rounds: beta.113 cap 2: min free 20356 / 3328 / 3040 B (the low runs are the ones where both streams overlap, dip snapshot 2 TX pcbs 11488 B), p95 1.21-1.43 s. Experiment cap 1 (WEB_FILE_MAX_INFLIGHT 1, not committed): 21196 / 7596 / 12968 / 17528 B, p95 1.45-1.50 s but one run p50 5.91 s p95 11.55 s max 12.67 s. Cap 1 raises the worst case from ~3.0 to 7.6 KB, still below the 8 KB target, at a real latency cost. Found and fixed on the way: 'else if (mb < 24000) cap = 2' raised a base cap of 1 to 2; now it only lowers the cap (no effect with the production base of 2).

2026-11-06 Robert chose the per-connection send-buffer experiment: webBeginRequest caps pcb->snd_buf at WEB_TCP_SEND_CAP 2872 (2 x MSS) through platformTcpCapSendBuffer (core lock, only while nothing is queued). Committed locally as beta.115 for the bench A/B; not pushed until the A/B is done.

2026-11-06 beta.115 (send cap 2872) A/B: 1 of 5 rebooted runs completed before Claude Code stopped the background job for low system memory: min free 3252 B, p50 1.55 s p95 1.89 s (baseline cap2: 3040-3328 B in overlap runs, p50 1.05-1.26 s). Inconclusive with one run. hd_dip_tx_bytes showed 11488 B, which the cap makes meaningless: the metric was TCP_SND_BUF - snd_buf, which counts the lowered credit as held. Fixed to sum the unsent and unacked segment lengths (uncommitted until the next build). Next: rebuild, then the full 5-run A/B when memory allows; not restarted automatically.
<!-- SECTION:NOTES:END -->
