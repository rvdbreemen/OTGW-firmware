# v1.2.0 Memory Decisions Implemented

This document records the selected issues, final decisions, and gains for the current v1.2.0 memory pass.

Legend:

- `Measured`: differential compile performed in current tree.
- `Derived`: exact byte math from old/new static layouts or symbol sizes.

## D1. WebSocket Controls (Selected Options 1/2/3)

- Decision:
  - Keep `WEBSOCKETS_MAX_DATA_SIZE=256`.
  - Enforce library client cap via `WEBSOCKETS_SERVER_CLIENT_MAX=3`.
  - Enforce app-level payload cap and route outgoing sends via `broadcastWebSocketCapped()` with `canSendWebSocket()` gating.
- Files:
  - `src/OTGW-firmware/networkStuff.h`
  - `src/OTGW-firmware/webSocketStuff.ino`
- Gain:
  - `408 B` RAM saved vs client cap 5 (`Measured`: global `53948 -> 54356` when probed to 5).
- Rationale:
  - Keeps OT Log WebSocket feature while reducing static WS footprint and runtime pressure.

## D2. Shared MQTT Autodiscovery Scratch + Guard

- Decision:
  - Replace duplicated function-local static buffers with one shared guarded scratch object.
- Files:
  - `src/OTGW-firmware/MQTTstuff.ino`
- Gain:
  - Old layout:
    - `doAutoConfigure`: `sLine[1200] + sTopic[200] + sMsg[1200] = 2600 B`
    - `doAutoConfigureMsgid`: `buffer[1200+200+1200] = 2600 B`
    - Total: `5200 B`
  - New layout:
    - `mqttAutoCfgScratch`: `1353 B` (current symbol size with `MQTT_MSG_MAX_LEN=1152`)
  - Net: `3847 B` (`Derived`).
- Rationale:
  - Largest deterministic static reduction in this area with minimal behavioral risk.

## D3. Source Discovery Dedupe: 2048-bit Bloom Filter

- Decision:
  - Replace open-addressed hash table with per-source 2048-bit Bloom filter.
- Files:
  - `src/OTGW-firmware/MQTTstuff.ino`
- Gain:
  - Old table: `3 * 192 * 4 = 2304 B`
  - New filter: `3 * (2048 / 8) = 768 B`
  - Net: `1536 B` (`Derived`).
- Rationale:
  - Strong RAM reduction with acceptable false-positive probability at selected size.

## D4. Reduce MQTT Flash-Literal Scratch Buffer

- Decision:
  - `sendMQTTData(F(topic), F(payload), ...)` scratch reduced from `1200` to `256`.
- Files:
  - `src/OTGW-firmware/MQTTstuff.ino`
- Gain:
  - `944 B` RAM saved (`Measured`: global `53948 -> 54892` when probed back to 1200).
- Rationale:
  - This path carries short status/error literals; hard cap + warning keeps behavior safe.

## D5. Reduce AceTime Zone Cache to 2

- Decision:
  - `CACHE_SIZE` reduced `3 -> 2`.
- Files:
  - `src/OTGW-firmware/OTGW-firmware.h`
- Gain:
  - `592 B` RAM saved for one cache entry (`Measured`: `2 -> 1` probe gave global `53948 -> 53356`; `3 -> 2` is the same one-entry delta).
- Rationale:
  - Good static win with low observed functional risk.

## D6. Reduce Generic Logging/Command Buffers

- Decision:
  - `CMSG_SIZE 512 -> 256`.
  - `OT_LOG_BUFFER_SIZE 512 -> 256`.
- Files:
  - `src/OTGW-firmware/OTGW-firmware.h`
  - `src/OTGW-firmware/OTGW-Core.ino`
- Gain:
  - `512 B` RAM saved (`Measured` in prior differential build pass).
- Rationale:
  - Aligned with WebSocket payload cap and observed OTGW line sizes.

## D7. Constrain MQTT Setting Field Storage

- Decision:
  - `MQTT_TOP_TOPIC_MAX_CHARS=12`
  - `MQTT_HA_PREFIX_MAX_CHARS=16`
  - `MQTT_UNIQUE_ID_MAX_CHARS=25`
- Files:
  - `src/OTGW-firmware/OTGW-firmware.h`
  - `src/OTGW-firmware/restAPI.ino` (max lengths derived from `sizeof`)
- Gain:
  - `68 B` RAM saved (`Measured`: global `53948 -> 54016` when probed back to 40/40/40).
- Rationale:
  - Matches practical use and keeps explicit headroom for custom unique IDs.

## D8. Re-size Autodiscovery Payload Buffer to Measured Worst Case

- Decision:
  - `MQTT_MSG_MAX_LEN 1200 -> 1152`.
  - Selected after validating worst expanded `mqttha.cfg` payload (`1123 B`) and keeping margin.
- Files:
  - `src/OTGW-firmware/MQTTstuff.ino`
- Gain:
  - `48 B` RAM saved (`Measured`: global `53948 -> 53996` when probed back to 1200).
- Rationale:
  - Robust minimal sizing based on real data, not guesswork.

## D9. Reliability/Compatibility Constraints Preserved

- OT Log WebSocket feature remains enabled.
- Source-separated MQTT behavior remains enabled by default.
- Heap-pressure throttling/backpressure paths remain in place.

## Aggregate Impact

Selected-set static RAM reduction:

- D1: `408 B` (Measured)
- D2: `3847 B` (Derived)
- D3: `1536 B` (Derived)
- D4: `944 B` (Measured)
- D5: `592 B` (Measured)
- D6: `512 B` (Measured)
- D7: `68 B` (Measured)
- D8: `48 B` (Measured)

Total: `8467 B` (mixed measured+derived exact byte accounting).
