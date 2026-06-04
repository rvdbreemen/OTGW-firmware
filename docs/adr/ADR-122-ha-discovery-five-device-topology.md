---
id: ADR-122
title: HA Discovery Five-Device Topology
status: Proposed
date: 2026-06-03
tags: [mqtt, ha-discovery, topology, legacy-mode, sat, esp, gateway, boiler, thermostat]
supersedes: []
superseded_by: []
related: [ADR-077, ADR-070, ADR-106, ADR-096, ADR-103, ADR-088, ADR-080]
deciders: [Robert van den Breemen]
---

# ADR-122: HA Discovery Five-Device Topology

## Status

Proposed. **Binding** (per ADR-080): the golden-file discovery test in `tests/`
plus `haDeviceForEntity()` as the single auditable PROGMEM routing map are the
CI gate for this ADR. Those artifacts and the gate itself are committed
Definition-of-Done items of TASK-648; they must land in the same implementation
commit and must not be deferred. Specifically: `check_ha_discovery_device_routing`
(to be added to `evaluate.py`) verifies (a) exactly five device blocks are
emitted, (b) each entity's device routing matches the PROGMEM table, and (c)
legacy mode reproduces the pre-split payload byte-for-byte against a captured
1.x.x golden file.

**Decision Maker:** Robert van den Breemen.

## Status History

status_history:
  - date: 2026-06-03
    status: Proposed
    changed_by: Claude (TASK-648)
    reason: Documents the five-device HA discovery topology design, the two independent legacy-mode axes (bLegacyMode for topology/unique_id; bUseLegacyOtTopics standalone for topic naming), and the HA-core-authoritative OT entity routing. Spec approved by maintainer before ADR was filed.
    changed_via: adr-kit

## Context

### The single-device problem

The firmware today publishes all 309+ Home Assistant MQTT discovery configs
under one device (identifier `{nodeId}`, the gateway chipId). Every sensor,
binary sensor, climate entity, number, select, and button lands in one HA device
card. This has three compounding problems:

