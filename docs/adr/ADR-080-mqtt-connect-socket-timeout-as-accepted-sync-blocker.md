# ADR-080: MQTT connect() socket timeout accepted as known main-loop sync-blocker

## Status

Proposed.

## Context

`handleMQTT()` runs from `doBackgroundTasks()`. In the `MQTT_STATE_TRY_TO_CONNECT` branch it issues a synchronous `MQTTclient.connect(...)` (PubSubClient) at `src/OTGW-firmware/MQTTstuff.ino:808` (anonymous) and `:813` (with credentials). Two lines above (`:778`), `MQTTclient.setSocketTimeout(15)` caps how long that synchronous call can stall the loop.

The second mainloop review (TASK-674, Tier-2 follow-up after TASK-671/TASK-673) identified three remaining synchronous blockers in the loop: the webhook HTTP call (Item 5), this MQTT connect (Item 6), and the per-sensor OneWire read in `pollSensors()` (Item 7). Item 5 was tightened in the same task (1000 ms → 500 ms; existing retry state machine absorbs the slack). Item 7 is bus-physics-bound and not firmware-tunable. Item 6 — this ADR — is the remaining case.

The 15-second socketTimeout was bumped from PubSubClient's 4-second default by `MQTTstuff.ino:778` ("Increased from 4 to 15 seconds for better stability"). No prior ADR records the rationale. The reachable invariant is that this stall only occurs in `MQTT_STATE_TRY_TO_CONNECT`, which is entered when the broker is unreachable (outage, hostname resolution stall, WiFi flap recovering MQTT) or on initial connect. In steady state — `MQTT_STATE_IS_CONNECTED` — `handleMQTT()` only invokes `MQTTclient.loop()`, which is non-blocking.

The retry cadence is governed by `DECLARE_TIMER_SEC(timerMQTTwaitforconnect, 42, …)` (`MQTTstuff.ino:751`): after a failed connect, the next attempt is gated for 42 s. So the worst-case main-loop disruption pattern during a broker outage is "15 s connect-stall every 42 s" — roughly 36 % CPU/loop budget consumed by the connect attempt, with the gateway-to-PIC OT serial path, telnet, OTA, and HTTP all still served between attempts.

### Forces

- **The PIC serial line is the safety-critical surface.** It is hardware-buffered (UART RX FIFO) and `handleOTGW()` is bounded (TASK-671) to drain four lines per call, so a 15 s loop stall during MQTT connect does not cause UART overrun.
- **OT message latency tolerance.** OT messages arrive on a ~1 Hz cadence. A 15 s stall delays MQTT publishes by up to 15 s; Home Assistant entity timeouts are configured for the >30 s `avty_t` window (ADR-074), so a single connect-stall does not flip entities to `unavailable`.
- **The 42 s retry gate is intentional.** Faster retries would consume more main-loop budget and hammer the broker with reconnects. The existing 42 s gives a slow broker breathing room.
- **Tighter socketTimeouts regressed stability in the field.** The "Increased from 4 to 15 seconds for better stability" comment is the only artefact of the regression that drove the bump. There is no measurement on file, but the maintainer's experience is the operative constraint.
- **Replacing PubSubClient is out of scope.** Async MQTT clients exist (`espMqttClient`, `AsyncMqttClient`) but require a full re-test of the discovery + retained-state + LWT pipeline. The 15 s budget is acceptable; the rewrite cost is not.

## Decision

Accept the existing `MQTTclient.setSocketTimeout(15)` as a known synchronous blocker bounded by:

- 15 s worst-case main-loop stall per connect attempt.
- 42 s minimum interval between attempts (`timerMQTTwaitforconnect`).
- Only entered during `MQTT_STATE_TRY_TO_CONNECT`. Steady-state operation (`MQTT_STATE_IS_CONNECTED`) is non-blocking.

Do not lower the value to PubSubClient's 4 s default. Document the rationale in the code comment at `MQTTstuff.ino:778` so a future maintainer does not re-litigate the value without superseding this ADR. The comment now explains *why 15 s* (sync-blocker accepted under outage conditions) rather than the historical *what changed*.

No code-behaviour change. The deliverable is the documented decision plus the comment rewrite.

## Alternatives Considered

### Alternative 1 — Revert to PubSubClient's 4 s default

Cut the worst-case stall by ~73 %. Simpler model: connect attempts take ≤4 s.

