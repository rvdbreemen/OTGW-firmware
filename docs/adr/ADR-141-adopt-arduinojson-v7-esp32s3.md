---
id: ADR-141
title: Adopt ArduinoJson v7 for JSON on the ESP32-S3 2.0.0 line
status: Accepted
date: 2026-06-15
tags: [json, arduinojson, mqtt, ha-discovery, rest, esp32s3, memory]
supersedes: [ADR-042]
superseded_by: []
related: [ADR-042, ADR-018, ADR-128, ADR-140, ADR-089, ADR-077, ADR-131]
deciders: [Robert van den Breemen]
---

# ADR-141: Adopt ArduinoJson v7 for JSON on the ESP32-S3 2.0.0 line

## Status

Accepted, 2026-06-15 (Proposed 2026-06-15; accepted by the maintainer 2026-06-15).
**Supersedes ADR-042** (Streaming JSON I/O — No ArduinoJson). ADR-042 stays immutable;
a "Superseded by ADR-141" status line is added to it (the sanctioned immutability
exception), its body unedited.

Guideline-level (per ADR-080) until the mechanical gate flips: ADR-042's ban is
enforced today by `evaluate.py::check_no_arduinojson`. The Definition of Done of the
implementing task (TASK-867) **removes/relaxes that gate** so ArduinoJson is permitted
on the ESP32-S3 line; until then this ADR is a decision of record, not yet CI-enforced.

## Status History

status_history:
  - date: 2026-06-15
    status: Accepted
    changed_by: maintainer (Robert van den Breemen)
    reason: Reverse the no-ArduinoJson stance for the ESP32-S3-only 2.0.0 line. ADR-128 dropped ESP8266 (the RAM/fragmentation premise of ADR-042); the ESP32-S3 has 300 KB+ heap. The hand-rolled two-pass MqttJsonWriter is the source of review findings F1 (MEASURE/WRITE desync) and F5 (manual string-escaping gap). Both ESP32 reference projects (OT-Thing, EMS-ESP32) use ArduinoJson v7. Adopt it, starting with the MQTT HA-discovery path.
    changed_via: manual

## Context

ADR-018 adopted ArduinoJson 6.x; ADR-042 (2026-02-28) then **banned** it in favour of a
hand-rolled streaming/`snprintf_P` approach, because on the ESP8266 (~40 KB usable heap)
the elastic document caused RAM pressure and heap fragmentation. That premise has since
changed:

1. **ESP8266 is gone.** ADR-128 dropped ESP8266 support from 2.0.0; the line is
   ESP32-S3-only, with 300 KB+ heap and (on some boards) PSRAM. The RAM/fragmentation
   argument that motivated ADR-042 no longer holds on the target hardware.
2. **The hand-rolled path has cost.** The MQTT HA-discovery builder uses a two-pass
   `MqttJsonWriter` (MEASURE then WRITE) because PubSubClient/`beginPublish` needed the
   length up front. The 2.0.0 MQTT review found this two-pass is the root of **F1** (the
   `deviceIntroduced[]` MEASURE-pass mutation desync that drops the device block / first
   entity) and that the manual writer never escaped device strings (**F5**).
3. **Both ESP32 reference projects use ArduinoJson v7.** OT-Thing-OTGW32 and EMS-ESP32
   (the mature ESP32 blueprint) build discovery/state JSON with ArduinoJson v7 and a
   `measureJson` + reserve/serialize publish pattern.
4. **espMqttClient frames atomically** (ADR-131): a publish is a single
   buffer-it-then-send call, which fits ArduinoJson's `measureJson` → allocate →
   `serializeJson` → publish → free pattern cleanly.

## Decision

**Adopt ArduinoJson v7 for firmware JSON building on the ESP32-S3 2.0.0 line**, replacing
the hand-rolled streaming/two-pass approach. ADR-042 is superseded for this line.

1. **Dependency.** Add `bblanchon/ArduinoJson @ ^7` (latest 7.x) to the ESP32 envs in
   `platformio.ini`.
2. **Build pattern (atomic publish on espMqttClient).** Build a `JsonDocument`, then
   `size_t n = measureJson(doc)`, allocate exactly `n` (or a reserved buffer),
   `serializeJson(doc, buf, n)`, publish via the `mqttPublishRaw` chokepoint, free.
   **Gate the allocation behind the ADR-089 heap tier** (refuse/defer under heap pressure,
   matching the EMS-ESP32 pattern).
