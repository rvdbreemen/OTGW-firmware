# ADR-148: BLE Sensors as Separate Home Assistant Child Devices via via_device (amend ADR-140's single-device rule for BLE probes only)

## Status

Proposed, 2026-06-20.
Guideline-level (per ADR-080): like the parent ADR-140, the discovery payload
shape is verified by field validation (a captured discovery dump on a real
device with BLE probes present), not by a clean `evaluate.py` forbid/require
pattern. There is no mechanically checkable forbid/require rule that expresses
"BLE probe entities form their own HA child device linked by via_device while
every other entity stays in the single device", so no CI gate is invented here.
**Amends ADR-140.** It does NOT revive ADR-124 (the seven-device split), which
stays superseded.

## Status History

status_history:
  - date: 2026-06-20
    status: Proposed
    changed_by: Agent
    reason: After shipping the BLE-probe HA discovery (TASK-871/872, commit 7e3da75d), the implementation emits each BLE probe as its own HA child-device linked to the main OTGW device via via_device, which diverges from ADR-140 §1 ("no via_device") and §2 ("BLE sensors are sat_-prefixed entities inside the single device"). The maintainer has decided to KEEP the BLE-as-child-device behaviour because BLE probes are physically separate wireless sensors with their own battery/RSSI/identity. This ADR narrows ADR-140's single-device rule to sanction that carve-out for BLE probes only; all non-BLE entities stay inside the single device exactly as ADR-140 specifies.
    changed_via: adr-kit

## Context

ADR-140 (Accepted, 2026-06-15) returned the 2.0.0 Home Assistant (HA) discovery
topology to ONE device per hardware OTGW after field testing found ADR-124's
seven-device split confusing. Bluetooth Low Energy (BLE) probes are wireless
SAT temperature/humidity sensors discovered over the air. ADR-140 is explicit
on two points that this ADR narrows:

- §1 "One device": every entity carries the identical
  `"dev":{"identifiers":<nodeId>}` and there is "No `via_device`, no
  multi-device split".
- §2, sensor recognizability: "BLE sensors are `sat_`-prefixed and keep a `BLE`
  token in the entity name (e.g. `sat_ble_temp` -> 'SAT BLE Temp')", i.e. BLE
  sensors live as entities INSIDE the single device.

The BLE-probe HA discovery shipped afterwards (TASK-871 collapse + TASK-872
prefix work, code landed in commit 7e3da75d) does something different for BLE
probes specifically. `satBLEPublishOneDiscovery()` emits each BLE probe as its
own HA child-device: it builds a `dev` block whose `identifiers` is
`<uniqueId>_ble_<macCompact>` and adds a `via_device` key pointing at the main
OTGW device. The relevant lines are `src/OTGW-firmware/MQTTstuff.ino:2963` (label
branch) and `:2989` (legacy branch), both emitting `"via_device":"%s"` with the
main device's `uniqueId`.

That `via_device` value is the same string the single OTGW device uses as its
`identifiers`. The single-device block sets `identifiers` to `ctx.nodeId`
(`MQTTstuff.ino:2277-2278`), `ctx.nodeId` is assigned from the global `NodeId`
(`MQTTstuff.ino:2216`), and `NodeId` is a copy of `settings.mqtt.sUniqueid`
(`MQTTstuff.ino:584`). The BLE function reads the same `settings.mqtt.sUniqueid`
into its local `uniqueId` (`MQTTstuff.ino:2921`). So the BLE child device's
`via_device` correctly references the parent OTGW device, and Home Assistant
nests each BLE probe under the main OTGW device card.

This carve-out only manifests when BLE probes are actually discovered. A live
broker capture on 2026-06-20 (test-rig broker, no BLE probes present) showed the
device publishing 105 entities all under ONE device identifier with zero
`via_device` keys. The `via_device` blocks appear only once a BLE probe is seen.

The maintainer (Robert, 2026-06-20) has decided to KEEP this behaviour rather
than fold BLE probes back inside the single device. The rationale: a BLE probe
is a physically separate wireless sensor with its own battery level, RSSI, and
identity. Modelling each probe as an HA child-device linked to the main OTGW
device via `via_device` is more faithful to the physical topology than rendering
its temperature, humidity, battery, and RSSI as four flat entities buried in the
gateway's entity list. It lets Home Assistant group and track each probe
independently (per-probe device card, per-probe area assignment, per-probe
availability and battery tracking) while still expressing the "this probe is
reached through that gateway" relationship.

## Decision

