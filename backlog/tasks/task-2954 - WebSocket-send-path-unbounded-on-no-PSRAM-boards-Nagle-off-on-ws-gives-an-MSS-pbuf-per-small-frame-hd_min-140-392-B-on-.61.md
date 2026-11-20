---
id: TASK-2954
title: >-
  WebSocket send path unbounded on no-PSRAM boards: Nagle off on /ws gives an
  MSS pbuf per small frame (hd_min 140-392 B on .61)
status: To Do
assignee: []
created_date: '2026-11-20T14:03:00Z'
updated_date: '2026-11-20T14:06:00Z'
labels:
  - firmware
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1089000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On .61 (OTGW32, no PSRAM, beta.403 loopback, SAT on) hd_min_free_heap fell to 140 B at 12:52:24 and 392/344 B in the poll10 run, while a /ws client from 192.168.88.32 (the laptop) was connected. Free heap fell 28.7 KB in 83 ms without any MQTT send, MQTT then refused ~25 packets (maxblk 3-7.7 KB), and +38.8 KB came back at the 'WebSocket[1] disconnected' (OTGW-field-data/bench-otgw32/2026-11-20_task2019-takeover-heap/run4-beta403/t2019-run4-61/telnet.raw 12:52:20-25). The /ws socket runs with Nagle off (ESPAsyncWebServer WebServer.cpp:112, AsyncTCP.cpp:1579), and under LWIP_NETIF_TX_SINGLE_PBUF (lwipopts.h:755) every small frame that is sent at once holds a ~1.5 KB MSS pbuf until ACKed, up to 16 per pcb. ADR-188/240 exclude /ws clients from the send cap, so nothing bounds WS send data. Preferred fix (architect memo): Nagle on the /ws pcb at WS_EVT_CONNECT (client->client()->setNoDelay(false), precedent networkStuff.ino:1285); fallback count-based backpressure (WS_MAX_QUEUED_MESSAGES build flag or skip textAll() when !availableForWriteAll()). TCP_OVERSIZE change rejected.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host or bench reproduction (slow /ws reader, hd_min) red on today's code, green on the fix
- [ ] #2 Log latency on /ws stays below 250 ms with the fix
- [ ] #3 Bench .61 discriminator with /ws: hd_min stays above 10 KB
- [ ] #4 ADR via otgw-architect once the measurement is in
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Architect memo (otgw-architect, 2026-11-20), pasted verbatim from its scratchpad ws-dip-memo.md:

# Decision memo: hd_min dips on .61 (no PSRAM), MQTT + WebSocket (otgw-architect, 2026-11-20)

Status: memo, not an ADR. ADR only after 09's discriminator (with/without /ws, beta.401).

## 1. The mechanism (verified in source, one caveat)

- ESP's lwipopts.h:755 sets `LWIP_NETIF_TX_SINGLE_PBUF 1` (arduino-libs esp32s3 = IDF 5.5.1; framework-espidf 5.5.1 port has the same line).
- tcp_out.c:238-244 (framework-espidf 5.5.1): under SINGLE_PBUF, `tcp_pbuf_prealloc()` sets `alloc = max_length` (= mss_local, about 1436 B) for EVERY new segment, whatever TCP_OVERSIZE, TF_NODELAY or TCP_WRITE_FLAG_MORE say. The Nagle/MORE heuristic (245-263) is compiled out.
- tcp_out.c phase 1 (`#if TCP_OVERSIZE`, ~496ff): a write that finds room in the LAST UNSENT segment is copied into that pbuf: no new allocation. That is the only coalescing left. Once a segment is sent (unacked), its room is gone.
- TCP_SND_QUEUELEN is not set by lwipopts, so opt.h:1400 applies: (4*5744+1435)/1436 = 16 pbufs per pcb.
- Result with Nagle off: every small write that is sent at once costs one ~1.5 KB pbuf until ACKed; ceiling about 16 x 1.5 KB = 24-25 KB per pcb.
- Caveat: liblwip.a is a prebuilt binary; the code is read from the matching IDF 5.5.1 source, not byte-verified against the .a.

## 2. Nagle is off on both sockets today

- MQTT: EMC_TCP_NODELAY (ADR-266 premise).
- /ws: ESPAsyncWebServer 3.11.2 WebServer.cpp:112 `_server.setNoDelay(true)`; AsyncTCP 3.4.10 AsyncTCP.cpp:1579 copies it to every accepted client, the /ws upgrade included.
- Precedent for turning it back on per socket: networkStuff.ino:1285 `debugTelnet.setNoDelay(false)`.

