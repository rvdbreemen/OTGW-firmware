---
id: "ADR-267"
title: "Turn Nagle on for the WebSocket socket at connect so a slow reader cannot pin a full-segment buffer per small frame"
status: "Proposed"
date: "2026-11-20"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "websocket"
  - "heap"
  - "lwip"
  - "nagle"
  - "no-psram"
aliases:
  - "Nagle on /ws"
  - "WebSocket setNoDelay(false)"
  - "WS MSS pbuf per frame"
  - "TCP_NODELAY on the live log socket"
components:
  - "WS_EVT_CONNECT handler in webSocketStuff.ino (per-client TCP option)"
symbols:
  - "onWebSocketEvent"
  - "WS_EVT_CONNECT"
  - "AsyncClient::setNoDelay"
  - "AsyncWebSocketClient::client"
  - "LWIP_NETIF_TX_SINGLE_PBUF"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-267 Turn Nagle on for the WebSocket socket at connect so a slow reader cannot pin a full-segment buffer per small frame

## Status

Proposed, 2026-11-20.

## Status History

```yaml
status_history:
  - date: 2026-11-20
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Initial proposal (TASK-2954); evidence section waits for the .61 bench numbers
    changed_via: adr-kit
```

## Context

The web UI's live log streams every OpenTherm frame as a small WebSocket text frame on `/ws`. A frame is typically under 150 B. The stream runs on port 80, through ESPAsyncWebServer 3.11.2 on AsyncTCP 3.4.10.

**Why each small frame costs a full TCP segment buffer.**

- ESP-IDF's lwIP port sets `LWIP_NETIF_TX_SINGLE_PBUF 1` (`lwipopts.h:755` in the arduino-libs esp32s3 package, IDF 5.5.1). With that option, `tcp_pbuf_prealloc()` allocates a buffer of a full segment (MSS, about 1436 B) for every new TCP segment, however small the write is (`tcp_out.c:238-244`, framework-espidf 5.5.1).
  - TCP_NODELAY (Nagle off) means each write is sent at once. A sent segment keeps its full-size buffer until the peer acknowledges it.
  - The only coalescing left is "phase 1" of `tcp_write()` (`tcp_out.c`, `#if TCP_OVERSIZE`). A new write is copied into the spare room of the last *unsent* segment, and that happens only while a segment is still waiting to be sent, which is what Nagle does.
- Caveat: `liblwip.a` is prebuilt. This is read from the matching IDF 5.5.1 source, not byte-verified against the archive.
- **Nagle is off on `/ws` today.** ESPAsyncWebServer's `WebServer.cpp:112` calls `_server.setNoDelay(true)`, and AsyncTCP's `AsyncTCP.cpp:1579` copies that to every accepted client, the `/ws` upgrade included.

**What one slow reader costs.**