**BLE probe sensors are emitted as separate Home Assistant child-devices, each
with a `via_device` key pointing at the main OTGW device's identifier
(`nodeId` = `settings.mqtt.sUniqueid`). All non-BLE entities continue to live
inside the single OTGW device exactly as ADR-140 specifies.**

This narrows ADR-140 as follows:

- **ADR-140 §1 ("No `via_device`")** is narrowed to: non-BLE entities carry no
  `via_device` and stay folded into the single OTGW device. BLE probe child
  devices DO carry `via_device`, pointing at the single OTGW device.
- **ADR-140 §2 (BLE sensors as `sat_`-prefixed entities inside the single
  device, e.g. `sat_ble_temp` -> "SAT BLE Temp")** is narrowed to: each BLE
  probe is its own HA child-device with its own `dev` block
  (`identifiers = <uniqueId>_ble_<macCompact>`, `model = "BLE Sensor"`,
  `manufacturer = "BLE"`), grouping that probe's temperature, humidity, battery,
  and RSSI under one per-probe device card. The probe's entity name retains a
  `BLE` token (or the user label per TASK-508). The child device is linked to the
  parent OTGW device by `via_device`. Consequently, ADR-140 §2's `sat_` in-device
  name-prefix no longer governs BLE entities: each probe is its own HA device with
  its own `dev` block, so the per-probe child-device model replaces the
  `sat_`-prefix grouping for BLE, not merely the `via_device` placement. The
  `sat_` prefix continues to govern the non-BLE SAT engine entities.

Everything else in ADR-140 is unchanged and remains binding:

- Exactly one OTGW device per hardware OTGW, identifier = bare `nodeId`.
- The five source/engine prefixes (`esp_`, `pic_`, `otd_`, `sat_`, `sensors_`)
  for all non-BLE entities, the bilateral `_boiler`/`_thermostat` suffix, the
  Dallas `sensors_` per-probe naming, `device_class`/`unit`/`state_class`
  typing, and the orthogonal `entity_category` layer all stay as written.
- The driver-set first-entity gate (F1 fix) and device-string escaping (F5)
  stay as written.

This amendment is BLE-probe-scoped only. It does NOT reintroduce ADR-124's
seven-device split for OTGW engine entities, and it does NOT add `via_device` to
any non-BLE entity. ADR-124 remains superseded by ADR-140.

## Alternatives Considered

### Alternative A: Revert the code to fold BLE probes inside the single device (strict ADR-140 §2)

Remove `via_device` from `satBLEPublishOneDiscovery()` and emit each BLE probe's
temperature/humidity/battery/RSSI as `sat_ble_*` entities under the single OTGW
device `identifiers`, exactly as ADR-140 §2 literally reads.

Rejected. It collapses every physically distinct wireless probe into the
gateway's flat entity list, where HA cannot group a probe's four readings, assign
it to a room/area, or track its battery and availability as a unit. It loses the
per-probe identity that the BLE probes inherently have. The maintainer judged
the physical-topology fidelity worth the deviation from single-device
simplicity.

### Alternative B: BLE probes as child devices via via_device (chosen)

Keep the shipped behaviour: each BLE probe is its own HA child-device with a
`via_device` link to the main OTGW device; all non-BLE entities stay in the
single device.

Chosen. It matches the physical reality (separate battery-powered wireless
sensors reached through the gateway), gives each probe an independent HA device
card with per-probe battery/RSSI/availability, and keeps the relationship to the
gateway explicit through `via_device`, without reopening the multi-device split
for the gateway's own engine entities.

### Alternative C: Full multi-device split (revert to ADR-124)

Return to splitting the gateway itself into multiple HA devices with a
`via_device` hub, of which BLE probes would just be more nodes.

Rejected. ADR-140 already killed this for good reason (field testing found it
confusing, plus the F1 two-pass-determinism bug). BLE probes being separate
physical sensors is a categorically different case from artificially fragmenting
one physical gateway into seven HA devices. This amendment deliberately keeps the
gateway single-device and limits the carve-out to genuinely separate hardware.

## Consequences

**Benefits**

- Each BLE probe appears in Home Assistant as its own device card, with its
  temperature, humidity, battery, and RSSI grouped under it, and can be assigned
  to a room/area and tracked for availability and battery independently.
- The physical topology is expressed faithfully: `via_device` records that each
  probe is reached through the OTGW, mirroring the real wireless link.
- No change to the gateway's own single-device model: all 100+ non-BLE entities
  stay folded into the one OTGW device, so ADR-140's readability win is
  preserved for everything except genuinely separate hardware.

