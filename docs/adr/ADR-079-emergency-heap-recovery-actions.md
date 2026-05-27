# ADR-079: Emergency Heap Recovery Actions

## Status

Accepted, 2026-05-23.

## Context

`emergencyHeapRecovery()` is invoked from `doBackgroundTasks()` whenever `getHeapHealth() == HEAP_CRITICAL` (heap below `HEAP_CRITICAL_THRESHOLD = 1500` bytes; see `src/OTGW-firmware/helperStuff.ino:863`). Today (post TASK-671) the function body is:

1. A 30-second rate-limit gate (`EMERGENCY_RECOVERY_INTERVAL_MS`).
2. A single `yield()` call — documented as "hand a scheduler slot to the SDK so it can run its own free-pool housekeeping" (`helperStuff.ino:1133-1135`).
3. Two `DebugTf` log lines bracketing the yield to report the (essentially unchanged) heap delta.

The yield is redundant: `feedWatchDog()` at the top of `doBackgroundTasks()` (`OTGW-firmware.ino:383`) already yielded one line earlier, and the SDK runs free-pool housekeeping on every yield. The function does not actually free any RAM; the name `emergencyHeapRecovery` misleads field operators reading telnet logs.

A second mainloop review (after TASK-671) flagged this as an item with three possible resolutions: delete the helper, leave it as a stub, or make it do real recovery. The maintainer chose "real recovery". This ADR documents the chosen recovery actions and the reasoning behind which buffers/clients are released vs kept.

### Forces

- **ESP8266 has ~40 KB usable RAM.** Each connected TCP socket holds ~2-4 KB of lwIP buffers (RX + TX windows + control blocks). With 1-2 WebSocket clients + 1 OTGWstream client + transient HTTP, socket buffers can dominate the difference between "heap healthy" and "heap critical".
- **The discovery drip (`loopMQTTDiscovery`, `MQTTstuff.ino:1466`) is the largest steady-state heap consumer once subscriptions are in place** — each `doAutoConfigureMsgid()` call temporarily allocates 200-400 bytes for a discovery JSON. When the heap is already critical, queuing additional drip work makes the situation worse.
- **Field operators rely on telnet for live diagnostics.** Dropping the telnet session during a heap incident defeats the purpose of having recovery: the operator can no longer watch the heap recover.
- **HA-side reconnect logic exists for WebSocket clients** (`src/OTGW-firmware/data/graph.js` runs an auto-reconnect loop) but **not** for the OTGWstream port 25238 — OTmonitor users have to reconnect manually.
- **MQTT broker reconnect is expensive on the ESP8266** — `MQTTclient.connect()` is synchronous and blocks up to 15s (`MQTTstuff.ino:778`, `setSocketTimeout(15)`). Forcing an MQTT disconnect during heap recovery would create a stall + a retained-state re-sync burst that *raises* heap pressure.
- **Discovery-verify (ADR-062, `mqtt_discovery_verify.h:37`) already has a built-in heap-abort path** in `tickDiscoveryVerification()` — no additional abort call is needed in the recovery helper.

## Decision

When `getHeapHealth() == HEAP_CRITICAL`, `emergencyHeapRecovery()` performs the following three actions in order, gated by the existing 30-second rate-limit (`EMERGENCY_RECOVERY_INTERVAL_MS`):

1. **Drop all WebSocket clients** via `webSocket.disconnect()` (close-all variant). Each connected client releases ~2-4 KB of lwIP socket buffer. Browsers reconnect via the existing `graph.js` auto-reconnect loop; no user action required.
2. **Drop all OTGWstream clients** via `OTGWstream.stop()` followed by `startOTGWstream()` to restart the listener on port 25238. Releases socket buffer for any connected OTmonitor / `nc` session; the user reconnects manually. Implementation must verify that `startOTGWstream()` is idempotent and does not re-allocate heap on restart — if it does, the action is removed from the helper.
3. **Clear the MQTT discovery pending bitmap** via `clearMQTTConfigPending()` (`MQTTstuff.ino:1368`). Stops the drip from queuing additional discovery publishes during the critical window. The next status burst re-arms discovery via the JIT path (ADR-073), so no discovery is permanently lost.

A single `DebugTf` line at the end reports `before`/`after` heap, `delta`, and an `actions` bitmask so field operators can confirm which actions ran and how much heap was recovered.

**Explicitly NOT done:**

- **Telnet clients are not dropped.** Losing the active debug session during a heap incident is anti-pattern: the operator wants to watch the recovery, not be disconnected from it.
- **MQTT is not disconnected/reconnected.** The reconnect cost (15s blocking + retained-state re-sync burst) exceeds the heap recovered. Broker side state is preserved by keeping the session alive.
- **No explicit `tickDiscoveryVerification()` abort.** The verify state machine already aborts on heap pressure (`mqtt_discovery_verify.h:37`); a second call would duplicate state machinery.

The `yield()` call is removed (redundant — see Context).

## Alternatives Considered

### Alternative 1 — Delete the helper entirely

Remove `emergencyHeapRecovery()` and let the firmware crash naturally if heap exhausts. Simplest possible code; honest about the lack of recovery.