**Rejected:** the historical bump from 4 s to 15 s addressed an unspecified-but-recorded "better stability" issue. Reverting risks the same regression. The 42 s retry gate already amortises the 15 s cost (36 % loop budget during outage, 0 % during steady state). The win is not worth the risk.

### Alternative 2 — Switch to an async MQTT client

`espMqttClient` or `AsyncMqttClient` are non-blocking. Connect cost would not block the loop at all.

**Rejected:** large-surface refactor (handlers, LWT, retained-state, discovery callbacks). Out of scope for a Tier-2 cleanup task. Tracked as a possible future direction but not opened as a backlog task — would need its own ADR.

### Alternative 3 — Add a "fast-fail" path that pings the broker IP before `connect()`

Probe with a 1 s TCP open against `MQTTbrokerIPchar:iBrokerPort`; only call `connect()` if the probe succeeds.

**Rejected:** doubles the network traffic during steady state and adds new code complexity without removing the synchronous nature of `connect()`. The 42 s retry gate already prevents tight-loop hammering.

## Consequences

### Positive

- The 15 s figure is now traceable to a documented decision instead of an undated comment. Future cleanup passes (TASK-674-style) can refer to this ADR and skip re-analysing the case.
- The connect-stall envelope is bounded and known: outage-only, 36 % loop budget, 0 % steady-state.
- The comment at `MQTTstuff.ino:778` now explains the invariant a future reader cares about (why this value is safe), not the historical change.

### Negative

- The 15 s stall remains a real number. Field operators observing a broker outage will see telnet pauses and delayed MQTT publishes during reconnect attempts.
- Locking in PubSubClient as the MQTT client makes a future async-client migration require this ADR to be superseded.

### Risk

- A future PubSubClient upgrade could change connect()'s internal behaviour (e.g., different DNS resolution timing) and shift the practical stall envelope away from the analysed 15 s + 42 s. The library is vendored under `src/libraries/` so the version is pinned; this risk is bounded to deliberate upgrades.

## Related Decisions

- **TASK-671** — bounded the `handleOTGW()` drain loops to 4 lines per call. That work removed the *unbounded* main-loop blockers; this ADR catalogues the *bounded* ones that remain.
- **TASK-673** — Tier-1 follow-up (`pendingUpgradePath` String→char[], `emergencyHeapRecovery()` real actions, BGTRACE removed).
- **TASK-674** — Tier-2 inventory of remaining sync-blockers (this ADR is the disposition for Item 6).
- **ADR-079** — emergency heap recovery explicitly *does not* drop the MQTT connection during heap incidents, partly because re-establishing it costs the 15 s stall this ADR accepts.
- **ADR-047** — non-blocking WiFi reconnect state machine; this ADR is the MQTT-side counterpart that explains why the MQTT connect path remains synchronous.

## References

- `src/OTGW-firmware/MQTTstuff.ino:751` — `timerMQTTwaitforconnect = 42 s`.
- `src/OTGW-firmware/MQTTstuff.ino:778` — `setSocketTimeout(15)` (the value this ADR accepts).
- `src/OTGW-firmware/MQTTstuff.ino:808` — anonymous `MQTTclient.connect(...)` callsite.
- `src/OTGW-firmware/MQTTstuff.ino:813` — credentialed `MQTTclient.connect(...)` callsite.
- `src/OTGW-firmware/MQTTstuff.ino:766-794` — `MQTT_STATE_INIT` / `MQTT_STATE_TRY_TO_CONNECT` state machine.
- `src/libraries/PubSubClient/` — vendored PubSubClient source.

## Enforcement

```json
{
  "forbid_pattern": [
    { "pattern": "MQTTclient\\.setSocketTimeout\\((?!15\\))", "path_glob": "src/OTGW-firmware/MQTTstuff.ino", "message": "ADR-080: do not change MQTTclient.setSocketTimeout from 15. The value is the accepted sync-blocker envelope (worst case 15 s, retry-gated to 42 s). To change it, supersede ADR-080 with a new ADR that captures the new envelope." }
  ],
  "require_pattern": [],
  "llm_judge": false
}
```

A single declarative guard is enough: the only mechanically expressible invariant is the literal `15` argument. The bounded retry cadence (42 s) lives in a different identifier (`timerMQTTwaitforconnect`); a regex on the timer value would be brittle and is left to the maintainer to spot in review.