1. **HA core divergence.** Home Assistant's own `opentherm_gw` integration
   (HA core `2024.10`, PR #124869) splits its entities across three devices:
   boiler, gateway, thermostat. A firmware that groups everything under a single
   device looks wrong next to the HA core integration and makes dual-use setups
   (HA integration alongside firmware discovery) ambiguous.

2. **Firmware-native entities have no home.** The firmware emits entities HA
   core has no concept of: ESP-node diagnostics (heap, WiFi, uptime, OTA health),
   and SAT (the 2.0.0 Smart Autotune Thermostat — setpoint, control mode, cycle
   verdict, autotune score). These have always been shoehorned into the single
   boiler/gateway device, which is semantically wrong.

3. **Origin-aligned source worldview.** The dev-branch work that introduced
   OT/ESP/PIC traffic separation (ADR-096, ADR-103) established that data is
   meaningful only in the context of its origin. The HA device topology should
   mirror that: where a value comes from determines which device it belongs to.

### HA core reference

The authoritative source for OT entity device assignment is the HA core
`opentherm_gw` integration on the `2024.10` branch: `__init__.py`, `entity.py`,
`sensor.py`, `binary_sensor.py`, `climate.py`, `switch.py`, `select.py`,
`button.py`. These files define a per-key `device_description.device_identifier`
for every entity. This ADR transcribes those assignments as the routing table
that `haDeviceForEntity()` implements; the per-key transcription is an
implementation step (TASK-648) but the principle, buckets, and bilateral set are
fixed here.

### ADR-106 standalone-flag context

ADR-106 (Accepted, 2026-05-21) shipped `settings.mqtt.bUseLegacyOtTopics`
as a standalone toggle: when true it restores legacy OT-spec topic names; when
false it uses the self-describing 2.0.0 names. That flag is intentionally scoped
to topic naming only and remains a standalone switch. This ADR introduces a
separate concept, device topology, governed by its own flag `bLegacyMode`. The
two flags are independent and default differently depending on install context
(see Section 5). ADR-106's standalone-flag decision is not superseded by this ADR;
the two ADRs complement each other with separate concerns.

## Decision

### 1. Five-device topology (modern default)

The firmware publishes HA discovery configs under five devices:

| # | Device | Identifier (modern) | Manufacturer | Model | Versions | Primary entities |
|---|---|---|---|---|---|---|
| 1 | Boiler | `{nodeId}-boiler` | `SLAVE_MEMBERID` | `SLAVE_PRODUCT_TYPE` | sw=`SLAVE_OT_VERSION`, hw=`SLAVE_PRODUCT_VERSION` | Slave fault/capability flags, boiler temps/pressures/counters, slave setpoint limits, OEM diag |
| 2 | Thermostat | `{nodeId}-thermostat` | `MASTER_MEMBERID` | `MASTER_PRODUCT_TYPE` | sw=`MASTER_OT_VERSION`, hw=`MASTER_PRODUCT_VERSION` | Master flags, climate entity, room setpoint/override/temp, outside temp |
| 3 | Gateway | `{nodeId}-gateway` | See below | See below | See below | OTGW internals: operating mode, DHW override, LED A-F, GPIO A/B + states, setback temp, setpoint-override mode, smart-power, thermostat-detect, Vref, ignore-transitions, override-HB |
| 4 | OTGW-firmware (ESP) | `{nodeId}-esp` | "Espressif" (or board vendor) | Board name (NodeMCU / Wemos D1 / OTGW32) | sw=firmware version, hw=chip | Free heap, min-heap, max-block, WiFi RSSI/SSID, IP, MAC, uptime, reboot reason, OTA, MQTT/WS health, Dallas temps, S0 pulse counter, OLED presence |
| 5 | SAT | `{nodeId}-sat` | "OTGW-firmware" | "Smart Autotune Thermostat" | sw=firmware version | `sat/*` setpoint, control mode, cycle verdict, autotune score, comfort/window/summer state, simulation, narration |

Devices 1-3 are the HA core trio (authoritative per HA core 2024.10). Devices
4-5 are firmware-native; HA core has no equivalent, so their layout is defined
here.

### 2. Gateway identity: PIC xor OTDirect

A firmware build uses exactly one bus-driver path at runtime: PIC (serial UART
via `HAS_PIC`) or OTDirect (ESP32-S3 native via `HAS_DIRECT_OT`). Both map to
the **same** `{nodeId}-gateway` identifier; only the identity metadata differs:

- **PIC present:** manufacturer = "Schelte Bron", model = "OpenTherm Gateway",
  sw_version = PIC about-string `PR=A` substring from index 18.
- **OTDirect active:** manufacturer = "OTGW-firmware", model = "OpenTherm Gateway
  (OTDirect)", sw_version = firmware version.

Using a shared identifier ensures that toggling between PIC and OTDirect hardware
does not orphan the gateway device in HA's entity registry.

### 3. HA-core-authoritative OT routing via `haDeviceForEntity()`

A central function `haDeviceForEntity(category, key)` returns an enum value
(`HaDevice::Boiler | Thermostat | Gateway | Esp | Sat`) that determines which
device block an entity is emitted under. This is the **single auditable
mapping** — every OT-entity routing decision flows through it, nowhere else.

Routing rules:

- **Slave-origin values** (slave flags `DATA_SLAVE_*`, boiler temps/pressures,
  burner/pump counters, slave setpoint limits, `OEM_DIAG`) → `HaDevice::Boiler`.
- **Master-origin values** (master flags `DATA_MASTER_*`, room setpoint/override/
  temp, outside temp) → `HaDevice::Thermostat`.
- **Bilateral values** (values HA core assigns to both boiler and thermostat,
  including but not limited to: `CONTROL_SETPOINT`, `CH_WATER_TEMP`, `DHW_TEMP`,
  `RETURN_WATER_TEMP`, `REL_MOD_LEVEL`, `SLAVE_PRODUCT_*`/`MASTER_PRODUCT_*`,
  `SLAVE_OT_VERSION`/`MASTER_OT_VERSION`, `SLAVE_MEMBERID`/`MASTER_MEMBERID`,
  `ROOM_SETPOINT*`, `ROOM_TEMP`, `OUTSIDE_TEMP`, the RBP remote-parameter flags,
  room-override priority flags) → **both** `HaDevice::Boiler` and
  `HaDevice::Thermostat`. Two discovery configs are emitted with distinct
  device-prefixed unique_ids (see section 4). The exact bilateral key list is
  transcribed from HA core's platform tables during implementation.
- **OTGW_*** internals (operating mode, LED A-F, GPIO A/B + states, about, build,
  clock, setback, setpoint-override mode, smart-power, thermostat-detect, Vref,
  DHW-override, ignore-transitions, override-HB, PIC extras beyond HA core's set)
  → `HaDevice::Gateway`.
- **ESP-node diagnostics and hardware sensors** (heap, WiFi, uptime, IP, MAC,
  OTA, MQTT/WS health, Dallas temps, S0 pulse counter, OLED presence) →
  `HaDevice::Esp`.
- **SAT entities** (by `sat/*` topic prefix or SAT emitter) → `HaDevice::Sat`.

The PROGMEM routing table for OT keys is transcribed verbatim from HA core's
platform files during TASK-648 implementation. The firmware-native buckets (ESP
and SAT) are rule-based (topic prefix / emitter) and require no table lookup.

### 4. unique_id scheme

- **Modern (default):** `{nodeId}-{device}-{label}` where `{device}` is the
  lowercase device short-name (`boiler`, `thermostat`, `gateway`, `esp`, `sat`).
  For bilateral values: `{nodeId}-boiler-{label}` and
  `{nodeId}-thermostat-{label}` are two distinct unique_ids for two distinct
  entities — one under the Boiler device, one under the Thermostat device.
- **Legacy (opt-in, `settings.mqtt.bLegacyMode = true`):** `{nodeId}-{label}`,
  reproducing the 1.x.x scheme exactly.

### 5. Two independent legacy-mode axes

The design uses two independent settings for two separate compatibility concerns.
They default differently and are toggled independently.

**Axis 1: Device topology (`settings.mqtt.bLegacyMode`, default `false` for
everyone)**

`bLegacyMode` governs device topology and the coupled unique_id scheme:

| `bLegacyMode` | Device topology | unique_id scheme |
|---|---|---|
| `false` (default) | Five devices as above | `{nodeId}-{device}-{label}` |
| `true` (advanced opt-in) | Single device (`{nodeId}`) | `{nodeId}-{label}` |

Default is `false` for all users, including users upgrading from 1.x.x. Upgraders
see entities re-registered under the five-device scheme on first 2.0.0 boot.
`bLegacyMode = true` is the advanced escape hatch for users who want to stay on the
1.x.x single-device layout and avoid any entity_id churn.

Changing `bLegacyMode` triggers a republish-and-clean cycle: all discovery configs
are republished under the new scheme, and the old scheme's retained discovery
payloads are cleared via the ADR-070 orphan-cleanup path (empty retained payload
published to every stale discovery topic). The cleanup state is persisted to
LittleFS so a power cut mid-drain resumes correctly on next boot.

**Axis 2: OT-topic label naming (`settings.mqtt.bUseLegacyOtTopics`, upgrade-aware
default)**

`bUseLegacyOtTopics` governs whether legacy OT-spec-derived topic names or the
new self-describing 2.0.0 aliases are used (ADR-106). It is a standalone setting,
not absorbed into `bLegacyMode`. The default is upgrade-aware:

- **Fresh install (2.0.0 config):** `false`. The key `MQTTuseLegacyOtTopics` is
  written by `writeSettings`, so it is present in the settings file and its stored
  or default value (`false`) is honoured. New users get self-describing topic names.
- **1.x.x upgrade:** `true`. A 1.x.x settings file predates ADR-106, so the key
  `MQTTuseLegacyOtTopics` is absent. `readSettings` detects the absent key and
  defaults to `true`, preserving legacy topic names. Existing MQTT automations and
  dashboards continue to work without changes. The user opts in to the new names
  explicitly.
- **Existing 2.0.0 alpha config:** the key is present; stored value is honoured
  as-is. No regression for current alpha testers.

Toggling `bUseLegacyOtTopics` triggers cleanup of the retained payloads in the
other name set via the persistent bitmap in `/mqtt_topic_cleanup.bin`
(ADR-106's cleanup-bitmap design).

### 6. Migration on 2.0.0 upgrade

The two axes produce two independent migration outcomes.

**Device topology (Axis 1, `bLegacyMode = false` default for all):**

An upgrading user sees the following on first 2.0.0 boot:

1. The firmware republishes all discovery configs under the five-device scheme
   with modern unique_ids.
2. The ADR-070 orphan-cleanup path clears the old single-device retained
   discovery payloads, so HA removes the stale single-device entities instead of
   leaving ghosts.
3. HA re-registers the entities under the new device cards and new unique_ids.
   Automations pinned to old HA entity_ids (which are derived from old unique_ids)
   will need to be repointed.
4. MQTT state (value) topics and payloads are **unchanged** — any MQTT consumer
   subscribing to value topics is unaffected. Only HA's entity registry changes.

Users who do not want device-topology churn: set `bLegacyMode = true` before or
immediately after upgrade. Entity_id churn is then zero.

**OT-topic label naming (Axis 2, `bUseLegacyOtTopics = true` default on upgrade):**

A 1.x.x settings file does not contain the `MQTTuseLegacyOtTopics` key (it
predates ADR-106). On upgrade `readSettings` detects the absent key and defaults
`bUseLegacyOtTopics = true`, so legacy OT-spec topic names remain in use. No MQTT
automations or dashboards break. The user can switch to self-describing names
explicitly by setting `bUseLegacyOtTopics = false` in the Web UI or via the REST API.

**Net effect for a 1.x.x upgrader:** entities re-register once into the new
five-device cards (a one-time HA entity_id event), but MQTT state topic names are
unchanged. The two axes can be toggled independently after upgrade.

### 7. Bilateral replication: resource note

Replicating bilateral values on both Boiler and Thermostat adds approximately
55 extra discovery entities versus the single-device layout. Discovery is already
streamed (ADR-077) and burst-windowed (ADR-088); ESP32-S3 (OTGW32) has ample
flash and RAM headroom. The implementation must verify that an ESP8266 build
still fits flash and that the discovery burst stays within the ADR-088 window
(`python build.py --firmware` flash check + discovery-burst heap measurement). If
ESP8266 cannot absorb the bilateral doubling, the implementation may fall back to
single-device-per-bilateral-value on ESP8266 only (the bilateral entity is placed
on the semantically primary device: Boiler for slave-origin bilaterals, Thermostat
for master-origin bilaterals). This ESP8266 fallback, if triggered, is a
**platform deviation** noted in code with a `HAS_ESP8266_BILATERAL_FALLBACK`
comment; it does not affect the ADR's logical design. The decision whether the
fallback is needed is deferred to implementation measurement.

### 8. No `via_device`

HA core 2024.10 sets no `via_device` on any of its three devices. This firmware
matches that behaviour and omits `via_device` from all five device blocks.
(Note: this *matches* HA core; it is not a deviation from it. It does supersede
the original TASK-648 AC#6, which predated confirming HA core had dropped
`via_device`.)

## Alternatives Considered

1. **Keep single-device grouping.** Rejected: creates ever-growing device cards
   with 309+ entities; diverges from HA core's publicly shipped three-device
   topology; misattributes firmware-native entities (ESP, SAT) to the boiler.

2. **Three-device topology (HA core parity only).** Rejected: leaves ESP-node
   diagnostics and SAT without a correct home. Putting heap/WiFi/uptime entities
   in the gateway or boiler device would be semantically wrong and surprising to
   users.

3. **Fully independent flags for all three axes.** The design settles on two flags,
   not three. Device topology and unique_id scheme are kept coupled under
   `bLegacyMode` because they are coherent as a pair: legacy topology with modern
   unique_ids or vice versa produces incoherent states (entities in the wrong device
   card, orphaned HA entity_ids). Topic naming (`bUseLegacyOtTopics`) is kept as a
   standalone flag because its upgrade-aware default differs from topology: upgraders
   benefit from keeping legacy topic names while getting the new five-device grouping.
   Coupling topic naming into `bLegacyMode` would force all upgraders to choose between
   topology churn and topic-name churn together, which is unnecessarily coarse.

4. **`via_device` chaining (firmware → gateway, boiler → gateway, etc.).**
   Rejected: HA core 2024.10 dropped `via_device`. Adding it to firmware
   discovery where HA core no longer uses it would create a firmware-specific
   hierarchy the HA UI does not reflect in the same way for all users.

5. **Routing by topic-prefix heuristic instead of `haDeviceForEntity()`.** 
   Rejected: a heuristic applied at every discovery callsite drifts across
   contributors. A single routing function with a PROGMEM table is auditable,
   testable, and can be CI-gated.

## Consequences

**Positive:**

- Five purpose-fit device cards in HA; boiler entities group with the boiler,
  thermostat entities with the thermostat, ESP diagnostics with the ESP node, SAT
  with the SAT card. Matches HA core's publicly shipped topology for the OT trio.
- Firmware-native entities (ESP, SAT) have semantically correct homes with no
  ambiguity.
- `bLegacyMode` gives users a single knob to restore 1.x.x device topology and
  unique_ids; topology and unique_id coherence is preserved (the two are always
  changed together). `bUseLegacyOtTopics` is a separate knob for topic naming,
  defaulting to legacy on upgrade so existing automations keep working.
- MQTT value topics and payloads are untouched; non-HA MQTT consumers see no
  change regardless of the topology mode.
- `haDeviceForEntity()` is a single, CI-gated, auditable routing table. No
  per-callsite routing decisions.

**Negative / trade-offs:**

- **One-time HA entity_id churn on 2.0.0 upgrade** for topology (all upgraders get
  five-device by default). Automations pinned to old entity_ids must be repointed.
  This is the expected cost of a major-version release and is fully escapable via
  `bLegacyMode = true`. MQTT state topic names are unaffected for upgraders
  (`bUseLegacyOtTopics` defaults legacy on upgrade).
- **~55 extra discovery entities** from bilateral replication. Burst-windowed
  (ADR-088); cost is tolerable on ESP32-S3. ESP8266 requires implementation
  measurement; a platform fallback is defined if needed.
- Contributor contract: every new entity that requires routing must call
  `haDeviceForEntity()` or follow a documented topic-prefix rule. The CI gate
  (`check_ha_discovery_device_routing`) enforces this at the entity-set level;
  point additions require code review.
- Device metadata for boiler and thermostat (product type, member ID, OT version)
  is read from the OT data cache and may be unknown on first boot. The discovery
  republish path re-fires once polled values arrive; this is the existing
  re-discovery pattern and not a new behaviour.

**Risks and mitigations:**

- *Risk:* bilateral replication overflows ESP8266 flash or ADR-088 burst budget.
  *Mitigation:* `python build.py --firmware` flash check + burst heap measurement
  gated in TASK-648 implementation; platform fallback defined and documented.
- *Risk:* `haDeviceForEntity()` routing table diverges from HA core as HA core
  evolves. *Mitigation:* the table cites its HA core source version (2024.10) and
  the implementation notes which PR to re-check on HA core upgrades.
- *Risk:* orphan cleanup of old single-device discovery topics misses some
  entries on the upgrade path. *Mitigation:* the ADR-070 cleanup path is
  bitmap-backed (persisted to LittleFS); carries forward ADR-106's proven design.

## Related Decisions

- **ADR-077** (Streaming MQTT HA Discovery Architecture): this ADR is built on
  top of the streaming discovery infrastructure; it changes what is emitted but
  not how streaming operates. Per-device "first entity" tracking replaces the
  global `isFirstEntity` flag.
- **ADR-070** (Orphan cleanup of stale discovery topics): the migration on mode
  toggle relies on ADR-070's orphan-cleanup path to clear stale retained payloads.
- **ADR-106** (MQTT Topic Naming Mode, Accepted 2026-05-21): this ADR complements
  ADR-106; it does not supersede it. ADR-106's `bUseLegacyOtTopics` remains a
  standalone flag governing topic naming independently of the device topology
  introduced here. The two flags coexist with separate defaults: `bLegacyMode`
  defaults modern for all (five-device); `bUseLegacyOtTopics` defaults legacy on
  1.x.x upgrade (key-absence detection). ADR-106's per-key naming table (37 alias
  rows) and cleanup-bitmap design are unchanged.
- **ADR-096 / ADR-103** (Worldview routing / publish gating by source and
  slave-echo): the per-entity device routing in this ADR is purely additive to the
  worldview; the canonical/override distinction and publish-gating rules are
  unchanged.
- **ADR-088** (MQTT Status-Burst Windowing): discovery burst (five devices, ~365
  entities in modern mode) must stay within the ADR-088 burst window; verified at
  implementation as a gated AC.
- **ADR-080** (Binding ADRs must have a CI gate): governs the classification note
  in the Status section above.

## References

- TASK-648 (this work; five-device topology implementation).
- Design spec: `docs/superpowers/specs/2026-06-03-ha-discovery-five-device-split-design.md`.
- HA core reference: `home-assistant/core` `opentherm_gw` 2024.10 (PR #124869) —
  `__init__.py`, `entity.py`, `sensor.py`, `binary_sensor.py`, `climate.py`,
  `switch.py`, `select.py`, `button.py`.
- ADR-106 (standalone, complementary): `docs/adr/ADR-106-mqtt-topic-naming-mode-new-default-legacy-toggle.md`.