**Rejected:** the maintainer explicitly chose "real recovery" over "delete" when offered both options. Field-test stability matters; an OOM crash forces a full reboot (lose WS state, lose OTGW context, lose the diagnostic window into what caused the leak). A scoped recovery preserves the post-incident diagnostic surface.

### Alternative 2 — Aggressive recovery (also drop telnet + force MQTT reconnect)

Drop everything that holds heap: telnet sessions, MQTT connection, WS, OTGWstream. Maximum heap freed.

**Rejected:** telnet drop blinds the operator at the moment they most need visibility. MQTT reconnect costs 15s blocking + retained-state burst — measured impact: ~3-5 KB extra heap pressure during the reconnect window. Net negative.

### Alternative 3 — Stub the helper (leave the body as a log-only no-op)

Keep the function for symmetry with the 2.0.0 worktree port, but no recovery actions. Lowest risk.

**Rejected:** the maintainer's stated goal is to make the name `emergencyHeapRecovery` honest. A logging-only stub perpetuates the current misnomer.

## Consequences

### Positive

- Concrete, measurable heap recovery action with a 30-second rate-limit ceiling on disruption.
- Field operators see `[heap-recovery]` log lines on telnet with `before/after` numbers so the effect is observable.
- WS clients reconnect automatically via existing browser-side logic — no user action for the common case.
- Discovery-drip back-pressure is released, preventing a positive-feedback loop where each tick under heap pressure queues more allocation work.
- Telnet stays alive for live diagnostics.

### Negative

- OTmonitor / `nc` users on port 25238 are disconnected and must reconnect manually. Acceptable cost: those are operator tools, not automated consumers.
- Any in-flight WS broadcast at the moment of recovery is lost (e.g., the last OT log line may not reach a graph client). Recovery cadence (≥30s) means at most one missed broadcast per incident.
- The `actions` bitmask in the log line introduces a new format string — field-test tooling that greps `[heap-recovery]` lines needs to know about it. Documented in the dev release notes.

### Risk

- `startOTGWstream()` is assumed idempotent and allocation-neutral. If implementation testing shows it re-allocates heap on every restart, action #2 (OTGWstream drop) is removed from the helper and only the WS-disconnect + discovery-pending-clear remain. The implementation task (TASK-673) verifies this before merging.
- Repeated triggers in a heap-thrashing scenario could oscillate (drop → drip re-queues → drop) without the 30s rate-limit, but the rate-limit caps the disruption frequency.

## Related Decisions

- **ADR-062** — discovery-verify state machine, already has its own heap-abort path that this helper does not duplicate.
- **ADR-073** — JIT discovery on status burst, ensures cleared pending bitmap is repopulated on the next OT message — no permanent loss.
- **ADR-107** (2.0.0 branch, sibling of this ADR) — same recovery actions adapted for the platform abstraction layer (`platformFreeHeap()`); to be drafted alongside this ADR.
- **TASK-671** — removed the `delay(10)` blocking call from this helper; this ADR finishes the cleanup by giving the helper a real purpose.
- **TASK-673** — implementation of this decision on dev.

## References

- `src/OTGW-firmware/helperStuff.ino:863` — `HEAP_CRITICAL_THRESHOLD = 1500`.
- `src/OTGW-firmware/helperStuff.ino:1119-1144` — current stub body that this ADR replaces.
- `src/OTGW-firmware/OTGW-firmware.ino:399-402` — single caller of `emergencyHeapRecovery()` in `doBackgroundTasks()`.
- `src/OTGW-firmware/MQTTstuff.ino:1368` — `clearMQTTConfigPending()`.
- `src/OTGW-firmware/webSocketStuff.ino:150` — example WS disconnect callsite (per-client form); recovery uses the broadcast form.
- `src/OTGW-firmware/helperStuff.ino:577` — `OTGWstream.stop()` callsite (existing, in restart path).
- `src/OTGW-firmware/data/graph.js` — browser-side WS reconnect loop (the safety net for action #1).
- `mqtt_discovery_verify.h:37` — built-in heap-abort path in discovery-verify; intentionally not duplicated by this ADR.

## Enforcement

```json
{
  "forbid_pattern": [
    { "pattern": "MQTTclient\\.disconnect\\(\\)", "path_glob": "src/OTGW-firmware/helperStuff.ino", "message": "ADR-079: do not disconnect MQTT in emergencyHeapRecovery() — reconnect cost exceeds heap recovered. See ADR Decision section, 'Explicitly NOT done'." },
    { "pattern": "debugTelnet\\.(stop|disconnect)\\(\\)", "path_glob": "src/OTGW-firmware/helperStuff.ino", "message": "ADR-079: do not drop telnet clients in emergencyHeapRecovery() — operators need live diagnostics during heap incidents." }
  ],
  "require_pattern": [],
  "llm_judge": false
}
```

Declarative-only enforcement is sufficient: the two "do not do this" patterns are the architecturally significant guard-rails. The chosen recovery actions themselves are verified by the implementation task's AC list, not by the judge.
