# ADR-077 Streaming MQTT HA Discovery Architecture

## Status

Accepted

## Context

Home Assistant MQTT auto-discovery emits one retained config message per entity. For OTGW-firmware the set is sizeable: 289 sensors, 53 binary sensors, 2 climate, 1 number, 13 SAT switches, 1 SAT select, plus runtime Dallas sensors. Each config payload is a JSON blob of roughly 300 to 1200 bytes.

The legacy approach (prior to v2.0.0) stored these configs as templates in `src/OTGW-firmware/data/mqttha.cfg`, part of the LittleFS image. A Python generator (`tools/generate_mqttha_progmem.py`, later `generate_mqttha_readable.py`) turned the file into PROGMEM pools (`mqttha_progmem.h` + `mqttha_progmem.cpp`) at build time. At runtime, discovery ran a loop that read each entry, expanded placeholders (`%node_id%`, `%hostname%`, `%mqtt_pub_topic%`, `%source_suffix%`) into a RAM buffer, and published the result.

This approach accumulated problems:

- Large staging buffers on the ESP8266 stack (`sLine[1200]` + `topicBuf[200]`) caused overflow under specific discovery sequences, including the publishToSourceTopic stack overflow fixed in ADR-040 backlog work.
- Passing PROGMEM pointers into functions that expected RAM strings (`printf %s`, `MQTTclient.write()`) caused Exception (3) crashes on ESP8266 Arduino core 3.1.2 due to unaligned 32-bit flash reads. Exception (2) crashes occurred when `strncmp_P` or `strstr_P` were used on binary data (TASK-278 / TASK-279).
- Each reconnect triggered a burst of all ~360 discovery messages, spiking heap fragmentation and TCP pressure on the ESP8266.
- The Python generator added a second build step outside arduino-cli/PlatformIO, with drift risk between `mqttha.cfg` and the generated PROGMEM pool.
- `mqttha.cfg` itself did not support newer entity types (switch, select), so TASK-81 entities were never emitted after the initial refactor to the generator.

Alternatives considered before adopting streaming compose:

1. **Keep mqttha.cfg, fix the bugs.** Would require stack-to-static buffer moves, PROGMEM domain audit, burst throttling. Does not solve the generator drift problem, and the template-expansion loop stays on the critical path.
2. **Compile-time code generation into plain C++ strings.** Removes the runtime templating cost but preserves the large-buffer publish pattern. Still requires a Python build step outside the normal toolchain.
3. **Streaming compose in handwritten C++** (chosen). The config is encoded directly as C++ that builds each JSON body incrementally into a chunked MQTT publish, with no intermediate full-message buffer.

## Decision

Home Assistant MQTT discovery is emitted by streaming compose functions living in `src/OTGW-firmware/MQTTHaDiscovery.cpp`. `data/mqttha.cfg` is archived to `docs/archive/mqttha.cfg` as a historical reference and is not part of the LittleFS image. The Python generators (`tools/generate_mqttha_*.py`) are considered outdated relative to this refactor and are not invoked by the build.

Architecture:

