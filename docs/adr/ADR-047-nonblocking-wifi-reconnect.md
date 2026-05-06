# ADR-047: Non-Blocking WiFi Reconnect State Machine

## Status

Superseded by ADR-061 (timeout and retry parameters only; state machine design unchanged), 2026-03-01. Relates to: ADR-007 (Timer-Based Task Scheduling), ADR-001 (ESP8266 Platform).

## Context

The WiFi reconnection logic used a blocking `restartWifi()` function with a `delay(100)` inside a `while` loop, stalling the main loop for up to 30 seconds:

```cpp
void restartWifi() {
  WiFi.disconnect();
  delay(1000);
  WiFi.begin();
  uint8_t count = 0;
  while (WiFi.status() != WL_CONNECTED && count < 300) {
    delay(100);  // blocks 100ms per iteration
    count++;
  }
}
```

This violated the cooperative scheduling model (ADR-007) and caused:
- Hardware watchdog timeout risk (ESP8266 watchdog fires at ~3.2s without `yield()`)
- MQTT/WebSocket disconnections from missed keepalives
- OT message queue backup (messages arrive at ~4/sec but can't be processed)
- Unresponsive web UI during reconnection attempts

The function was called from `doTaskMinuteChanged()` (a timer callback), making the blocking even worse — it blocked the entire timer cascade.

## Decision

**Replace the blocking `restartWifi()` with a non-blocking state machine `loopWifi()` that runs cooperatively in the main loop.**

### State machine design

```
           WiFi.status() != WL_CONNECTED
  IDLE ──────────────────────────────────► DISCONNECTED
    ▲                                          │
    │                                  WiFi.begin()
    │                                          │
    │                                          ▼
    │  connected                         CONNECTING
    │◄──────────────────────────────────      │
    │                                         │
    │  retries < MAX_RETRIES && timeout       │
    │◄──────────────────── FAILED ◄───────────┘
         (restart from                  timeout without
          DISCONNECTED)                  WL_CONNECTED

                retries >= MAX_RETRIES
          FAILED ──────────────────────► doRestart() [device reboots]
```

**States:**
- `WIFI_IDLE` — WiFi connected, monitoring for disconnection
- `WIFI_DISCONNECTED` — Connection lost, preparing to reconnect
- `WIFI_CONNECTING` — `WiFi.begin()` called, waiting for association (5s timeout)
- `WIFI_RECONNECTED` — Successfully reconnected, log and return to IDLE
- `WIFI_FAILED` — Attempt failed, increment retry counter, try again or give up

**Key properties:**
- Zero blocking: each call to `loopWifi()` returns immediately
- Uses `DECLARE_TIMER_SEC` from safeTimers.h for timeout tracking
- Up to 15 reconnection attempts before giving up and triggering a device reboot (prevents infinite retry storm)
- Called from `doBackgroundTasks()` before the WiFi-dependent service checks
- `yield()` and `feedWatchDog()` called at appropriate points

### Integration point

```cpp
void doBackgroundTasks() {
  loopWifi();  // non-blocking WiFi state machine
  if (WiFi.status() != WL_CONNECTED) return;  // skip network tasks
  // ... MQTT, WebSocket, HTTP server, etc.
}
```

## Alternatives Considered

### Alternative 1: ESP8266WiFi auto-reconnect
Enable `WiFi.setAutoReconnect(true)` and let the SDK handle it.

**Why not chosen:** The SDK auto-reconnect is not deterministic — it may or may not reconnect depending on the disconnect reason. It doesn't provide visibility into the reconnection state, making debugging impossible. We need explicit control for logging, retry limits, and integration with the watchdog.

### Alternative 2: FreeRTOS task for WiFi management
Run WiFi reconnection in a separate task/thread.

**Why not chosen:** ESP8266 does not support FreeRTOS multitasking in the Arduino framework. The single-core Tensilica L106 uses cooperative scheduling only.

### Alternative 3: Timer-based retry with callbacks
Use a `Ticker` or safeTimer to schedule reconnection attempts.

**Why not chosen:** Timer callbacks on ESP8266 run in interrupt context (for Ticker) or still need a state machine for multi-step reconnection. The explicit state machine in the main loop is clearer and doesn't have the ISR-context limitations.

## Consequences

### Positive
- **Zero main-loop blocking:** Each `loopWifi()` call takes <1ms
- **Watchdog safe:** No risk of WDT timeout during reconnection
- **Observable:** State transitions logged via DebugTf
- **Bounded retries:** Gives up after 15 attempts then reboots, preventing infinite retry storms
- **Cooperative:** Other services (OT message processing, MQTT keepalive) continue running
- **Consistent pattern:** Same state machine approach used for webhook (ADR-048)

### Negative
- **Reconnection takes longer:** Non-blocking approach spreads the reconnection over multiple loop iterations instead of a single blocking call
  - Accepted: The delay is barely noticeable (5s timeout × 15 retries = 75s max) and all services remain responsive during the process
- **More code:** State machine is more verbose than a simple while loop
  - Accepted: The clarity and safety benefits outweigh the verbosity

## Implementation

Refactored in P9 of the C++ refactoring plan (OTGW-firmware.ino):
- `restartWifi()` removed from `doTaskMinuteChanged()`
- `loopWifi()` added to `doBackgroundTasks()` as first call
- States: `WIFI_IDLE`, `WIFI_DISCONNECTED`, `WIFI_CONNECTING`, `WIFI_RECONNECTED`, `WIFI_FAILED`
- 5-second connection timeout, 15 retry attempts before rebooting

## DHCP management rule (confirmed by issue #525)

**`wifi_station_dhcpc_start()` must only be called when the STA is NOT connected.**

Calling it while the station is associated resets the IP address to 0.0.0.0 immediately.
Once the DHCP client has been manually started, the SDK's `setAutoReconnect` path
(`wifi_station_connect()`) no longer calls `dhcpc_start()` on reconnection — DHCP is
considered "user-managed". After a router reboot, the device re-associates at L2 but the
DHCP client only tries to RENEW the old lease rather than sending a fresh DISCOVER; if the
router does not honour the renewal, the device remains unreachable indefinitely.

**Rules derived from issue #525 root-cause analysis:**
1. Call `wifi_station_dhcpc_start()` **only** in `WIFI_DISCONNECTED`, before `WiFi.begin()`.
2. **Never** call `dhcpc_stop/start` while the station is connected (not in `startNTP()`,
   not in `startWiFi()`, not in `WIFI_RECONNECTED`).
3. `WiFi.hostname()` can safely be called at any time — it only sets the in-memory hostname
   used for the *next* DHCP exchange; it does not disrupt the current connection.

Detailed analysis: `docs/reviews/2026-04-07_issue-525-sdk-dhcp-analysis/ANALYSIS_REPORT.md`

## Related Decisions
- ADR-007: Timer-Based Task Scheduling (cooperative scheduling model)
- ADR-011: External Hardware Watchdog (must not block >3s)
- ADR-001: ESP8266 Platform Selection (single-core, cooperative only)
- ADR-048: Non-Blocking Webhook State Machine (same pattern)

## References

<!-- TODO: populate from inline citations or external sources cited in the body. -->
