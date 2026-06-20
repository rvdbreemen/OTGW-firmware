# ADR-140: Single-Device HA Discovery Topology with Source-Prefix Entity Clustering in One Device (align 2.0.0 with the 1.6.x single-device model)

## Status

**Amended by [ADR-148](ADR-148-ble-sensors-as-ha-child-devices-via-device.md) (2026-06-20):** BLE probes only: each BLE probe becomes its own HA child-device linked by `via_device`; §1 "no via_device" and §2 "BLE sensors inside the single device" are narrowed for BLE probes. All non-BLE entities stay inside the single OTGW device. This status line is the sanctioned immutability exception (ADR body below is unedited).

Accepted, 2026-06-15 (Proposed 2026-06-15; accepted by the maintainer 2026-06-15).
Guideline-level (per ADR-080): the discovery payload shape is verified by field
validation (a captured discovery dump on a real device), not by a clean
`evaluate.py` forbid/require pattern. **Supersedes ADR-124** (Seven-Device
Topology) and, transitively, the multi-device line ADR-124 sits on (ADR-122
five-device). On acceptance (applied now), a "Superseded by ADR-140" status line
is added to ADR-124 (the sanctioned immutability exception); ADR-124's body is not
edited.

**Acceptance note (2026-06-15):** acceptance incorporates the maintainer's naming
directive — the in-device clustering is **five source/engine prefixes**
(`esp_`, `pic_`, `otd_`, `sat_`, `sensors_`), NOT the seven functional categories
(no `boiler_` / `thermostat_` / `gateway_` prefixes). §2 below is the accepted
clustering scheme; the original Proposed draft's "seven categories" wording is
superseded by it within this same ADR (the revision was applied while Proposed,
before the flip to Accepted).

## Status History

status_history:
  - date: 2026-06-15
    status: Proposed
    changed_by: Agent
    reason: After field testing, revert the 2.0.0 multi-device HA topology (ADR-124 seven-device split with via_device hub) back to ONE device per hardware OTGW with entity_category clustering, matching the 1.6.x/dev model. The multi-device layout is confusing in practice; the seven-device path is also the source of the F1 two-pass determinism bug (ADR-077 Risks materialized). Hard-remove the seven-device code; single-device becomes the only model.
    changed_via: manual
  - date: 2026-06-15
    status: Accepted
    changed_by: maintainer (Robert van den Breemen)
    reason: Accepted with the maintainer naming directive folded into §2 — in-device clustering uses five source/engine prefixes (esp_/pic_/otd_/sat_/sensors_), not the seven functional categories. Boiler/thermostat disambiguation moves to the 1.x bilateral _boiler/_thermostat suffix; gateway/OT-core fold onto the active engine prefix (pic_ or otd_). Supersedes ADR-124.
    changed_via: manual

## Context

The 2.0.0 line diverged from the 1.6.x/dev MQTT Home Assistant discovery topology.
Where 1.6.x publishes **one** HA device per hardware OTGW and clusters entities with
`entity_category`, 2.0.0 progressively split the entities across **multiple** devices:
ADR-122 (five devices), then ADR-124 (seven devices: Boiler, Thermostat, Gateway,
Esp, OtCore, Sat, Sensors) with a `via_device` hub pointing every non-Gateway device
at `<nodeId>-gateway`.

Two problems surfaced.

1. **Field testing found the multi-device layout confusing.** Splitting one physical
   gateway into seven HA devices fragments the entities, multiplies device cards, and
   makes the relationship between values (e.g. a boiler flow temperature and the
   thermostat room setpoint that drives it) harder to read, not easier. The maintainer
   wants to return to one device per hardware OTGW with a clear logical grouping inside
   it, as 1.6.x does.

