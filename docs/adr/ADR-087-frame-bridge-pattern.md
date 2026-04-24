# ADR-087: Frame Bridge Pattern — Raw OT Frames to PIC-Format Text

**Status:** Accepted
Renumbered from ADR-065 on 2026-04-24 to resolve duplicate numbering (TASK-412). Content unchanged.
**Date:** 2026-04-04

## Context

The existing firmware has a mature downstream processing pipeline built around PIC-formatted text strings. The PIC co-processor outputs 9-character lines: a direction character (`T` for thermostat request, `B` for boiler response, `R` for gateway request override, `A` for gateway response override) followed by 8 hex digits representing a 32-bit OpenTherm frame. For example: `T80401234`, `B00401234`.

This text format feeds into:

- **`processOT()`** -- the central parser that decodes message IDs, data types, and values
- **MQTT publisher** -- publishes decoded values to topic trees
- **WebSocket streamer** -- sends real-time OT log lines to the browser
- **REST API** -- returns current OT values as JSON
- **Home Assistant auto-discovery** -- generates HA MQTT config based on seen message IDs

The OT-direct driver on OTGW32 works with raw 32-bit OpenTherm frames from the `opentherm_library` (interrupt-driven Manchester decoding). These frames arrive as `unsigned long` values with no text formatting.

### Alternatives considered

1. **Duplicate the downstream pipeline for raw frames** -- Rejected: would require parallel implementations of the parser, MQTT publisher, WebSocket streamer, REST responses, and HA discovery. Massive code duplication and maintenance burden.
2. **Refactor the entire pipeline to accept raw 32-bit frames** -- Rejected: invasive change touching every consumer. High risk of regressions in the stable PIC code path. Violates minimal-change-surface principle.
3. **Bridge pattern: format raw frames as PIC-style text, feed into existing pipeline** -- Chosen: one small adapter function, zero changes to downstream code.

## Decision

### `bridgeFrameToParser()`

A bridge function formats raw 32-bit OT frames into the 9-character PIC-compatible text format and passes them to `processOT()`:

```cpp
void bridgeFrameToParser(unsigned long frame, char origin) {
  char buf[10];  // 1 char origin + 8 hex digits + null
  snprintf_P(buf, sizeof(buf), PSTR("%c%08lX"), origin, frame);
  processOT(buf, 9);
}
```

The `origin` parameter maps to the standard PIC direction characters:

| Origin char | Meaning |
|---|---|
| `T` | Thermostat request (from thermostat to boiler) |
| `B` | Boiler response (from boiler to thermostat) |
| `R` | Gateway request override (injected by firmware) |
| `A` | Gateway response override (injected by firmware) |

### `synthesizeResponse()`

For OT-direct command confirmations (e.g., setpoint changes via MQTT or REST), the bridge generates "XX: value" text responses matching the format that `processOT()` expects for command acknowledgments. This ensures MQTT status updates and WebSocket log entries appear identically whether the command was executed by a PIC or by the OT-direct driver.

### Call sites

In OT-direct mode, the OpenTherm library's interrupt callbacks deliver raw frames. The cooperative loop in `OTDirect.ino` checks for completed frame transactions and calls `bridgeFrameToParser()` with the appropriate origin character. From that point forward, the data flow is identical to the PIC path.

## Consequences

### Benefits:
- Entire downstream stack (parser, MQTT, WebSocket, REST, HA discovery) works unchanged -- zero modifications required
- No duplication of formatting, publishing, or logging logic
- PIC-format text is a proven, well-tested interface boundary -- reusing it reduces regression risk
- Bridge function is trivial (~5 lines) and easy to verify
- OT-Thing project already validates this approach via its TCP server outputting PIC-compatible text

### Trade-offs:
- Adds a format-then-parse round-trip: raw frame -> text -> parsed values. CPU cost is negligible on ESP32-S3 (snprintf + hex parse < 10 microseconds), but it is architecturally indirect
- Debugging OT-direct issues requires awareness that frames pass through the text bridge before reaching the parser
- Any future change to the PIC text format must be mirrored in the bridge function

### Risks:
- If `processOT()` is ever refactored to accept raw frames natively, the bridge becomes unnecessary overhead (but removal is trivial)
- Origin character mapping must stay consistent between PIC firmware and the bridge -- a mismatch would cause silent misclassification of frame direction

## Related

- **ADR-063**: OTGW32 hardware support -- establishes the dual build and frame bridge concept
- **ADR-038**: OpenTherm data flow pipeline -- documents the `processOT()` parser and downstream consumers
- **Code**: `OTDirect.ino` (`bridgeFrameToParser`, `synthesizeResponse`), `OTGW-Core.ino` (`processOT`)
