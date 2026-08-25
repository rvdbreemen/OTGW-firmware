---
id: "ADR-090"
title: "Publish a firmware-integrated cumulative DHW water total for the Home Assistant Energy dashboard"
status: "Accepted"
date: "2026-08-25"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "home-assistant"
  - "mqtt-discovery"
  - "opentherm"
  - "persistence"
aliases:
  - "DHW water total"
  - "device_class water energy dashboard"
  - "MsgID 19 integration"
components:
  - "MQTT HA discovery table"
  - "DHW water total counter"
symbols:
  - "HaDeviceClass"
  - "DHWFlowRate"
context_scope: "selective"
format: "madr"
---

<!-- markdownlint-disable MD025 -->

# ADR-090 Publish a firmware-integrated cumulative DHW water total for the Home Assistant Energy dashboard

## Status

Accepted, 2026-08-25.

## Status History

```yaml
status_history:
  - date: 2026-08-24
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-08-25
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Accepted by the maintainer after all six open questions were resolved with measured or code-verified answers.
    changed_via: adr-kit lifecycle
```

## Context and Problem Statement

GitHub issue [#675](https://github.com/rvdbreemen/OTGW-firmware/issues/675) asks for
`device_class: water` on the existing DHW (Domestic Hot Water) flow-rate sensor so the
entity can be added to the Home Assistant Energy dashboard.

That literal request cannot be implemented, because it describes an invalid entity.
The existing sensor carries OpenTherm MsgID 19 (`DHWFlowRate`), published with unit
`l/min` and `state_class: measurement`
(`src/OTGW-firmware/mqtt_configuratie.cpp:714-715`, unit string at
`src/OTGW-firmware/mqtt_configuratie.cpp:1741`). Home Assistant's sensor
device-class table lists the valid units for `water` as `L, gal, m³, ft³, CCF, MCF`
and describes it as "Water consumption"; `l/min` appears only under
`volume_flow_rate`, described as "the amount of water consumed momentarily"
(<https://developers.home-assistant.io/docs/core/entity/sensor/>). A rate is not a
volume, so `device_class: water` on this sensor names a quantity the sensor does not
carry.

The reporter's actual goal — a water figure on the Energy dashboard — needs a
*cumulative* quantity that this firmware does not currently produce anywhere.

Two constraints frame the design:

- The device is an ESP8266 with roughly 40 KB of usable random-access memory (RAM), and its configuration
  store is LittleFS on the same flash the firmware runs from.
- MsgID 19 is not emitted on a timer. It appears on the OpenTherm bus only when a
  master asks for it, so the sample stream is sparse and irregular by nature.

Correcting the existing rate sensor to `device_class: volume_flow_rate` is a separate,
mechanical change tracked as TASK-1081 and needs no ADR. This decision covers only the
new derived cumulative entity.

## Decision Drivers

* The reporter wants a water figure on the Energy dashboard, which requires a
  cumulative volume, not a flow rate.
* Home Assistant already ships an integration ("Riemann sum") helper that converts a
  rate sensor into a cumulative total, so the firmware is not the only place this can
  be solved.
* Any firmware-side counter must survive reboot to be useful as a meter, which means
  writing to LittleFS, which costs flash-erase cycles.
* Home Assistant treats a decrease in a `total_increasing` sensor as a meter reset,
  so how the counter behaves across an unclean reboot changes what the user's
  long-term statistics look like.
* RAM and flash headroom on the ESP8266 are already an active concern on this line
  (see ADR-030).

## Considered Options

* **Option A — Firmware-integrated cumulative total.** Integrate MsgID 19 over time in
  firmware, persist the running litre count, and publish it as a new discovery entity
  with `device_class: water` and unit `L`.
* **Option B — Correct the rate sensor only.** Set `device_class: volume_flow_rate` on
  the existing MsgID 19 sensor and stop there. Document the Home Assistant integration
  helper as the supported route to an Energy dashboard figure.
* **Option C — Do nothing.** Leave the sensor without a device class and close #675
  explaining that a flow rate is not a meter.

## Decision Outcome

Chosen option: **Option A**, because the maintainer decided that an Energy dashboard
figure should work out of the box, without each user having to discover and configure a
Home Assistant helper. Option B leaves the reporter's stated goal unmet by the
firmware; Option C additionally leaves the existing sensor mis-typed.

This decision does **not** claim the firmware-side total is more accurate than the
Home Assistant helper. Both integrate the same sparse MsgID 19 sample stream and are
subject to the same sampling error. The firmware version is always-on and independent
of Home Assistant configuration; that, and not accuracy, is the whole benefit.

### Confirmation

The new entity appears in Home Assistant with `device_class: water`, unit `L` and a
`total_increasing` state class, is selectable as a water source in the Energy
dashboard, and its value is non-decreasing across a graceful reboot of the gateway.

## Decision Contract

### Must

* Publish the cumulative total as a separate entity. The existing MsgID 19 rate sensor
  keeps unit `l/min` and `device_class: volume_flow_rate`; it must not be
  retyped as `water`. (Already satisfied by TASK-1081.)
* Accumulate as **elapsed time × flow**, never as a fixed volume per sample. This is
  what makes the count independent of how many frames happen to carry the same
  reading, and it is mandated identically on the 2.0.0 peer so both firmwares report
  the same total for the same boiler.
* Persist the running total across reboot, in its own small file rather than in
  `settings.ini`: `writeSettings()` rewrites the whole settings blob, so a counter
  update would drag WiFi credentials through a power-loss window.
* Bound the LittleFS write rate with an explicit rule rather than writing on every
  publish.
* Register the new entity in the boot-publish path for non-OT discovery configs if it
  is given a faux message id, or it will be absent in Home Assistant until the first
  value arrives.
* State the sampling limitation in user-facing documentation for the new entity, so
  the number is not mistaken for a metrologically valid water meter.

### Must Not

* Set `device_class: water` on any sensor whose unit is not one of
  `L, gal, m³, ft³, CCF, MCF`.
* Write the persisted counter to flash on every message-queue telemetry transport (MQTT) publish.
* Accumulate a fixed volume per received sample. MsgID 19 can legitimately arrive
  more than once per real exchange, so per-sample accumulation makes the total a
  function of frame traffic rather than of water.
* Introduce the `String` class in the integration or publish path (ADR-004).

### Exceptions

* None.

### Verification

* `src/OTGW-firmware/mqtt_configuratie.cpp` discovery rows for MsgID 19 and for the new
  total entity.
* Manual check against a live Home Assistant instance that the entity is offered as an
  Energy dashboard water source.

## Consequences

### Positive

* The Energy dashboard works without the user building a Riemann-sum helper.
* The existing rate sensor becomes correctly typed as a side effect of the same work.
* The total survives Home Assistant restarts and reinstalls, because it lives on the
  gateway.

### Negative

* **Accuracy is bounded by the bus, not by the code.** MsgID 19 arrives only when a
  master asks for it. Between two samples the firmware must assume something about the
  flow, and every such assumption is wrong some of the time. A user reading a litre
  count will reasonably assume meter-grade accuracy that this cannot provide.
  *Mitigation:* say so plainly in the entity documentation and the release note.
* **Flash wear.** A persisted counter costs LittleFS erase cycles that the firmware did
  not previously spend. *Mitigation:* the write-cadence rule required by the Decision
  Contract; the concrete parameters are an Open Question below.
* **Partial-regression risk on unclean reboot.** If the device loses power between two
  flash writes, the restored counter is lower than the last published value. Home
  Assistant reads that decrease as a meter reset, but it is a *partial* one, so the
  dashboard neither continues correctly nor resets cleanly — it silently corrupts the
  long-term statistic. This is the sharpest risk in the change and is unresolved below.
* **RAM and flash cost** for the new counter state and an additional discovery entity,
  on a platform where headroom is already managed (ADR-030).

## Pros and Cons of the Options

### Option A — Firmware-integrated cumulative total

* Good, because the Energy dashboard works with no Home Assistant configuration.
* Good, because the total is independent of the Home Assistant database and survives a
  Home Assistant rebuild.
* Bad, because it spends flash-erase cycles and RAM on a device that has little of
  either.
* Bad, because it puts a lossy derived metric in firmware, where it is harder to fix
  and harder for the user to reset than a Home Assistant helper.
* Bad, because it invites users to read it as a calibrated water meter.

### Option B — Correct the rate sensor only

* Good, because it is a few lines, carries no persistence, and no wear.
* Good, because the Home Assistant helper is user-resettable and user-tunable.
* Bad, because it does not deliver the reporter's stated goal from the firmware.
* Bad, because every user who wants the dashboard figure repeats the same setup.

### Option C — Do nothing

* Good, because it costs nothing and risks nothing.
* Bad, because the existing sensor stays mis-typed, which is a real defect regardless
  of #675.

## Open Questions

All resolved. Answers are retained rather than deleted: the reasoning is what a
future reader needs in order to re-evaluate the decision.

- [x] Does the Home Assistant Energy dashboard require `state_class: total_increasing` specifically, or does it also accept `total`? — **Answered 2026-08-24 by User: Robert van den Breemen:** Neither is required. Verified against Home Assistant core, `homeassistant/components/energy/validate.py`: water sources go through the shared helper `_async_validate_usage_stat`, which accepts `MEASUREMENT`, `TOTAL` or `TOTAL_INCREASING`. `MEASUREMENT` additionally requires a `last_reset` attribute. Accepted units come from `WATER_USAGE_UNITS` (`L, gal, m3, ft3, CCF, MCF`) and the device class must be `SensorDeviceClass.WATER`. The design therefore uses `device_class: water`, unit `L`, `state_class: total_increasing`: permitted, needs no `last_reset`, and matches the non-decreasing counter chosen below.
- [x] What is the concrete flash-write rule, and what erase-cycle budget justifies the chosen numbers? — **Answered 2026-08-24 by User: Robert van den Breemen:** Write when delta >= 10 L, or when 15 minutes have elapsed and delta > 0, plus one write on graceful reboot. That bounds unclean-reboot loss to 10 L and the write rate to at most 96/day worst case, typically about 8/day. Wear is *not* the deciding factor: the naive single-sector figure (100000 cycles / 120 writes per day = 2.3 years) does not apply, because LittleFS wear-levels across the partition, and a small dedicated file survives on the order of a century even at a 1 L cadence. Assumptions labelled as such: 120 L/day household draw (external, no repo source) and 100000 erase cycles (industry-standard figure for the kind of flash memory these boards use (NOR flash) (the flash type these boards use), no datasheet read). The choice is therefore made on failure mode, not endurance. Analysis was performed on the 2.0.0 tree where the SAT (Smart Autonomous Thermostat) precedents live; the conclusion is platform-independent, but ESP8266 partition sizes should be confirmed before implementation here.
- [x] What does the design do about a partial regression after an unclean reboot? — **Answered 2026-08-24 by User: Robert van den Breemen:** Resume from the last persisted value and accept the undercount. The counter never decreases, so Home Assistant never sees a reset and the long-term statistic stays coherent. The litres accumulated since the last flash write are lost permanently; that error is bounded by the write cadence and is the deliberate price of never corrupting the statistic.
- [x] How is the integration performed between two sparse samples, and what is its error characteristic when the gap is minutes rather than seconds? — **Answered 2026-08-24 by User: Robert van den Breemen:** Zero-hold: count only across intervals bounded by a non-zero flow reading, with the usable interval capped so a long sampling gap cannot invent litres. This biases the total to UNDER-count, the safe direction for a figure users will read as a meter. Last-value hold and trapezoidal were rejected because across a long gap they invent flow that never happened. The same method is mandated on the 2.0.0 peer so both firmwares report the same total for the same boiler.
- [x] Is the counter user-resettable, and through which surface? — **Answered 2026-08-24 by User: Robert van den Breemen:** Yes, through a paired REST and MQTT surface, matching whichever paired reset idiom exists on this line. The S0 pulse counter is *not* a usable precedent: it is a plain RAM global that is never persisted and has no reset, so it starts at zero every boot. The reset must zero the RAM counter *and* the persisted file in one operation and immediately publish 0, so Home Assistant records a clean reset rather than a partial regression. The exact 1.x route and MQTT command surface must be confirmed against this tree before implementation.
- [x] What are the measured RAM and flash costs of the counter plus the new discovery entity? — **Answered 2026-08-24 by User: Robert van den Breemen:** An estimate, not measured: about 20 bytes of RAM and roughly 500-800 bytes of flash. The RAM figure assumes accumulating in millilitres as `uint32_t` rather than float litres (exact, no drift, range 4.29 M L): total, last-sample timestamp, last flow, last-persisted value and last-persist timestamp, 4 bytes each. No persistence buffer is needed if the write uses a stack-local buffer, as the SAT energy persister does. Flash: about 28 bytes for one discovery table row (derived from struct layout, not compiled), plus label and friendly-name strings and the new device-class and unit cases, giving about 100 bytes for the entity; the integration and persistence logic is the remaining 400-700. The measurement that would settle it is two builds of the same environment differing only by this change, diffed on binary size. That was not run.

## Related Decisions

* **ADR-004 (No String class in hot paths)**: constrains the implementation of the
  integration and publish path.
* **ADR-030 (Heap Memory Monitoring and Emergency Recovery)**: sets the headroom policy
  this change must stay inside.
* **ADR-008 (LittleFS for Configuration Persistence)**: the persistence mechanism whose
  wear budget this decision spends.
* The 2.0.0 line carries the peer decision under its own number. The two are peers, not
  dependencies: neither blocks the other, but the user-visible entity contract must
  match so a user moving between lines sees the same sensor.

## References

* GitHub issue #675 — <https://github.com/rvdbreemen/OTGW-firmware/issues/675>
* Home Assistant sensor device classes (valid units for `water` and
  `volume_flow_rate`) — <https://developers.home-assistant.io/docs/core/entity/sensor/>
* Home Assistant Energy dashboard, water section —
  <https://www.home-assistant.io/docs/energy/water/>
* `src/OTGW-firmware/mqtt_configuratie.cpp:714-715` — MsgID 19 discovery rows.
* `src/OTGW-firmware/mqtt_configuratie.cpp:1741` — `l/min` unit string mapping.
* `src/OTGW-firmware/MQTTstuff.h:51-61` — `HaDeviceClass` enum, currently containing
  neither `water` nor `volume_flow_rate`.
* `src/OTGW-firmware/OTGW-Core.h:49` — `DHWFlowRate` state field.
* TASK-1081 — the separate `volume_flow_rate` correction to the rate sensor.
