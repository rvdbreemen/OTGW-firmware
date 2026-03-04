# ADR-045: PS=1 Print Summary Parsing

**Status:** Accepted  
**Date:** 2026-03-02

## Context

The OTGW PIC firmware supports a `PS=1` (Print Summary) mode. When active, the PIC stops outputting individual OpenTherm frames in the standard `T`/`B`/`R`/`A` format and instead emits a single comma-separated summary line once per OpenTherm communication cycle. The summary contains 25 fields (PIC firmware < v5) or 34 fields (PIC firmware v5+).

This mode is used by OTmonitor and older Domoticz integrations. Its presence must be detected because time sync commands (`SC=...`) must be suppressed when PS=1 is active (ADR-037).

### Previous behaviour (before v1.3.0)

Before this decision, the firmware:
1. **Detected** PS=1 mode from the `PS=1` echo on the serial line.
2. **Suppressed** time sync in PS=1 mode (ADR-037).
3. **Discarded** the comma-separated summary lines — no parsing, no MQTT, no HA discovery.

Users running in PS=1 mode (typically those sharing the serial port with Domoticz or OTmonitor) received no Home Assistant sensor entities and no MQTT values.

### Constraints

- **Two summary formats:** Old PIC firmware (< v5) emits 25 fields; new (v5+) emits 34 fields. The parser must support both.
- **Binary data handling:** Summary lines may contain numeric strings, flag pairs (`HB/LB`), and binary flag strings (`XXXXXXXX/YYYYYYYY`). The standard OT `decodeAndPublishOTValue()` pipeline already handles these formats per MsgID.
- **Buffer size:** PS=1 summary lines can exceed 256 characters (34 fields × ~8 chars average + commas). The serial read buffer must be at least 512 bytes.
- **MQTT / HA discovery:** Fields must be published via the same topic structure and HA discovery flow as normal OT frames to ensure zero-configuration HA integration.
- **Memory:** Static buffers required; no heap allocation for parsing (ADR-004).
- **PROGMEM:** MsgID lookup tables must live in flash (ADR-009).

## Decision

**Implement `processPSSummary()` that fully parses PS=1 summary lines and publishes each field through the existing `decodeAndPublishOTValue()` pipeline.**

Key design choices:

1. **Field-to-MsgID mapping tables in PROGMEM:** Two `static const uint8_t[]` arrays (`PSSUMMARY_MSGIDS_OLD[25]` and `PSSUMMARY_MSGIDS_NEW[34]`) map each CSV position to the corresponding OpenTherm MsgID. The same MsgID drives the existing decode/publish logic, so no new MQTT topic structure is needed.

2. **Format detection by comma count:** Old format has 24 commas (25 fields); new format has 33 commas (34 fields). Any other count is silently rejected — the line is not a valid PS=1 summary.

3. **Three field encodings handled:**
   - `HH/LL` decimal pair → high-byte and low-byte sub-fields (MsgIDs 6, 15, 48, 49, 70)
   - `XXXXXXXX/YYYYYYYY` binary 8-bit strings → status bit flags (MsgID 0 and 6)
   - Plain decimal or f8.8 → passed directly to `decodeAndPublishOTValue()` as hex

4. **Serial read buffer extended to 512 bytes** (`MAX_BUFFER_READ 512`) because PS=1 summary lines can exceed the previous 256-byte limit. The buffer is stack-allocated inside `readSerial()`.

5. **Reuse of existing pipeline:** `processPSSummary()` calls `decodeAndPublishOTValue()` for each field with a synthetic frame string, using the same MsgID, so MQTT topics, HA discovery, WebSocket streaming, and `OTcurrentSystemState` updates all happen automatically without any PS=1-specific code in those subsystems.

6. **WebSocket display:** Summary lines are shown in the OT log as `"PS=1 mode; No UI updates."` instead of the raw comma-separated data, consistent with the previous behaviour of suppressing individual frame streaming in PS=1 mode.

## Alternatives Considered

### Alternative 1: Continue discarding PS=1 summary lines
**Pros:**
- No code change required
- Simpler parser (none)

