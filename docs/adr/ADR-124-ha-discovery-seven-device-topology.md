---
id: ADR-124
title: HA Discovery Seven-Device Topology — Dedicated OT-Core and Sensors Devices, Gateway via_device Hub
status: Proposed
date: 2026-06-05
tags: [mqtt, ha-discovery, topology, via-device, pic, otdirect, sensors, dallas, s0, esp, gateway]
supersedes: [ADR-122]
superseded_by: []
related: [ADR-122, ADR-077, ADR-070, ADR-106, ADR-088, ADR-080]
deciders: [Robert van den Breemen]
---

# ADR-124: HA Discovery Seven-Device Topology — Dedicated OT-Core and Sensors Devices, Gateway via_device Hub

## Status

Proposed. Date: 2026-06-05. **Supersedes ADR-122** (five-device topology).

**Decision Maker:** User: Robert van den Breemen. This is a maintainer decision
revising the device split based on field insight after living with the
five-device layout; it overrides three specific points of ADR-122 (the Gateway's
absorption of the OT-core identity, the placement of hardware sensors on the Esp
device, and the deliberate omission of `via_device`).

**Binding** (per ADR-080): the same golden-file discovery test + `evaluate.py`
gate that ADR-122 introduced (`check_ha_discovery_device_routing`) is the CI gate
for this ADR; it must be updated in the implementing commit (TASK-826) to assert
(a) **seven** device blocks, (b) the new OT-Core and Sensors routing, and (c) a
`via_device` pointing at `<nodeId>-gateway` on every non-Gateway device. Legacy
single-device mode (`bLegacyMode = true`) remains byte-identical and is unchanged.

## Status History

status_history:
  - date: 2026-06-05
    status: Proposed
    changed_by: Claude (TASK-826)
    reason: Maintainer revised the five-device topology (ADR-122) to seven devices — splitting the OpenTherm-core (PIC/OTDirect) into its own device, giving the physical hardware sensors (Dallas + S0) their own device, and adding a Gateway-as-hub via_device hierarchy. Decision taken by Robert van den Breemen.
    changed_via: adr-kit

## Context

ADR-122 (Proposed, 2026-06-03) introduced a five-device HA discovery topology —
`HaDevice { Boiler, Thermostat, Gateway, Esp, Sat }` — routed by the single
auditable `haDeviceForEntity()` PROGMEM map. After living with that layout, three
of its choices read wrong in the Home Assistant device view:

1. **The OpenTherm-core has no device of its own.** ADR-122 §2 folded the bus
   driver — the PIC (classic OTGW, `HAS_PIC`) or the OTDirect GPIO core (OTGW32,
   `HAS_DIRECT_OT`) — into the **Gateway** device, sharing the
   `<nodeId>-gateway` identifier and differing only in metadata. In practice the
   OT-core has its own identity, firmware version, and lifecycle (a PIC firmware
   flash, an OTDirect mode change) that the user wants to see as a distinct
   device, separate from the OTGW gateway *configuration* entities (LEDs, GPIO,
   operating mode) that genuinely belong to the Gateway.

2. **Physical hardware sensors are mixed into ESP-node diagnostics.** ADR-122 §3
   routed the Dallas 1-wire temperature sensors and the S0 pulse counter to the
   **Esp** device, alongside heap/WiFi/uptime/OTA telemetry. A wired temperature
   probe and a kWh pulse counter are *physical sensors attached to the unit*, not
   ESP-node health metrics; grouping them with diagnostics is semantically off and
   clutters the Esp card.

3. **`via_device` was omitted to mirror HA core, but the maintainer wants a
   hub.** ADR-122 §8 deliberately dropped `via_device` to match HA core 2024.10,
   which sets none. The maintainer's preference is the opposite: nest the
   firmware-side devices under the Gateway so HA renders a clear hierarchy
   (Gateway → Boiler / Thermostat / Esp / Pic / Sat / Sensors) instead of seven
   peers floating side by side.

These are user-experience judgements about the firmware's *own* topology, made
deliberately in the knowledge that points 1 and 3 diverge from the HA-core
`opentherm_gw` reference that ADR-122 tracked.

## Decision

Expand the topology from five to **seven** devices and add a Gateway-rooted
`via_device` hierarchy:

```
HaDevice { Boiler, Thermostat, Gateway, Esp, Pic, Sat, Sensors }
```

### 1. Seven devices

