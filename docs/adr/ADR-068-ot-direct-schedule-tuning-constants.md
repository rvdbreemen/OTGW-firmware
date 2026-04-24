# ADR-068: OT-Direct Schedule Tuning Constants

**Status:** Accepted
**Date:** 2026-04-09

## Context

The OT-direct master mode scheduler in `OTDirect.ino` contains several numeric constants that were chosen during implementation but were never documented in ADR-063 through ADR-067. These constants affect correctness, protocol compliance, and memory usage, so their rationale should be recorded.

The constants are:

| Constant | Value | Location |
|---|---|---|
| `OT_STATUS_INTERVAL_MS` | 800 ms | Polling interval for MsgID 0 (status) |
| `OT_CMD_QUEUE_SIZE` | 8 frames | Ring buffer for queued write commands |
| `OT_RESPONSE_OVERRIDE_MAX` | 16 entries | SR= response override table |
| `OT_RESPONSE_MODIFY_MAX` | 8 entries | RM= response modifier table |
| `OT_UNKNOWN_ID_MAX` | 16 entries | UI= unknown-ID intercept table |
| 3-strike threshold | 3 consecutive UNKNOWN_DATA_ID | Auto-disable schedule entries |

## Alternatives Considered

### `OT_STATUS_INTERVAL_MS = 800`

The OpenTherm specification requires the master to send MsgID 0 (status) at least every 1000 ms (1 second). The boiler considers the master offline if no status frame arrives within 1100 ms.

- **1000 ms (spec minimum)**: Leaves no margin for jitter. A single cooperative-loop overrun could push the interval past 1100 ms and trigger a boiler fault.
- **800 ms (chosen)**: Provides a 200 ms safety margin against cooperative-loop jitter while matching the OT-Thing reference implementation. Sends 1.25 status frames per second instead of 1.0 — negligible overhead on a 32-bit bus.
- **500 ms**: Would be safe but sends twice as many status frames as needed, consuming bus time that could be used for data polling.

### `OT_CMD_QUEUE_SIZE = 8`

The command queue drains one frame per master cycle (~800 ms minimum, governed by `OT_STATUS_INTERVAL_MS`). Users may issue multiple commands in a burst (e.g., `TT=20`, `SW=50`, `HW=1` from a Home Assistant automation).

- **4 entries**: Insufficient for a 4-command MQTT burst. Commands would be silently dropped.
- **8 entries (chosen)**: Covers typical automation bursts (3-5 commands). At 800 ms drain rate, 8 frames queue for up to 6.4 seconds — acceptable latency. Memory cost: 8 × 4 bytes = 32 bytes.
- **16 entries**: Double the memory cost for marginal benefit. No documented use case generates more than 8 simultaneous commands.

### 3-strike auto-disable threshold

When a boiler returns `UNKNOWN_DATA_ID` for a schedule entry, the entry is a candidate for auto-disabling to avoid wasting bus time on MsgIDs the boiler does not support.

- **1 strike**: Too aggressive. A single corrupted frame (CRC error) or transient bus glitch would permanently disable a valid MsgID.
- **2 strikes**: Better, but back-to-back transient errors (rare but possible) would still false-disable.
- **3 strikes (chosen)**: Three consecutive `UNKNOWN_DATA_ID` responses are statistically implausible as transient errors. Any MsgID that truly returns `UNKNOWN_DATA_ID` three times in a row is genuinely unsupported. Counters are stored as 2-bit packed values (32 bytes for all 128 MsgIDs). MsgID 0 (status) is exempt — it is never auto-disabled regardless of response.
- **5 strikes**: Would delay disabling by two extra polling cycles (up to 2 minutes for slow-poll entries). Unnecessary given the 3-strike reliability.

The `AA=` command resets the 3-strike counter for a MsgID and re-enables it in the schedule, allowing manual recovery without a reboot.

### Table sizes (`OT_RESPONSE_OVERRIDE_MAX = 16`, `OT_RESPONSE_MODIFY_MAX = 8`, `OT_UNKNOWN_ID_MAX = 16`)

These tables are linear-scanned on each thermostat frame or response, making memory the dominant constraint. Sizes were chosen to cover realistic use cases:

- **Response overrides (SR=)**: 16 entries accommodates all commonly overridden data IDs (TSet, TrSet, DHW setpoint, status, etc.) with headroom. Memory: 16 × 4 bytes = 64 bytes.
- **Response modifiers (RM=)**: 8 entries is sufficient; this feature is less commonly used than SR=. Memory: 8 × 4 bytes = 32 bytes.
- **Unknown-ID intercept (UI=)**: 16 entries matches the response-override table for consistency. Memory: 16 × 1 byte = 16 bytes.

All tables return `NS` (No Space) error when full, matching PIC firmware behavior.

### Peek-before-dequeue in command queue

The command queue uses a peek-then-dequeue pattern: the frame at the tail is read before attempting an async send, and the tail pointer advances only on successful send. This prevents losing a command frame if the OT master bus is busy at the moment the queue is serviced. If the send fails (bus busy), the frame remains at the tail and is retried on the next scheduler tick (~100 ms later).

## Decision

All constants listed above are **Accepted as implemented**. No changes to values are required.

The 3-strike threshold, 800 ms status interval, queue size of 8, and table sizes are documented here as the rationale record. They should not be changed without considering the consequences described above.

## Consequences

### Benefits:
- Status interval of 800 ms provides a 200 ms guard against cooperative-loop jitter while matching OT-Thing
- Command queue of 8 handles typical automation bursts without dropped commands
- 3-strike threshold balances false-positive suppression against bus efficiency
- Peek-before-dequeue ensures zero command loss when the OT bus is temporarily busy
- Table sizes are sized to practical use cases with minimal RAM overhead

### Trade-offs:
- 800 ms status interval sends 25% more status frames than the spec minimum — acceptable given the ESP32-S3's throughput
- Hard-coded table sizes mean heavy users (many SR= overrides) hit the `NS` limit; no dynamic expansion
- 3-strike counter state is lost on reset (GW=R clears counters via `resetTransientState()`), requiring the boiler to re-teach which IDs it doesn't support

### Risks:
- If a boiler supports a MsgID intermittently (e.g., due to firmware bugs), the 3-strike mechanism may disable it. The `AA=` command provides manual recovery.
- Queue size of 8 is not validated against real-world MQTT burst scenarios on production hardware; monitor for dropped-command log messages.

## Related

- **ADR-064**: OT-Direct operating mode architecture — establishes the scheduler and command queue concepts
- **ADR-087**: Frame bridge pattern — the bridge is called from the scheduler for every queued frame
- **ADR-066**: Thermostat auto-detection — the 800 ms status interval is also the heartbeat for thermostat connectivity detection
- **Code**: `OTDirect.ino` — `OT_STATUS_INTERVAL_MS`, `OT_CMD_QUEUE_SIZE`, `OT_RESPONSE_OVERRIDE_MAX`, `OT_RESPONSE_MODIFY_MAX`, `OT_UNKNOWN_ID_MAX`, `otUnknownCounters[]`, `incUnknownCount()`, `scheduleMasterRequest()`
