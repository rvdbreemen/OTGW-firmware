---
id: ADR-143
title: Telnet + ser2net transport on AsyncTCP (AsyncSimpleTelnet) to remove loop-task socket-write blocking
status: Accepted
date: 2026-06-17
tags: [telnet, ser2net, asynctcp, twdt, core-pinning, loop-stall, esp32s3, simpletelnet]
supersedes: []
superseded_by: []
related: [ADR-024, ADR-123, ADR-131, ADR-132, ADR-133, ADR-135, ADR-139]
deciders: [Robert van den Breemen]
---

# ADR-143: Telnet + ser2net transport on AsyncTCP (AsyncSimpleTelnet) to remove loop-task socket-write blocking

## Status

Accepted, 2026-06-17 (accepted by the maintainer Robert van den Breemen in session
after the green build on all three targets). Guideline-level structural ADR (per ADR-080): it records a
transport/architecture choice reviewed at PR, not a pattern enforced by a
dedicated `evaluate.py` gate. The verification evidence is the green build on all
three ESP32-S3 targets plus the field kill-test described under Consequences.

**Decision Maker:** Robert van den Breemen.

## Status History

status_history:
  - date: 2026-06-17
    status: Proposed
    changed_by: Claude (TASK-866/879)
    reason: Documents the migration of the debug telnet (port 23) and ser2net/OTmonitor bridge (port 25238) from the synchronous polled SimpleTelnet to the AsyncTCP-based AsyncSimpleTelnet, to eliminate the loop-task socket-write blocking identified as the root cause of the OTGW32 slow-webserver + Task-Watchdog-reboot symptom.
    changed_via: manual
  - date: 2026-06-17
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Accepted by the maintainer in session ("accepteer adr 143") after the async transport implementation built green on all three ESP32-S3 targets (esp32 98.7% flash, esp32-classic 96.0%, esp32-combo 94.8%) and evaluate.py --quick stayed clean. Field kill-test (curl with no telnet/ser2net client attached) remains the runtime confirmation, tracked under TASK-866/879.
    changed_via: manual

## Context

A multi-agent static root-cause hunt (TASK-866/879) traced the OTGW32 field
symptom — webserver responses 4-8 s late, root `/` exceeding a 10 s client
timeout, and recurring `Boot : Task watchdog` resets with a healthy heap — to a
single mechanism: **synchronous socket writes on the Arduino loop task.**

Every `Debug*`/`DebugT*` macro funnels through `debugTelnet` (port 23) and every
OpenTherm frame is mirrored to `OTGWstream` (ser2net/OTmonitor port 25238); both
were `SimpleTelnet<N>` instances whose `write()` bottoms out in the ESP32 Arduino
core `NetworkClient::write` (`NetworkClient.cpp:386-447`). That path runs a
`select()` loop with `WIFI_CLIENT_SELECT_TIMEOUT_US = 1 000 000` (1 s) ×
`WIFI_CLIENT_MAX_WRITE_RETRY = 10`, so a stalled or half-open client blocks the
**loop task** up to ~10 s per write, with partial progress resetting the retry
counter — and no `feedWatchDog()` inside (it is vendored core code).

Because AsyncTCP is pinned to core 1 (`CONFIG_ASYNC_TCP_RUNNING_CORE=1`, ADR-139)
where the Arduino loop task also runs, a blocking write starves AsyncTCP, which is
the multi-second HTTP latency floor; several such writes summing within one
inter-`feedWatchDog()` window cross the 30 s loop-task TWDT (ADR-135) and reboot
the device. Debug defaults are all-on and alpha auto-enables debug, so the stream
is dense, and the stall is observer-coupled: it exists only while a telnet or
ser2net client is connected, so a capture taken over telnet partly induces it.

The synchronous SimpleTelnet only coexists with the AsyncWebServer (ADR-132) as
long as the loop services it; under load that servicing is exactly the blocking
write. The fix must stop the loop task from ever blocking on a socket write.

## Decision

Move both telnet instances to the **AsyncTCP transport** provided by the upstream
SimpleTelnet 2.0.0 library (`AsyncSimpleTelnet`), the same stack as
`ESPAsyncWebServer` (ADR-132) and the espMqttClient async engine (ADR-131).

1. **Bump the vendored `src/libraries/SimpleTelnet` submodule** to upstream
   `main` (`55512dc`), which refactors the protocol logic into a shared
   `SimpleTelnetCore` base and adds the `AsyncSimpleTelnet<MAX_CLIENTS>` variant
   (`src/AsyncSimpleTelnet.h`, ESP32-only, depends on AsyncTCP).