3. **Scope and order.** MQTT HA-discovery building first (highest value — F1/F5 live
   there), then the MQTT state/source publishes, then REST JSON building/parsing
   (`parseJsonKVLine` inbound can move to `deserializeJson`). This ADR records the
   direction; the discovery rewrite lands with ADR-140 (single-device) under TASK-867.
4. **Removal.** Delete the `MqttJsonWriter` two-pass scaffold and its
   `writeRam`/`writeProgmem`/`writeJsonKV` helpers. **F1** disappears (no hand-rolled
   two-pass) and **F5** disappears (ArduinoJson escapes string values automatically).
5. **Gate.** Remove/relax `evaluate.py::check_no_arduinojson` as part of TASK-867's
   Definition of Done, so the ban no longer fails the ESP32-S3 build.

## Alternatives Considered

### Alternative A: Keep the hand-rolled two-pass writer (status quo, ADR-042)
Rejected. It keeps the F1 determinism bug and the F5 escaping gap, and the two-pass
length-measure complexity, for a RAM constraint that no longer exists on ESP32-S3.

### Alternative B: Fix F1/F5 inside the two-pass writer, keep the ban
Viable and in fact done for the single-device rewrite (ADR-140 fixes F1 by a driver-set
first-entity gate and F5 by manual escaping). But it preserves the hand-rolled
complexity long-term; the maintainer chose to converge on the library both reference
projects use rather than maintain a bespoke writer.

### Alternative C: Adopt ArduinoJson v7 (chosen)
Matches the reference projects, removes F1/F5 by construction, and is affordable on the
ESP32-S3 heap.

## Consequences

**Benefits**
- Removes F1 and F5 by construction (no hand-rolled two-pass; automatic escaping).
- Aligns with the OT-Thing and EMS-ESP32 reference implementations.
- Simpler discovery/state/REST JSON code; deletes the `MqttJsonWriter` scaffold.

**Trade-offs**
- ArduinoJson v7 keeps an elastic heap-resident document while building. Affordable on
  ESP32-S3 (300 KB+), bounded by the per-config worst case (~900 B) and freed immediately
  after publish; still gated by the ADR-089 heap tier. PSRAM, where present, can back the
  allocator later (EMS-ESP32 `AllocatorPSRAM`).
- Flash cost of the library (watch the esp32-combo build headroom).
- ADR-042's ban is reversed for this line only; the decision is recorded as a supersession,
  not an edit to ADR-042.

**Risks and mitigations**
- *Risk*: entity-set regression during the discovery rewrite. *Mitigation*: the golden
  test `tests/webui/ha-discovery-golden.test.mjs` asserts entity-set parity before/after.
- *Risk*: heap spike on a full discovery republish. *Mitigation*: the ADR-089 tier defers
  under pressure; `measureJson`-sized allocation is freed immediately after publish.

## Related Decisions

- **ADR-042 (No ArduinoJson)**: **superseded by this ADR** for the ESP32-S3 line.
- **ADR-018 (ArduinoJson for Data Interchange)**: the original adoption ADR-042 reversed;
  this ADR returns to a (newer, v7) ArduinoJson.
- **ADR-128 (Drop ESP8266 from 2.0.0)**: the premise change that makes this affordable.
- **ADR-140 (Single-Device HA Topology)**: the discovery rewrite that consumes ArduinoJson
  (one `JsonDocument` per entity with a shared device block).
- **ADR-089 (Heap tier machine)**: the gate the allocate/serialize path sits behind.
- **ADR-131 (espMqttClient async engine)**: the atomic-publish model ArduinoJson serializes into.
- **ADR-077 (Streaming MQTT HA Discovery)**: the two-pass MEASURE/WRITE contract this removes.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is guideline-level until
  the `evaluate.py` ban is replaced (TASK-867 DoD).

## References

- Review findings folded in: F1 (`MQTTHaDiscovery.cpp` MEASURE-pass `deviceIntroduced[]`
  mutation), F5 (unescaped hostname/manufacturer/model in the device block).
- Reference pattern: EMS-ESP32 `src/core/mqtt.cpp` (ArduinoJson v7 + `measureJson`/reserve
  + heap-gate); OT-Thing-OTGW32 `Firmware/src/mqtt.cpp` (ArduinoJson v7 discovery).
- Change surface: `platformio.ini` (lib_deps), `src/OTGW-firmware/MQTTHaDiscovery.cpp`,
  `src/OTGW-firmware/MQTTstuff.{ino,h}` (`MqttJsonWriter` deletion), `evaluate.py`
  (`check_no_arduinojson` removal). Implementation tracked under TASK-867.
