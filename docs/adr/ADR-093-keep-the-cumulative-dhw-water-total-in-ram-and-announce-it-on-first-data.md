---
id: "ADR-093"
title: "Keep the cumulative DHW water total in RAM and announce it on first data"
status: "Proposed"
date: "2026-08-26"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "madr"
topics:
  - "home-assistant"
  - "mqtt-discovery"
  - "opentherm"
  - "persistence"
aliases:
  - "DHW water total"
  - "dhw_water_total"
  - "MsgID 19 integration"
  - "water meter gap clamp"
components:
  - "DHW water total counter"
symbols:
  - "updateDHWWaterMeter"
  - "dhwWaterMeterHasData"
  - "publishDHWWaterMeter"
  - "DHW_METER_MAX_GAP_MS"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-093 Keep the cumulative DHW water total in RAM and announce it on first data

## Status

Proposed, 2026-08-26.

## Status History

```yaml
status_history:
  - date: 2026-08-26
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context and Problem Statement

ADR-090 decided that the gateway should publish a cumulative DHW (domestic hot
water) volume so the Home Assistant Energy dashboard works without the user
building a Riemann-sum helper. That decision stands. Its Decision Contract,
written before any code existed, additionally required three things that turned
out to be wrong once the feature was built and tested against a live Home
Assistant:

1. **Persist the running total across reboot**, in its own small file, with an
   explicit write-rate bound.
2. **Register the entity in the boot-publish path** for non-OT discovery
   configs, because otherwise it is "absent in Home Assistant until the first
   value arrives".
3. Keep the MsgID 19 rate sensor on unit `l/min`.

Implementation (TASK-1091) and a live verification produced three findings that
contradict those requirements:

- **Persistence is not needed for the stated goal, and its failure mode is
  worse than the problem it solves.** Home Assistant's `total_increasing` state
  class treats any decrease as a meter reset and continues the long-run sum, so
  a counter that restarts at zero after a reboot does not lose dashboard
  history. ADR-090 itself identified the sharpest risk of persisting: a power
  loss between two flash writes restores a counter that is lower than the last
  published value, which Home Assistant reads as a *partial* reset, silently
  corrupting the statistic. ADR-090 left that risk unresolved. Not persisting
  removes it entirely, because every reboot is then a clean reset rather than a
  partial one. The existing `s0pulsecounttot` counter in this firmware is
  RAM-only for the same reason (`src/OTGW-firmware/OTGW-firmware.h:573`).
- **Boot-publishing the entity manufactures a meter that does not exist.** The
  gateway does not poll MsgID 19; frames appear only when the thermostat
  requests that id, and many thermostats never do. Registering the pseudo-ID in
  `publishNonOTDiscoveryConfigs()` therefore gives *every* gateway a water meter
  entity pinned at 0.0 L, including those whose bus carries no flow data at all.
  What ADR-090 recorded as a defect to avoid ("absent until the first value
  arrives") is the correct behaviour: absence is the honest signal.
- **`l/min` is rejected by Home Assistant.** `DEVICE_CLASS_UNITS` in
  `homeassistant/components/sensor/const.py` lists
  `UnitOfVolumeFlowRate.LITERS_PER_MINUTE`, whose string is `L/min` with a
  capital L. With `device_class: volume_flow_rate` attached, the lowercase
  string makes Home Assistant discard the whole discovery config and the sensor
  disappears (GH #675, fixed in TASK-1092).

A fourth question ADR-090 did not address at all: what the integrator does when
the bus falls silent. That is the single most consequential detail in a counter
built on an unpolled message id, and it needs a recorded decision.

## Decision Drivers

* Home Assistant's `total_increasing` semantics already solve reboot continuity,
  so persistence buys dashboard history that is not actually at risk.
* The partial-restore corruption that ADR-090 named as its sharpest risk has no
  mitigation that fits an unclean power loss on this platform.
* LittleFS erase cycles are a finite budget on the ESP8266, and this firmware
  already manages flash and RAM headroom deliberately (ADR-030).
* MsgID 19 arrival is a property of the installation, not of the firmware:
  it may arrive every second, only during a draw, or never.
* An entity that reads 0.0 L forever on a system with no flow data is
  indistinguishable from a broken meter, and generates support questions.
* A litre figure invites meter-grade expectations that a sparse sample stream
  cannot support, whatever the integration method.

## Considered Options

* **Option A — RAM-only counter, JIT discovery, explicit gap clamp.** Integrate
  in RAM, publish nothing until the first MsgID 19 frame decodes, announce
  discovery at that same moment, and refuse to integrate any interval longer
  than a fixed clamp.
* **Option B — Comply with ADR-090 as written.** Add a persisted counter in its
  own LittleFS file with a bounded write cadence, and register the pseudo-ID in
  the boot-publish path.
* **Option C — Withdraw the firmware counter.** Revert to ADR-090's Option B
  (rate sensor only) and document the Home Assistant integration helper as the
  supported route.

## Decision Outcome

Chosen option: **Option A**, because it delivers the same user-visible outcome
as ADR-090 (a working Energy dashboard figure with no user configuration) while
removing the one risk ADR-090 could not resolve, and because it answers the
gap-handling question that ADR-090 left open.

Concretely, superseding ADR-090's contract on three points:

**The total is not persisted.** `dhwWaterTotalL` lives in RAM
(`src/OTGW-firmware/dhwWaterMeter.ino:40`) and starts at zero on every boot.
Home Assistant reads the drop as a meter reset and keeps the long-run sum, so
the Energy dashboard survives what the gateway forgets. No LittleFS file, no
write-cadence rule, and no partial-restore window.

**Discovery is announced just in time, on first data.** `publishDHWWaterMeter()`
returns without publishing until `dhwWaterMeterHasData()` is true, and queues
the discovery config at that same first publish
(`src/OTGW-firmware/MQTTstuff.ino:1139-1152`). This matches how OT message ids
already behave in this firmware: configs publish JIT as each id arrives, and
only the genuinely device-wide entities are queued at boot. A gateway whose bus
never carries MsgID 19 gains no entity at all.

**A silent bus adds no water.** Each sample is applied to the interval that
preceded it, and an interval longer than `DHW_METER_MAX_GAP_MS`
(60 seconds, `src/OTGW-firmware/dhwWaterMeter.ino:38`) is treated as a hole in
the measurement rather than as flow
(`src/OTGW-firmware/dhwWaterMeter.ino:60`). This is deliberately *not* the
behaviour of Home Assistant's `max_sub_interval`, which holds the last reading
across silence and keeps integrating: on a bus that goes quiet at 8 l/min, that
books water that never flowed. Under-counting a gap is recoverable; inventing
volume is not.

ADR-090's remaining requirements stand unchanged and are satisfied: the total is
a separate entity from the rate sensor, accumulation is elapsed-time times flow
rather than a fixed volume per sample, `device_class: water` appears only with a
unit from the permitted set, and no `String` is used in the integration or
publish path (ADR-004).

### Confirmation

The entity appears in Home Assistant with `device_class: water`, unit `L` and
`state_class: total_increasing`, and is offered as an Energy dashboard water
source. A gateway that never sees MsgID 19 gains no such entity. The counter
starts at zero after a reboot, and Home Assistant's long-run statistic
continues across that reset rather than dropping.

Verified on 2026-08-26 against the live Home Assistant at
`homeassistant.local:8123` without flashing, by publishing a discovery config
carrying exactly the fields this entity emits: `GET
/api/states/sensor.otgw_unit_probe` returned 200 with `state: "42.0"` and
attributes `device_class: water`, `unit_of_measurement: L`, `state_class:
total_increasing`. The paired negative control for TASK-1092 (the same config
with lowercase `l/min` on `volume_flow_rate`) produced no entity at all, which
is what a unit rejection looks like from the outside.

## Decision Contract

### Must

* Publish the cumulative total as a separate entity from the MsgID 19 rate
  sensor, which keeps `device_class: volume_flow_rate` and the unit string
  `L/min` (capital L, the value Home Assistant accepts).
* Accumulate as elapsed time times flow, never as a fixed volume per received
  sample.
* Refuse to integrate an interval longer than an explicit clamp, and update the
  sample timestamp anyway so the skipped gap is not charged to the next sample.
* Withhold both the state publish and the discovery announcement until at least
  one MsgID 19 frame has decoded on this boot.
* Feed the integrator only from frames that are valid for the master topic, so
  gateway substitutions and answer overrides (ADR-066, ADR-082) never reach the
  meter.
* State the sampling limitation wherever the entity is described to users, so
  the number is not mistaken for a metrologically valid water meter.

### Must Not

* Persist the running total to flash, in any file, at any cadence.
* Register the water-total pseudo-ID in `publishNonOTDiscoveryConfigs()` or any
  other unconditional boot-publish path.
* Set `device_class: water` on any sensor whose unit is not one of
  `L, gal, m3, ft3, CCF, MCF`.
* Introduce the `String` class in the integration or publish path (ADR-004).

### Exceptions

* None.

### Verification

* `src/OTGW-firmware/dhwWaterMeter.ino` — integrator, clamp, and the
  first-frame gate.
* `test/host/test_dhwWaterMeter.cpp` — host-compiled tests covering normal
  cadence, the clamp boundary at 60000 and 60001 ms, an over-clamp gap, the
  interval after a gap, `millis()` wrap, zero and negative flow, the
  no-data-before-first-frame gate, and monotonicity. Run with
  `test\run_tests.bat`.
* `src/OTGW-firmware/mqtt_configuratie.cpp:1127` — the pseudo-ID 243 discovery
  row.
* Manual check against a live Home Assistant that the entity is offered as an
  Energy dashboard water source.

## Consequences

### Positive

* The partial-restore statistic corruption that ADR-090 named as its sharpest
  risk cannot occur, because there is no restore.
* No LittleFS erase cycles are spent on this feature at all.
* Cost is 8 bytes of RAM for the counter state and 400 bytes of flash for the
  code and the discovery row, measured as the difference between the beta.3
  build with and without this change (761208 to 761608 bytes; the later
  761588 figure is after the first-data gate replaced the dead force branch).
* Gateways with no MsgID 19 traffic are unaffected: no entity, no retained
  topic, no support question about a meter stuck at zero.
* A silent bus can only under-count, never over-count.

### Negative

* **The total resets to zero on every reboot.** Home Assistant's long-run sum
  survives, but the entity's own value does not, and a user reading the entity
  directly (rather than the Energy dashboard) sees a counter that restarts.
  *Mitigation:* `state_class: total_increasing` is exactly the contract for
  this, and the reset is clean rather than partial. Say so in the release note.
* **Accuracy remains bounded by the bus, not by the code.** MsgID 19 arrives
  only when a master asks. The clamp bounds the error rather than removing it:
  a draw sampled every 30 seconds is integrated in 30-second rectangles, and
  the right-hand rule under-counts the ramp at the start of a draw by roughly
  one sample interval. *Mitigation:* state the limitation in user-facing
  documentation, as ADR-090 already required.
* **A draw shorter than the sampling interval can be missed entirely.** If the
  bus carries one MsgID 19 frame per minute and a tap runs for ten seconds
  between two frames, the flow may never be sampled while non-zero.
  *Mitigation:* none available in firmware; this is a property of an unpolled
  message id. Users who need better resolution can request MsgID 19 more often
  from the thermostat side (`AA=19`).
* **Two totals can coexist and disagree.** A user who already built the Home
  Assistant helper now has both. They will not match, because
  `max_sub_interval` integrates across silence and the firmware clamp does not.
  *Mitigation:* documented in the GH #675 reply, with an invitation to report
  divergence.

## Pros and Cons of the Options

### Option A — RAM-only counter, JIT discovery, explicit gap clamp

* Good, because it removes ADR-090's unresolved partial-restore risk rather
  than mitigating it.
* Good, because it spends no flash-erase cycles.
* Good, because installations without MsgID 19 traffic gain nothing rather than
  a meter fixed at zero.
* Good, because the gap rule is stated and tested rather than implied.
* Bad, because the entity's own value restarts at zero on every reboot, which
  looks wrong to a user who reads the entity instead of the dashboard.

### Option B — Comply with ADR-090 as written

* Good, because the entity's value survives a graceful reboot, which is what a
  user intuitively expects of a meter.
* Good, because it needs no change to an Accepted decision.
* Bad, because the unclean-power-loss case restores a partial value and
  corrupts the Home Assistant statistic in a way that is silent and hard to
  detect. ADR-090 recorded this as unresolved.
* Bad, because it spends LittleFS erase cycles on a derived metric.
* Bad, because boot-publishing gives every gateway a water meter entity,
  including the majority whose bus carries no MsgID 19 frames.

### Option C — Withdraw the firmware counter

* Good, because it is the smallest possible firmware surface.
* Good, because a Home Assistant helper is user-resettable and user-tunable.
* Bad, because it reverses a maintainer decision that the Energy dashboard
  should work out of the box, and MQTT discovery cannot create the helpers on
  the user's behalf.

## Open Questions

* None.

## Related Decisions

* **ADR-090 (Publish a firmware-integrated cumulative DHW water total for the
  Home Assistant Energy dashboard)**: superseded by this ADR. The decision to
  ship a firmware-side total is carried forward unchanged; three requirements of
  its Decision Contract (persistence, boot-publish registration, and the
  `l/min` unit string) are replaced.
* **ADR-004 (Static Buffer Allocation Strategy)**: constrains the integration and
  publish path; satisfied.
* **ADR-030 (Heap Memory Monitoring and Emergency Recovery)**: the heap and
  headroom regime ADR-090 cited when weighing this feature's cost.
* **ADR-066 (MQTT Publish Gating by Source and Per-MsgID Slave-Echo Classification)** and **ADR-082 (Surface gateway overrides as distinct override state)**: define
  which frames may feed device state; the meter is fed only from master-valid
  frames.

## References

* TASK-1091 (implementation), TASK-1092 (the `L/min` unit fix this depends on).
* GH #675 — the original request, the reporter's Home Assistant helper recipe,
  and the error log that identified the unit rejection:
  <https://github.com/rvdbreemen/OTGW-firmware/issues/675>
* `src/OTGW-firmware/dhwWaterMeter.ino:38,40,48,60,71`
* `src/OTGW-firmware/OTGW-Core.ino:2013` — the MsgID 19 hook, on the
  master-valid branch of `print_f88()`.
* `src/OTGW-firmware/MQTTstuff.ino:1139-1152` — first-data gate and JIT
  discovery announcement.
* `src/OTGW-firmware/mqtt_configuratie.cpp:653,1127,1451` — sensor count,
  pseudo-ID 243 row, and index entry.
* `src/OTGW-firmware/OTGW-firmware.ino:262` — 60-second republish.
* `src/OTGW-firmware/OTGW-firmware.h:573` — `OTGWs0pulseCountTot`, the existing
  RAM-only cumulative counter this follows.
* Home Assistant `DEVICE_CLASS_UNITS` and `DEVICE_CLASS_STATE_CLASSES`:
  <https://github.com/home-assistant/core/blob/dev/homeassistant/components/sensor/const.py>
* Home Assistant integration (Riemann sum) `max_sub_interval`, the host-side
  behaviour this decision deliberately differs from:
  <https://www.home-assistant.io/integrations/integration/>

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "dhwWaterTotalL[^;]*(writeFile|LittleFS|\\.open\\()",
      "path_glob": "src/OTGW-firmware/**",
      "message": "ADR-093: the DHW water total is RAM-only. Do not persist it to flash."
    },
    {
      "pattern": "publishNonOTDiscoveryConfigs[\\s\\S]{0,400}?OTGWdhwmeterid",
      "path_glob": "src/OTGW-firmware/MQTTstuff.ino",
      "message": "ADR-093: the water-total pseudo-ID is announced JIT on first MsgID 19 data, not at boot."
    }
  ],
  "forbid_import": [],
  "require_pattern": [
    {
      "pattern": "DHW_METER_MAX_GAP_MS",
      "path_glob": "src/OTGW-firmware/dhwWaterMeter.ino",
      "message": "ADR-093: the gap clamp must remain an explicit named constant."
    }
  ]
}
```