2. **Switch the two instances** from `SimpleTelnet<N>` to `AsyncSimpleTelnet<N>`
   (`debugTelnet` `<1>` port 23; `OTGWstream` `<2>` port 25238). The public API,
   the `Stream`/`Print` interface, and the constructor are identical — `loop()`
   remains a no-op for source compatibility. `OTGWstream` keeps PULL-mode input
   (no `onInputReceived`), so its `available()`/`read()` command pump keeps
   working via the library's RX ring.
3. **Force `NEG_OFF` on `OTGWstream`** at `begin()`. Port 25238 is a raw serial
   bridge for OTmonitor; the new library default `NEG_REFUSE` parses and strips
   telnet IAC (`0xFF`), which would corrupt binary PIC bytes. `debugTelnet`
   (text console) keeps the `NEG_REFUSE` default, which is correct for an
   interactive telnet client.

Writes are now non-blocking with TX backpressure: bytes that do not fit the
AsyncClient send buffer go to a bounded TX ring drained on the `onAck` event, and
overflow is dropped rather than blocking. The loop task never blocks on a socket
write again.

## Alternatives Considered

- **A — `availableForWrite()` gate + drop in the synchronous library.** Gate
  `SimpleTelnet::write` and drop bytes that do not fit instead of entering the
  10× select loop. Hand-rolled, keeps the polled `WiFiServer` accept model, and
  re-implements in the old library exactly the bounded-drop behaviour the async
  variant already provides. Rejected as duplicate effort with no upstream home.
- **B — Move debug + ser2net emission to a dedicated FreeRTOS task with a ring
  buffer the loop enqueues into.** Lossless and robust, but a larger, bespoke
  change (new task, lifecycle, locking) that re-invents AsyncTCP's own event loop.
  Rejected as more surface for the same outcome.
- **C — `AsyncSimpleTelnet` (chosen).** The upstream, purpose-built non-blocking
  transport on the same AsyncTCP stack the firmware already runs; near drop-in;
  preserves the ser2net pull pump. Chosen.
- **Just add `feedWatchDog()` inside the write.** Rejected: masks the 30 s reboot
  but leaves the multi-second AsyncTCP starvation (the HTTP latency floor) intact.

## Consequences

Positive:

- The loop task never blocks on a telnet/ser2net write, removing both the HTTP
  latency floor and the Task-Watchdog reboot regime at their shared source.
- Telnet and ser2net now ride the same AsyncTCP event loop as the webserver and
  MQTT, consistent with the 2.0.0 async modernization (ADR-123).
- Near drop-in: the protocol behaviour, API, and `Stream` interface are unchanged
  beyond the explicit `NEG_OFF` on the raw bridge.

Negative / risks:

- **Upstream `AsyncSimpleTelnet` is a prototype** — host-compiled and unit-tested
  at the protocol-core level, but not yet hardware-validated. The 2.0.0 alpha
  line is the validation vehicle; the alpha.200 loop-stall watermark
  (`state.heapdiag.iMaxLoopGapMs`) and the kill-test below measure before/after.
- **TX-ring overflow is lossy** under sustained backpressure. Acceptable for the
  debug stream; the ser2net/OTmonitor bridge becomes best-effort under a slow
  client — but any non-blocking fix (A/B/C) must drop or bound, and blocking was
  the bug.
- Telnet + ser2net add AsyncTCP traffic on core 1 (non-blocking). The current
  AsyncTCP pulls a deprecated `AsyncClient::close(bool)` (warnings only).

Verification (in lieu of a CI gate):

- `python build.py` green on `esp32` (OTGW32), `esp32-classic`, and `esp32-combo`;
  `python evaluate.py --quick` shows no new failures.
- **Field kill-test:** power-cycle the OTGW32, connect nothing to port 23 or 25238,
  then `curl http://<ip>/`. Pre-fix the floor is present from a telnet/ser2net
  client; post-fix `maxLoopGap` should fall to near zero and HTTP latency should be
  sub-second regardless of attached clients.

## Related Decisions

- ADR-123 2.0.0 concurrency model / async modernization — this completes the
  telnet/ser2net side of that migration.
- ADR-135 TWDT primary watchdog — the 30 s loop-task watchdog this change stops
  tripping.
- ADR-139 AsyncTCP service task pinned to core 1 — the shared core whose
  starvation produced the symptom.
- ADR-131/132/133 espMqttClient / AsyncWebServer / AsyncWebSocket — the existing
  AsyncTCP consumers this aligns with.
- ADR-024 debug telnet command console — the console now served over AsyncTCP.

## References

- TASK-866 (diagnosis) / TASK-879 (fix).
- Root-cause hunt workflow `wf_79e88a89-6f8` (16 agents); primary-source evidence
  `NetworkClient.cpp:386-447`.
- Upstream `AsyncSimpleTelnet.h` + `SimpleTelnetCore.h` at SimpleTelnet `55512dc`;
  `docs/ASYNC.md` (prototype status).
- alpha.200 loop-stall detector (commit `bc84b9b7`).
