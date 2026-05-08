# ADR-101: Flat Per-Value MQTT Topics Over Aggregated JSON Payloads

## Status

Proposed. Awaiting acceptance by Decision Maker.

**Decision Maker:** User: Robert van den Breemen (rvdbreemen) — explicit design choice stated 2026-05-08.

## Context

OTGW-firmware 2.0.0 targets the OTGW32 hardware (ESP32-S3 with direct GPIO OpenTherm, no PIC microcontroller). The firmware publishes OpenTherm sensor values, SAT control state, BLE sensor readings, S0 pulse counts, and network diagnostics over MQTT. There are two broad strategies for structuring these MQTT messages:

1. **Flat per-value topics** — one topic per sensor/control value (e.g. `OTGW/boilertemp` = `64.5`). This is the approach taken by the firmware today: 385 sensor topics, 58 binary sensor topics, each a plain scalar payload. HA auto-discovery is implemented by publishing a separate JSON config message on `homeassistant/sensor/<nodeid>/boilertemp/config` that supplies metadata (icon, unit, device_class, entity_category). The value topic itself carries only the scalar.

2. **Aggregated JSON payloads** — one or a few topics carry JSON blobs combining multiple values (e.g. `<base>/state` = `{"slave":{"flow_t":64.5,"flame":true,…},"thermostat":{"ch_set_t":20.0,…}}`). This is the approach used by OT-Thing (Seegel Systeme), Tasmota (`tele/<device>/SENSOR` JSON), and several other IoT firmware projects.

A third-party audit of the SAT Python integration (thermo-nova branch) in 2026-05-08 identified that the SAT OT-Thing coordinator subscribes to a single `<base>/state` topic with nested `slave`/`thermostat`/`status` JSON, and would not receive data from OTGW-firmware 2.0.0 as-is. This raised the question whether the firmware should add an OT-Thing-compatible JSON publish path.

The firmware owner explicitly declined, stating: *"I deliberately choose flat topics. That is deliberate — I want the auto-discovery to be very clear. I decide NOT to support the OT-Thing MQTT approach."*

This ADR documents and ratifies that decision, and defines it as a binding constraint for future contributors.

## Decision

**OTGW-firmware 2.0.0 uses flat per-value MQTT topics. Each sensor or control value is published as a plain scalar on its own dedicated topic. Aggregated JSON payloads on value topics are not permitted.**

HA auto-discovery metadata (icon, unit, device_class, state_class, entity_category, template expressions) is published separately on `homeassistant/#` config topics using the existing streaming discovery architecture (ADR-077). The value topics themselves carry only the scalar, never JSON.

This applies to all MQTT publish paths: OpenTherm sensor values, SAT control state, OTDirect state, BLE sensor readings, S0 pulse counts, Ventilation readings, Dallas 1-Wire temperatures, and firmware diagnostics.

Exceptions: HA auto-discovery config topics (`homeassistant/sensor/…/config`, `homeassistant/binary_sensor/…/config`, etc.) always carry JSON. This is required by the HA MQTT discovery protocol and is not a value topic.

## Alternatives Considered

### Alternative A: OT-Thing nested JSON state topic

Publish all OpenTherm state on a single `<base>/state` topic as a nested JSON object with `slave` and `thermostat` sub-objects, matching the OT-Thing coordinator contract expected by the SAT Python integration's OT-Thing mode.

**Why not chosen:**
- Requires an HA-side SAT coordinator plugin (Python custom component) to parse the JSON and expose entities to HA. Without the plugin, the data is invisible to HA unless the user writes manual MQTT sensor templates.
- The firmware's flat topic + HA auto-discovery approach achieves zero-configuration HA integration without any third-party plugin. Adding a JSON path would create two parallel publish systems with different entity sets visible in different HA configurations.
- A SAT user can configure SAT in OpenThermGateway-MQTT mode instead of OT-Thing mode. The firmware already speaks the OT prefix format (`CS=`, `SW=`, `PM=`, `MM=`, `CH=`, `TC=` via OTDirect PIC-command compatibility layer). This gives SAT connectivity without requiring the nested JSON path.
- OT-Thing is a third-party firmware with its own design choices. OTGW-firmware is an independent firmware with its own identity; it is not obligated to mimic OT-Thing's wire format.

### Alternative B: Tasmota-style SENSOR JSON

Publish all related sensors grouped into a JSON object on a `tele/<device>/SENSOR` topic (e.g. `{"OpenTherm":{"BoilerTemp":64.5,"FlameActive":true,…}}`), matching the Tasmota convention.

