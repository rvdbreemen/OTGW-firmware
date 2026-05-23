# ADR-107: Emergency Heap Recovery Actions (2.0.0 platform variant)

## Status

Accepted, 2026-05-23.

## Context

`emergencyHeapRecovery()` is invoked from `doBackgroundTasks()` whenever `getHeapHealth() == HEAP_CRITICAL` (heap below `HEAP_CRITICAL_THRESHOLD = 1536` bytes; see `src/OTGW-firmware/helperStuff.ino:853`). Today — after the TASK-672 port of dev's TASK-671 — the function body is:

1. A 30-second rate-limit gate (`EMERGENCY_RECOVERY_INTERVAL_MS`).
2. A single `yield()` call documented as a "scheduler slot for SDK free-pool housekeeping" (`helperStuff.ino:1083-1085`).
3. Two `DebugTf` log lines bracketing the yield to report the (essentially unchanged) heap delta via `platformFreeHeap()`.

The yield is redundant: `feedWatchDog()` at the top of `doBackgroundTasks()` already yielded one line earlier, and the SDK runs free-pool housekeeping on every yield. The function does not actually free any RAM; the name `emergencyHeapRecovery` misleads field operators reading telnet logs.

A second mainloop review on the dev branch (after TASK-671) flagged this as an item with three resolutions: delete, stub, or real recovery. The maintainer chose real recovery. The dev decision lives in ADR-079 on the `dev` branch. This ADR is the 2.0.0 sibling: same architectural decision, adapted to the 2.0.0 platform abstraction (`platformFreeHeap()`, `platformMaxFreeBlock()`, ESP32 reality).

### Forces (2.0.0 specifics)

- **ESP32 has ~320 KB usable heap** — roughly an order of magnitude more than the ESP8266 baseline that ADR-079 reasons against. HEAP_CRITICAL is therefore a rarer event on 2.0.0, but the failure mode (positive-feedback loop of drip + WS broadcasts under pressure) is identical.
- **OTGW32 hardware variants** (Ethernet via W5500, BLE sensors, SAT control loop, OLED display) introduce additional heap consumers compared to dev. A working recovery helper has more cumulative value on 2.0.0 because the consumer set is larger.
- **The platform abstraction** (`platformFreeHeap()` / `platformMaxFreeBlock()` in `helperStuff.ino`) is the documented heap API on 2.0.0. Direct `ESP.getFreeHeap()` is forbidden in the recovery body so the same source compiles cleanly on both ESP8266 and ESP32 builds of the 2.0.0 branch.
- **OTGWstream restart pattern is already used** in 2.0.0's `doRestart()` path (`helperStuff.ino:526`: `OTGWstream.stop()`). Idempotency check (no heap re-allocation on `startOTGWstream()`) applies symmetrically to 2.0.0.
- **HA-side WebSocket reconnect** logic is the same browser-side `graph.js` loop on both branches.
- **MQTT broker reconnect cost** on 2.0.0 has the same `setSocketTimeout(15)` blocking property as dev — the cost calculus rejecting "force MQTT reconnect" applies equally.
- **Discovery-verify state machine** (ADR-062 dev equivalent) has the same internal heap-abort path in 2.0.0; this ADR does not duplicate it.

## Decision

When `getHeapHealth() == HEAP_CRITICAL`, `emergencyHeapRecovery()` performs the following three actions in order, gated by the existing 30-second rate-limit (`EMERGENCY_RECOVERY_INTERVAL_MS`):

1. **Drop all WebSocket clients** via `webSocket.disconnect()` (close-all variant). Each connected client releases socket buffer (~2-4 KB on ESP8266 build, larger on ESP32 due to lwIP defaults). Browsers reconnect via the existing `graph.js` auto-reconnect loop; no user action required.
2. **Drop all OTGWstream clients** via `OTGWstream.stop()` followed by `startOTGWstream()` to restart the listener on port 25238. Implementation must verify that `startOTGWstream()` is idempotent and does not re-allocate heap on restart — if it does, the action is removed from the helper. Same verification gate as ADR-079 on dev.
3. **Clear the MQTT discovery pending bitmap** via `clearMQTTConfigPending()`. Stops the drip from queuing additional discovery publishes during the critical window. The next status burst re-arms discovery via the JIT path (dev ADR-073, equivalent in 2.0.0).

A single `DebugTf` line at the end reports `before`/`after` heap (via `platformFreeHeap()`), `delta`, and an `actions` bitmask so field operators on both ESP8266 and ESP32 builds see the same diagnostic format.

**Explicitly NOT done** (identical to ADR-079 on dev):

- Telnet clients are not dropped (operator visibility during incidents).
- MQTT is not disconnected/reconnected (reconnect cost > heap recovered).
- No explicit `tickDiscoveryVerification()` abort (already handled internally).

The `yield()` call is removed (redundant).

**Differences from ADR-079 (dev) — none, except API surface:**

- Uses `platformFreeHeap()` instead of `ESP.getFreeHeap()`.
- All `#if HAS_*` board-feature gates that wrap WebSocket / OTGWstream / MQTT discovery in the 2.0.0 worktree are honoured by the existing helper APIs (`hasWebSocketClients()`, `clearMQTTConfigPending()` etc. already check their own preconditions). No new conditional compilation in the recovery body.
- No SAT / BLE / OLED / Ethernet-specific actions in this revision — adding board-specific recovery would couple the helper to HAL internals. If future 2.0.0 incidents show OLED buffer or BLE scan buffer dominating heap under pressure, a follow-up ADR adds those actions.

