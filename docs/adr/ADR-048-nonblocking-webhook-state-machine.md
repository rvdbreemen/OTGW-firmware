# ADR-048: Non-Blocking Webhook State Machine with Retry

**Status:** Accepted
**Date:** 2026-03-01
**Relates to:** ADR-007 (Timer-Based Task Scheduling), ADR-047 (WiFi State Machine)

## Context

The webhook feature fires an HTTP GET or POST request to a local-network device (e.g., Shelly relay, Home Assistant) when an OpenTherm status bit changes state. The original `evalWebhook()` detected the bit change and immediately called `sendWebhook()`, which performed a blocking HTTP request inline:

```cpp
void evalWebhook() {
  bool bitState = /* read trigger bit */;
  if (bitState != webhookLastState) {
    webhookLastState = bitState;
    sendWebhook(bitState);  // blocks up to 3000ms
  }
}
```

Problems:
1. **3-second blocking:** `http.setTimeout(3000)` could stall the main loop for 3s if the target device was slow or unreachable
2. **No retry:** If the HTTP request failed (network glitch, target temporarily down), the event was lost forever
3. **String allocation in error path:** `http.errorToString(code).c_str()` created a temporary `String` object on every failure (ADR-004 violation)
4. **Tight coupling:** Bit-change detection and HTTP sending were in the same function, making testing and state management difficult

## Decision

**Refactor webhook into a three-state machine that decouples detection from sending and adds retry with exponential backoff.**

### State machine design

```
           bit changed
  IDLE ─────────────────► PENDING
    ▲                        │
    │                   attemptSendWebhook()
    │                        │
    │   success         ┌────┴────┐
    │◄──────────────────┤ success? │
    │                   └────┬────┘
    │                    no  │
    │                        ▼
    │  retries >= 3    RETRY_WAIT
    │◄─── give up          │
    │                 DUE(timer 30s)
    │                      │
    └──────────────────────┘
         back to PENDING
```

**States:**
- `WH_IDLE` — monitoring trigger bit for changes; no pending send
- `WH_PENDING` — state change detected, ready to attempt HTTP send
- `WH_RETRY_WAIT` — send failed, waiting 30s before retry (up to 3 attempts)

### Key design choices

1. **HTTP timeout reduced: 3000ms → 1000ms**
   Local LAN targets (Shelly, Home Assistant) respond in <500ms. A 1-second timeout is generous for LAN and limits main-loop blocking.

2. **`attemptSendWebhook()` returns `bool`**
   Separates the send attempt from retry logic. Returns `true` on HTTP 2xx, `false` on any error. Policy blocks (non-local URL) return `true` to prevent retrying a permanent failure.

3. **30-second retry interval with 3 attempts**
   Uses `DECLARE_TIMER_SEC(timerWebhookRetry, 30, SKIP_MISSED_TICKS)` for timing. After 3 failures (~90s total), the webhook gives up — the target is likely down and will recover independently.

4. **Trigger bit always evaluated**
   Even during `WH_RETRY_WAIT`, `evalTriggerBit()` runs to track the latest state. If the bit changes again during retry, the new state supersedes the pending one.

5. **No String in error path**
   Replaced `http.errorToString(code).c_str()` with `snprintf_P(errBuf, sizeof(errBuf), PSTR("HTTP error %d"), code)` — fully ADR-004 compliant.

6. **Extracted `evalTriggerBit()` helper**
   Validates and clamps the trigger bit (0–15), logs if out of range, and reads from `OTcurrentSystemState.Statusflags`. Keeps `evalWebhook()` focused on state machine logic.

## Alternatives Considered

### Alternative 1: Background task with Ticker
Use a `Ticker` timer to schedule webhook retries in the background.

**Why not chosen:** Ticker callbacks run in interrupt context on ESP8266, where HTTP client calls are not safe. Would need to set a flag and process in the main loop anyway — which is exactly what the state machine does, more explicitly.

### Alternative 2: Queue-based approach
Queue webhook events and process them from a FIFO.

**Why not chosen:** Over-engineered for a single-event feature. There's only ever one pending webhook (the most recent state change). A state machine with one pending-state boolean is simpler and sufficient.

### Alternative 3: Keep blocking with shorter timeout
Just reduce the timeout from 3s to 1s and accept the blocking.

**Why not chosen:** Even 1s of blocking is significant at 4 OT messages/second. And without retry, failed sends are still lost. The state machine solves both problems.

## Consequences

### Positive
- **Max 1s blocking per attempt** (down from 3s), and only when WiFi is connected
- **Automatic retry:** Up to 3 attempts with 30s backoff — transient failures recovered
- **No heap allocation:** ADR-004 compliant error reporting
- **Observable:** State transitions and retry counts logged via DebugTf
- **Consistent pattern:** Same state machine approach as WiFi reconnect (ADR-047)
- **Testable:** `attemptSendWebhook(bool)` can be called independently via REST API test endpoint

### Negative
- **State machine complexity:** More code than a simple "detect and send" function
  - Accepted: The retry and non-blocking benefits justify the added structure
- **30s retry delay:** A failed webhook may take up to 90s to succeed
  - Accepted: This is a best-effort notification, not a safety-critical control path

## Implementation

Refactored in P10 of the C++ refactoring plan (webhook.ino):
- `sendWebhook()` → `attemptSendWebhook()` returning `bool`
- `evalTriggerBit()` extracted as helper
- `evalWebhook()` rewritten as `WH_IDLE`/`WH_PENDING`/`WH_RETRY_WAIT` state machine
- `testWebhook()` unchanged — calls `attemptSendWebhook()` directly
- HTTP timeout: 3000ms → 1000ms
- Error reporting: `String` → `snprintf_P` with stack buffer

## Related Decisions
- ADR-007: Timer-Based Task Scheduling (cooperative scheduling model)
- ADR-047: Non-Blocking WiFi Reconnect (same state machine pattern)
- ADR-004: Static Buffer Allocation (no String in error path)
- ADR-003: HTTP-Only (webhook targets local HTTP only)
- ADR-032: No Authentication / Local Network Security (webhook URL validation)