**Trade-offs**

- This deviates from the single-device simplicity ADR-140 deliberately chose.
  ADR-140's mental model "one hardware device = one IP = one MQTT device in HA"
  now has a documented exception: a BLE probe is separate hardware (its own
  battery and radio) even though it has no IP of its own, so it gets its own HA
  device linked by `via_device`. The exception is justified by physical-probe
  identity, but it does make the topology rule conditional rather than absolute.
- Installs that previously received BLE entities under the single device (if any
  shipped that way) would see those become orphaned when the child-device form
  takes over; the same one-time manual-cleanup caveat ADR-140 and ADR-124
  already carry applies here.

**Risks and mitigations**

- *Risk*: the carve-out is read as a licence to reintroduce `via_device` for
  non-BLE entities or to re-split the gateway.
  *Mitigation*: the Decision states explicitly that this is BLE-probe-scoped,
  that ADR-124 stays superseded, and that no non-BLE entity may carry
  `via_device`.
- *Risk*: the `via_device` target drifts from the main device's `identifiers`
  (e.g. a future change makes `NodeId` no longer equal `settings.mqtt.sUniqueid`),
  silently orphaning every BLE child device.
  *Mitigation*: the equality `via_device` value = `uniqueId` = `NodeId` =
  `settings.mqtt.sUniqueid` = main device `identifiers` is recorded in Context
  and References with `file:line` anchors; field validation of a discovery dump
  with a BLE probe present confirms HA nests the probe under the OTGW device.

## Related Decisions

- **ADR-140 (Single-Device HA Discovery Topology)**: **amended by this ADR.**
  ADR-140 stays Accepted and immutable; this ADR narrows its §1 "no via_device"
  and §2 "BLE sensors inside the single device" rules so that BLE probes become
  child devices linked by `via_device`. All other ADR-140 provisions are
  unchanged.
- **ADR-124 (Seven-Device Topology)**: remains superseded by ADR-140. This
  amendment does NOT revive it; the carve-out is BLE-probe-only.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this amendment is labeled
  guideline-level because, like ADR-140, the payload shape is field-validated,
  not CI-gated. No automated gate is invented.
- **ADR-077 (Streaming MQTT HA Discovery Architecture)**: the discovery emission
  architecture this BLE path rides on; untouched. The BLE child-device payload is
  composed and published through the same single-buffer / `mqttPublishRaw`
  chokepoint, so no streaming-contract change is implied.
- **ADR-106 (MQTT Topic Naming Mode)**: orthogonal (topic label naming, not
  device topology). Untouched.

## References

- Implementing code (BLE child device with `via_device`):
  `src/OTGW-firmware/MQTTstuff.ino:2963` (label branch) and `:2989` (legacy
  branch), inside `satBLEPublishOneDiscovery()`.
- `via_device` target equality (BLE child links to the parent OTGW device):
  BLE `uniqueId` from `settings.mqtt.sUniqueid` (`MQTTstuff.ino:2921`); main
  device `identifiers = ctx.nodeId` (`MQTTstuff.ino:2277-2278`);
  `ctx.nodeId = NodeId` (`MQTTstuff.ino:2216`); `NodeId` copied from
  `settings.mqtt.sUniqueid` (`MQTTstuff.ino:584`).
- Single-device block (no `via_device`, for contrast): `buildDiscoveryDeviceBlock()`
  at `MQTTstuff.ino:2256-2285`.
- Tasks: TASK-871 (collapse HA discovery to single device, ADR-140 F1/F5),
  TASK-872 (in-device source-prefix grouping). Implementing commit: 7e3da75d
  (<https://github.com/rvdbreemen/OTGW-firmware/commit/7e3da75d>).
- Home Assistant MQTT discovery device/`via_device` semantics:
  <https://www.home-assistant.io/integrations/mqtt/#sharing-device-information>.
- Live evidence: test-rig broker capture, 2026-06-20: with no BLE probes present,
  105 entities published under one device identifier, `via_device` count = 0;
  the carve-out manifests only when BLE probes are discovered.
- Parent decision: ADR-140
  (`docs/adr/ADR-140-single-device-ha-topology-entity-category-clustering.md`).

This ADR has a code surface (the `via_device` emit in `satBLEPublishOneDiscovery`)
but is enforced at field validation and PR review, not by an automated gate:
there is no clean forbid/require pattern for "BLE probe entities form their own
HA child device linked by via_device while every other entity stays in the single
device". For now it is guideline-level per ADR-080, mirroring its parent ADR-140.
