# ADR-047: Non-Blocking WiFi Reconnect State Machine

**Status:** Accepted
**Date:** 2026-03-01
**Relates to:** ADR-007 (Timer-Based Task Scheduling), ADR-001 (ESP8266 Platform)

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

## Related Decisions
- ADR-007: Timer-Based Task Scheduling (cooperative scheduling model)
- ADR-011: External Hardware Watchdog (must not block >3s)
- ADR-001: ESP8266 Platform Selection (single-core, cooperative only)
- ADR-048: Non-Blocking Webhook State Machine (same pattern)
