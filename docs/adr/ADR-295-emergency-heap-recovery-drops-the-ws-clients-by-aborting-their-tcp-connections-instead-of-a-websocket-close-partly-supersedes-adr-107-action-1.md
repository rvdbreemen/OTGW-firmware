---
id: "ADR-295"
title: "Emergency heap recovery drops the /ws clients by aborting their TCP connections instead of a WebSocket close (partly supersedes ADR-107 action 1)"
status: "Proposed"
date: "2026-11-22"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - emergency heap recovery
  - websocket
  - tcp abort
aliases:
  - heap recovery ws abort
components:
  - emergencyHeapRecovery
symbols:
  - emergencyHeapRecovery
  - doWebSocketDisconnectAll
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-295 Emergency heap recovery drops the /ws clients by aborting their TCP connections instead of a WebSocket close (partly supersedes ADR-107 action 1)

## Status

Proposed, 2026-11-22.

## Status History

```yaml
status_history:
  - date: 2026-11-22
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

ADR-107 action 1 drops all WebSocket clients when the heap is CRITICAL, so that each one "releases socket buffer". In the 2.0.0 code that action is `doWebSocketDisconnectAll()` = `otLogWs.closeAll()` (helperStuff.ino:1313-1317, webSocketStuff.ino:53-55). `closeAll()` is a WebSocket-level close: per client it sets `WS_DISCONNECTING` and queues a close frame (ESPAsyncWebServer AsyncWebSocket.cpp:500-548). The client only goes away when that frame is sent and acknowledged. At CRITICAL heap that may not happen, and until it does the client keeps its TCP connection and its lwIP send queue, so the action frees nothing at the moment it is needed.

Evidence. TASK-3017 AC3 capture on .61 (2026-11-21): the /ws client left `WS_CONNECTED` with its TCP open and received nothing for 174 min. The heap was 3132 B at 22:00:32; the recovery line of that moment was lost, so the link is consistent with the data but not proven. TASK-3017 now closes such a client after its 70 s AsyncTCP rx timeout. 09's 2 h capture on 045e1fd (2026-11-22) still shows the pattern: at 05:17:42 and 04:08:04 a `[heap-recovery]` line was followed by 73 s of /ws silence, until the rx timeout freed the client. The 02:38:27 recovery of the 4846f20 bench closed a healthy reader the same way.

Constraint. The recovery runs on the loop task (OTGW-firmware.ino:1110). The library's client list and its lock are private, and the client objects are deleted on the async_tcp task (`AsyncWebSocket::_handleDisconnect`, AsyncWebSocket.cpp:1008-1017). So a loop-task call that walks the clients or holds an `AsyncClient*` can race with that deletion.

## Decision

At CRITICAL heap, ADR-107 action 1 aborts every /ws TCP connection in lwIP, with a RST, instead of `otLogWs.closeAll()`.

How it is done:

- On `WS_EVT_CONNECT` (async_tcp) the /ws track table (webSocketStuff.ino, TASK-3017) also records the connection's remote IP and remote port.
- The recovery takes `LOCK_TCPIP_CORE()` and walks `tcp_active_pcbs`, the same pattern as `platformTcpActivePcbCount()` (platform_esp32.h:497-503). It calls `tcp_abort()` on each pcb whose local port is 80 and whose remote IP and port match a tracked /ws row.
- No `AsyncClient` or `AsyncWebSocketClient` pointer is touched. The pcb is only used while lwIP's own list holds it, under the core lock, so a pcb that is gone is not found and a freed pcb is never used.
- `tcp_abort()` runs AsyncTCP's error callback in that same locked context. That callback clears `client->_pcb` at once and posts an error event (AsyncTCP.cpp `tcp_error`). The async_tcp task then handles the event: the library erases the client, the same path as a peer RST.
- The memory of the send queue is free when `tcp_abort()` returns, and the browser's `onclose` reconnects.

Scope: only action 1 of ADR-107 changes. Actions 2 and 3, the 30 s rate limit, the debug line and the actions bitmask stay as ADR-107 decides them.

## Decision Contract

### Must

- The abort runs under `LOCK_TCPIP_CORE()` and only on pcbs found in `tcp_active_pcbs` with local port 80 and the remote IP and port of a tracked /ws connection.
- The recovery touches no `AsyncWebSocketClient` or `AsyncClient` object and does not walk the library's client list.
- The abort count goes to a counter in device/info, and the recovery line keeps its actions bitmask (bit 0x01 = /ws aborted).

### Must Not

- No WebSocket-level `closeAll()` in the recovery path.
- No library change.

### Exceptions

- None.

### Verification

- A host test that compiles the real recovery helper against a stub lwIP pcb list: a tracked /ws pcb is aborted, an HTTP pcb on port 80 and a pcb with another remote port are not; red on the code before (closeAll()).
- Bench (otgw-bench-tester): a forced CRITICAL or 09's reproduction shows the /ws client gone at once, its memory back in the recovery line's `after`, and a web UI tab reconnecting.

## Alternatives Considered

- **Keep `closeAll()` and rely on the TASK-3017 rx timeout.** Rejected: the stall still lasts about 70 s per recovery (09's capture: 73 s at 05:17:42 and 04:08:04), and the memory the action is meant to free stays held all that time.
- **Abort through the library (`otLogWs.client(id)` then `AsyncClient::abort()`).** Rejected: `client(id)` only returns clients in `WS_CONNECTED` (AsyncWebSocket.cpp:1043-1053), so a stuck client is missed. And the returned pointer can be deleted on async_tcp before the abort runs.
- **A build-time library patch adding a locked `abortAll()`** (the ADR-232 route). Rejected for now: it needs a patch to maintain, and the lwIP route needs none.
- **Abort every port-80 pcb.** Rejected: it would also cut REST and file transfers. ADR-107 targets the WebSocket clients only.

## Consequences

**Positive:**

- The recovery frees the /ws send queues at once, which is what action 1 is for, and a stuck /ws client is gone at the next recovery instead of after 70 s.

**Negative:**

- A browser sees a RST instead of a close frame. v2.js reconnects on `onclose` in both cases (5 s).
- If the error-event allocation in AsyncTCP's `tcp_error` fails at very low heap, the library never sees the disconnect. The client object then stays in the list with `_pcb` null until reboot. That is the same exposure as any RST from a peer today, and it is bounded by MAX_WEBSOCKET_CLIENTS + 1 rows.
- Matching relies on remote IP and port. A new connection from the same IP and port between the connect and the abort cannot exist while the tracked one is alive, because lwIP keeps one pcb per 4-tuple.

## Open Questions

- [ ] Robert: accept the change of ADR-107 action 1 from a WebSocket close to a TCP abort.

## Related Decisions

- **ADR-107**: action 1 is replaced by this decision; actions 2 and 3, the rate limit and the debug line stand.
- **ADR-189**: the same abort-instead-of-graceful-close reasoning for web connections; TASK-3017 applies it to /ws on the ack timeout.
- **ADR-167**: the heap ladder that decides CRITICAL.

## References

- TASK-3025 (parent TASK-3024), TASK-3017.
- src/OTGW-firmware/helperStuff.ino:1299-1338, webSocketStuff.ino (doWebSocketDisconnectAll, wsTrack), src/libraries/Platform/src/platform_esp32.h:455-503.
- ESPAsyncWebServer 3.11.2 AsyncWebSocket.cpp:500-548, :1008-1017, :1043-1053; AsyncTCP 3.4.10 AsyncTCP.cpp `tcp_error`.
- OTGW-field-data/bench-otgw32/2026-11-21_task3017-ac3-56dd422/, 2026-11-22_task3017-fix-4846f20/, 2026-11-22_task3017-ac3-045e1fd/.
