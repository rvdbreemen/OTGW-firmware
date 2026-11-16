---
id: TASK-1675
title: Rare /favicon.ico request gets no answer within 12 s under two-tab load
status: Done
assignee:
  - '@claude'
created_date: '2026-11-06T19:39:00Z'
updated_date: '2026-11-16T07:52:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 code-only research (workflow wf_e6a4797e-cc2, 3 angles + refute pass, no device): plausible: (a) SYN/GET dropped below TCP during the heap dip (MEMP_MEM_MALLOC=1, lwIP tcp_in drops a SYN silently when tcp_alloc fails, AsyncTCP returns ERR_ABRT on pcb==NULL; needs >= 7-9 s of drops to beat client SYN backoff, not shown); (b) GET ACKed then held in refused_data; (c) first tcp_output fails at the dip and nothing re-arms the connection; (d) ADR-216 cap == ESPAsyncWebServer copy buffer gives stop-and-wait once _sentLength > 5760 (does not explain the #697-era cases, before the cap); (e) ADR-189 ack-abort blind after 24 h uptime (AsyncTCP _rx_last_ack/_tx_last_packet wrap; only for uptime > 24 h). Refuted as cause: accept-path abort, TIME_WAIT pool exhaustion. Latent vendored-lib bug found: AsyncTCP accept returns ERR_OK after tcp_abort on AsyncClient OOM (AsyncTCP.cpp:1580-1585); library change needs Robert. Correction: hd_webfile_queued=0 does not prove the request never entered the queue (counter only counts requests still deferred after the first pump). Next step for every hypothesis: client-side pcap (pktmon/Wireshark) plus per-request logging (path, time to first byte, bytes, how it ended) under the two-tab load on .61, A/B WEB_TCP_SEND_CAP 5744 vs 2872.

2026-11-07 05:4x trace on .61 beta.116 (with the ADR-216 send cap), raw-socket client logging phase, local port, timings and end per request (scratchpad twotab_trace.py): 400 of 400 complete 200s, hd_weback_abort 0, no 503, bootcount steady; hd_min_free_heap since boot stayed 15372 B (no deep dip during the run). Across beta.116 that is 0 failures in 900 requests (500 in the TASK-1673 A/B + 400 here); the hangs were seen on beta.112/113 (no cap, about 1 %) and on the PR #697 builds. Consistent with the dip-linked hypotheses (SYN/GET dropped or first tcp_output failing at the heap minimum) now that ADR-216 makes the deep dip rare; root cause not observed (no pcap: no Wireshark/tshark and no admin rights for pktmon here). Uptime 7 h, so the >24 h ack-abort wrap hypothesis does not apply to these runs. Moved to In Review: re-open with the trace script and a pcap if the symptom returns in the field or in the TASK-1679 work. ACs not met: not reproduced on current dev.

2026-11-09 read-only review for the TASK-1740 AC2 gate (system32-78, coordinator request; no bench). State: seen only on beta.112/113 (no send cap, ~1 %) and PR #697 builds; 0 failures in 900 requests on beta.116 with the ADR-216 cap (WEB_TCP_SEND_CAP 2872, webServerCompat.h:155-167); root cause not observed. The notes' leading hypotheses (a) SYN/GET dropped and (c) first tcp_output failing both need a heap dip; (e) needs uptime > 24 h. None of the 900 clean requests ran in a deep dip (hd_min_free_heap 15372 B) or past 24 h uptime, so 'not reproduced' so far only covers the easy conditions.
Proposed reproduction (no new hardware, no admin rights; rides on bench time that is already planned):
R1 dip window: during the TASK-1528 step-b boots on .61 (that task measures the boot MQTT window where the heap low-water fell to ~1.7 KB), run the two-tab per-request trace (raw-socket client logging phase, local port, connect time, time to first byte, bytes, how it ended; the 11-07 twotab_trace.py method) from boot +5 s to +120 s on each of the 5 boots, next to the firmware's '[HEAP] boot win=... minWin=... outbox=...' line. A hang then shows its phase: connect timeout = SYN dropped (a); connected but no first byte = GET held or first tcp_output lost (b/c); stall mid-body = send path (d).
R2 long uptime: at the end of the RC 24 h soak (TASK-1740 AC3/AC6), 400 two-tab requests on .61 and on .63 (uptime > 24 h, hypothesis e).
R3 optional pcap: pktmon needs admin, so only if Robert runs it himself during R1.
Decision rule: a hang in R1 or R2 reopens the task with its phase (AC1), then the beta.109 comparison (AC2) on the same script. 0 hangs in R1+R2 (~800 more requests in the two risk conditions) supports the waiver below.
Side finding to keep (not this symptom): the vendored ESP32Async AsyncTCP accept path logs 'couldn't allocate client', calls tcp_abort(pcb) and then returns ERR_OK (.pio/libdeps/esp32-combo/AsyncTCP/src/AsyncTCP.cpp ~1579-1584, earlier noted as 1580-1585). lwIP expects ERR_ABRT after an abort in the accept callback, so on an AsyncClient OOM this is a use-after-free risk (a crash, not a hang). A library change needs Robert.
Draft waiver text for Robert (stable notes, Known issues): 'Rarely (about 1 in 100 requests on early 2.0.0 betas, not seen since beta.116) the web UI can wait about 12 s for a small file such as the page icon when two browser tabs load at the same time on a busy gateway. Reload the page if a tab stays blank. We could not reproduce it on the current build; if it happens to you, a capture log helps us find it.'

