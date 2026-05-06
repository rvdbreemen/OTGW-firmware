# ADR-059: Ser2net Queue Awareness and Serial Bus Coordination

## Status

Accepted, 2026-03-28.

## Context

The OTGW firmware communicates with the PIC microcontroller over a single 9600-baud serial link. Two independent sources can send commands to the PIC:

1. **Command queue** — used by the firmware itself (MQTT, REST API, PIC settings polling, time sync). Commands are queued via `addOTWGcmdtoqueue()` and dispatched by `handleOTGWqueue()` once per second.
2. **Ser2net (port 25238)** — a transparent TCP-to-serial bridge for legacy clients like OTmonitor. Every byte from the TCP connection is written directly to `OTGWSerial` as it arrives, providing the raw serial passthrough that legacy tools expect.

These two paths are unaware of each other, creating three problems:

- **Logical interference:** The queue sends `CS=55` (from Home Assistant) while OTmonitor sends `CS=30` via ser2net. The PIC processes `CS=30`, but the queue still has `CS=55` pending and re-sends it on the next retry, overriding what the user set via OTmonitor.
- **Response confusion:** The PIC's response to a ser2net command (e.g., `"CS: 30.00"`) is picked up by `checkOTGWcmdqueue()`, which may incorrectly match and remove a different queued command with the same 2-letter prefix.
- **Timing collisions:** If `handleOTGWqueue()` dispatches a queued command while the PIC is still processing a ser2net command, the PIC receives two commands in rapid succession with unpredictable interleaving.

Routing ser2net through the command queue was considered but rejected: it would add at least 1 second of latency and break the timing expectations of OTmonitor and similar tools, which assume raw serial passthrough. The ser2net path is the original OTGW interface — clients know nothing about our firmware and expect flat serial communication.

## Decision

**Keep ser2net as a direct serial passthrough, but make the command queue _aware_ of ser2net traffic so it can avoid conflicts.**

Three mechanisms, implemented together:

### 1. Activity timestamp

A `static uint32_t lastSer2netCmdMs` tracks the last time a complete command (line ending in CR with `XX=` format) arrived from ser2net. Updated in the `handleOTGW()` ser2net CR handler.

### 2. Queue pause during ser2net activity

At the top of `handleOTGWqueue()`:

```cpp
if ((millis() - lastSer2netCmdMs) < SER2NET_QUIET_MS) return;
```

When ser2net has been active within the last 2 seconds (`SER2NET_QUIET_MS = 2000`), queue processing is skipped entirely. This gives the PIC time to process the ser2net command and respond, without the queue injecting competing commands.

The timestamp is initialized to `0 - SER2NET_QUIET_MS` (unsigned wraparound) so the quiet period is already expired at boot and does not delay initial queue processing.

### 3. Conflicting queue entry removal

When a ser2net command is detected (CR received, `sWrite` contains `XX=...`), the queue is scanned for entries with the same 2-character command prefix. A matching entry is removed via `removeFromCmdQueue()`, preventing the queue from overriding what the ser2net client just sent.

For PR commands (which can have multiple entries with different register letters, e.g., `PR=O`, `PR=S`), the removal also matches on the register letter at position 3 (`sWrite[3]` vs `cmdqueue[qi].cmd[3]`), consistent with the register-level matching used in `checkOTGWcmdqueue()`.

## Alternatives Considered

<!-- TODO: document at least 2 alternatives that were considered and rejected, with reasoning. -->

## Consequences

### Benefits

- **Ser2net remains fully transparent:** No changes to the byte-by-byte passthrough. Legacy clients see no difference.
- **Queue respects ser2net:** Commands from OTmonitor are not overridden by stale queue entries within seconds.
- **PIC gets breathing room:** The 2-second quiet period prevents rapid-fire command collisions on the serial bus.
- **Minimal code:** ~20 lines added to the existing CR handler and queue processor.

### Trade-offs

- **Queue latency after ser2net:** Queued commands are delayed up to 2 seconds after the last ser2net command. For periodic commands (PIC settings polling every 3s, time sync every 60s) this is negligible.
- **Single entry removal:** Only the first matching queue entry is removed per ser2net command. If `forceQueue=true` caused duplicate entries with the same prefix, subsequent duplicates remain. In practice, deduplication in `addOTWGcmdtoqueue()` prevents this for non-PR commands, and PR commands use register-level matching.
- **No deep integration:** Ser2net commands are not tracked in the queue, so the queue cannot retry or log them. This is intentional — ser2net is a passthrough, not a managed channel.

## Related Decisions

- [ADR-016: OpenTherm Command Queue](ADR-016-opentherm-command-queue.md) — queue design and deduplication
- [ADR-031: Two-Microcontroller Coordination Architecture](ADR-031-two-microcontroller-coordination-architecture.md) — ESP8266/PIC serial link
- [ADR-058: Non-blocking PIC Command/Response](ADR-058-nonblocking-pic-command-response.md) — async PR= handling that populates the queue

## References

<!-- TODO: populate from inline citations or external sources cited in the body. -->

## Enforcement

```json
{
  "llm_judge": true
}
```
