# ADR-058: Non-blocking PIC Command/Response for PR= Queries

## Status

Accepted, 2026-03-27.

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

## Alternatives Considered

### Alternative A: Keep `executeCommand()` and yield/feed background tasks inside its busy-wait

Leave the synchronous `executeCommand("PR=...")` callers untouched but extend the internal wait loop to call `doBackgroundTasks()` (or at least `httpServer.handleClient()`) in addition to `feedWatchDog()`, so the cooperative scheduler runs while waiting for the PIC response.

**Rejected** because `doBackgroundTasks()` is the entry point that itself invokes `queryNextPICsetting()`, `queryOTGWgatewaymode()`, and the rest of the periodic work. Re-entering it from inside a synchronous PIC wait would create unbounded recursion and shared-buffer aliasing (see the "Static buffers, cooperative scheduling" rule in CLAUDE.md, where re-entry via `feedWatchDog()` is already documented as fragile). Selectively pumping just the HTTP server would split the scheduler in two and force every other subsystem (MQTT, WebSocket, telnet) to be added one by one — the complexity collapses back to "make the whole loop reentrant", which the firmware was explicitly designed to avoid.

### Alternative B: Lengthen the PIC settings polling interval to mask the blocking

Reduce the impact by spreading the 15 PR= queries across a much longer interval (e.g. one every 30 seconds instead of every 3 seconds), so the cumulative web-GUI starvation per minute drops below the user-visible threshold.

**Rejected** because it does not fix the root cause: every individual `executeCommand("PR=...")` still blocks the loop for up to 2 seconds, which is long enough to cause "half pages" and timeouts on a single page load that happens to land in that window (the original v1.3.1 bug report). It also stretches the full PIC-settings discovery cycle from ~45 seconds to ~7.5 minutes, delaying MQTT discovery and Home Assistant entity availability after every reboot. Hiding a blocking call behind reduced frequency is a workaround, not a fix.

### Alternative C (chosen): Fire-and-forget queue submission with centralized async response handling

Queue every PR= via `addOTWGcmdtoqueue(cmd, len, /*forceQueue=*/true)` and dispatch responses through a single `handlePRresponse(buf, len)` in `processOT()`, which updates state and publishes MQTT when the response actually arrives.

**Trade-off accepted**: state updates become eventually consistent (response lands ~100ms after the query, not synchronously), and `checkOTGWcmdqueue()` matches PR entries imprecisely on the 2-letter prefix. Both are tolerable: the PIC processes commands in FIFO order on a single-threaded serial link, and `handlePRresponse()` keys off the register letter in the response payload itself, so data routing is correct regardless of which queue slot held the request. The web GUI stays responsive during the full 45-second discovery cycle, and queue retries (5 attempts at 5s intervals) replace the previous single-attempt 1s timeout.

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

## Related Decisions

- **ADR-016** — Command queue with deduplication (the infrastructure this change leverages)
- **ADR-037** — Gateway mode detection via PR=M (decision unchanged; implementation now async)
- **ADR-028** — File streaming over loading (related HTTP performance concern)

## References

- `OTGW-Core.ino` — `handlePRresponse()`, `queryNextPICsetting()`, `queryOTGWgatewaymode()`, `getpicfwversion()`, `processOT()`
- `OTGW-firmware.ino` — `doTaskEvery60s()` caller restructuring

## Enforcement

```json
{
  "llm_judge": true
}
```
