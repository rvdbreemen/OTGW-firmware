# ADR-075: WiFi Reconnect Timeout Tuning

**Status:** Accepted
**Date:** 2026-04-03
**Supersedes:** ADR-047 (timeout and retry parameters only; state machine design unchanged)

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

## Consequences

### Positive
- WiFi reconnection succeeds reliably — `WiFi.begin()` gets a full 30-second window to complete association + DHCP
- SDK auto-reconnect handles brief glitches without triggering the state machine
- Matches the proven 30-second window from v1.2.0's `restartWifi()`

### Negative
- Longer time before reboot on persistent failure (300s vs 75s)
  - Accepted: 5 minutes is still reasonable, and correct reconnection is more important than fast failure

## Related
- ADR-047: Non-Blocking WiFi Reconnect State Machine (design unchanged, parameters updated)
- `loopWifi()` in `OTGW-firmware.ino`
- `startWiFi()` in `networkStuff.ino`