**Cons:**
- Users sharing the serial port with Domoticz or OTmonitor lose all HA sensor entities and MQTT values
- PS=1 mode is a legitimate use case; ignoring it silently degrades the integration

**Why not chosen:** The firmware's primary purpose is to bridge OpenTherm to MQTT and HA. Silently dropping valid data from a supported PIC mode is contrary to that purpose.

### Alternative 2: Build a separate PS=1 MQTT topic namespace
**Pros:**
- Clearly separates PS=1 data from live-frame data

**Cons:**
- Doubles the number of MQTT topics for PS=1 users
- Requires separate HA discovery config keys
- Breaks the "same topic regardless of source" expectation

**Why not chosen:** Sharing the same topic namespace and MsgID-driven pipeline is simpler and consistent with the existing data flow architecture (ADR-038).

### Alternative 3: Re-enable normal frame processing by switching PS=1 off
**Pros:**
- No parser needed; existing pipeline handles normal frames

**Cons:**
- The OTGW's PS=1 mode is set externally (by Domoticz or OTmonitor); clearing it without coordination would break the third-party tool
- `PS=0` would need to be sent on every reconnect, creating a race condition

**Why not chosen:** Overriding external PS=1 state is incompatible with shared-port deployments.

### Alternative 4: Parse only a subset of PS=1 fields
**Pros:**
- Less code; lower risk of parser bugs

**Cons:**
- Partial data in HA/MQTT is confusing (some sensors missing, no clear reason why)
- Full field coverage is achievable with the MsgID lookup table approach

**Why not chosen:** Complete field coverage is achievable at low incremental cost with the table-driven approach.

## Consequences

**Positive:**
- PS=1 mode users now receive a full set of HA sensor entities and MQTT values with zero extra configuration
- Both old (< v5) and new (v5+) PIC firmware formats are supported
- Reuses the existing decode/publish pipeline; no new MQTT topic structure
- Additive HA discovery: no breaking changes for existing non-PS=1 users (ADR-040 pattern)

**Negative:**
- Serial read buffer increased from 256 to 512 bytes (stack allocation in `readSerial()`)
- Parser adds ~300 lines of firmware code (PROGMEM tables + `processPSSummary()`)

**Risks and Mitigation:**
- *Stack overflow from 512-byte buffer:* The buffer is allocated inside `readSerial()` which is called from the main `loop()`. ESP8266 has ~4 KB CONT stack; a single 512-byte buffer is within safe limits. The previous 256-byte limit was already a stack allocation.
- *Field count mismatch across PIC versions:* Detected by comma count; non-matching lines are silently rejected, preserving existing behaviour for any line that is not a valid PS=1 summary.
- *Wrong data published on format misidentification:* Only two valid comma counts (24 and 33) are accepted; all others are dropped. The risk of mis-classifying a non-PS=1 line as a summary is negligible.

## Related Decisions

- **ADR-037:** Gateway Mode Detection — PS=1 mode already detected; `bPSmode` flag and time sync suppression remain in place
- **ADR-038:** OpenTherm Message Data Flow Pipeline — `processPSSummary()` is a new entry point into the same fan-out pipeline
- **ADR-004:** Static Buffer Allocation — PROGMEM tables and stack buffer; no heap allocation
- **ADR-006:** MQTT Integration Pattern — PS=1 fields published to same topic structure as normal frames
- **ADR-009:** PROGMEM Usage — MsgID lookup tables are `static const uint8_t[] PROGMEM`
- **ADR-041:** JIT HA Discovery — discovery entries for PS=1 fields generated on first publish, same as normal frames

## References

- Implementation: `src/OTGW-firmware/OTGW-Core.ino` (`processPSSummary()`, lines ~1875–2180)
- Buffer extension: `src/OTGW-firmware/OTGW-Core.ino` (`MAX_BUFFER_READ 512`, line ~2727)
- Call site: `src/OTGW-firmware/OTGW-Core.ino` (`readSerial()`, line ~2688)
- PROGMEM tables: `PSSUMMARY_MSGIDS_OLD[25]`, `PSSUMMARY_MSGIDS_NEW[34]`
- Introduced in: v1.3.0-beta
