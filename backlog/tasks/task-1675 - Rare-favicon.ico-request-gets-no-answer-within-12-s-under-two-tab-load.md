---
id: TASK-1675
title: Rare /favicon.ico request gets no answer within 12 s under two-tab load
status: In Review
assignee:
  - '@claude'
created_date: '2026-11-06T19:39:00Z'
updated_date: '2026-11-07T04:41:00Z'
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
<!-- SECTION:NOTES:END -->