- **`MqttJsonWriter` two-pass writer** (declared in `MQTTstuff.h`). First pass: `MEASURE` mode counts bytes without emitting. Second pass: `WRITE` mode emits via forwarding helpers `writeMqttChunkExt()`, `writeMqttProgmemChunkExt()`, `writeMqttByteExt()`. The byte count from the MEASURE pass is passed to `PubSubClient::beginPublish(topic, len, retain)` so the underlying TCP stack knows the total length up front and does not reallocate.
- **128-byte chunked publish**. `writeMqttChunk()` and `writeMqttProgmemChunk()` push data to the PubSubClient in 128-byte segments with `feedWatchDog()` between chunks. No single buffer holds the full payload.
- **Data-driven tables for bulk entities**. `mqttHaSensors[289]` and `mqttHaBinSensors[53]` (PROGMEM, `MqttHaSensorCfg` / `MqttHaBinSensorCfg` structs) encode per-entity enums (device_class, unit, state_class, icon, entity_category) that decode into JSON fields via `composeSensorPayload` / `composeBinSensorPayload`. Lookup tables `mqttHaSensorIndex[256]` / `mqttHaBinSensorIndex[256]` map OT message ID to the first entry in the sorted array for that ID.
- **Hardcoded streaming functions for special entities**: `streamClimateDiscovery` (2 entries: Thermostat, DHW Control), `streamNumberDiscovery` (1: Toutside override), `streamSatSwitchDiscovery` (13 SAT booleans via shared `streamSatBoolSwitch` helper), `streamSatSelectDiscovery` (1: sat_heating_system), `streamDallasSensorDiscovery` (runtime address-based).
- **Bitmap-driven drip publisher**. `MQTTautoCfgPendingMap[8]` (32 bytes, 256 bits, one per OT message ID) tracks which IDs still need discovery. `markAllMQTTConfigPending()` fills the bitmap on connect/reconnect. `loopMQTTDiscovery()` drips one ID per tick (default 3 s, backs off to 30 s under heap warning). `doAutoConfigureMsgid(OTid)` emits all entities keyed to that ID. Pseudo-IDs: `0` covers climate plus SAT switches and select, `27` covers the number entity.
- **Heap guards**. `STREAM_HEAP_MIN = 4000` bytes before a discovery publish is attempted. `canPublishMQTT()` gates all publishes with tiered heap thresholds. On drip, if heap is below the minimum the entry stays pending and is retried next tick.
- **Context struct for runtime substitutions**. `HaDiscoveryContext` holds `haPrefix`, `nodeId`, `hostname`, `mqttPubTopic`, `mqttSubTopic`, `version`, `isFirstEntity`, device manufacturer and model. Stream functions receive the context by reference and compose directly, no template string is parsed at runtime.

Topic object-ids for SAT switch discovery are derived from the uniq_id suffix at runtime: the leading `-` is stripped and remaining `-` are mapped to `_`. For example `-sat-solar-gain-enable` becomes `sat_solar_gain_enable` in the topic path. This keeps the single helper generic across the 13 switches without per-switch topic constants.

## Consequences

Benefits:

- No large staging buffers. The previous `sLine[1200]` and `topicBuf[200]` are gone. Stack and heap pressure during discovery drop from kilobytes to hundreds of bytes.
- No PROGMEM-to-RAM bridging. Writers accept PROGMEM and RAM sources explicitly via `writeProgmem()` and `writeRam()`, eliminating the Exception (3) class of bugs.
- Discovery respects heap pressure. The drip can stall and resume without dropping entities. Reconnect bursts are spread out by design.
- Adding a new entity type is a single stream function plus a caller. No build step outside the normal arduino-cli/PlatformIO flow.
- `data/mqttha.cfg` is no longer in the LittleFS image, reclaiming flash space and removing a runtime-parsed configuration surface.

Trade-offs:

- Adding a new entity means touching C++ code (`MQTTHaDiscovery.cpp` + declarations in `MQTTstuff.h`), whereas previously a config-file append was enough. Given the real frequency (new entities correspond to new firmware features) this is acceptable.
- The Python generators under `tools/generate_mqttha_*.py` are now dead code relative to the shipped firmware. They are kept as reference for the archived format but should either be removed or explicitly marked deprecated in a follow-up.
- The 2.0.0 refactor initially dropped 13 switch and 1 select entities from TASK-81 because the streaming pipeline had no corresponding stream functions; this was caught and restored in TASK-284. Future entity-type additions (e.g. `button`, `cover`) will require matching streaming functions and caller wiring.
- Pseudo-ID 0 is now overloaded (climate + SAT switch + select). If the drip load under pseudo-ID 0 ever becomes problematic (e.g. 50+ switches), a split into a dedicated pseudo-ID will be required.

