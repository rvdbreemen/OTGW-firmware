# ADR-041: Memory Headroom Optimization Profile for v1.2.0

**Status:** Accepted  
**Date:** 2026-02-19  
**Decision Maker:** User: Robert van den Breemen

## Context

v1.2.0 introduced additional MQTT source-separation behavior and related reliability fixes. On ESP8266, this increased pressure on static RAM and runtime heap headroom while keeping critical features mandatory:

- OT Log WebSocket streaming must remain available.
- MQTT auto-discovery reliability must remain robust.
- Heap-pressure safety behavior (throttling/blocking in critical state) must stay intact.

Verified baseline after this optimization profile:

- Sketch: `669916` bytes
- Global RAM: `53948` bytes (`27972` bytes remaining)
- IRAM: `29300 / 32768` (`3468` bytes remaining)
- RAM segments:
  - `DATA=1820`
  - `RODATA=14072`
  - `BSS=38056`

## Decision

Adopt a targeted profile that favors deterministic static reductions with minimal behavior risk:

1. Cap WebSocket server clients at 3 (`WEBSOCKETS_SERVER_CLIENT_MAX=3`).
2. Enforce WebSocket payload caps and route all outbound sends through a capped helper with `canSendWebSocket()` gating.
3. Reduce AceTime zone cache to size 2.
4. Reduce `CMSG_SIZE` and OT log aggregation buffer from 512 to 256.
5. Replace duplicated MQTT discovery statics with one shared guarded scratch buffer.
6. Replace source discovery hash table with a 2048-bit Bloom filter per source.
7. Reduce flash-literal MQTT scratch buffer from 1200 to 256 bytes.
8. Constrain MQTT setting field storage (`topTopic`, `haprefix`, `uniqueid`) to practical bounded sizes.
9. Re-size `MQTT_MSG_MAX_LEN` to 1152 after measured worst-case autodiscovery payload analysis.

Measured and derived accounting for this decision set yields `8467 B` static RAM reduction.

## Alternatives Considered

### Alternative 1: Remove or heavily restrict WebSocket logging

- Pros:
  - Larger memory savings possible.
- Cons:
  - Removes required operator workflow and existing UX.
- Rejected because:
  - OT log streaming is a required feature.

### Alternative 2: Aggressive data model compression now (timestamps, queues, etc.)

- Pros:
  - Additional 512B+ savings per item possible.
- Cons:
  - Higher regression risk (timestamp fidelity, queue behavior).
- Rejected because:
  - Current cycle prioritized low-risk memory wins first.

### Alternative 3: Heap-based replacements for large static buffers

- Pros:
  - Lower static footprint.
- Cons:
  - Higher fragmentation risk on ESP8266; less deterministic behavior.
- Rejected because:
  - Deterministic static memory remains preferred for long-lived reliability.

## Consequences

### Positive

- Static RAM pressure reduced significantly while preserving required features.
- WebSocket and MQTT paths are more defensive under heap pressure.
- Auto-discovery memory use is reduced with bounded, explicit behavior.
- Current build retains more free dynamic memory budget (`27972` bytes).

### Negative

- Bloom filter dedupe is probabilistic (false positives possible, though low).
- WebSocket client cap is lower than library default.
- Some future memory savings are deferred to avoid behavior risk.

### Risks and Mitigation

- Risk: Bloom filter false positives skip some discovery publishes.
  - Mitigation: 2048-bit filter size and cache clear on reconnect path.
- Risk: Lower WebSocket client cap may reject additional browser sessions.
  - Mitigation: explicit runtime limit + clear debug logging.
- Risk: Smaller MQTT flash-literal buffer truncation.
  - Mitigation: explicit warning log on truncation path.
- Risk: Tightened MQTT field limits may reject very long custom values.
  - Mitigation: explicit API/UI max lengths derived from `sizeof()` and documented limits.

## Related Decisions

- ADR-004 (Static Buffer Allocation Strategy)
- ADR-005 (WebSocket for Real-Time Streaming)
- ADR-006 (MQTT Integration Pattern)
- ADR-009 (PROGMEM Usage)
- ADR-015 (NTP and AceTime Time Management)
- ADR-030 (Heap Memory Monitoring and Emergency Recovery)
- ADR-040 (MQTT Source-Specific Topics)

## References

- `docs/reviews/2026-02-19_memory-optimization-follow-up/README.md`
- `docs/reviews/2026-02-19_memory-optimization-follow-up/MEMORY_OPTIMIZATION_OPTIONS.md`
- `docs/reviews/2026-02-19_memory-optimization-follow-up/DECISIONS_IMPLEMENTED.md`
- `docs/reviews/2026-02-19_memory-optimization-follow-up/DEVELOPER_FINAL_REPORT.md`
- `src/OTGW-firmware/MQTTstuff.ino`
- `src/OTGW-firmware/networkStuff.h`
- `src/OTGW-firmware/webSocketStuff.ino`
- `src/OTGW-firmware/OTGW-firmware.h`