## Alternatives Considered

### Alternative 1 — Delete the helper

Same as ADR-079 Alternative 1. Rejected for the same reason: maintainer chose real recovery; OOM crashes lose the diagnostic window.

### Alternative 2 — Aggressive recovery (drop telnet + force MQTT reconnect)

Same as ADR-079 Alternative 2. Rejected for the same reasons: telnet drop blinds operator; MQTT reconnect cost exceeds heap recovered. On ESP32 the absolute heap cost of MQTT reconnect is a smaller fraction of available RAM but the latency cost (15s socket timeout) is identical and worse for user-facing responsiveness.

### Alternative 3 — Stub the helper

Same as ADR-079 Alternative 3. Rejected for the same reason: the name `emergencyHeapRecovery` must do something.

### Alternative 4 — Diverge from dev: add board-specific recovery actions on 2.0.0 only

Add OLED display buffer release, BLE scan abort, or Ethernet socket teardown on the OTGW32 / ESP32 variants where those subsystems hold significant heap.

**Rejected for v1 of this ADR.** Two reasons. First, no field incident has demonstrated those subsystems as the dominant heap consumer under pressure — adding speculative recovery actions violates YAGNI and increases test surface. Second, keeping ADR-079 and ADR-107 architecturally symmetric reduces the cognitive cost of cross-branch maintenance. If a board-specific incident emerges, a successor ADR can supersede this one to add targeted actions.

## Consequences

### Positive

- Symmetric behaviour with dev (ADR-079): one mental model for heap recovery across both branches.
- Uses the existing platform abstraction (`platformFreeHeap()`), so the same `helperStuff.ino` body compiles for ESP8266 and ESP32 targets within the 2.0.0 worktree.
- Telnet stays alive for live diagnostics on both targets.
- WS clients reconnect automatically via `graph.js`.

### Negative

- OTmonitor / `nc` users on port 25238 are disconnected on both ESP8266 and ESP32 builds; manual reconnect required.
- New `[heap-recovery]` log format introduces a tooling change for field-test log scrapers — same impact noted on dev (ADR-079).

### Risk

- `startOTGWstream()` idempotency assumption: verified by the implementation task (TASK-675). If `startOTGWstream()` re-allocates heap on each restart, action #2 is removed from the helper — same fallback as dev.
- ESP32 path: `webSocket.disconnect()` close-all semantics inherited from the WebSocketsServer library; behaviour identical to ESP8266 path. Verified by the implementation task.
- Speculative board-specific consumers (OLED, BLE, Ethernet) are NOT addressed; if a field incident demonstrates one of those as the dominant heap consumer, a successor ADR is required.

## Related Decisions

- **ADR-079** (dev branch sibling, also Proposed/Accepted alongside this ADR) — same recovery actions for the ESP8266-only dev branch. Cross-reference for the architecturally-symmetric decision pair.
- **ADR-062** (dev) / equivalent in 2.0.0 — discovery-verify already has its own heap-abort; not duplicated here.
- **ADR-073** (dev) / equivalent in 2.0.0 — JIT discovery on status burst, ensures cleared pending bitmap is re-armed without permanent discovery loss.
- **TASK-672** — 2.0.0 port of TASK-671 that left the helper as a stub; this ADR finishes that work.
- **TASK-675** — implementation of this decision on the 2.0.0 worktree.

## References

- `src/OTGW-firmware/helperStuff.ino:853` — `HEAP_CRITICAL_THRESHOLD = 1536` on 2.0.0.
- `src/OTGW-firmware/helperStuff.ino:1067-1093` — current stub body that this ADR replaces.
- `src/OTGW-firmware/OTGW-firmware.ino:555` — single caller in `doBackgroundTasks()`.
- `src/OTGW-firmware/webSocketStuff.ino:151,161` — example per-client WS disconnect callsites; recovery uses the close-all form.
- `src/OTGW-firmware/helperStuff.ino:526` — `OTGWstream.stop()` precedent in the existing `doRestart()` path.
- `src/OTGW-firmware/data/graph.js` — browser-side WS reconnect loop.

## Enforcement

```json
{
  "forbid_pattern": [
    { "pattern": "MQTTclient\\.disconnect\\(\\)", "path_glob": "src/OTGW-firmware/helperStuff.ino", "message": "ADR-107: do not disconnect MQTT in emergencyHeapRecovery() — reconnect cost exceeds heap recovered. See ADR Decision section, 'Explicitly NOT done'." },
    { "pattern": "debugTelnet\\.(stop|disconnect)\\(\\)", "path_glob": "src/OTGW-firmware/helperStuff.ino", "message": "ADR-107: do not drop telnet clients in emergencyHeapRecovery() — operators need live diagnostics during heap incidents." },
    { "pattern": "ESP\\.getFreeHeap\\(\\)", "path_glob": "src/OTGW-firmware/helperStuff.ino", "message": "ADR-107: use platformFreeHeap() in emergencyHeapRecovery() so the same source compiles for ESP8266 and ESP32 targets on 2.0.0." }
  ],
  "require_pattern": [],
  "llm_judge": false
}
```

The third declarative rule enforces the platform-abstraction discipline that distinguishes ADR-107 from ADR-079 — without it, a casual port from dev would slip a `ESP.getFreeHeap()` call into the 2.0.0 helper and break the ESP32 build path.
