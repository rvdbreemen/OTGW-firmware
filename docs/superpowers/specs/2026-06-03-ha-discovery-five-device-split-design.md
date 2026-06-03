# HA Discovery Five-Device Split — Design

Date: 2026-06-03
Status: Draft (brainstorm complete, pending user review)
Task: TASK-648 (re-scoped to 2.0.0, five-device)
Related: ADR-077 (streaming HA discovery), ADR-070 (orphan cleanup), ADR-106 (legacy OT-topic toggle), ADR-096/103 (source-topic worldview), ADR-088 (status-burst windowing)
HA-core reference: `home-assistant/core` `opentherm_gw` (2024.10, PR #124869) — `__init__.py`, `entity.py`, `sensor.py`, `binary_sensor.py`, `climate.py`, `switch.py`, `select.py`, `button.py`

## Problem

OTGW-firmware emits all 309+ HA discovery entries under a single device. HA core's
`opentherm_gw` (2024.10) splits its entities across three devices (boiler, gateway,
thermostat). The firmware also produces entities HA core has no concept of: ESP-node
diagnostics, SAT (the 2.0.0 virtual thermostat), and PIC/OTDirect gateway internals.
The dev-branch work to separate OT / ESP / PIC traffic (ADR-096/103 source worldview)
points at a device topology that mirrors *where data originates*.

Goal: a five-device HA topology that is faithful to HA core for the OpenTherm devices,
extends cleanly to the firmware-native devices, ships as the **modern default** in
2.0.0, and offers a single **legacy** opt-out that restores 1.x.x behaviour byte-for-byte.

## Locked decisions (from brainstorm)

1. **Five devices** (see model below): boiler, thermostat, gateway, ESP-firmware, SAT.
2. **Modern is the default in 2.0.0.** Legacy is opt-in.
3. **One umbrella legacy switch** governs topic naming AND device topology AND
   unique_id scheme together. It broadens the existing ADR-106 `bUseLegacyOtTopics`
   into a single "1.x.x compatibility" mode (renamed `bLegacyMode`, with
   `bUseLegacyOtTopics` kept as a deprecated alias for one release).
4. **HA core is authoritative for the OpenTherm devices** (boiler/thermostat/gateway):
   per-entity device assignment is transcribed from HA core's platform tables.
5. **Modern unique_id scheme: `{nodeId}-{device}-{label}`** (HA-core aligned). Legacy
   scheme: `{nodeId}-{label}` (1.x.x). Switched by the umbrella legacy flag.
6. **Bilateral OT values are replicated** on both boiler and thermostat (HA-core-exact),
   with distinct device-prefixed unique_ids. Accepts ~55 extra entities (resource note
   below).
7. **Gateway device is PIC xor OTDirect** (mutually exclusive per setup); the single
   gateway device adopts the identity of whichever is active.
8. **No `via_device`** — HA core 2024.10 sets none; we match it. (This overrides the
   original TASK-648 AC#6, which predated confirming HA core dropped via_device.)

## The five-device model

| # | Device | identifier (modern) | manufacturer | model | sw_version / hw_version | Owns |
|---|---|---|---|---|---|---|
| 1 | Boiler | `{nodeId}-boiler` | `SLAVE_MEMBERID` | `SLAVE_PRODUCT_TYPE` | sw=`SLAVE_OT_VERSION`, hw=`SLAVE_PRODUCT_VERSION` | slave fault/capability flags, boiler temps/pressures/counters, slave-side setpoint limits |
| 2 | Thermostat | `{nodeId}-thermostat` | `MASTER_MEMBERID` | `MASTER_PRODUCT_TYPE` | sw=`MASTER_OT_VERSION`, hw=`MASTER_PRODUCT_VERSION` | master flags, climate entity, room setpoint/override/temp, outside temp |
| 3 | Gateway | `{nodeId}-gateway` | PIC: "Schelte Bron" / ODT: "OTGW-firmware" | PIC: "OpenTherm Gateway" / ODT: "OpenTherm Gateway (OTDirect)" | sw = PIC `PR=A` about-string `[18:]` (PIC) or firmware version (ODT) | OTGW internals: operating mode, DHW override, LED A-F, GPIO A/B (+ states), setback temp, setpoint-override mode, smart-power, thermostat-detect, Vref, ignore-transitions, override-HB |
| 4 | OTGW-firmware (ESP) | `{nodeId}-esp` | "Espressif" (or board vendor) | board (NodeMCU / Wemos D1 / OTGW32) | sw = firmware version, hw = chip | free heap, min-heap, max-block, WiFi RSSI/SSID, IP, MAC, uptime, reboot reason, OTA, MQTT/WS health, Dallas temps, S0 pulse counter, OLED presence |
| 5 | SAT | `{nodeId}-sat` | "OTGW-firmware" | "Smart Autotune Thermostat" | sw = firmware version | sat/* setpoint, control mode, cycle verdict, autotune score, comfort/window/summer state, simulation, narration |

Devices 1-3 are the HA-core trio (authoritative). Devices 4-5 are firmware-native; HA
core has no equivalent, so their layout is ours to define.

### Gateway identity (PIC xor OTDirect)

A setup uses exactly one. Selected at runtime by the existing capability/feature
detection (`HAS_PIC` / OTDirect active). The gateway device block emits the PIC
identity when a PIC is present, otherwise the OTDirect identity. Same `{nodeId}-gateway`
identifier either way, so toggling hardware does not orphan the device.

## Per-entity routing

A central function `haDeviceForEntity(category, key)` returns the owning device enum
(`HaDevice::Boiler|Thermostat|Gateway|Esp|Sat`). It is the single auditable mapping the
ADR documents. Rules:

- **OpenTherm entities** (msgID-derived sensors, binary_sensors, the climate entity,
  setpoint selects/numbers): follow HA core's `device_description.device_identifier`,
  transcribed from HA core's platform tables. Reference (from HA core dev branch, to be
  transcribed verbatim into a PROGMEM routing table during implementation):
  - **Slave-origin** values → Boiler (slave flags `DATA_SLAVE_*`, boiler temps/pressures,
    burner/pump counters, slave setpoint limits, `OEM_DIAG`).
  - **Master-origin** values → Thermostat (master flags `DATA_MASTER_*`, room setpoint/
    override/temp, outside temp).
  - **Bilateral** values that HA core lists on both (e.g. `CONTROL_SETPOINT`,
    `CH_WATER_TEMP`, `DHW_TEMP`, `RETURN_WATER_TEMP`, `REL_MOD_LEVEL`, the slave/master
    *_PRODUCT_*/*_OT_VERSION/*_MEMBERID, `ROOM_SETPOINT*`, `ROOM_TEMP`, `OUTSIDE_TEMP`,
    the RBP remote-parameter flags, the room-override priority flags) → **both** Boiler
    and Thermostat (two entities, distinct device-prefixed unique_ids).
  - **OTGW_*** internals (mode, LED A-F, GPIO A/B + states, about, build, clock, setback,
    setpoint-override mode, smart-power, thermostat-detect, Vref, DHW-override,
    ignore-transitions, override-HB) → Gateway.
- **Firmware-native entities** route by origin, by topic prefix / emitter:
  - ESP-node diagnostics + hardware sensors (heap, wifi, uptime, IP, OTA, Dallas, S0,
    OLED, MQTT/WS) → Esp.
  - `sat/*` entities → Sat.
  - PIC-internal extras the firmware adds beyond HA core's OTGW_* set → Gateway.

The transcription of HA core's exact per-key assignment is an implementation step (build
the PROGMEM table from the HA core platform files); this spec fixes the principle, the
buckets, and the bilateral set.

## unique_id and the umbrella legacy mode

- **Modern (default):** `<nodeId>-<device>-<label>` where `<device>` ∈
  {boiler,thermostat,gateway,esp,sat}. Device blocks carry the per-device identifier,
  metadata, and (per decision 8) no via_device.
- **Legacy (opt-in, `settings.mqtt.bLegacyMode = true`):** restores 1.x.x exactly —
  single device (identifier `{nodeId}`, the current behaviour), `<nodeId>-<label>`
  unique_ids, and legacy OT-topic naming (the former `bUseLegacyOtTopics=true`
  behaviour). Byte-for-byte identical to the pre-split payloads.

`bLegacyMode` subsumes `bUseLegacyOtTopics`: on load, if the deprecated
`bUseLegacyOtTopics` is set true it maps to `bLegacyMode=true` (one-release migration);
a new ADR records the merge and supersedes ADR-106's standalone-flag decision.

## Migration

Because modern is the default and modern unique_ids differ from 1.x.x, an upgrading user
sees a **one-time entity re-registration**:

1. On boot in modern mode (or when `bLegacyMode` is toggled), the firmware republishes
   all discovery configs under the new device + unique_id scheme.
2. The old single-device discovery configs are cleared via the existing ADR-070 orphan
   cleanup path (publish empty retained payload to the old discovery topics), so HA
   removes the stale single-device entities instead of leaving ghosts.
3. State (value) topics are unchanged, so MQTT consumers subscribing to value topics are
   unaffected; only HA's entity registry re-groups.

Users who do not want the churn flip `bLegacyMode` on and stay on 1.x.x grouping +
unique_ids. Toggling either way is a one-way republish-and-clean cycle.

## Backward compatibility / breakage

- **Legacy mode** = byte-for-byte 1.x.x. Zero breakage.
- **Modern default** = a major-version (2.0.0) one-time migration. What breaks: HA
  automations that reference the *old* entity_ids (derived from old unique_ids) must be
  repointed at the new entity_ids OR the user flips `bLegacyMode`. What does NOT break:
  MQTT value topics, payloads, and any consumer subscribing to topics rather than HA
  entity_ids.
- Net: the only functional break is HA entity_id churn on upgrade, fully escapable via
  the legacy switch. Aligned with 2.0.0 being a major release.

## Architecture / components

- `MQTTHaDiscovery.cpp`:
  - `HaDiscoveryContext` gains a `device` dimension (which of the 5) plus per-device
    metadata accessors.
  - `writeDeviceBlock()` emits the per-device identifier + metadata; "full block once"
    tracking becomes **per device** (first-entity-of-boiler, of-thermostat, etc.) rather
    than a single `isFirstEntity`.
  - unique_id builders insert `<device>` when modern.
  - `haDeviceForEntity(category, key)` — the routing function (PROGMEM table for OT keys
    + rule for firmware-native).
- Gateway identity: a small helper picks PIC vs OTDirect metadata from capability flags.
- Device metadata sourcing: read product-id values (`SLAVE_*`/`MASTER_*`) from the OT
  data cache; emit best-known at discovery time and rely on the existing re-discovery
  path to refresh once polled (product IDs may be unknown on first discovery).
- Settings: `settings.mqtt.bLegacyMode` (default false); deprecate `bUseLegacyOtTopics`
  (alias on load).
- Migration: hook the orphan-cleanup of old single-device topics into the discovery
  republish entry point.

## ESP8266 resource note

Bilateral replication adds ~55 entities; modern default also means all entities carry a
slightly longer unique_id and an extra device block on each per-device first entity.
Discovery is already streamed (ADR-077) and burst-windowed (ADR-088), and ESP32-S3 has
ample headroom. **The implementation must verify an ESP8266 build still fits flash and
that the discovery burst stays within the ADR-088 window** (measure `python build.py
--firmware` flash usage + a discovery-burst heap check). If ESP8266 cannot absorb the
bilateral doubling, fall back to single-device-per-value on ESP8266 only (a documented,
flag-free platform deviation) — decided at implementation if measurement requires it.

## Testing

- **Discovery-payload test (host-side):** capture the emitted retained discovery configs
  (mock MQTT sink or parse `sendMQTTData` output) and assert: five device blocks with
  the right identifiers/metadata; per-entity device routing matches the HA-core table;
  modern unique_ids are `{nodeId}-{device}-{label}`; bilateral keys appear on both
  boiler and thermostat; and **legacy mode reproduces the pre-split payload byte-for-byte**
  (golden-file comparison against a captured 1.x.x discovery set).
- **Build/evaluate:** `python build.py --firmware` exits 0 on esp32 AND esp8266 (flash
  check); `python evaluate.py --quick` no new findings.
- **Manual HA validation:** flash, confirm five device cards group correctly; toggle
  `bLegacyMode` and confirm single-device 1.x.x grouping returns and old entities are
  cleaned up.

## ADR

A new ADR (Proposed) documents: the five-device topology and identifiers; modern-default
+ umbrella legacy mode (superseding ADR-106's standalone flag); the HA-core-authoritative
OT routing + the firmware-native ESP/SAT/gateway buckets; bilateral replication; the
PIC-xor-OTDirect gateway identity; the deliberate deviations (no via_device; ESP + SAT
beyond HA core; possible ESP8266 single-device fallback). The routing function is the
auditable mapping the ADR references.

## Scope / phasing

This is large (5 devices, full per-entity routing, unique_id rework, legacy umbrella,
migration, ADR, docs, golden-file test). The implementation plan should decompose it,
suggested order:
1. Settings + umbrella `bLegacyMode` (alias `bUseLegacyOtTopics`); no behaviour change yet.
2. `HaDiscoveryContext` device dimension + per-device `writeDeviceBlock` + metadata
   (devices emitted, routing stubbed to today's single bucket) behind modern/legacy.
3. unique_id scheme switch + golden-file legacy byte-identical test.
4. `haDeviceForEntity` routing table (OT keys from HA core) + firmware-native buckets +
   bilateral replication.
5. Gateway PIC-xor-OTDirect identity.
6. Migration (orphan cleanup of old single-device topics on republish).
7. ESP8266 flash/burst verification + fallback decision.
8. ADR + MQTT.md routing table + docs.

Each phase builds + evaluates green and is independently reviewable.

## Out of scope (YAGNI)

- `via_device` (HA core dropped it).
- Per-axis legacy switches (one umbrella switch only).
- HA 2024.11 device-bundle discovery (separate future work).
- Changing any state/value topic name or payload.
