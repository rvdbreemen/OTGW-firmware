# ADR-108: MQTT connect() socket timeout accepted as known main-loop sync-blocker (2.0.0)

## Status

Superseded by ADR-131 (2026-06-14). The premise of this ADR (PubSubClient's synchronous `connect()` accepted as a bounded loop-blocker, capped by `setSocketTimeout(5)`) no longer holds: TASK-865.7 replaced PubSubClient with espMqttClient, whose `connect()` is asynchronous, so there is no synchronous connect-stall to bound and `setSocketTimeout` was removed. See ADR-131.

## Context

`handleMQTT()` runs from `doBackgroundTasks()`. In the `MQTT_STATE_TRY_TO_CONNECT` branch it issues a synchronous `MQTTclient.connect(...)` (PubSubClient) at `src/OTGW-firmware/MQTTstuff.ino:1058` (anonymous) and `:1063` (with credentials). Two lines above the credentials selector (`:1028`), `MQTTclient.setSocketTimeout(5)` caps how long that synchronous call can stall the loop.

This ADR is the 2.0.0 sibling of dev ADR-080 (TASK-674 / TASK-676 Item 6). Both branches inherited PubSubClient's synchronous `connect()` from the 1.x line; both ran a sync-blocker inventory in 2026-05 (dev: TASK-674; 2.0.0: TASK-676). The two branches diverged on the **value** of the socketTimeout:

- **dev** uses 15 s (`src/OTGW-firmware/MQTTstuff.ino:778`), with the historical rationale "Increased from 4 to 15 seconds for better stability". Accepted in dev ADR-080.
- **2.0.0** uses 5 s (`src/OTGW-firmware/MQTTstuff.ino:1028`), with an in-code rationale prioritising HTTP/WS responsiveness during outage. This ADR documents that decision at the architecture level.

The divergence is intentional: 2.0.0 is the new-platform branch where the responsiveness trade-off was re-evaluated against the same PubSubClient surface; the maintainer chose tighter latency over the longer-stability envelope that dev preserves. There is no platform-layer reason the two values must match — both branches run synchronous PubSubClient on an ESP-class MCU, and the platform abstraction layer (`platformFreeHeap()` etc.) does not change connect() semantics.

The retry cadence is governed by `timerMQTTwaitforconnect` plus the 10-minute fallback after 5 consecutive failures (per the in-code comment at `MQTTstuff.ino:1023-1027`). Steady state — `MQTT_STATE_IS_CONNECTED` — only invokes `MQTTclient.loop()`, which is non-blocking.

### Forces

- **HTTP/WS responsiveness during outages.** A 15 s connect-stall noticeably degrades the WebUI and graph live feed when the broker drops. 2.0.0 weights this higher than dev does.
- **The PIC serial line is the safety-critical surface.** UART-buffered + bounded `handleOTGW()` drain (same TASK-671 pattern as dev) means a 5–15 s loop stall does not cause UART overrun on either branch.
- **OT message latency tolerance.** OT messages arrive on ~1 Hz cadence. A 5 s stall delays MQTT publishes by up to 5 s; HA `avty_t` windows tolerate this comfortably.
- **The 5 s value is empirical, not measured.** The in-code comment at `MQTTstuff.ino:1023-1027` is reasoned but not backed by field-test data on this branch. If post-merge testing shows reconnect-flap regressions, this ADR is the explicit anchor to revisit.
- **Replacing PubSubClient is out of scope.** Same conclusion as dev ADR-080.

## Decision

Accept the existing `MQTTclient.setSocketTimeout(5)` on 2.0.0 as a known synchronous blocker bounded by:

- 5 s worst-case main-loop stall per connect attempt.
- Retry-gated by `timerMQTTwaitforconnect` between attempts; 10-minute fallback after 5 failures.
- Only entered during `MQTT_STATE_TRY_TO_CONNECT`. Steady-state operation is non-blocking.

Do not raise the value to dev's 15 s without measurement justification. Do not lower below 5 s — sub-5 s timeouts risk false-negative connects on slow brokers that the field has not yet been profiled against. The in-code comment at `MQTTstuff.ino:1023-1027` already explains the responsiveness trade-off; this ADR adds a single line cross-referencing the decision.

No code-behaviour change. The deliverable is the documented decision plus the cross-reference comment line.

## Alternatives Considered

### Alternative 1 — Align with dev's 15 s

Take the dev value to keep both branches in lock-step.

**Rejected:** the 2.0.0 maintainer explicitly chose 5 s based on responsiveness trade-offs that dev does not weigh as heavily. Aligning would regress that decision. Cross-branch parity is desirable for ADR *structure* (both branches have an ADR documenting the bound) but not for the *value* (which reflects branch-local priorities).

### Alternative 2 — Switch to an async MQTT client

Same option considered in dev ADR-080.

**Rejected:** same rationale as dev — large-surface refactor (handlers, LWT, retained-state, discovery callbacks). Out of scope for a Tier-2 cleanup task. Would need its own ADR if pursued.

### Alternative 3 — Skip the ADR; trust the in-code comment

The in-code comment at `MQTTstuff.ino:1023-1027` is already explanatory.

**Rejected:** a future cleanup pass on this branch would lack the same anchor that dev has via ADR-080; the divergence rationale would be unrecorded; and the "5 s is the accepted bound" decision would not survive a future maintainer thinking "this looks aggressive, let me bump it for safety." Symmetric ADRs across the two branches make the decision-tracking honest.

## Consequences

### Positive

- 2.0.0 now has parity with dev's ADR-tracked sync-blocker decision. A future cleanup pass on either branch refers to the matching ADR rather than re-analysing the case from scratch.
- The divergence (5 s vs 15 s) is recorded with explicit rationale, so a cross-branch port (in either direction) is a deliberate decision and not a copy-paste accident.
- The 5 s figure is now traceable to a documented decision in the ADR index, not just an inline comment.

### Negative

- Two ADRs are now slaved to PubSubClient's synchronous connect() semantics. Replacing the MQTT client requires superseding both (ADR-080 on dev, ADR-108 on 2.0.0).
- If post-merge field testing shows the 5 s value causing reconnect flaps that dev's 15 s avoids, this ADR will need to be revisited (likely superseded with a measurement-backed value).

### Risk

- A PubSubClient upgrade could shift the practical stall envelope. The library is vendored under `src/libraries/` so the version is pinned; this risk is bounded to deliberate upgrades on this branch.

## Related Decisions

- **ADR-080** (dev sibling) — accepts 15 s on the 1.5.x line; identical structure to this ADR with the divergent value.
- **ADR-107** (2.0.0) — emergency heap recovery explicitly does *not* drop the MQTT connection, partly because re-establishing it costs the connect-stall this ADR accepts.
- **TASK-674** (dev) — Tier-2 sync-blocker inventory, parent of dev ADR-080.
- **TASK-675** (2.0.0) — Tier-1 mainloop fixes ported from dev TASK-673; this ADR is the Tier-2 follow-up.
- **TASK-676** (2.0.0) — Tier-2 dispositions on this branch; this ADR is its Item 6 deliverable.

## References

- `src/OTGW-firmware/MQTTstuff.ino:1023-1028` — accepted `setSocketTimeout(5)` callsite with its in-code rationale.
- `src/OTGW-firmware/MQTTstuff.ino:1046-1064` — `MQTT_STATE_TRY_TO_CONNECT` branch.
- `src/OTGW-firmware/MQTTstuff.ino:1058` / `:1063` — synchronous `MQTTclient.connect(...)` callsites.
- `src/libraries/PubSubClient/` — vendored PubSubClient source.
- `docs/adr/ADR-080-mqtt-connect-socket-timeout-as-accepted-sync-blocker.md` (dev branch) — sibling ADR.

## Enforcement

```json
{
  "forbid_pattern": [
    { "pattern": "MQTTclient\\.setSocketTimeout\\((?!5\\))", "path_glob": "src/OTGW-firmware/MQTTstuff.ino", "message": "ADR-108: do not change MQTTclient.setSocketTimeout from 5 on the 2.0.0 branch. The value is the accepted sync-blocker envelope (worst case 5 s). To change it, supersede ADR-108 with a new ADR that captures the new envelope and the measurement that justifies it." }
  ],
  "require_pattern": [],
  "llm_judge": false
}
```

Single declarative guard mirrors ADR-080's pattern with the branch-local literal.
