# ADR-058: Non-blocking PIC Command/Response for PR= Queries

**Status:** Accepted
**Date:** 2026-03-27

## Context

The PIC settings polling feature (v1.3.0) and gateway mode detection (ADR-037) used `executeCommand()` to send `PR=` commands synchronously: write command to serial, then busy-wait up to 2 seconds for the response. During the wait, only `feedWatchDog()` ran — `httpServer.handleClient()` never fired.

The PIC settings cycle queries 15 registers at 3-second intervals (~45 seconds total). Each query blocked the main loop for up to 2 seconds, starving the HTTP server and causing web GUI timeouts. User report: "web GUI enormously slow, half pages, timeouts" in v1.3.1, fixed by reverting to v1.2.0.

Similarly, `queryOTGWgatewaymode()` (PR=M) and `getpicfwversion()` (PR=A) used `executeCommand()`, blocking for up to 2 seconds each on every 60-second tick.

**Root cause:** `executeCommand()` is a synchronous send-and-wait function. Its blocking wait loops call `feedWatchDog()` but not `doBackgroundTasks()`, so the cooperative scheduler (HTTP server, MQTT, WebSocket) is starved.

## Decision

**Replace all `executeCommand("PR=...")` calls with non-blocking command queue submissions, and process responses asynchronously in `processOT()`.**

### Send path: fire-and-forget via command queue

All PR= queries now use `addOTWGcmdtoqueue(cmd, len, true)`:

- `queryNextPICsetting()` — queues `PR=O`, `PR=S`, ... `PR=V` (15 registers, one per 3s tick)
- `queryOTGWgatewaymode()` — queues `PR=M` (throttled to once per 60s)
- `getpicfwversion()` — queues `PR=A` (only when PIC identity is unknown)

The `forceQueue=true` flag bypasses the 2-letter prefix deduplication in `addOTWGcmdtoqueue()`. Without it, all PR= commands share the "PR" prefix and would overwrite each other. With `forceQueue=true`, each gets its own queue slot and benefits from the existing retry logic (5 retries at 5-second intervals).

### Receive path: centralized `handlePRresponse()` in processOT()

A new static function `handlePRresponse(buf, len)` is called from `processOT()` when a line starting with "PR:" is received. It dispatches by register letter:

- **Standard registers** (O, S, W, G, I, L, T, D, P, R, B, C, Q, N, V): parse `X=value`, update `state.picSettings.*`, publish MQTT, notify WebSocket.
- **Register 'M'** (gateway mode): parse `M=G` or `M=M`, update `state.otgw.bGatewayMode` with change detection, publish MQTT on change.
- **Banner** (PR=A response): detect "OpenTherm Gateway" substring, copy `OTGWSerial.firmwareVersion()` etc. into `state.pic.*`, publish version info via MQTT.

### Caller restructuring

`doTaskEvery60s()` callers no longer read state synchronously after queuing a command. All state updates and MQTT publishing happen inside `handlePRresponse()` when the response arrives asynchronously.

## Consequences

**Benefits:**
- Web GUI remains responsive during PIC settings discovery (the main fix)
- Main loop never blocks on serial I/O for PR= commands
- Automatic retry via command queue (previously: single attempt with 1s timeout)
- Centralized response handling reduces code duplication

**Trade-offs:**
- State updates are eventually consistent (response arrives ~100ms after query, not synchronously)
- `checkOTGWcmdqueue()` matches on "PR" prefix, which is imprecise when multiple PR entries exist via `forceQueue=true`. In practice this is correct because PIC responds in FIFO order (single-threaded). Data processing in `handlePRresponse()` is always correct regardless, since it reads the register letter from the response content.
- `executeCommand()` is retained but no longer called for PR= commands. It remains available for potential future use (e.g. debug console).

## Related

- **ADR-016** — Command queue with deduplication (the infrastructure this change leverages)
- **ADR-037** — Gateway mode detection via PR=M (decision unchanged; implementation now async)
- **ADR-028** — File streaming over loading (related HTTP performance concern)
- `OTGW-Core.ino` — `handlePRresponse()`, `queryNextPICsetting()`, `queryOTGWgatewaymode()`, `getpicfwversion()`, `processOT()`
- `OTGW-firmware.ino` — `doTaskEvery60s()` caller restructuring