## 3. Arithmetic check: one /ws pcb does not explain all of it

- Observed: -28.7 KB in 83 ms, +38.8 KB at disconnect. Ceiling for one pcb: ~24-25 KB.
- Rest must come from elsewhere: AsyncWebSocket `_messageQueue` (WS_MAX_QUEUED_MESSAGES 32, AsyncWebSocket.h:17; `_closeWhenFull` default false, so it discards, not closes), client objects, or a second pcb from the same client. Open; ask 09 to log the /ws pcb snd_queuelen and the client queue length at the dip.
- Worst case structurally: MAX_WEBSOCKET_CLIENTS 3 (webSocketStuff.ino:60) x ~24 KB = ~72 KB pbuf on a heap that starts at ~68 KB. ADR-188 subtracts the /ws clients from its send cap (ADR-188 lines 106, 121; kept by ADR-240), so no gate bounds WS send data today. That is the gap.

## 4. Remedies weighed

(b) CONFIG_LWIP_TCP_OVERSIZE_QUARTER_MSS / DISABLE via a custom sdkconfig: REJECT.
- QUARTER_MSS: no effect; SINGLE_PBUF ignores TCP_OVERSIZE for the allocation size.
- DISABLE: does not build. init.c:232-233: `#error "LWIP_NETIF_TX_SINGLE_PBUF needs TCP_OVERSIZE enabled"`. Had it built, it would remove phase 1, the coalescing ADR-266's Nagle variant relies on.
- The knobs that would change pbuf size (LWIP_NETIF_TX_SINGLE_PBUF 0, a smaller TCP_MSS) live in the prebuilt liblwip.a (SINGLE_PBUF is hard-coded in lwipopts.h, not sdkconfig): a lib rebuild/hybrid compile, untested WiFi driver interaction, and slower HTTP serving. Not for 2.0.0.
- So (b) is NOT a joint MQTT+WS fix and does not change ADR-266. ADR-266 gets one context line later: CONFIG_LWIP_TCP_OVERSIZE_MSS must stay.

Preferred, the joint fix: Nagle on the /ws pcb, the same lever as ADR-266 on MQTT.
- One app-side line at WS_EVT_CONNECT: `client->client()->setNoDelay(false)` (AsyncWebSocket.h:261 accessor). No library change.
- With Nagle, small frames fill the oversize room of the one unsent segment (phase 1), so pbufs carry full payload; the queue is bounded by snd_buf (5744 B, about 4 pbufs, ~6 KB) instead of 16 small pbufs (~24 KB).
- Cost: 40-200 ms extra latency with the reader's delayed ACK; harmless for a live log. REST/static stay NODELAY.
- Test: add the /ws variant to the TASK-2949.5 A/B rig (same probe, same events, plus a slow /ws reader).

(a) Stricter WS heap gate on no-PSRAM boards: NOT as a heap tier.
- A heap-tier gate re-introduces what ADR-167 removed: needs a superseding ADR.
- It also gates at the wrong moment: textAll() only enqueues; the pbufs are allocated later in _runQueue() on the async_tcp task.
- If a bound is still needed after Nagle: count-based backpressure, not heap tiers. Options: skip textAll() when !otLogWs.availableForWriteAll(), or a lower WS_MAX_QUEUED_MESSAGES as a build flag (#ifndef-guarded, compiled from libdeps source, no library edit). ADR-167 untouched.

(c) App-side coalescing of log lines: only if Nagle fails the A/B. TCP does it for free; app batching adds a buffer, a timer and an ordering rule.

ADR-165 (N*=2 parallelism): untouched; this concerns the long-lived /ws stream, not request concurrency.

## 5. Component 1: the ~60 KB boot/connect/discovery dip without any WS client (beta.401)

- Belongs to ADR-266 / TASK-2949.5 (boot discovery is already one of its three events), plus ADR-265 pacer for the outbox.
- 60 KB is more than one MQTT pcb can hold (~24-25 KB), so other contributors are open: espMqttClient outbox copies, web UI serving at boot, discovery buffers. Not solved here; the 2949.5 probe should record the split.

## 6. Recommendation

1. Wait for 09's discriminator. If /ws confirmed: a small task "Nagle on the /ws socket" bench-tested in the 2949.5 rig on .61 and .63; ADR (new, or folded into ADR-266 as "Nagle on the long-lived publish sockets") via adr-kit after the numbers.
2. Do not pursue (b).
3. Count-based WS backpressure only as a fallback, with the WS_MAX_QUEUED_MESSAGES flag the cheapest form.
<!-- SECTION:NOTES:END -->