| # | Device | Identifier (modern) | Named | Primary entities | Change vs ADR-122 |
|---|---|---|---|---|---|
| 1 | Boiler | `<nodeId>-boiler` | `boiler` | (unchanged — slave-origin) | — |
| 2 | Thermostat | `<nodeId>-thermostat` | `thermostat` | (unchanged — master-origin) | — |
| 3 | Gateway | `<nodeId>-gateway` | `gateway` | OTGW *config* internals: operating mode, LED A-F, GPIO A/B + states, setback, override mode, smart-power, Vref, etc. | OT-core identity moves OUT (see #5) |
| 4 | Esp | `<nodeId>-esp` | `esp` | ESP-node diagnostics only: heap, WiFi, uptime, IP, MAC, OTA, MQTT/WS health, OLED presence | Dallas + S0 move OUT (see #7) |
| 5 | OT-Core | `<nodeId>-pic` **or** `<nodeId>-otdirect` | `pic` (HAS_PIC) / `otdirect` (HAS_DIRECT_OT) | PIC firmware version / device-id / fw-type (PIC) **or** OT-Direct mode / status (OTDirect) | **NEW** |
| 6 | Sat | `<nodeId>-sat` | `sat` | (unchanged — `sat/*`) | — |
| 7 | Sensors | `<nodeId>-sensors` | `sensors` | Dallas 1-wire temperatures + S0 pulse counter (+ future discrete hardware sensors) | **NEW** |

### 2. OT-Core device named per hardware (one enum slot, not two)

The OT-Core device is a single `HaDevice` value rendered as `pic` on a `HAS_PIC`
build and `otdirect` on a `HAS_DIRECT_OT` build — never both, because a build
links exactly one bus driver. The platform-conditional name lives in
`haDeviceSuffix()` / the device-name switch, gated by the existing `HAS_PIC` /
`HAS_DIRECT_OT` capability flags (no raw `#ifdef` outside the platform layer).

### 3. Gateway as `via_device` hub

Every non-Gateway device emits `"via_device":"<nodeId>-gateway"` in its modern
device block (Boiler, Thermostat, Esp, OT-Core, Sat, Sensors). HA then nests them
under the Gateway. This is emitted only in modern mode; legacy single-device mode
is unchanged.

### 4. Routing and re-homing

`haDeviceForEntity()` (the single auditable map) gains the two new buckets:

- PIC firmware-info entities → `HaDevice::Pic` on `HAS_PIC`; OT-Direct status
  entities → `HaDevice::Pic` on `HAS_DIRECT_OT`.
- Dallas temps + S0 pulse counter → `HaDevice::Sensors` (was `HaDevice::Esp`).

All other routing from ADR-122 (slave→Boiler, master→Thermostat, bilateral→both,
OTGW-config→Gateway, ESP-diagnostics→Esp, SAT→Sat) is unchanged.

## Alternatives Considered

### Alternative A: Keep the ADR-122 five-device topology

Rejected. It is the status quo this ADR revises. It leaves the OT-core with no
distinct identity (folded into Gateway) and groups physical sensors with ESP
diagnostics — the two semantic mismatches that motivated the change.

### Alternative B: Two enum values, `Pic` and `OTDirect`

Rejected. A firmware build is `HAS_PIC` **xor** `HAS_DIRECT_OT`; one of the two
would never be populated, wasting an enum slot and a `devMeta[]`/`deviceIntroduced[]`
array index, and risking an empty device block. One slot named per hardware
captures "the OT-core, whatever drives it" without the dead value.

### Alternative C: No `via_device` (keep HA-core parity, as ADR-122 §8)

Rejected by the maintainer in favour of a nested hierarchy. This is the one point
where the firmware now **deliberately diverges from HA core 2024.10**, which sets
no `via_device`. The trade-off is documented in Consequences; the maintainer
judges the nested Gateway hub clearer for users than peer-level parity with HA
core.

### Alternative D: A combined "Hardware" device (sensors + OT-core together)

Rejected. The OT-core (an OpenTherm interface with its own firmware) and the
ambient hardware sensors (temperature, energy pulses) are unrelated subsystems;
merging them would reproduce the same "wrong bucket" problem one level over.

## Consequences

**Benefits**

- The OT-core (PIC/OTDirect) is a first-class device with its own name, version,
  and lifecycle, cleanly separated from OTGW gateway-config entities.
- Physical sensors (Dallas, S0) get a dedicated `Sensors` card instead of cluttering
  ESP diagnostics.
- HA renders one Gateway hub with the other devices nested under it, instead of
  seven peers.

**Trade-offs**

- **Deliberate divergence from HA core 2024.10 on `via_device`.** ADR-122 §8
  matched HA core by omitting it; this ADR adds it. Dual-use setups (firmware
  discovery alongside the HA `opentherm_gw` integration) will show the firmware's
  devices nested and HA core's trio flat. Accepted as a UX preference.
- **One-time HA entity re-home churn** for the moved entities: Dallas temps + S0
  (Esp → Sensors) and PIC/OTDirect info (Gateway → OT-Core) change their owning
  device. HA moves them on the next discovery republish; automations pinned to the
  old device are unaffected (entity unique_ids are `<nodeId>-<device>-<label>`, so
  a re-home **does** change the unique_id of the moved entities → a registry event,
  same class of one-time churn ADR-122 already accepted on the 2.0.0 upgrade).
- **`devMeta[]` / `deviceIntroduced[]` arrays grow 5 → 7** and every `< 5` index
  guard becomes `< 7`: two extra `HaDeviceMeta` structs + two bools of RAM. Inserting
  `Pic` at index 4 shifts `Sat`'s enum value 4 → 5; the enum is transient (never
  persisted), so this is safe provided no code hardcodes a numeric device index.
- **`via_device` adds ~30 bytes** to each non-Gateway device's first-entity block.
  Discovery is streamed (ADR-077) and burst-windowed (ADR-088); negligible.

**Risks and mitigations**

- *Risk:* the golden-file discovery test still asserts five device blocks and
  fails, or worse, is updated carelessly and stops guarding routing.
  *Mitigation:* TASK-826 updates `check_ha_discovery_device_routing` to seven
  devices + via_device as a Definition-of-Done item in the same commit (ADR-080).
- *Risk:* a contributor adds a sensor entity and forgets it now routes to
  `Sensors`, dropping it back onto Esp. *Mitigation:* routing remains centralized
  in `haDeviceForEntity()`; the CI gate checks the entity-set routing.
- *Risk:* the per-hardware OT-Core name leaks a raw `#ifdef` into discovery code.
  *Mitigation:* gate on the existing `HAS_PIC` / `HAS_DIRECT_OT` capability flags
  inside the platform-allowed switch, per the ESP-abstraction rule.

## Enforcement

```json
{
  "llm_judge": true,
  "guidance": "MQTT HA discovery device topology MUST be the seven-device set {Boiler, Thermostat, Gateway, Esp, Pic, Sat, Sensors} in modern mode. Flag changes that: (a) add or remove a HaDevice enum value without superseding this ADR; (b) route Dallas temperature or S0 pulse-counter entities to any device other than HaDevice::Sensors; (c) route PIC firmware-info or OT-Direct status entities to any device other than HaDevice::Pic; (d) emit a non-Gateway device block without via_device:<nodeId>-gateway in modern mode; or (e) emit via_device in legacy (bLegacyMode) mode. The golden-file discovery test in tests/ and check_ha_discovery_device_routing in evaluate.py are the authoritative gate; changes to device routing must update both."
}
```

## Related Decisions

- **ADR-122 (HA Discovery Five-Device Topology)**: *superseded by this ADR.*
  ADR-122's `haDeviceForEntity()` routing, two-axis legacy model (`bLegacyMode`
  for topology/unique_id, `bUseLegacyOtTopics` for topic naming), bilateral
  replication, and migration/orphan-cleanup design are all carried forward
  unchanged; only the device set (5→7), the OT-core/Sensors split, and the
  `via_device` decision are revised here.
