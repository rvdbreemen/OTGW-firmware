# ADR-061: WiFi Reconnect Timeout Tuning

## Status

Accepted, 2026-04-03. Supersedes: ADR-047 (timeout and retry parameters only; state machine design unchanged).

## Context

Users reported that v1.3.x firmware goes offline and fails to recover. Investigation traced the root cause to the `loopWifi()` state machine parameters introduced in ADR-047.

The per-attempt timeout was set to 5 seconds. Each timeout transitions back to `WIFI_DISCONNECTED`, which calls `WiFi.begin()`. On ESP8266, `WiFi.begin()` restarts the full WiFi association process from scratch (scan + authentication + association + DHCP). This process routinely takes 5-10+ seconds depending on AP load and signal strength.

The result: each `WiFi.begin()` call was cancelled by the next one before it could complete, creating a cycle of interrupted connection attempts that never succeeded.

The previous `restartWifi()` (v1.2.0) called `WiFi.begin()` once and waited up to 30 seconds — enough time for the full process.

Additionally, `WiFi.setAutoReconnect(true)` (set in `startWiFi()`) remains enabled. With a 5-second timeout, `loopWifi()` would call `WiFi.begin()` while the SDK auto-reconnect was mid-association, cancelling it. With a 30-second timeout, this interference no longer occurs — the SDK has ample time to handle brief glitches transparently before `loopWifi()` intervenes.

## Decision

**Increase the per-attempt timeout from 5 seconds to 30 seconds and reduce max retries from 15 to 10.**

The state machine design from ADR-047 is unchanged — only the timing parameters are adjusted.

| Parameter | ADR-047 | This ADR |
|-----------|---------|----------|
| Per-attempt timeout | 5s | 30s |
| Max retries | 15 | 10 |
| Max time before reboot | 75s | 300s |
| `WiFi.setAutoReconnect()` | `true` (unchanged) | `true` (unchanged) |

`WiFi.setAutoReconnect(true)` is kept enabled. It handles brief WiFi glitches (channel hops, momentary interference) transparently at the radio level in under 1 second. `loopWifi()` serves as the fallback for longer outages where the SDK auto-reconnect is insufficient.

## Alternatives Considered

### Alternative A: Keep the 5-second per-attempt timeout from ADR-047

Leave the original ADR-047 parameters in place (5-second timeout, 15 retries, ~75-second budget before reboot) and look for the bug elsewhere — perhaps in `WiFi.setAutoReconnect()` or in the order of state-machine transitions.

**Rejected** because root-cause analysis showed the 5-second timeout *is* the bug: ESP8266's `WiFi.begin()` restarts the full association process from scratch (scan + auth + association + DHCP), which routinely takes 5-10+ seconds in normal field conditions depending on AP load and signal strength. A 5-second deadline therefore guarantees that each `WiFi.begin()` is cancelled by the next one before it can finish, creating an infinite loop of interrupted attempts. The previous v1.2.0 `restartWifi()` used a 30-second window and worked reliably, which is the proof-by-existence that 5 seconds is too aggressive on this hardware.

### Alternative B: Disable `WiFi.setAutoReconnect()` and rely solely on `loopWifi()` to drive reconnects

Set `WiFi.setAutoReconnect(false)` so the SDK never auto-reconnects in the background, eliminating any chance of `loopWifi()` racing the SDK's own attempt. The state machine becomes the single owner of reconnect timing.

**Rejected** because the SDK's auto-reconnect handles brief radio-level glitches (channel hops, momentary interference, beacon misses) transparently in well under 1 second — a class of disconnects the application layer should never even see. Disabling it would push every transient hiccup into the full `loopWifi()` state-machine cycle (currently bounded at 30 seconds per attempt), turning sub-second blips into multi-second outages from MQTT and WebSocket clients' perspectives. The 30-second timeout already gives the SDK ample room to handle these transparently before `loopWifi()` would intervene, so the race condition disappears without sacrificing the radio-level recovery path.

### Alternative C (chosen): Lengthen per-attempt timeout to 30 s, reduce retries from 15 to 10

Match the proven v1.2.0 30-second window per attempt, and pull the retry count down so the total budget before reboot stays bounded (300 s instead of an unbounded 450 s).

**Trade-off accepted**: time-to-reboot on a genuinely unrecoverable failure grows from ~75 s to ~300 s. This is the right direction — successful reconnection matters far more than fast failure for an unattended IoT device, and 5 minutes is still a tolerable upper bound for the rare hard-failure case (AP genuinely gone, credentials wrong). The state-machine design from ADR-047 is preserved; only the timing parameters move.

## Consequences

### Positive
- WiFi reconnection succeeds reliably — `WiFi.begin()` gets a full 30-second window to complete association + DHCP
- SDK auto-reconnect handles brief glitches without triggering the state machine
- Matches the proven 30-second window from v1.2.0's `restartWifi()`

### Negative
- Longer time before reboot on persistent failure (300s vs 75s)
  - Accepted: 5 minutes is still reasonable, and correct reconnection is more important than fast failure

## Related Decisions

- ADR-047: Non-Blocking WiFi Reconnect State Machine (design unchanged, parameters updated)

## References

- `loopWifi()` in `OTGW-firmware.ino`
- `startWiFi()` in `networkStuff.ino`

## Enforcement

```json
{
  "llm_judge": true
}
```