2. **The seven-device path carries a live correctness bug (review finding F1).** The
   per-device first-entity tracking array `deviceIntroduced[]` is mutated **inside the
   compose lambda during the MEASURE pass** (`MQTTHaDiscovery.cpp:2373`), which the
   two-pass `measureMallocPublish` (MEASURE then WRITE) reads on the WRITE pass. The
   WRITE pass then emits the minimal device block, `byteCount != measured len`, and the
   publish fails. Net effect in modern mode (the default): the full device block
   (manufacturer/model/name/via_device) is never emitted and the first entity of each
   multi-entity msgID is dropped. This is exactly the two-pass-determinism hazard ADR-077
   names in its Risks section. The legacy single-device path is immune by construction
   because its first-entity gate is **driver-set** (set by the caller, not mutated inside
   compose).

**Home Assistant grouping reality.** HA has no native sub-grouping inside a device beyond
`entity_category`. One device renders as one entity list with at most three sections:
primary, **Diagnostic** (`entity_category: diagnostic`), and **Configuration**
(`entity_category: config`). Richer card layouts (a "Central Heating" card next to a
"DHW" card) are a Lovelace dashboard the user builds, fed by naming / `device_class` /
labels; MQTT discovery cannot author them. So "logical clustering like 1.6.x" precisely
means: one device + `entity_category` (three cards) + `device_class`/`unit`/`state_class`
typing + self-describing entity names. This is verified in the dev tree
(`mqtt_configuratie.cpp:1926-1948` single device block; `entity_category` emit at
`:2070-2071`/`:2148`; no `via_device` anywhere in dev `src/OTGW-firmware`).

## Decision

**2.0.0 publishes ONE HA device per hardware OTGW and clusters entities with
`entity_category`, matching the 1.6.x model. The seven-device split is removed.**

**Mental model (the invariant): one hardware device = one IP address = one MQTT
device in Home Assistant.** The HA device identity is the per-hardware `nodeId`
(derived from the device, stable across reboots); there is exactly one HA device
per physical OTGW, never more. Everything the gateway exposes (boiler values,
thermostat values, gateway/ESP diagnostics, SAT, sensors) lives as entities under
that single device, sorted into cards by `entity_category`.

Concrete shape:

1. **One device.** Every entity carries the identical `"dev":{"identifiers":<nodeId>}`,
   so HA folds them into a single device. The full device block (`manufacturer`,
   `model`, `name = "OpenTherm Gateway (<hostname>)"`, `sw_version`) is emitted only on
   the first entity of a republish cycle; subsequent entities emit the bare
   `identifiers`. No `via_device`, no multi-device split, no per-device suffix or
   metadata.

2. **Clustering = five source/engine prefixes inside the one device (maintainer
   directive 2026-06-15).** Every entity is prefixed by the SOURCE/ENGINE that produced
   it, drawn from exactly five values — and the functional-role labels `boiler_`,
   `thermostat_`, `gateway_` are **NOT** used as prefixes:

   | Prefix | Source / engine |
   |---|---|
   | `esp_` | ESP32 firmware + diagnostics (heap, wifi, uptime, version, …) |
   | `pic_` | OpenTherm values mediated by the OTGW Classic **PIC** firmware |
   | `otd_` | OpenTherm values produced by the **OTDirect** engine (OTGW32) |
   | `sat_` | Smart Autotune Thermostat subsystem, **including its BLE sensors** |
   | `sensors_` | on-board hardware sensors (Dallas DS18B20, S0 pulse counter) |

   The prefix is the engine/source, not the OpenTherm direction. Exactly one OT command
   interface is active per build — `pic_` on the esp32-classic PIC build, `otd_` on the
   esp32/OTGW32 OTDirect build, resolved at boot on esp32-combo — so every OT value
   carries `pic_` or `otd_` accordingly. The former Boiler / Thermostat / Gateway /
   OT-Core groupings all collapse onto the single source prefix of the active engine.
   `deviceForOTId` and the `HaDevice` enum are RETAINED but repurposed to select this
   source prefix instead of a device.

   **Boiler vs thermostat disambiguation** (the bilateral case — one OT value reported by
   both the thermostat side and the boiler side) is preserved by the existing 1.x
   bilateral **suffix** `_boiler` / `_thermostat` on the entity id, NOT by a prefix. Both
   readings stay distinct under the same source prefix without reintroducing a role prefix.

   **Sensor recognizability (maintainer directive):** Dallas probes are `sensors_`-prefixed
   with the probe address (or its configured label) in the entity name and a stable
   per-address `unique_id`, so each physical probe is individually identifiable and
   re-discovery does not duplicate. BLE sensors are `sat_`-prefixed and keep a `BLE` token
   in the entity name (e.g. `sat_ble_temp` → "SAT BLE Temp").

   **HA constraint (why a name prefix, not native sections):** Home Assistant has only
   THREE native within-device sections (primary / Configuration / Diagnostic, via
   `entity_category`). More native collapsible sections are not possible. The five source
   prefixes are therefore a naming-prefix grouping (and optionally an HA label per source
   for dashboard filtering). `entity_category` (`config` for writable settings,
   `diagnostic` for fault/counter/connectivity/PIC-settings readbacks) remains an
   ORTHOGONAL secondary layer that drops those entities into HA's native Config/Diagnostic
   sections where it adds value, independent of the source prefix.

