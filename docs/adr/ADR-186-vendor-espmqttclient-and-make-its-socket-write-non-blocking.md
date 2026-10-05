---
id: "ADR-186"
title: "Vendor espMqttClient and make its socket write non-blocking"
status: "Accepted"
date: "2026-10-05"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "mqtt"
  - "espmqttclient"
  - "loop-stall"
  - "vendored-library"
aliases:
  - "non-blocking MQTT write"
  - "vendored espMqttClient"
  - "MQTT loop stall"
components:
  - "vendored espMqttClient (src/libraries/espMqttClient)"
  - "ClientSync non-blocking write"
symbols:
  - "ClientSync::write"
  - "MqttClient::_checkOutbox"
  - "MSG_DONTWAIT"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-186 Vendor espMqttClient and make its socket write non-blocking

## Status

Accepted, 2026-10-05.

## Status History

```yaml
status_history:
  - date: 2026-10-05
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-05
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: "Maintainer accepted 2026-10-05 after the bench old-vs-fix (TASK-1213): max loop gap 516-3115 ms before, 140-144 ms after under a 500 ms broker delay"
    changed_via: adr-kit lifecycle
```

## Context

ADR-131 runs the MQTT (Message Queuing Telemetry Transport) client espMqttClient 1.7.2 with `UseInternalTask::NO`: the client is pumped only by
`MQTTclient.loop()` on the Arduino loop task, which keeps every MQTT callback on that task.

In the TASK-1036 re-soak (2026-10-04, OTGW32, alpha.412) the loop task stalled for up to 6125 ms, and
the heap reached a since-boot minimum of 532 B. The broker ran in Docker/WSL (Windows Subsystem for Linux) on a laptop at 98%
memory. TASK-1213 traced the stall to the write path:

- `MqttClient::_checkOutbox()` writes the whole outbox in one `loop()` call, while
  `_sendPacket()` returns more than 0.
- `ClientSync::write()` is `NetworkClient::write()` (Arduino-ESP32). That is a blocking socket
  write: up to `WIFI_CLIENT_MAX_WRITE_RETRY` attempts, each with a `select()` timeout. When the
  broker is slow to take data and the TCP (Transmission Control Protocol) send buffer is full, the loop task waits.

The bench showed that broker latency drives both symptoms.
- With `tc netem delay 500ms` on the rig broker, a discovery republish gave 20 stalls over
  200 ms, the longest 516 ms, and the free heap fell from 29408 B to 2696 B.
- With a healthy broker the same step stalled 344 ms.

A drip-side byte budget for discovery alone (TASK-1213, first attempt) did not help.
- The largest stalls followed ordinary value bursts and REST (Representational State Transfer) traffic, not discovery.
- Every publisher that queues more than one TCP send buffer can block the loop.

## Decision

Vendor espMqttClient 1.7.2 into `src/libraries/espMqttClient` (MIT, Massachusetts Institute of Technology, licence), drop it from
`lib_deps`, and patch only `ClientSync::write()`. The patched method sends with
`send(fd, buf, len, MSG_DONTWAIT)`:
- it returns the bytes the socket accepted, or 0 when the send buffer is full (`EAGAIN` / `EWOULDBLOCK`);
- it never waits.

espMqttClient already handles a short write: `_sendPacket()` adds the written count to
`_bytesSent` and `_checkOutbox()` stops when a write returns 0. The rest of the packet goes out
on the next `loop()` call. The loop task therefore never waits on the broker, whichever
publisher queued the bytes. Threading (ADR-131), the callbacks and the publish path stay as they
are.

## Decision Contract

### Must

- `src/libraries/espMqttClient` holds the 1.7.2 sources unchanged except `ClientSync::write()`.
  The patch is marked with a comment naming this ADR and TASK-1213.
- The library leaves `lib_deps` in `platformio.ini`, so the vendored copy is the only one built.
- `ClientSync::write()` returns 0 on `EAGAIN` / `EWOULDBLOCK`, and never blocks.

### Must Not

- Must not change espMqttClient's threading model (`UseInternalTask::NO` stays, ADR-131).
- Must not patch other library files. If a later fix needs more, write a new ADR.

### Exceptions

- None.

### Verification

- Bench old vs fix on the OTGW32 with `tc netem delay 500ms` on the rig broker, during a
  discovery republish after a reboot (scratchpad `t1213_netem.py`, TASK-1213). Pass: max loop gap
  under 1000 ms, hd_min_free_heap above the old run, and retained config count unchanged (398).
- Build green for esp32, esp32-classic and esp32-combo; `evaluate.py --quick` clean.

## Alternatives Considered

- **Discovery-only byte budget in the drip (tried).** It does not cover other publishers. On the
  bench the largest stalls followed value bursts, not discovery.
- **`UseInternalTask::YES`, a dedicated MQTT task.** ADR-131 rejected it because callbacks
  would run off the loop task and break the single-threaded state access. It would need a new
  ADR, callback marshalling through a queue, and a mutex around publish. Too large for this
  defect.
- **Short socket send timeout with drop and reconnect (the 1.x approach).** It bounds the stall,
  but a slow broker then causes reconnects, repeated discovery and lost commands (compare 1.x GitHub
  issue #682).

## Consequences

**Positive:**

- A slow broker no longer stalls the loop task, for every MQTT publisher, with a one-function
  patch. Bench, 500 ms broker delay: max loop gap 516-3115 ms before, 140-144 ms after.
- No change to threading, callbacks or the publish chokepoint.

**Negative:**

- The library is now vendored, so upstream updates are manual and the patch must be carried
  forward. Mitigation: the patch is one function, marked with this ADR.
- A packet can now span several `loop()` calls. espMqttClient's keep-alive and timeout logic
  already treats a partial write as progress (`_lastClientActivity` updates on each write).

## Open Questions

- [x] Does the bench show the loop stalls gone, with the heap dip no worse, under the 500 ms netem delay (old vs fix)? — **Answered 2026-10-05 by User: Robert van den Breemen:** Yes, 2026-10-05, OTGW32, combo, 'tc netem delay 500ms' on the rig broker, reboot then a discovery republish (t1213_netem.py). OLD (alpha.412/414, three runs): max loop gap 516, 967 and 3115 ms, with 20, 23 and 3 stalls over 200 ms. FIX (alpha.415, vendored non-blocking write, two runs): max loop gap 144 and 140 ms, 0 stalls over 200 ms. Retained discovery configs: 398 in every run, and MQTT stayed connected. The since-boot minimum free heap was 15744 and 24048 B on FIX, against 2696 to 23036 B on OLD, so no worse.

## Related Decisions

- ADR-131 (MQTT engine on espMqttClient, single-threaded loop pumping), kept unchanged.

## References

- TASK-1213 notes: the investigation and the old-vs-fix runs.
- espMqttClient 1.7.2: `src/MqttClient.cpp` (`_checkOutbox`, `_sendPacket`), `src/Transport/ClientSync.cpp`.
- Arduino-ESP32 `libraries/Network/src/NetworkClient.cpp`, `NetworkClient::write` (lines 388-418).