Risks:

- Drift between the archived `mqttha.cfg` and the streaming implementation is silent; there is no tool that diffs the two. Mitigation: treat `docs/archive/mqttha.cfg` strictly as historical, do not hand-edit it as if it were still a source of truth.
- The `MqttJsonWriter` two-pass contract depends on the compose lambda being deterministic across MEASURE and WRITE passes. A future change that reads state between the two passes could produce a mismatched length and a truncated publish. This is a testable invariant and should be covered by a review comment in the relevant code.

## Amendment 2026-05-01 (TASK-499 / 1B-M1) — Bounded-payload exception

The two-pass MEASURE-then-WRITE shape above is required for **unbounded**
or large variable-length payloads (the OT-message-id streaming functions
that compose JSON from runtime state of unknown size). For payloads that
are **statically bounded** at compile time, a single-buffer publish via
the same streaming primitives is permitted, provided:

- The bound is small enough to comfortably fit on the stack (rule of
  thumb: ≤ 1 KB; current example uses 768 B).
- `canPublishMQTT()` and `MQTT_DISCOVERY_HEAP_MIN` gate the call so the
  heap-tier deferral semantics still apply.
- A `feedWatchDog()` is called on every return path that follows a
  network attempt (see TASK-496 / 2B-H1).

Canonical example: `satBLEPublishOneDiscovery()` in `MQTTstuff.ino`
publishes one of four discovery configs per call. Each payload is
≤ 600 B (verified by snprintf_P bounds), so the function uses a single
`char payload[768]` stack buffer and writes it as one
`writeMqttChunk()` call. This avoids the two-pass dance for what is a
fixed-size payload while still respecting the heap-safety gates that
ADR-077 was written to enforce.

The exception does NOT extend to:
- Per-MsgID OT JSON streamers (variable structure, must remain two-pass).
- Large device-block aggregations (stay two-pass).

Future bounded-payload helpers should reuse the
`satBLEPublishOneDiscovery` shape rather than re-derive it.

## Related

- Prior ADRs: ADR-004 (no String class in hot paths), ADR-040 (MQTT source-specific topics), ADR-051 (OTGWSettings/OTGWState architecture), ADR-073 (SAT MQTT topic structure), ADR-088 (status-burst windowing producer side that lets this drip coexist with bursty Status traffic).
- Implementation commits on `feature-dev-2.0.0-otgw32-esp32-sat-support`: `2b12834c` (legacy replacement with compact streaming API), `ff159819` (async bitmap-driven drip publisher), `a91220af` (PROGMEM-as-RAM crash fix), `413d8b00` (PROGMEM pool linkage validation guard), `7f663524` / `267fa013` (elimination of `sLine[1200]` and `topicBuf[200]`), `1df3eca5` (discovery burst fix + PROGMEM index), `5384be2a` (TASK-284: restore SAT switch/select streams), `4db9543f` (archive `mqttha.cfg` as reference-only).
- Backlog tasks: TASK-81 (HA number/switch/select entities), TASK-276 / TASK-277 (staging buffer elimination), TASK-278 / TASK-279 / TASK-280 (Exception (2)/(3) crash fixes), TASK-281 (mqttha_progmem readable refactor), TASK-282 (compact array streaming constructors), TASK-283 (boot loop on MQTT connection), TASK-284 (restore SAT switches/select).
- Source files: `src/OTGW-firmware/MQTTHaDiscovery.cpp` (2700+ lines), `src/OTGW-firmware/MQTTstuff.h` (public API), `src/OTGW-firmware/MQTTstuff.ino` (state machine, drip loop, callers), `docs/archive/mqttha.cfg` (historical reference).
- Outdated tooling: `tools/generate_mqttha_data.py`, `tools/generate_mqttha_progmem.py`, `tools/generate_mqttha_readable.py`.