2026-11-09: the AsyncTCP accept side finding is filed as TASK-1746 (unconfirmed crash risk, needs-maintainer-ok).

R1, 2026-11-09 11:09-12:40, .61 beta.162 (with TASK-1528 step b, boots 2/4/6/8/10): the two-tab trace from the first HTTP answer after boot to +120 s, 1496 requests in total. Results: boot 2: 274 ok / 2 fail; boot 4: 303/1; boot 6: 308/0; boot 8: 296/0; boot 10: 312/0. All 3 failures are '/' (175047 B) with status 200 and a connection RESET mid-body (phase d, send path): 83288 B at 11:21:21.9 (boot +8 s), 22976 B at 11:21:32.6 (boot +19 s), 74672 B at 11:36:22.5 (boot +20 s). t_conn 13-31 ms, t_first 74-98 ms. 0 hangs, 0 connect timeouts, 0 favicon failures; worst t_first 0.63 s. Context: in these boots the native heap minimum was 2544 resp. 1368 B (TASK-1528 notes), so the resets coincide with near heap exhaustion in the boot window. Reading per the decision rule: no hang in R1 (the rule reopens on a hang); the mid-body resets are a different symptom (body abort under low heap) and are not covered by the waiver text. Robert/coordinator decide whether this reopens the task or goes into the TASK-1528 heap thread. R2 (end of the RC 24 h soak) still to do. Data local: scratchpad t1528step-b/run-20261109/boot*T-trace.log.

2026-11-16 01:12-01:24, 09 bench on .61 (OT-Direct), beta.331+e4dbf70 (= origin/dev 0a6ffd33e + TASK-1979/1867, no web-server change), uptime 0:17-0:29, bootcount 799: NOT reproduced. Two-tab load (2 threads, each round GET / with Accept-Encoding gzip (205366 B) then GET /favicon.ico (3627 B), fresh connection per request, Connection: close, 12 s timeout), telnet captured alongside. Run 1: 2 x 100 rounds = 400 requests, 0 failures; / median 1486 ms, p99 2108, max 2985; favicon median 74 ms, p99 144, max 174. Run 2: 2 x 250 rounds = 1000 requests, 0 failures; / median 1474 ms, p99 1992, max 3200; favicon median 81 ms, p99 144, max 295. hd_webfile_* counters before/after: queued 0, admitted 0, timeout 0, 503 0, webconn_503 0, weback_abort 0, inflight_hwm 1 -> 2. So 0 of 1400 vs the original 4 of 400 on beta.112. AC1 stays open: no hang to capture on current code; AC2 (beta.109 comparison) not run, it needs a reproduction first. Proposal for the owner: close as not reproducible on 2.0.0-beta.331 unless a field report shows it again. Evidence (local, 09 scratchpad t1675/): twotab.py, r1_results.json, r2/results.json, r1_telnet.raw, r2/telnet.raw, info0/1.txt.

2026-11-16 closed on Robert's decision (DM 1571794242546962504, option A): not reproducible since ADR-216; AC1-AC3 waived. Reopen recipe: run the two-tab load from 09 scratchpad t1675/twotab.py (2 threads; per round GET / with Accept-Encoding gzip, then GET /favicon.ico; fresh connection per request, Connection: close, 12 s timeout) with telnet web debug captured alongside, and read hd_webfile_* / hd_weback_abort from /api/v2/device/info before and after. Conditions not yet covered and worth targeting on a reopen: a deep heap dip (hd_min_free_heap well below 15 KB) and uptime > 24 h (the ack-abort wrap hypothesis). Add a pcap if admin rights for pktmon or a Wireshark host are available. Compare with beta.109 (AC2) only after a reproduction exists.
<!-- SECTION:NOTES:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-11-09T17:10:00Z

---

Robert, 2026-11-09: stays open. The boot-time heap drop (1.1-3.6 KB under web load right after boot) and the 3 mid-body resets on '/' from R1 move to TASK-1757 (high, 2.0.0 stable gate); R2 (trace at the end of the RC 24 h soak) stays here.

---
<!-- COMMENTS:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as not reproducible since ADR-216 (the WEB_TCP_SEND_CAP send cap). AC1-AC3 waived by Robert, DM 1571794242546962504 (option A, 2026-11-16). Evidence: seen only on beta.112/113 (no send cap, 4 of 400, about 1 %) and on the PR #697 builds; since the cap 0 failures in 2300 requests: beta.116 900 (TASK-1673 A/B 500 + trace 400), beta.331+e4dbf70 on .61 1400 (2026-11-16 01:12-01:24, favicon p99 144 ms, max 295 ms; hd_webfile timeout/503/weback_abort all 0). Not covered: a run during a deep heap dip and at uptime > 24 h; root cause never observed (no pcap). Reopen recipe in the notes (09 scratchpad t1675/twotab.py).
<!-- SECTION:FINAL_SUMMARY:END -->