3. **Typing.** `device_class` + `unit_of_measurement` + `state_class` are set per entity
   for icon/unit/precision/statistics. Names are self-describing `friendlyName`
   (`_` to space, Title Case at emit) with the hostname dropped from entity names (the
   device card title already carries it).

4. **First-entity gate is driver-set, not compose-mutated.** The single-device emit uses
   the legacy-style caller-set `isFirstEntity`, so the MEASURE pass never mutates state
   the WRITE pass reads. This removes finding F1 by construction.

5. **Device-string escaping.** `hostname`, `manufacturer` and `model` are JSON-escaped
   when written into the device block (finding F5). This is mandatory in the same change:
   once the single device block reliably emits, an unescaped quote/backslash in the
   hostname would break the device-metadata JSON, which F1 currently masks.

6. **Hard removal of the multi-DEVICE emission, not the classification.** The
   seven-DEVICE output is deleted: the seven separate `dev` blocks, `via_device`, the
   per-device suffix/metadata tables, and the `deviceIntroduced[]` per-device array all
   go. The `HaDevice` seven-value enum and the `deviceForOTId` map are RETAINED but
   repurposed to drive the source name-prefix (point 2) instead of device selection.
   The bilateral boiler/thermostat handling is kept (a single OT value reported by both
   sides still yields two distinct entities), now disambiguated by the `_boiler` /
   `_thermostat` SUFFIX under the same source prefix rather than by a second device or a
   role prefix. Single-device is the only topology;
   the former `bLegacyMode` topology axis is retired (it was never persisted or parsed).
   The orthogonal topic-naming axis `bUseLegacyOtTopics` (ADR-106) is untouched.

## Alternatives Considered

### Alternative A: Keep the seven-device topology (status quo, ADR-124)

Retain the multi-device split and the `via_device` hub.

Rejected. Field testing found it confusing, and it carries the F1 two-pass bug. The
extra structure does not help users read related values and fragments one physical
device into seven HA devices.

### Alternative B: Make single-device the default but keep seven-device behind a toggle

Default to single-device, keep the seven-device emit reachable via a flag as a fallback.

Rejected. It keeps the entire seven-device code path (and the F1 bug, which would then
still need a separate fix in the modern branch), doubles the discovery test surface, and
preserves dead-by-default complexity for a layout the maintainer has decided against. The
KISS choice is to remove it.

### Alternative C: Single-device hard removal (chosen)

Remove the seven-device code; single-device with `entity_category` clustering is the only
model. Simplest, matches the proven 1.6.x model, and eliminates F1 by adopting the
driver-set first-entity gate.

## Consequences