- **ADR-077 (Streaming MQTT HA Discovery Architecture)**: unchanged; this ADR only
  changes what is emitted, not how streaming operates. Per-device first-entity
  tracking now spans seven slots.
- **ADR-070 (Orphan cleanup of stale discovery topics)**: the re-home of moved
  entities relies on the orphan-cleanup path to clear their old-device retained
  payloads.
- **ADR-106 (MQTT Topic Naming Mode)**: complementary and unchanged; `bUseLegacyOtTopics`
  governs topic naming independently of device topology.
- **ADR-088 (MQTT Status-Burst Windowing)**: the discovery burst (now seven devices)
  must stay within the burst window; verified at implementation.
- **ADR-080 (Binding ADRs must have a CI gate)**: governs the binding classification.

## References

- TASK-826 (this work; seven-device topology implementation).
- ADR-122 (superseded): `docs/adr/ADR-122-ha-discovery-five-device-topology.md`.
- HA core reference (for the divergence note): `home-assistant/core` `opentherm_gw`
  2024.10 (PR #124869) — sets no `via_device`.
- Affected code (current five-device state): `src/OTGW-firmware/MQTTstuff.h`
  (`HaDevice` enum ~line 366, `devMeta[]`/`deviceIntroduced[]` arrays),
  `src/OTGW-firmware/MQTTHaDiscovery.cpp` (`haDeviceSuffix` ~2222, device-name
  switches ~2580/~3798, `writeDeviceBlock` ~2241, `< 5` guards),
  `src/OTGW-firmware/MQTTstuff.ino` (Dallas routing ~2815, devMeta population).
