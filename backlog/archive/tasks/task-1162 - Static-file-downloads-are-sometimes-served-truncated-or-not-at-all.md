---
id: TASK-1162
title: >-
  Under request overload, large static files stall mid-body and the client is
  left with a short 200
status: Done
assignee:
  - '@claude'
created_date: '2026-09-23 21:39'
updated_date: '2026-10-06 09:21'
labels:
  - web
  - bug
dependencies: []
priority: medium
ordinal: 293000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen 2026-09-23 on the OTGW32 bench (alpha.376): three back-to-back GETs of /settings.ini (5752 B on LittleFS):
- the first returned 4172 B with HTTP 200;
- the second got no response (curl 000);
- the third returned the full 5752 B.
A truncated 200 is worse than an error, because it looks like a valid file.

Re-scoped 2026-10-02 (maintainer's choice):
- Sequentially the symptom does not reproduce on alpha.401. Four bench cells gave 840 requests with no short 200: steady state, right after a LittleFS write, app-only reboot, and settings flush.
- It does reproduce under request overload: the refresh_storm.py arms with 6 and 8 workers, 3-4x the ADR-165 cap of 2.
- There, large static files (45-393 KB) stop mid-body with the connection still open. The client waits out its 10 s read timeout and is left with a 200 and part of the body.
- In the same 8-worker arm the device also stopped accepting new connections for about 20 s, then recovered without a reboot.
This task covers the overload behaviour.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduce: repeated GETs of a multi-KB static file on the bench, record size and status per request
- [x] #2 Root cause identified
- [x] #3 Under the 8-worker storm (refresh_storm.py --workers 8, seed 1124) no static response stalls mid-body: each is served complete, refused with a 503 before the body, or ends in a connection abort the client sees at once
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-30 reproduction tooling ready (scripts only, no firmware, no bump).
Premise shift from the triage: in both failure windows REST timed out too, and on this bench BLE is dormant (no PSRAM, risk-ack false), so the symptom looks like device-wide HTTP unresponsiveness after a LittleFS image write, not a static-file bug. Reproduce first; fix only after the cause is known.
scripts/tests/static_integrity.py: strictly sequential raw-socket GETs with Connection: close (never more than one request in flight, --gap default 0.2 s). Per request it records status, Content-Length, bytes received, close type (complete, FIN-short, RST, timeout, connect-fail) and elapsed ms. Targets: /settings.ini, /sat-slider.js (control), /index.html, /v2-bundle.css, /index.js, plus REST controls /api/v2/device/info and /api/v2/settings; expected sizes come from /api/listfiles. Each batch records heap telemetry from device/info; 503s count as gate responses, not failures. --batches / --minutes (a timed run needs --batches 0), --csv output.
Evidence: py_compile OK; tests/test_static_integrity.py 33 tests OK (re-run in the main tree today) against a local stub server that serves a correct file, a truncated body under a longer Content-Length, an RST and a stall. Workflow review: 28 mutants of the classifier all killed (the original version let 6 survive; the fixup closed them).
Bench plan (cells, per the triage): after a LittleFS image write, an app-only-reboot control, steady state, and an alternating-value settings flush, each with REST and heap telemetry. Not run yet: the OTGW32 is off the network until it is provisioned. AC#3 rewording (option b) and the title re-scope wait for the cell data.

2026-10-02 bench cells on the OTGW32, alpha.401+0d35143, scripts/tests/static_integrity.py: strictly sequential, one request in flight, 30 batches x 7 targets = 210 requests per cell. CSVs in %LOCALAPPDATA%/OTGW-capture/task1162-bench-20261002/.

Cells:
- steady state: CLEAN. 210 of 210 complete, no 503, pcbs max 1, no reboot.
- right after a LittleFS image write (app + fs over USB, measured from the first health 200): CLEAN 210/210.
- app-only reboot control: CLEAN 210/210.
- settings flush: 80 alternating POSTs of ui_graphtimewindow during the run, each rewriting settings.ini, all answered 200.
  - One GET /settings.ini answered HTTP 500 with a complete 23 B error body, during a rewrite. That is an honest error status, not a short 200.
  - 5 responses matched a listing re-read: the file really changed between the reads.
  - No reboot.

The 2026-09-23 symptom does not reproduce on alpha.401 under sequential load, after an FS write included: no truncated 200, no curl 000.

Short 200s do reproduce under overload. The TASK-1124 request storm the same evening (refresh_storm.py, 15 paths, 33% client aborts) returned 2 short 200s (body shorter than its Content-Length) in each of the 6-worker and 8-worker arms. Those arms also had about 20 timeouts and the pcb pool at 11-15 of 16. Arms with 2 and 4 workers had none.

AC#1 done: the defect is reproduced (under overload) and every request is recorded with size and status.

AC#2 and AC#3 stay open. The root cause of a mid-body close under overload is not identified. The task's scope (sequential symptom vs overload-only) needs the maintainer's call before a fix is designed.

2026-10-02 correction, re-scope and hypotheses.

Correction to my earlier note. It called the storm's short 200s "body shorter than its Content-Length". That is true but incomplete:
- All six were stalls, not closes: four in the first storm, two in the re-run.
- In each, the connection stayed open (eof false) and no byte arrived within the 10 s read timeout.
- So the "RST instead of FIN" option I put to the maintainer does not apply.
- Stalled paths: /v2.js twice (292022 B), / (45055 B), /index.js (392995 B), /v2.html (50340 B), /graph.js (46699 B).
- Received before the stall: 11283 to 275513 B. Every stalled file is at least 45 KB; no smaller file stalled.

Serve path: webSendFile() (webServerCompat.h) takes the file gate, then sends beginResponse(LittleFS, path), an AsyncFileResponse. The gate slot comes back on disconnect, so a stalled response holds its slot until the client leaves.

Hypotheses, none confirmed:

H1, ESPAsyncWebServer 3.11.0 in-flight credit (as resolved in .pio/libdeps; ASYNCWEBSERVER_USE_CHUNK_INFLIGHT defaults to 1 in ESPAsyncWebServer.h:72, _in_flight_credit starts at 2 in WebResponseImpl.h:70). In AsyncAbstractResponse::write_send_buffs() (WebResponses.cpp:346-500):
- a credit comes back only with an ACK, never with a poll;
- once _sentLength > CONFIG_LWIP_TCP_WND_DEFAULT, a call made with no credit returns at once;
- every send pass takes a credit, even one that sent nothing (OOM on the 2xMSS buffer, or add() returning 0 under ERR_MEM).
- So one empty pass, just as the last in-flight data is ACKed, leaves credit 0 and nothing in flight. No ACK can come, every poll is ignored, and the response sits until the client leaves.
- Supporting: AsyncTCP's 5 s ack timeout only runs while data is unacked, and no server-side close came within 10 s.
- No upstream release from 3.11.1 to 3.12.1 mentions this.

H2, TCP retransmission backoff on a congested WiFi link. Weaker: the 5 s ack timeout would then normally close the connection within the 10 s window.

H3, async_tcp starvation. The 8-worker arm also stopped accepting connections for about 20 s.

Evidence that separates them: a client-side capture of a stalled connection. The capture needs admin rights for pktmon; Wireshark/npcap is not installed.
- H1: last server segment ACKed, window open, then silence.
- H2: retransmissions with growing gaps.
- H3: the device stops ACKing the client at all.
Then old-vs-fix with the storm. For H1 that means a build with -D ASYNCWEBSERVER_USE_CHUNK_INFLIGHT=0, which is a build flag, not a library edit.

H1 EXPERIMENT, flag-OFF run #1 (2026-10-03 10:31 local). Bench OTGW32 192.168.88.61 running 2.0.0-alpha.402+febf7f4, the diagnostic build with -DASYNCWEBSERVER_USE_CHUNK_INFLIGHT=0 and ESPAsyncWebServer 3.11.2. SAT disabled, host idle.
Command: .venv/Scripts/python.exe scripts/tests/refresh_storm.py --host 192.168.88.61 --workers 2,4,6,8 --seed 1124. These are the same args as the flag-ON idle rerun of 2026-10-02 22:42: 45 s per arm, abort 0.33, 15 paths, 2 /ws subscribers.
Log: %LOCALAPPDATA%\OTGW-capture\refresh-storm\refresh_storm-192.168.88.61-storm-20261003-103124.ndjson.
8-worker arm, flag-ON (22:42) against flag-OFF (now):
- total 301 against 561;
- ok 41 against 102;
- 503 208 against 418;
- short 2 against 0;
- timeout 35 against 0;
- aborted 15 against 41.
The 2-, 4- and 6-worker arms had no shorts or timeouts in either run. Tool verdict PASS; bootcount 4->4.
This is consistent with H1: with the in-flight credit off, no response stalls mid-body and throughput nearly doubles. Each condition has one run so far; a repeat of each is under way.
Note: python on PATH is now /c/Tools/Codex/python 3.12, which has no websocket-client. Use the repo .venv interpreter.

CORRECTION, flag-OFF run #2 (2026-10-03, same build, same args, idle host). It contradicts run #1.
Log: refresh_storm-192.168.88.61-storm-20261003-<second run>.ndjson. Output: scratchpad storm1162_flagoff_2.txt. Tool VERDICT FAIL (exit 1).
8-worker arm: 126 requests; ok 9, 503 39, short 2, timeout 67, aborted 9. The probe answered at 2.2 s (pcbs 10/16, maxblk 14324), then timed out at 9, 16, 23, 30, 37 and 44 s. For about 35 s the whole device stopped answering, not just single bodies. The gate balance failed (/api/v2/settings timed out 3x) and passed again after the arm. No reboot (boot 4->4); /ws errors 16.
Conclusion: switching the in-flight credit off (CHUNK_INFLIGHT=0) does NOT prevent the stalls. Across the three 8-worker runs:
- flag-ON 22:42: 37 of 301 stalled;
- flag-OFF #1: 0 of 561;
- flag-OFF #2: 69 of 126.
Run-to-run variance dominates, so H1 is not supported as the cause. The device-wide unresponsiveness in run #2 points to H3 (async_tcp starvation) or H2 (link congestion): stalled bodies would then be a symptom of the device going quiet.
Next step: discriminate H2 from H3 with a capture during an episode. Device side: telnet async_tcp/loop watermarks, pcb counts, 'first 200 after stop'. Client side: pktmon (admin). One run per condition is not enough; compare stall counts over several runs per condition.

LAYER PROBES, 2026-10-03, alpha.404+1d9ae71, flag ON (default), MQTT connected to the Docker test-rig broker. Scratchpad layer_probe_1162.py, run next to refresh_storm.py --workers 6,8 --seed 1124. It probes once per second: ICMP ping to the device, ICMP ping to the router (control, run 2 only), TCP connect to :80, and GET /api/v2/health.
Run 2 (logs layers_1162_run2.jsonl and storm1162_layers_run2.txt): the 8-worker arm had 3 shorts and 43 timeouts. In the HTTP failure window, 83-204 s (120 s):
- device ping 0/120
- ROUTER ping 120/120
- TCP connect 0/40
- HTTP 0/41
Run 1 (no control ping): windows of 9-212 s and 220-501 s with device ping 2/202 and 1/279, and connect 0/67 and 1/93.
CONCLUSION: the device stops answering at the IP level, ICMP and the TCP handshake included, while the network is healthy. lwIP answers both without the application, so this is NOT async_tcp starvation (H3 refuted) and NOT the link (the router answered throughout). The device's own network stack goes deaf for 35-280 s.
SMOKING GUN: afterwards /api/v2/device/info showed hd_min_free_heap = 504. The native allocator low-water mark since boot is 504 bytes free, and this boot (bootcount 5, uptime 02:24) only saw the two layer-probe storms. No reboot; free heap recovered to 80696 and maxfreeblock to 31732.
New leading hypothesis, H4: under 6-8 parallel requests the heap runs out, the WiFi driver and lwIP cannot allocate RX buffers or pbufs, incoming frames are dropped in the driver, and the device stays network-deaf until the heap frees up. The shorts and timeouts in the storm are the symptom.
Next step to confirm: capture COM4 during a storm. esp_wifi and lwIP ESP_LOG errors do reach USB-CDC (scripts/tests/_serialcap.py); note that opening COM4 resets the board via RTS. Then look at the heap budget per concurrent static-file serve.

Run 3 (layer probes, router control ping, and a COM4 serial capture with host epoch timestamps, scratchpad serialcap_ts.py). The 8-worker arm had 2 shorts and 6 timeouts, then a stall of 289 s (112-401 s):
- device ping 0/287
- ROUTER ping 286/286
- TCP connect 0/96
- HTTP 0/97
Three of three runs show the same signature: the device's IP stack goes deaf for 120-289 s while the network is healthy.
Serial: 0 lines in 600 s. Nothing reached USB-CDC during the stall, so there is no direct log confirmation. Closing COM4 at the end reset the board (bootcount 5 -> 6), so run 3's heap low-water mark is lost. The device was still deaf when the probes stopped at 401 s; whether it recovered before the reset is unknown.
Known open item, from the platformio.ini comment next to the async_tcp stack size: '16384 stops the PERMANENT wedge, not the transient conc>=6 overload; that needs an accept-layer heap guard + fewer/gzipped assets'. This task measures exactly that transient conc>=6 overload.
Next options:
(a) Diagnostic build that prints free heap, maxblk and pcbs every second to the USB-CDC (HWCDC) console, which does not depend on WiFi, to time the collapse against the stall.
(b) The fix direction: an accept-layer heap guard that refuses new TCP connections below a heap floor before any request object is allocated. Optionally shrink the NimBLE host pools (TASK-1199, about 20.6 KB while BLE runs) to widen headroom.

Related observation, 2026-10-03 (from the TASK-1199 OLD baseline): with BLE switched on at runtime on the no-PSRAM OTGW32 (alpha.404, satbleriskack true), internal_free fell to about 15 KB, and the device went silent on REST for about 3 minutes WITHOUT any storm: 13 POSTs in a row timed out, hd_min_free_heap read 396 B, there was no reboot, and ping answered afterwards. It is the same signature as the storm episodes, reached through a different heap consumer. The TASK-1162 storm runs ran with BLE off (satbleriskack false), so BLE is not what caused those.

DIAG RUN 2026-10-04. The test-only image T1162diag v2 (alpha.404+b6e900a plus a 1 Hz USB-CDC telemetry task, never committed) ran on the OTGW32 bench. Storm: refresh_storm.py --workers 2,4,6,8 --seed 1124, with layer probes (ping, connect and http to the device; ping to the router as control). Closing COM4 with dtr/rts held low did NOT reset the board (two listen runs, boots stayed 32).
WINDOW 1, deaf for 165.5 s: ping 0/163 and connect 0/55 while the router answered 164/164.
- The CDC telemetry kept flowing, 165 of 165 lines with no seq gap, so the CPU and the tasks were alive throughout.
- At onset: free 29780, maxblk 7668, dma 22020, dma_blk 4852, failed allocations af=204, lwIP TX queue txq about 12 KB.
- Inside: min free 11960, min maxblk 1780, min dma_blk 1524, txq up to 23.6 KB. 444 more failed allocations in the default/internal caps, but 0 more DMA-cap failures.
- The station netif stayed up/link/ip for the whole window.
- The window ENDED with a WiFi disconnect plus reconnect plus got-ip (reason code 16). Right after, heap was back at free 74884, maxblk 31732, txq 150.
WINDOW 2 (40 s) was a reboot: boots 32 to 33 with reset reason 6 (task watchdog).
Reading (an interpretation, not proven):
- The deafness starts while the device is still associated and its heap is near exhaustion, with internal-cap allocations failing.
- It ends not by memory draining but by a WiFi re-association, which drops every TCP pcb and the queued TX data.
- The task watchdog reset that followed is a separate finding: the storm can also crash the device.
Evidence (out of the repo): %LOCALAPPDATA%/OTGW-capture/task1162-diag/run-2026-10-04/ (timeline.csv 537x148, cdc.jsonl, probe.jsonl, summary.txt). The tool's exit code 1 was the storm's own return code (aborted requests), not a tool fault.

ANALYSIS of run-2026-10-04 timeline.csv (537 rows, 1 Hz CDC plus probes). Root-cause CANDIDATE, not yet falsified.
FACTS (from the timeline):
(1) Before onset the storm repeatedly pushes lwIP queued TX bytes (txq) to 12-15 KB while internal free and the largest blocks collapse, and the device recovers each time (txq back to 0).
(2) Onset t=149.8 s lies in arm 4 (8 workers, 33% of requests aborted by the client after 1024 body bytes). At onset: txq 12.7 KB, 9 ESTABLISHED pcbs, DMA largest block 4852.
(3) Inside the 165 s window:
  - txq is FROZEN at 23637 B (qlen 22, 9 ESTABLISHED).
  - rxage climbs to 212 s, so no RX reaches lwIP.
  - cpu_atcp is about 1 (async_tcp idle) while the CDC stream and tasks run normally.
  - Failed allocations keep coming, all af_last_size=2312 with caps 0x1800 (INTERNAL|DEFAULT): af_di +444, af_dma unchanged at 72.
  - The largest DMA/DRAM block is 1524 B, while the largest INTERNAL block (4084, the maxblk column) equals rtc_blk, i.e. it is RTC fast memory.
(4) The window ends with a WiFi disconnect, reason 16 = WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT (esp_wifi_types). That is the AP dropping the station at a GTK rekey, not the firmware recovering. Right after it: txq 150 B, heap 75 KB.
(5) ESP.getMaxAllocHeap() (Esp.cpp:171) is heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), and it backs platformMaxFreeBlock(), which webFileGateTryAdmit() uses. During the window that number is the RTC block, which (fact 3) does not satisfy a 2312 B INTERNAL|DEFAULT malloc. The gate therefore sees 4-7.6 KB 'free block' while DRAM is starved. Its cap also never drops below 1.
MECHANISM (inference that fits all facts):
- Memory deadlock. TCP send queues of the stormed responses pin internal DRAM until the RX-path allocation fails.
- With no RX there are no ACKs, so the queues never drain and the memory never returns. The station stays associated but deaf, until an external event (here the AP's rekey timeout) tears the pcbs down.
NOT proven:
- That the 2312 B allocation is the WiFi driver's RX copy. The WiFi blob is closed; this rests on the size, the 802.11 max frame body.
- Which pcbs hold the 23.6 KB. The diag has no per-pcb breakdown.
Next (per review): rebase the diag onto current dev (the alpha.404 diag still had the per-call /.health write, TASK-1205). Then falsify with one variable: a DRAM-headroom 503 (gate on a DEFAULT-capable largest block) and/or a no-progress abort of stalled sends. The fix design goes to the maintainer first. Window 2 (TWDT reset, rst=6) is filed separately.

2026-10-05 rebased diag runs (variants built on dev 1624eab34 = alpha.412 code, T1162diag test-only telemetry; storm arms 2/4/6/8 workers x 45 s, seed 1124, --require-cdc).
- BASE (no variant): 1 deaf window of 7.0 s (t=226-233 s); no reboot (boots 7 throughout); storm exit 0; telemetry seq 43..460 with 41 device-side drops. Compare: on alpha.404 (2026-10-04) the same storm gave a 165.5 s deaf window and then a TWDT reboot. The effect is much weaker on current code; single-run variance is not yet known.
- Variant A (headroom 503 at 8192 B INTERNAL|DEFAULT) was built and flashed, but its storm run was killed by the host memory-pressure reaper before it finished. Variants B and A+B are not built yet (also reaped).
Evidence: %LOCALAPPDATA%/OTGW-capture/task1162-diag/variants-alpha412/ (run_base/, run_base.log, firmware_*.bin).

Falsification runs complete, 2026-10-05, same storm for each, one run per variant. Deaf = ping AND connect failing for 5 s or more.
| variant | deaf windows | headroom 503 | aborts | min DEFAULT block |
| BASE | 7.0 s | - | - | 1268 B |
| A (headroom 8192, INTERNAL\|DEFAULT) | 11.0 s | 426 | - | 1332 B |
| B (abort on ack timeout) | 97.0 s (ended by WiFi reassoc, reason 2) + 273.6 s still deaf at the end | - | 4 | 1524 B |
| A+B | 281.7 s + 6.6 s | 302 | 9 | 1204 B |
Reading:
- A refused 426 new requests and the DRAM still ran out. The memory is held by connections and TCP queues admitted earlier, not by new admissions.
- B's ack-timeout abort fired only 4-9 times and did not break the deadlock.
- Neither variant, nor both together, fixes the deafness.
- Run-to-run spread is large: BASE gave 7 s today and 165.5 s on alpha.404 yesterday. Single runs rank nothing finely, but none of A/B/A+B shows an improvement.
Root-cause status (AC#2): the memory-deadlock mechanism stands (TX queues pin DRAM; RX allocations fail; no ACKs arrive). The two app-level levers tested do not reach it.
Remaining levers, all at library or framework level:
- capping each TCP pcb's send buffer (pcb->snd_buf at accept in AsyncTCP);
- capping the concurrent AsyncTCP connections (CONFIG_ASYNC_TCP_MAX_ACK_TIME / the accept limit);
- lwIP/WiFi buffer sizing (TCP_SND_BUF, WiFi static RX buffers). The Arduino core ships these prebuilt, so they need a framework rebuild.
These are maintainer decisions. Evidence: %LOCALAPPDATA%/OTGW-capture/task1162-diag/variants-alpha412/ (run_base, run_A, run_B, run_AB with cdc.jsonl, probe.jsonl and timeline.csv).

2026-10-05 15:00: maintainer chose 'also limit connections'. Test variants added in wt-1162 (test code, never commit): C = cap each web pcb's snd_buf at T1162_SNDBUF (2920 B, 2x MSS) in webBeginRequest under LOCK_TCPIP_CORE; D = cheap 503 through the existing REST/file gate path when more than T1162_CONNCAP (4) pcbs on port 80 are active (counted from tcp_active_pcbs; no AsyncTCP change; no abort mid-handler to avoid a use-after-free on the request). Diag line adds sndcap= and conn503=. Built: firmware_C.bin, firmware_D.bin (%LOCALAPPDATA%/OTGW-capture/task1162-diag/variants-CD/). C+D build was stopped by Claude Code under memory pressure (commit charge 91%); not restarted. Bench is busy with the TASK-1036 soak until about 00:55; storm runs (base vs C vs D vs C+D, several runs each) follow after it.

2026-10-06 01:30-03:40: variants C and D, interleaved, 3 runs each (run_1162_diag.py, refresh_storm --workers 2,4,6,8 --seed 1124, OTGW32, all on the alpha.412 T1162diag tree; base = variants-alpha412/firmware_base.bin). Deaf windows (ping AND connect failing >= 5 s):
- base: r1 269.2+5.5+267.0 s; r2 49.4+272.7 s; r3 149.5+225.3 s (still deaf at the last probe, recovered without reboot). Earlier base run 2026-10-05: 7.0 s.
- C (snd_buf cap 2920 B per web pcb): r1 6.2 s; r2 176.7 s; r3 0. Cap applied 1110-1523 times per run. Helps sometimes, not reliably.
- D (cheap 503 when > 4 port-80 pcbs active): r1 0; r2 0; r3 0. 2046 / 2543 / 2113 refusals per run.
- C+D: r1 0; r2 0; r3 0. 2499 / 2144 / 2169 refusals, 2758 / 2401 / 2406 caps.
No run rebooted (bootcount continuous 19..30).
Cost of D: the storm's post-arm gate check (10 sequential requests with retries) failed in 3 of 6 D/CD runs: one lone request still got 503 (rest_busy on /api/v2/settings, or empty_body on /). Likely cause: tcp_active_pcbs also counts pcbs in FIN_WAIT/CLOSE_WAIT/LAST_ACK left by the storm's aborted clients, so the count stays above 4 for a while after the burst. Those pcbs still hold queued TX memory, so the refusal is defensible, but it is too strict for a single well-behaved client. Refinement to test: count only ESTABLISHED pcbs, or cap at a higher N, or exempt one slot.
Conclusion: a connection-count cap at the web layer (D) removes the deafness in 6/6 runs where base was deaf in 3/3; the per-pcb send-buffer cap (C) alone does not. Next step is a maintainer decision: port D (no library change; app-layer check in the existing REST/file gates) to dev under an ADR, refine the count, and re-measure on dev (which since alpha.415 also has the non-blocking MQTT write, ADR-186). Output: %LOCALAPPDATA%/OTGW-capture/task1162-diag/variants-CD/run_r{1,2,3}-{base,C,D,CD}/. Bench is back on alpha.416+9f64919.

2026-10-06: maintainer chose 'D to dev, with ADR': connection cap in the existing REST/file gates, refine the count to ESTABLISHED pcbs only, ADR (accept only on the maintainer's yes), re-measure on dev old vs fix, >= 3 runs each.

2026-10-06 dev implementation, alpha.417 (ADR-188, accepted by the maintainer): web connection cap. webConnCapExceeded() in restAPI.ino runs first in the REST and static-file gates and answers the cheap 503 while more than WEB_MAX_TX_CONNECTIONS (4) port-80 connections hold or can queue send data (ESTABLISHED, or unsent/unacked non-empty; platformTcpTxHoldingOnPort() in platform_esp32.h under LOCK_TCPIP_CORE), minus the /ws clients (webSocketClientCount()). New counter hd_webconn_503. Interleaved on the same alpha.417 tree (cap off = -DWEB_MAX_TX_CONNECTIONS=1000) plus alpha.416:
- no cap, 6 runs: deaf 0 / 0 / 270 / 292 / 21 / 253 s; storm verdict 3 PASS, 3 FAIL; short 200s >= 2 / 3 / >= 2 in the 3 cap-off runs.
- cap 4, 5 runs: deaf 0 / 7.2 / 0 / 0 / 0 s; storm verdict 5/5 PASS; short 200s 0 / 0 / 1 / 2 / 0; 1508-4695 cap refusals per run.
- nominal (3 browser-like clients with /ws, 180 s): hd_webconn_503 = 0 (9 other 503s came from the ADR-165 in-flight gates, 3 tabs loading at once).
- builds esp32, esp32-classic, esp32-combo SUCCESS; evaluate --quick 71/0.
AC#2 (root cause): checked. The memory is held in the lwIP send queues of already-admitted connections (up to TCP_SND_BUF 5744 B each, up to 16 active); the WiFi RX alloc (~2.3 KB) then fails, no ACKs arrive, and nothing drains. Shown by the T1162diag telemetry (2026-10-04/05) and by the variant series: only limiting the number of send-holding connections removes the deafness.
AC#3 NOT met: the cap reduces the mid-body stalls (short 200s) from about 2-3 per run to under 1 per run, not to zero. Still open: a response whose connection stalls mid-body under the storm. Data: %LOCALAPPDATA%/OTGW-capture/task1162-diag/dev-a416/, dev-a417/.

2026-10-06 AC#3: alpha.418 adds ADR-189 (accepted): webBeginRequest() replaces the request's ack-timeout callback with AsyncClient::abort(), so a response with no ACK for 5 s ends in a RST the client sees at once instead of a FIN queued behind an undrainable body. Counter hd_weback_abort. Bench, 5 storm runs (run_1162_diag + refresh_storm --workers 2,4,6,8 --seed 1124): deaf 0/5, mid-body stalls (timeout during the body, counted from the storm ndjson) 0, storm verdict 5/5 PASS, 2 aborts; the one incomplete exchange the client saw ended as reset/body after 5.9 s. Comparison: no cap 17 stalls in 6 runs; cap alone 2 in 5. Nominal (3 clients with /ws, 180 s) after a fresh boot: hd_webconn_503 0. Right after 5 storms the cap refused 3 of 219 nominal requests (heap fragmented, responses slower), the ADR-165 gates 10; the ADR-184 client queue retries both. Builds esp32/classic/combo SUCCESS (alpha.418; combo on the bench built before a comment-only ADR-188 -> ADR-189 edit), evaluate --quick 71/0. Data: %LOCALAPPDATA%/OTGW-capture/task1162-diag/dev-a418/.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Under a burst of web requests the OTGW32 went deaf for minutes and some static responses stopped mid-body. Root cause: the memory sat in the lwIP send queues of already-admitted connections, so the WiFi RX allocation failed, no ACKs came in, and nothing drained; the library's graceful close then queued its FIN behind that undrainable data. Fix on dev, no library change: ADR-188 refuses a new request with the cheap 503 while more than 4 port-80 connections hold or can queue send data (/ws clients excluded; counter hd_webconn_503); ADR-189 aborts a connection on the 5 s ack timeout instead of closing it, so the client sees a RST at once (counter hd_weback_abort). Evidence, OTGW32 combo, interleaved storm runs: without the cap deaf in 4 of 6 runs (up to 292 s) with 17 mid-body stalls; with cap and abort (alpha.418) deaf 0 of 5, 0 stalls, 5/5 storm PASS. Nominal load with 3 browser-like clients and /ws after boot: no cap refusals. Builds green for the three targets, evaluate 71/0. Related: the task-watchdog reset under the same storm was TASK-1208 (ADR-186).
<!-- SECTION:FINAL_SUMMARY:END -->