- A pcb (lwIP's per-connection control block) can queue at most `TCP_SND_QUEUELEN` = 16 buffers: lwipopts does not set it, so the `opt.h:1400` default (4 × 5744 + 1435) / 1436 applies.
- At about 1.5 KB per buffer, one `/ws` client whose ACKs come slowly can hold about 24-25 KB.
- `MAX_WEBSOCKET_CLIENTS` is 3 (`webSocketStuff.ino:60`), so the structural worst case is about 72 KB. A board without PSRAM starts with about 68 KB of free heap.
- No other limit applies to that WebSocket send data:
  - ADR-188's send cap subtracts the `/ws` clients (ADR-188 "Decision", kept by ADR-240);
  - ADR-167 left only a CRITICAL-tier block in `canSendWebSocket()`.

**Observed on the bench** (TASK-2954): the OTGW32 bench unit without PSRAM (then noted as .61; it is the same unit as marvin-otgw32,
MAC 10:20:BA:21:B4:F8, now at 192.168.88.76; otgw-bench-tester 2026-11-27: find it by MAC or hostname), running beta.403 with SAT on, with a `/ws` client connected.

- Free heap fell 28.7 KB in 83 ms with no MQTT send in flight, and `hd_min_free_heap` reached 140 B.
- MQTT then refused about 25 packets.
- When the `/ws` client disconnected, 38.8 KB came back at once (field-data `bench-otgw32/2026-11-20_task2019-takeover-heap/run4-beta403`, `telnet.raw` 12:52:20-25).
- A beta.401 run with `/ws` reached 116 B after 12 minutes (otgw-bench-tester, relayed by the coordinator).
- Without any WebSocket client, beta.401 bottomed out at 5048 B. That dip belongs to the boot and MQTT path (ADR-266), not to this decision.

**Precedent.** TASK-1333 solved the same mechanism on the debug telnet console. A debug burst held about 40 KB in send buffers, with minimum free heap at 1064 B. The fix was one line, `debugTelnet.setNoDelay(false)` (`networkStuff.ino:1280-1285`). Port 25238 keeps NoDelay because the PIC's request and response timing needs it.

## Decision

At `WS_EVT_CONNECT`, the firmware turns Nagle on for that WebSocket client's TCP connection with `client->client()->setNoDelay(false)`. The rest of port 80 keeps the server default (NoDelay on).

- **The bound.** With Nagle on, small frames are copied into the one unsent segment until it is full or the previous segment is acknowledged. Buffers then carry full payload, and the per-client queue is bounded by `TCP_SND_BUF` (5744 B, about 4 buffers, about 6 KB) instead of 16 small buffers (about 24 KB).
- **The cost.** The reader's delayed ACK can add latency, typically 40-200 ms. That is acceptable for a live log view; TASK-2954 AC2 holds it below 250 ms.
- **Same lever as ADR-266.** ADR-266 (Accepted 2026-11-25) turns Nagle on for the MQTT socket; it shipped in beta.451 (0f229b343, `-DEMC_TCP_NODELAY=0`). This ADR applies it per connection, on the one long-lived port-80 stream whose frames are small.

## Decision Contract

### Must

- The `WS_EVT_CONNECT` branch in `webSocketStuff.ino` calls `setNoDelay(false)` on the accepted client's `AsyncClient`. It does so after the client-limit and heap checks have accepted the connection.
- The call is made in our firmware, not in a patched library.

### Must Not

- Must not change the server-wide NoDelay default for port 80. REST and static files keep NoDelay.
- Must not change lwIP options (`TCP_OVERSIZE`, `LWIP_NETIF_TX_SINGLE_PBUF`, `TCP_MSS`) through a custom sdkconfig for this purpose (see the alternatives).
- Must not reintroduce a heap-tier gate for WebSocket sends: ADR-167 retired that model.

### Exceptions

- None.

### Verification

- `tests/test_ws_nagle_on_connect.py` (to be written under TASK-2954). It compiles or scans the real `WS_EVT_CONNECT` branch, passes when `setNoDelay(false)` is called on the accepted client, and fails on the code from before this ADR.
- Bench, TASK-2954 AC3: the no-PSRAM OTGW32 bench unit (192.168.88.76) with a `/ws` client keeps `hd_min_free_heap` above 10 KB. AC2: log latency on `/ws` stays below 250 ms.
- The bench runs on a build that already has ADR-266 (beta.451 or later) and ADR-295, so the MQTT segment term and the emergency
  recovery are the same in both arms. Arms: that build without this change, and with it (the parked commit 7e700ee1d on
  park/task-2954-ws-nagle, rebased); n >= 3 each, with a slow `/ws` reader, the same load and the boot of the build under test as the start.

## Alternatives Considered

- **(b) Smaller `TCP_OVERSIZE` through a custom sdkconfig.** Rejected, because it cannot work:
  - `CONFIG_LWIP_TCP_OVERSIZE_QUARTER_MSS` has no effect, because `LWIP_NETIF_TX_SINGLE_PBUF` ignores `TCP_OVERSIZE` when it sizes the allocation.
  - `CONFIG_LWIP_TCP_OVERSIZE_DISABLE` does not build: `init.c:232-233` has `#error "LWIP_NETIF_TX_SINGLE_PBUF needs TCP_OVERSIZE enabled"`.
  - The options that would change the buffer size (`LWIP_NETIF_TX_SINGLE_PBUF 0`, a smaller `TCP_MSS`) live in the prebuilt `liblwip.a`. They would need a library rebuild, would interact with the WiFi driver in untested ways, and would slow HTTP serving.
  - So this is not a joint fix for MQTT and WebSocket, and ADR-266 keeps `CONFIG_LWIP_TCP_OVERSIZE_MSS`.
- **(a) A stricter WebSocket heap gate on boards without PSRAM.** Rejected as primary:
  - A heap-tier gate brings back what ADR-167 removed and would need a superseding ADR.
  - It gates at the wrong moment: `textAll()` only enqueues, and the buffers are allocated later in `_runQueue()` on the async_tcp task.
  - Kept as a fallback in count-based form, outside ADR-167's scope: skip `textAll()` while `availableForWriteAll()` is false, or set a lower `WS_MAX_QUEUED_MESSAGES` as a build flag. That define is `#ifndef`-guarded and compiled from libdeps source, so no library edit is needed.
- **(c) Coalescing log lines on the WebSocket side.** Rejected as primary: TCP already does this when Nagle is on. Doing it in the application adds a buffer, a timer and an ordering rule for the same effect. It is the fallback only if Nagle fails the bench.
- **Do nothing.** Rejected: one slow reader can take the heap to near zero on a board without PSRAM (140 B observed), and MQTT and SAT publishing then fail.

## Consequences

**Positive:**

- Expected: per-client WebSocket send memory falls from up to about 24 KB to about 6 KB, and the worst case for 3 clients from about 72 KB to about 18 KB. To be confirmed by the bench numbers (Open Questions).
- A one-line change in our own code with a measured precedent (TASK-1333), and no library change.

**Negative:**

- Live log frames can arrive up to about 200 ms later, through the reader's delayed ACK. Mitigation: AC2 measures it; a log view does not need sub-100 ms delivery.
- ADR-188/240 still do not bound WebSocket send data; this ADR shrinks the cost per client but does not add a cap. Mitigation: the count-based fallback from alternative (a), if the bench shows the 6 KB bound is not enough.

## Open Questions

- [ ] Bench (no-PSRAM unit): with Nagle on for `/ws` and a slow reader connected, the minimum `hd_min_free_heap`, against the beta.401 baseline of 116 B after 12 min (TASK-2954 AC3: above 10 KB). **Measured 2026-11-20 (otgw-bench-tester, relayed by the coordinator): 324 B after 14 min. Insufficient: the AC fails.** The WebSocket segment mechanism exists, but it is not the dominant term of the dip. beta.401 without any `/ws` client already reached 828 B. This ADR is on hold, with no acceptance proposal, until the dominant consumer is known.
  **Context change since (otgw-architect, 2026-11-27):** that run had TCP_NODELAY on the MQTT socket. On the same unit, MQTT with
  TCP_NODELAY took hd_min to 728-2736 B and with Nagle on to 13-22 KB (TASK-2949.5 notes, n=3 per arm, 2026-11-21). So the 324 B result
  does not separate the two terms, and with ADR-266 in the build the `/ws` term may now be the larger one. To be measured again.
- [ ] Bench (no-PSRAM unit): `/ws` log latency p95 with Nagle on (TASK-2954 AC2: below 250 ms).
- [ ] Is the rest of the -28.7 KB/+38.8 KB above one pcb's ceiling of about 25 KB the AsyncWebSocket `_messageQueue` (32 messages, `AsyncWebSocket.h:17`), client objects, or a second pcb? This decides whether the count-based fallback is needed as well.
- [ ] Robert: acceptance. Robert unparked this ADR (question 27, answer 'A', DM 1575746774247673857, relayed by the coordinator; not seen
  by otgw-architect). Reading (the coordinator's, as the question put it; not Robert's words): the ADR goes to Robert for acceptance only
  after otgw-bench-tester has reproduced TASK-2954 on the no-PSRAM unit and the bench above has its numbers.

## Related Decisions

- **ADR-266 (Bound the lwIP segments one espMqttClient loop creates, with Nagle on the MQTT socket)**: the same lever on the MQTT socket. Both rely on lwIP's phase-1 coalescing and on `CONFIG_LWIP_TCP_OVERSIZE_MSS` staying set.
- **ADR-188 (Cap the web connections that hold send data)** and **ADR-240**: they exclude `/ws` clients from the send cap. This ADR reduces the per-client cost they leave unbounded.
- **ADR-167 (Heap tier gating removed)**: the reason the fallback is count-based, not heap-tiered.
- **ADR-300 (REST admission up to 4 in flight; supersedes ADR-165)**: not affected; this concerns the long-lived `/ws` stream, not request concurrency.
- **ADR-295 (emergency heap recovery aborts the /ws TCP connections)**: the recovery path when a `/ws` client already holds too much; this ADR lowers how much one client can hold.

## References

- TASK-2954 (implementation and bench); TASK-1333 (telnet precedent, `src/OTGW-firmware/networkStuff.ino:1280-1285`).
- `src/OTGW-firmware/webSocketStuff.ino:60` (MAX_WEBSOCKET_CLIENTS), `:264` (WS_EVT_CONNECT), origin/dev 834bfd7b8.
- Parked implementation: commit 7e700ee1d on park/task-2954-ws-nagle (`tests/test_ws_nagle_on_connect.py`, 4/4 on the change, FAIL on the copy before it, per its commit message; not re-run by otgw-architect).
- ESPAsyncWebServer 3.11.2 `src/WebServer.cpp:112`, `src/AsyncWebSocket.h:17` and `:261`; AsyncTCP 3.4.10 `src/AsyncTCP.cpp:1169-1176` and `:1579`.
- framework-espidf 5.5.1 `components/lwip/lwip/src/core/tcp_out.c:228-275` and `init.c:232-233`; arduino-libs esp32s3 `include/lwip/port/include/lwipopts.h:639-649` and `:755`; `lwip/opt.h:1400`.
- Architect memo, pasted in the TASK-2954 implementation notes (2026-11-20).

## Enforcement

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [
    {
      "pattern": "setNoDelay\\(false\\)",
      "path_glob": "src/OTGW-firmware/webSocketStuff.ino",
      "message": "ADR-267: the WS_EVT_CONNECT branch turns Nagle on for the /ws client."
    }
  ],
  "llm_judge": false,
  "llm_judge_reason": "The rule is one mechanical call; the require_pattern and tests/test_ws_nagle_on_connect.py cover it."
}
```