**Why not chosen:**
- Tasmota JSON requires the user to write HA value_template expressions (e.g. `{{ value_json.OpenTherm.BoilerTemp }}`) for every entity. This places the burden on the user and removes the zero-configuration benefit.
- The firmware supports 385+ individual sensor entities. Encoding all of them into a single JSON document would produce a large payload (~8 KB) on every publish tick, with most values unchanged. Flat topics allow publish-on-change per sensor, significantly reducing broker traffic.
- No native HA auto-discovery support for sub-keys within a JSON blob; each entity still requires a manual sensor config referencing the parent topic and a value_template.

### Alternative C: Keep flat topics (chosen)

Continue publishing one scalar per MQTT topic. HA auto-discovery handles all metadata. This is the current implementation and the one being ratified here.

## Consequences

**Benefits**

- Zero-configuration HA integration: all 385 sensors, 58 binary sensors, switches, and number entities appear automatically in HA after the firmware publishes discovery payloads. No user-side template authoring required.
- Minimal broker traffic on stable state: only changed values are published. A boiler at steady state (temperature stable, flame on, no mode change) may publish only 1-2 topics per minute, not a full JSON blob every poll cycle.
- HA entity metadata is rich and firmware-controlled: icons (`mdi:thermometer`), entity_category (`diagnostic`), state_class (`measurement`), unit_of_measurement, device_class — all set by the firmware, not by the user. Verified by the discovery verification system (ADR-062).
- Topic shape is human-readable and debuggable with any MQTT client (MQTT Explorer, mosquitto_sub). No JSON parsing required to read a value.
- Enforced by the existing discovery architecture (ADR-077) and topic shape ADRs (ADR-096, ADR-097, ADR-098).

**Trade-offs**

- SAT Python integration OT-Thing coordinator mode is incompatible with this firmware. SAT users must use the OpenThermGateway-MQTT coordinator mode instead. (This mode is supported via the OTDirect PIC-command compatibility layer.)
- 385 retained topics in the broker consume more broker storage than a handful of JSON topics. On a home Mosquitto broker this is negligible (< 50 KB).
- Any consumer that wants all OpenTherm state at once must subscribe to a wildcard (`OTGW/#`) rather than a single topic.

**Risks and mitigations**

- *Risk:* A future contributor adds a JSON payload to a value topic for convenience (e.g. to bundle related values). *Mitigation:* This ADR is binding. The pre-commit `adr-judge` Enforcement block (below) detects JSON construction patterns in MQTT publish paths and blocks the commit.
- *Risk:* A new OT bus feature (e.g. a complex multi-field MsgID) tempts a JSON bundle. *Mitigation:* Each field of the MsgID maps to its own topic, consistent with the existing 385-topic model. Any new MsgID follows the same pattern.

## Related Decisions

- **ADR-077 (Streaming MQTT HA Discovery Architecture)**: Defines the streaming publish model for auto-discovery config topics. This ADR is complementary: ADR-077 governs the `homeassistant/` config topic shape; ADR-101 governs the value topic shape.
- **ADR-096 (MQTT Source-Subtopic Worldview Semantics)**: Defines the semantic meaning of `/thermostat` and `/boiler` subtopics. ADR-101 governs whether those subtopics carry scalars or JSON (scalars only).
- **ADR-097 (MQTT Source Topic Sibling Suffix Shape)**: Defines the suffix naming convention for source-separated topics. Governed by ADR-101 in terms of payload.
- **ADR-098 (MQTT Discovery Topic Sibling Suffix Shape)**: Defines the discovery topic shape for source-variant entities. The discovery payload is JSON (required by HA); the value topic is scalar (required by ADR-101).

## References

- SAT Python audit, 2026-05-08: `docs/guides/SAT_AUDIT_REPORT.md` (pending creation)
- SAT OT-Thing coordinator topic contract: `other-projects/SAT-releases-thermo-nova/custom_components/sat/coordinator/mqtt/otthing.py:153-198`
- Firmware MQTT publish entry point: `src/OTGW-firmware/MQTTstuff.ino`
- HA auto-discovery streaming: `src/OTGW-firmware/MQTTHaDiscovery.cpp`
- TASK-592 (archived): feat(mqtt) OT-Thing SAT interop — archived 2026-05-08 per this design decision

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "sendMQTTData\\([^)]*\\{[^)]*\\)",
      "path_glob": "src/**/*.{ino,cpp,h}",
      "message": "MQTT value topics must carry plain scalars, not JSON objects. See ADR-101."
    },
    {
      "pattern": "snprintf_P?\\([^;]+\\{[^;]+\\}[^;]+sendMQTTData",
      "path_glob": "src/**/*.{ino,cpp,h}",
      "message": "Do not construct JSON in a buffer and publish it to a value topic. See ADR-101."
    }
  ],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": true
}
```
