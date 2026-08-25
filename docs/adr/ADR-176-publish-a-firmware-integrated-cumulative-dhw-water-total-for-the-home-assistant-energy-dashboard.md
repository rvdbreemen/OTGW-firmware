---
id: "ADR-176"
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

# ADR-176 Publish a firmware-integrated cumulative DHW water total for the Home Assistant Energy dashboard

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
    reason: Accepted by the maintainer as the 2.0.0 peer of ADR-090, after all seven open questions were resolved with measured or code-verified answers.
    changed_via: adr-kit lifecycle
```

## Context and Problem Statement

GitHub issue [#675](https://github.com/rvdbreemen/OTGW-firmware/issues/675) was filed
against the 1.x line, asking for `device_class: water` on the DHW (Domestic Hot Water)
flow-rate sensor so the entity can be added to the Home Assistant Energy dashboard. The
same sensor and the same gap exist on this line, so the decision is taken here as a
peer.

The literal request describes an invalid entity. The existing sensor carries OpenTherm
MsgID 19 (`DHWFlowRate`) with unit `l/min` and `state_class: measurement`
(`src/OTGW-firmware/MQTTHaDiscovery.cpp:884-885`, unit string at
`src/OTGW-firmware/MQTTHaDiscovery.cpp:2048`). Home Assistant's sensor device-class
table lists the valid units for `water` as `L, gal, m³, ft³, CCF, MCF` and describes it
as "Water consumption"; `l/min` appears only under `volume_flow_rate`, described as
"the amount of water consumed momentarily"
(<https://developers.home-assistant.io/docs/core/entity/sensor/>). A rate is not a
volume.

Note that `HaDeviceClass` on this line (`src/OTGW-firmware/MQTTstuff.h:139-148`)
contains neither `water` nor `volume_flow_rate`. The `water` identifier at
`src/OTGW-firmware/MQTTstuff.h:234` belongs to the *icon* enum (`mdi:water`) and is
unrelated.

The reporter's actual goal — a water figure on the Energy dashboard — needs a
cumulative quantity this firmware does not produce.

Two constraints frame the design on this line:

- MsgID 19 is not emitted on a timer. It appears only when a master asks for it, so the
  sample stream is sparse and irregular. This is a property of the OpenTherm bus and is
  identical on both firmware lines.
- This line targets the ESP32-S3, so random-access memory (RAM) is far less scarce than on the 1.x ESP8266.
  Flash-erase wear on the persisted counter remains a real cost, and the OTDirect path
  means MsgID 19 can also originate from this firmware's own master scheduler rather
  than from a PIC-relayed bus observation.

Correcting the existing rate sensor to `device_class: volume_flow_rate` is a separate,
mechanical change and needs no ADR. This decision covers only the new derived
cumulative entity.

## Decision Drivers

* The reporter wants a water figure on the Energy dashboard, which requires a
  cumulative volume, not a flow rate.
* Home Assistant already ships an integration ("Riemann sum") helper that converts a
  rate sensor into a cumulative total, so the firmware is not the only possible home
  for this.
* A firmware-side counter must survive reboot to be useful as a meter, which means
  persisting it and spending flash-erase cycles.
* Home Assistant treats a decrease in a `total_increasing` sensor as a meter reset, so
  behaviour across an unclean reboot determines whether long-term statistics stay
  coherent.
* The user-visible entity contract must match the 1.x line, so a user moving between
  firmware lines sees the same sensor rather than a renamed or retyped one.

## Considered Options

* **Option A — Firmware-integrated cumulative total.** Integrate MsgID 19 over time,
  persist the running litre count, and publish it as a new discovery entity with
  `device_class: water` and unit `L`.
* **Option B — Correct the rate sensor only.** Set `device_class: volume_flow_rate` on
  the existing MsgID 19 sensor and document the Home Assistant integration helper as
  the supported route to an Energy dashboard figure.
* **Option C — Do nothing.** Leave the sensor without a device class and close #675
  explaining that a flow rate is not a meter.

## Decision Outcome

Chosen option: **Option A**, because the maintainer decided the Energy dashboard should
work out of the box rather than requiring every user to discover and configure a Home
Assistant helper, and because the 1.x line is taking the same decision — a firmware
that produced the entity on one line and not the other would be a worse outcome than
either option applied consistently.

This decision does **not** claim the firmware-side total is more accurate than the Home
Assistant helper. Both integrate the same sparse MsgID 19 sample stream and carry the
same sampling error. The firmware version is always-on and independent of Home
Assistant configuration; that, and not accuracy, is the benefit.

### Confirmation

The new entity appears in Home Assistant with `device_class: water`, unit `L` and a
`total_increasing` state class, is selectable as a water source in the Energy
dashboard, and its value is non-decreasing across a graceful reboot of the gateway.
The entity name, unit and device class match the 1.x line's entity exactly.

## Decision Contract

### Must

* Publish the cumulative total as a separate entity. The existing MsgID 19 rate sensor
  keeps unit `l/min` and `device_class: volume_flow_rate`; it must not be retyped
  as `water`. (Already satisfied at `MQTTHaDiscovery.cpp:884-885`.)
* Accumulate as **elapsed time × flow**, never as a fixed volume per sample. This is
  what makes the count independent of how many frames carry the same reading.
* Call the accumulator from **both** state write sites — `print_f88`
  (`OTGW-Core.ino:2500`) and `updatePSSummaryFloatState` (`OTGW-Core.ino:4038`). The
  second bypasses the first entirely, so hooking only one silently stops counting
  under PS=1.
* Persist the running total across reboot, in its own file rather than in
  `settings.ini`.
* Bound the flash write rate with an explicit rule rather than writing on every publish.
* Keep the entity contract (name, unit, device class, state class) identical to the
  1.x line's peer decision. Storage mechanism is explicitly *not* bound by that
  mandate.
* Register the new entity in the boot-publish path for non-OT discovery configs if it
  is given a faux message id, or it will be absent in Home Assistant until the first
  value arrives.
* State the sampling limitation in user-facing documentation, so the number is not
  mistaken for a metrologically valid water meter.

### Must Not

* Set `device_class: water` on any sensor whose unit is not one of
  `L, gal, m³, ft³, CCF, MCF`.
* Write the persisted counter to flash on every message-queue telemetry transport (MQTT) publish.
* Accumulate a fixed volume per received sample. MsgID 19 legitimately arrives more
  than once per real exchange — the OTDirect cache replay
  (`OTDirect.ino:2525-2526`) and the PS=1 summary self-echo (`OTDirect.ino:1563`)
  both re-present the same reading. Under time × flow those are harmless; under
  per-sample accumulation the PS=1 echo alone inflates the total roughly twelvefold.
* Accumulate from a stale OTDirect cache replay. If the 3-strike UNKNOWN_DATA_ID rule
  disables schedule entry 19 (`OTDirect.ino:1300-1310`) while the cache holds a
  non-zero flow, the replay keeps serving that frozen value at the thermostat's poll
  rate, each gap short enough to pass the interval cap, and the counter runs away.

### Exceptions

* None.

### Verification

* `src/OTGW-firmware/MQTTHaDiscovery.cpp` discovery rows for MsgID 19 and for the new
  total entity.
* Manual check against a live Home Assistant instance that the entity is offered as an
  Energy dashboard water source.
* A bench check on the OTDirect path confirming a single accumulation per unit of real
  flow.

## Consequences

### Positive

* The Energy dashboard works without the user building a Riemann-sum helper.
* The existing rate sensor becomes correctly typed as a side effect of the same work.
* The total survives Home Assistant restarts and reinstalls, because it lives on the
  gateway.
* Users moving between the 1.x and 2.0.0 lines keep the same entity.

### Negative

* **Accuracy is bounded by the bus, not by the code.** MsgID 19 arrives only when a
  master asks for it. Between two samples the firmware must assume something about the
  flow, and every such assumption is wrong some of the time. A user reading a litre
  count will reasonably assume meter-grade accuracy this cannot provide.
  *Mitigation:* say so plainly in the entity documentation and the release note.
* **Flash wear** from a persisted counter the firmware did not previously keep.
  *Mitigation:* the write-cadence rule required by the Decision Contract; concrete
  parameters are an Open Question below.
* **Partial-regression risk on unclean reboot.** If power is lost between two flash
  writes, the restored counter is lower than the last published value. Home Assistant
  reads that decrease as a meter reset, but it is a *partial* one, so the dashboard
  neither continues correctly nor resets cleanly — it silently corrupts the long-term
  statistic. This is the sharpest risk in the change and is unresolved below.
* **A second accumulation source.** On the OTDirect path this firmware is itself the
  master, so MsgID 19 can arrive from its own scheduler as well as from observed bus
  traffic. That surface does not exist on the 1.x line and is a double-counting hazard
  specific to this decision.

## Pros and Cons of the Options

### Option A — Firmware-integrated cumulative total

* Good, because the Energy dashboard works with no Home Assistant configuration.
* Good, because the total is independent of the Home Assistant database and survives a
  Home Assistant rebuild.
* Good, because ESP32-S3 RAM headroom makes the counter state cheap on this line.
* Bad, because it spends flash-erase cycles the firmware did not previously spend.
* Bad, because it puts a lossy derived metric in firmware, where it is harder to fix
  and harder for the user to reset than a Home Assistant helper.
* Bad, because the OTDirect path adds a double-counting surface that must be reasoned
  about and tested.

### Option B — Correct the rate sensor only

* Good, because it is a few lines, carries no persistence, and no wear.
* Good, because the Home Assistant helper is user-resettable and user-tunable.
* Bad, because it does not deliver the reporter's stated goal from the firmware.
* Bad, because it would diverge from the 1.x line, which is taking Option A.

### Option C — Do nothing

* Good, because it costs nothing and risks nothing.
* Bad, because the existing sensor stays mis-typed, which is a real defect regardless
  of #675.

## Open Questions

All resolved. Answers are retained rather than deleted: the reasoning is what a
future reader needs in order to re-evaluate the decision.

- [x] Does the Home Assistant Energy dashboard require `state_class: total_increasing` specifically, or does it also accept `total`? — **Answered 2026-08-24 by User: Robert van den Breemen:** Neither is required. Verified against Home Assistant core, `homeassistant/components/energy/validate.py`: water sources go through the shared helper `_async_validate_usage_stat`, which accepts `MEASUREMENT`, `TOTAL` or `TOTAL_INCREASING`. `MEASUREMENT` additionally requires a `last_reset` attribute. Accepted units come from `WATER_USAGE_UNITS` (`L, gal, m3, ft3, CCF, MCF`) and the device class must be `SensorDeviceClass.WATER`. The design therefore uses `device_class: water`, unit `L`, `state_class: total_increasing`.
- [x] On the OTDirect path, is MsgID 19 counted once or twice when this firmware is the master? — **Answered 2026-08-24 by User: Robert van den Breemen:** Once per real exchange on every path, PROVIDED the integrator is time-based. MsgID 19 is `OT_READ` (`OTGW-Core.h:387`) and `is_value_valid_for_master_topic` admits it only as `OT_READ_ACK` (`:1631`), so request frames never write state. Classic PIC (the OpenTherm Gateway's PIC microcontroller), OTDirect gateway mode and OTDirect master mode each yield exactly one write per exchange. Two extra sample sources exist: in master mode with a thermostat attached, `handleMasterModeSlaveFrame` replays the cached value as a synthetic READ_ACK A frame (`OTDirect.ino:2525-2526`), reaching canonical by design as a proxy A (ADR-103); and under PS=1, `emitSummaryLine` feeds its own summary back through `processOT` (`OTDirect.ino:1563`) roughly every 800 ms. Neither adds litres under the settled zero-hold rule, because litres are elapsed time times flow and an extra sample only subdivides an interval. Under per-sample accumulation the PS=1 echo alone would inflate the total about twelvefold (800 ms echo against a 10 s true poll). Note there are *two* state write sites, not one: `print_f88` (`OTGW-Core.ino:2500`) and `updatePSSummaryFloatState` (`:4038`), the latter bypassing the former entirely, so a single helper must be called from both. Residual hazard to guard: if the 3-strike UNKNOWN_DATA_ID rule disables schedule entry 19 (`OTDirect.ino:1300-1310`) while the cache holds a non-zero flow, the replay keeps serving that frozen value at the thermostat poll rate, each gap short enough to pass the interval cap, and the counter runs away. Exclude the cache-replay path from accumulation, or bound it by an independent liveness signal.
- [x] What is the concrete flash-write rule, and what erase-cycle budget justifies the chosen numbers? — **Answered 2026-08-24 by User: Robert van den Breemen:** Write when delta >= 10 L, or when 15 minutes have elapsed and delta > 0, plus one write on graceful reboot. That bounds unclean-reboot loss to 10 L and the write rate to at most 96/day worst case. Wear is *not* the deciding factor: the naive single-sector figure (100000 cycles / 120 writes per day = 2.3 years) does not apply because LittleFS wear-levels across the partition. The `spiffs` partition is `0x180000`, 384 blocks of 4 KB with roughly 140 free after web assets, giving about 14 M block-erases, so a small dedicated file at about 3 block-erases per write survives roughly 105 years even at a 1 L cadence and about 1575 years at the recommended cadence. Assumptions labelled as such: 120 L/day household draw (external, no repo source) and 100000 erase cycles (industry-standard figure for the kind of flash memory these boards use (NOR flash) (the flash type these boards use), no datasheet read; the repo already uses this figure at `OTGW-firmware.ino:886` and `SATcontrol.ino:1784`).
- [x] What does the design do about a partial regression after an unclean reboot? — **Answered 2026-08-24 by User: Robert van den Breemen:** Resume from the last persisted value and accept the undercount. The counter never decreases, so Home Assistant never sees a reset and the long-term statistic stays coherent. The litres accumulated since the last flash write are lost permanently; that error is bounded by the write cadence and is the deliberate price of never corrupting the statistic. Same answer as the 1.x peer.
- [x] How is the integration performed between two sparse samples, and what is its error characteristic when the gap is minutes rather than seconds? — **Answered 2026-08-24 by User: Robert van den Breemen:** Zero-hold: count only across intervals bounded by a non-zero flow reading, with the usable interval capped so a long sampling gap cannot invent litres. Biases to UNDER-count, the safe direction for a meter. Last-value hold and trapezoidal were rejected because across a long gap they invent flow that never happened. Identical to the 1.x peer by mandate, so both firmwares report the same total for the same boiler.
- [x] Is the counter user-resettable, and through which surface? — **Answered 2026-08-24 by User: Robert van den Breemen:** Yes, through a paired REST and MQTT surface, following the `reset_integral` / `flush` idiom, which is the only paired reset pattern in this firmware: REST at `restAPI.ino:1253-1257` and the MQTT dispatch row at `MQTTstuff.ino:670` with its adapter at `:618`. Proposed: `POST /api/v2/otgw/reset_water_total` plus an MQTT `set/<nodeId>/otgw/reset_water_total` command. The reset must zero the RAM counter *and* the persisted file in one operation and immediately publish 0, so Home Assistant records a clean reset rather than a partial regression. The S0 pulse counter is *not* a usable precedent: `OTGWs0pulseCountTot` (`OTGW-firmware.h:850`) is a plain RAM global, never persisted, with no reset.
- [x] Does the persisted counter belong in the existing settings store or in its own file, given it changes far more often than settings do? — **Answered 2026-08-24 by User: Robert van den Breemen:** Its own file, *not* `settings.ini`, and the reason is the failure mode rather than wear. `writeSettings()` opens `SETTINGS_FILE` with mode `w` (`settingStuff.ino:244`) and rewrites the whole roughly 7 KB blob, so every counter update would drag all persistent configuration through a power-loss window. Losing WiFi credentials to a water-counter write is a far worse outcome than losing 10 L. Follow the existing `satSaveEnergyState()` pattern instead (`SATcontrol.ino:1788-1798`): a small dedicated JSON file, a stack-local 64-byte buffer, no new global. Non-Volatile Storage (NVS) is technically the best fit, and the 20 KB `nvs` partition is present and unused for application data, but the repo has no precedent for hot-value writes to that store and `platform_esp32.h:370-375` warns against timer-driven ones. The 'match the 1.x line' mandate binds the user-visible entity contract, not the storage mechanism.

## Related Decisions

* The 1.x line carries the peer decision under its own number. The two are peers, not
  dependencies: neither blocks the other, but the user-visible entity contract must
  match.
* Decisions governing MQTT source-topic worldview and discovery shape on this line
  constrain where the new entity's state topic sits; confirm the new topic follows the
  established shape before acceptance.

## References

* GitHub issue #675 — <https://github.com/rvdbreemen/OTGW-firmware/issues/675>
* Home Assistant sensor device classes (valid units for `water` and
  `volume_flow_rate`) — <https://developers.home-assistant.io/docs/core/entity/sensor/>
* Home Assistant Energy dashboard, water section —
  <https://www.home-assistant.io/docs/energy/water/>
* `src/OTGW-firmware/MQTTHaDiscovery.cpp:884-885` — MsgID 19 discovery rows.
* `src/OTGW-firmware/MQTTHaDiscovery.cpp:2048` — `l/min` unit string mapping.
* `src/OTGW-firmware/MQTTstuff.h:139-148` — `HaDeviceClass` enum, currently containing
  neither `water` nor `volume_flow_rate`.
* `src/OTGW-firmware/MQTTstuff.h:234` — the unrelated `water` *icon* value.
* `src/OTGW-firmware/OTGW-Core.h:73` — DHW state fields on this line.