**Benefits**
- Adopts the proven 1.6.x single-device topology (one device per hardware OTGW, easy to
  read), while preserving entity grouping via five source/engine prefixes
  (`esp_`/`pic_`/`otd_`/`sat_`/`sensors_`) so no source information is lost.
- Removes finding F1 by construction (driver-set first-entity gate, no compose mutation).
- Net code reduction: the `HaDevice` split, `deviceForOTId`, bilateral two-pass,
  `via_device`, per-device metadata, and `deviceIntroduced[]` all go.
- Folds in F5 (device-string escaping) at the one point the device block is built.

**Trade-offs**
- Within-device grouping is limited to the three `entity_category` buckets
  (primary/Diagnostic/Configuration). Finer "card" layouts are a Lovelace dashboard the
  user builds; discovery cannot emit them. This is the same constraint 1.6.x lives with.
- Existing installs that already received the seven-device discovery will have stale
  multi-device entities in HA after the change. Those become orphans and need a one-time
  manual cleanup (delete the old devices), the same re-home caveat ADR-124 itself
  carried. A retained-discovery clear on the old per-device topics is emitted where
  feasible to reduce orphans.

**Risks and mitigations**
- *Risk*: the discovery payload shape regresses (entities fail to bind, wrong card).
  *Mitigation*: field-validate a captured discovery dump on a real device against the
  1.6.x reference (the same field-validation gate ADR-124 used); there is no clean
  CI forbid/require pattern for full payload shape.
- *Risk*: `entity_category` mapping disagreement (an entity in the wrong card).
  *Mitigation*: the mapping mirrors 1.6.x row-for-row; deviations are a quick table edit,
  not a structural change.

## Related Decisions

- **ADR-124 (Seven-Device Topology)**: **superseded by this ADR.** ADR-124 stays
  immutable (Accepted); a "Superseded by ADR-140" status line is added on acceptance, its
  body unedited.
- **ADR-122 (Five-Device Topology)**: already superseded by ADR-124; this ADR ends the
  multi-device line entirely by returning to single-device.
- **ADR-077 (Streaming MQTT HA Discovery Architecture)**: the two-pass MEASURE/WRITE
  contract whose determinism Risk (line 63) the F1 bug realized; this ADR keeps the
  streaming architecture but removes the per-device state that broke determinism.
- **ADR-106 (MQTT Topic Naming Mode)**: orthogonal (topic label naming, not device
  topology). Untouched.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is labeled
  guideline-level because the payload shape is field-validated, not CI-gated.

## References

- 1.6.x/dev reference (the model being matched):
  `OTGW-firmware/src/OTGW-firmware/mqtt_configuratie.cpp:1926-1948` (single device block),
  `:2070-2071` and `:2148` (`entity_category` emit), `MQTTstuff.h:150-155`
  (`HaEntityCat` enum), `:2016-2028` (friendlyName rendering); `via_device` absent
  across the whole dev `src/OTGW-firmware`.
- 2.0.0 change surface: `src/OTGW-firmware/MQTTHaDiscovery.cpp` (`writeDeviceBlock`,
  `deviceForOTId`, the stream* composers), `src/OTGW-firmware/MQTTstuff.ino`
  (`buildDiscoveryDeviceBlock` twin, `doAutoConfigureMsgid`, `g_haDeviceIntroduced`),
  `src/OTGW-firmware/MQTTstuff.h` (`HaDevice` enum, `bLegacyMode`).
- Review findings folded in: F1 (`MQTTHaDiscovery.cpp:2373` MEASURE-pass mutation),
  F5 (`MQTTHaDiscovery.cpp:2356` unescaped hostname/manufacturer/model).
- The MQTT review (2026-06-15) and the design research that produced this decision.

This ADR has a code surface but is enforced at field validation and PR review, not by an
automated gate: there is no clean forbid/require pattern for "the discovery payload
describes exactly one device with entity_category clustering". The mechanically checkable
signal (absence of `via_device` and of the `HaDevice` seven-device split) may later be
added to `evaluate.py`; for now it is guideline-level per ADR-080.
