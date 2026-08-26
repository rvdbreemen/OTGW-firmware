---
id: "ADR-094"
title: "Correct the DHW water total record: announce gating, live citations and measured costs"
status: "Proposed"
date: "2026-08-26"
binding: false
gate: null
documents_shipped: true
verified_in:
  - "src/OTGW-firmware/dhwWaterMeter.ino"
  - "src/OTGW-firmware/MQTTstuff.ino"
  - "test/host/test_dhwWaterMeter.cpp"
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
  - "water meter announce gating"
  - "MsgID 19 integration"
components:
  - "DHW water total counter"
symbols:
  - "updateDHWWaterMeter"
  - "dhwWaterMeterHasData"
  - "dhwWaterMeterNeedsAnnounce"
  - "markAllMQTTConfigPending"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-094 Correct the DHW water total record: announce gating, live citations and measured costs

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

ADR-093 replaced three requirements of ADR-090 and was accepted the same day it
was written. An adversarial review of that record then raised thirty findings,
of which eighteen survived independent verification. Two were defects in the
shipped code; the rest were defects in the record itself, including citations
of a superseded decision as if it were binding.

The decision ADR-093 made is not in question and is carried forward unchanged:
the cumulative DHW (domestic hot water) total stays in RAM (random-access
memory), the entity is announced when the first MsgID 19 frame decodes, and an
interval longer than the clamp adds no volume. What needs correcting is
everything around that decision.

**The code did not implement its own contract.** ADR-093 Must #4 required both
the state publish and the discovery announcement to be withheld until first
data. Only the state half was. `markAllMQTTConfigPending()` marks every id that
has a row in the discovery tables, and the water meter has one
(`src/OTGW-firmware/mqtt_configuratie.cpp:1451`), so the daily discovery
auto-heal (TASK-1048, default on) published the retained config on gateways
whose bus never carries MsgID 19. Those users gained an entity that never
receives state and shows as `unknown`. Separately, the announce latch was a
function-local static that the broker-restart path could not reach, so once a
restarted broker dropped the retained configs the meter was never re-announced
until the gateway rebooted. Both are fixed in TASK-1093, shipped in
v1.7.5-beta.4; this ADR records the rule the fix implements.

**The record cited decisions that are not in force.** ADR-093 twice cited
ADR-004 for the `String` prohibition. ADR-004 is Superseded by ADR-053; the
live decision is ADR-049. It also attributed the flash-wear constraint to
ADR-030, which is about heap monitoring and says nothing about flash.

**The record misrepresented what it superseded.** ADR-093 stated that ADR-090
did not address what the integrator does when the bus falls silent. ADR-090
decided exactly that in its Open Question 4: zero-hold, counting only across
intervals bounded by a non-zero reading, with the usable interval capped. What
is true is narrower: ADR-090 fixed no cap value and put the rule nowhere in its
Decision Contract. ADR-093 also declared ADR-090's remaining requirements
"satisfied" without auditing them; two were not. ADR-090 Must #2 mandates the
same accumulation method on the 2.0.0 peer so both firmwares report the same
total for the same boiler, and its Open Question 5 requires the counter to be
user-resettable through a paired REST and MQTT surface. The shipped 1.x
implementation has no reset surface at all.

**Two claims about Home Assistant were overstated.** `total_increasing` does
not treat *any* decrease as a meter reset: the recorder treats a drop below 90%
of the previous value as a reset, and a smaller dip is logged and absorbed. A
reboot to zero always clears that bar, so the conclusion holds and the sentence
did not.

**The measured costs were wrong.** ADR-093 reported 8 bytes of RAM and 400
bytes of flash. The counter state plus the announce latch is 13 bytes of static
storage, and 400 was the delta of an intermediate build that carried a dead
branch; the shipped delta is 380.

## Decision Drivers

* An Accepted ADR is what a future agent treats as binding, so a citation of a
  superseded decision inside one is worse than no citation.
* The gap between ADR-093's contract and the code was invisible to the
  pre-commit judge, because both of its declarative rules were inert.
* A user-facing meter with no reset surface is a gap a future reader should
  find recorded, not rediscover.
* Correcting an Accepted ADR is only possible by superseding it, and the
  corrections are numerous enough that a reader of ADR-093 alone would be
  misled on several points.

## Considered Options

* **Option A — Supersede ADR-093 with a corrected record.** Carry the decision
  forward verbatim, fix every verified defect, add the announce-gating rule the
  code now implements, and record the two ADR-090 requirements that remain
  genuinely unmet.
* **Option B — Leave ADR-093 standing and fix only the code.** The code is
  already fixed by TASK-1093; the record would keep its wrong citations.
* **Option C — Amend rather than supersede.** Write a short additive record
  listing corrections without restating the decision.

## Decision Outcome

Chosen option: **Option A**, because a reader who lands on ADR-093 through the
index has no way to know that six of its citations and claims are wrong, and an
amendment (Option C) leaves them reading two documents to assemble one truth.
Option B was rejected outright: the whole point of the record is that an agent
can act on it without re-deriving it, and an ADR that cites a superseded
decision as binding actively misleads.

The decision itself is unchanged from ADR-093 and is restated here in full so
this record stands alone:

**The total is not persisted.** `dhwWaterTotalL` lives in RAM
(`src/OTGW-firmware/dhwWaterMeter.ino:40`) and starts at zero on every boot.
Home Assistant treats a drop below 90% of the previous value as a meter reset
and continues the long-run sum, and a reboot to zero always clears that bar. No
LittleFS file, no write-cadence rule, and no partial-restore window. The
partial-restore corruption that ADR-090 accepted as "the deliberate price"
(its Open Question 3) does not arise.

**Nothing is announced until there is a meter to announce.** Both the state
publish and the discovery config are withheld until the first MsgID 19 frame
decodes, on *every* path that can queue a discovery config, not only the boot
path. Concretely: `publishDHWWaterMeter()` returns early while
`dhwWaterMeterHasData()` is false, the water-meter row is skipped inside
`markAllMQTTConfigPending()`'s table scan under the same condition
(`src/OTGW-firmware/MQTTstuff.ino:1554`), and the pseudo-ID is absent from
`publishNonOTDiscoveryConfigs()`. This is what ADR-093 claimed and did not
deliver.

**The announce latch is re-armed when the broker forgets.** The latch lives in
`dhwWaterMeter.ino` rather than inside the publisher, and the broker-restart
path clears it next to `clearMQTTConfigDone()`
(`src/OTGW-firmware/MQTTstuff.ino:882`), so a restarted broker that dropped the
retained configs gets the entity re-announced on the next publish. This puts
the meter on the same footing as OT (OpenTherm) ids under the JIT (just in
time) discovery lineage ADR-041 to ADR-073 to ADR-088, rather than merely
resembling it.

**A silent bus adds no water.** Each sample is applied to the interval that
preceded it, and an interval longer than `DHW_METER_MAX_GAP_MS` (60 seconds,
`src/OTGW-firmware/dhwWaterMeter.ino:38`) is a hole in the measurement rather
than flow. This is the cap ADR-090's Open Question 4 called for and never
fixed to a value. It is deliberately not the behaviour of Home Assistant's
`max_sub_interval`, which holds the last reading across silence and keeps
integrating.

**Two ADR-090 requirements are knowingly unmet and are not carried forward as
satisfied.** The counter has no user-reset surface (ADR-090 Open Question 5),
and the 1.x accumulation is not yet reconciled with the 2.0.0 peer decision
that ADR-090 Must #2 invokes. Both are recorded as Open Questions below rather
than silently dropped.

### Confirmation

A gateway with no MsgID 19 traffic publishes no `dhw_water_total` discovery
config, including after the daily auto-heal has run and after a manual
discovery republish. A gateway that does see the id gains the entity on the
first frame, and regains it without a reboot after a broker restart clears the
retained configs.

Verified for the entity's field shape on 2026-08-26 against the live Home
Assistant at `homeassistant.local:8123`: a discovery config carrying exactly
the fields this entity emits produced an entity whose attributes read back
`device_class: water`, `unit_of_measurement: L`, `state_class:
total_increasing`. The announce gating itself is covered by
`test/host/test_dhwWaterMeter.cpp` (18 checks) and by reading the three call
sites named above; it has not yet been observed on a device across a day
boundary, which is the one remaining field check.

## Decision Contract

### Must

* Publish the cumulative total as a separate entity from the MsgID 19 rate
  sensor, which keeps `device_class: volume_flow_rate` and the unit string
  `L/min` (capital L, the value Home Assistant accepts).
* Accumulate as elapsed time times flow, never as a fixed volume per received
  sample.
* Refuse to integrate an interval longer than an explicit named clamp, and
  update the sample timestamp anyway so the skipped gap is not charged to the
  next sample.
* Withhold the state publish AND the discovery announcement, on every path that
  can queue a discovery config, until at least one MsgID 19 frame has decoded
  on this boot. `markAllMQTTConfigPending()` is a path.
* Keep the announce latch outside the publisher and re-arm it wherever the
  broker-restart path clears the discovery done-bitmap.
* Feed the integrator only from frames that are valid for the master topic, so
  gateway substitutions and answer overrides (ADR-066, ADR-082) never reach the
  meter.
* State the sampling limitation wherever the entity is described to users, so
  the number is not mistaken for a metrologically valid water meter.

### Must Not

* Persist the running total to flash, in any file, at any cadence.
* Register the water-total pseudo-ID in `publishNonOTDiscoveryConfigs()`, or
  mark it pending in `markAllMQTTConfigPending()` or any other path, while
  `dhwWaterMeterHasData()` is false.
* Set `device_class: water` on any sensor whose unit is outside the set Home
  Assistant accepts for that class. The authoritative list is
  `DEVICE_CLASS_UNITS[SensorDeviceClass.WATER]` in
  `homeassistant/components/sensor/const.py`, cross-checked against
  `WATER_USAGE_UNITS` in `homeassistant/components/energy/validate.py`; this
  firmware ships `L`, which both accept. Do not restate the set from memory:
  the cubic units use the superscript-three character, not the digit 3.
* Introduce the `String` class in the integration or publish path (ADR-049).

### Exceptions

* None.

### Verification

* `src/OTGW-firmware/dhwWaterMeter.ino` — integrator, clamp, first-frame gate,
  announce latch.
* `src/OTGW-firmware/MQTTstuff.ino:882,1146,1148,1554` — latch re-arm on broker
  restart, state gate, JIT announce, and the auto-heal scan gate.
* `test/host/test_dhwWaterMeter.cpp` — 18 host checks, run with
  `test\run_tests.bat`.
* Manual check against a live Home Assistant that the entity is offered as an
  Energy dashboard water source, and that a gateway with no MsgID 19 traffic
  still has no such entity after a day boundary.

## Consequences

### Positive

* The record no longer cites a superseded decision as binding, so an agent
  reading it through `adr_context` acts on the live String prohibition.
* The announce rule now names the path that actually leaked, so the same defect
  cannot be reintroduced by someone who reads only the contract.
* The two genuinely unmet ADR-090 requirements are visible as Open Questions
  instead of being buried under a blanket "satisfied".
* Cost is 13 bytes of RAM (the counter state and the announce latch, after
  alignment) and 380 bytes of flash, measured as the difference between two
  beta.3 builds differing only by this feature (761208 to 761588 bytes). The
  400-byte figure in ADR-093 described an intermediate build carrying a dead
  branch that no longer exists.

### Negative

* **A third record now covers one feature.** ADR-090, ADR-093 and this one. A
  reader arriving at the oldest has two hops to the truth. *Mitigation:* the
  supersession chain is written on both sides by the lifecycle command, and the
  generated index shows only this record as live.
* **The entity still resets to zero on every reboot**, and a user reading the
  entity directly rather than the Energy dashboard sees that. *Mitigation:*
  `total_increasing` is the contract for exactly this, and the reset is clean
  rather than partial.
* **Accuracy remains bounded by the bus.** The clamp bounds the error rather
  than removing it: a draw sampled every 30 seconds is integrated in 30-second
  rectangles, and a draw shorter than the sampling interval can be missed
  entirely. *Mitigation:* none available in firmware; state it in user-facing
  documentation.
* **No reset surface.** A user who wants to zero the meter must reboot the
  gateway, which is a side effect rather than an operation. Recorded as an Open
  Question.

## Pros and Cons of the Options

### Option A — Supersede ADR-093 with a corrected record

* Good, because the live record is correct on its own, with no errata to chase.
* Good, because the announce rule can name `markAllMQTTConfigPending()`, which
  is the path that actually failed.
* Bad, because it adds a third ADR to a single feature's history.

### Option B — Leave ADR-093 standing and fix only the code

* Good, because it costs nothing and the code is already right.
* Bad, because ADR-093 keeps citing ADR-004, which is superseded, and keeps
  claiming ADR-090 said nothing about gaps, which is false.
* Bad, because its Enforcement block would keep advertising mechanical
  enforcement that cannot fire.

### Option C — Amend rather than supersede

* Good, because it is shorter to write.
* Bad, because the reader must hold two documents in their head, and the
  erroneous sentences stay in the record that the index presents as live.

## Open Questions

* [ ] Should the cumulative total be user-resettable, and through which
  surface? ADR-090 Open Question 5 answered yes, through a paired REST and MQTT
  surface, but the shipped 1.x implementation has none: the only way to zero
  the counter is a reboot. Either implement the paired surface or record that a
  RAM-only counter with a reboot-to-zero is the accepted reset mechanism.
* [ ] How is the 1.x accumulation reconciled with the 2.0.0 peer? ADR-090
  Must #2 mandates the same method on both lines so the two firmwares report
  the same total for the same boiler. The 2.0.0 peer record lives in the other
  worktree and has not been read against this implementation, in particular
  whether it persists and whether its gap rule uses the same 60-second cap.

## Related Decisions

* **ADR-093 (Keep the cumulative DHW water total in RAM and announce it on
  first data)**: superseded by this ADR. Its decision is carried forward
  unchanged; its citations, its account of ADR-090, its cost figures and its
  Enforcement rules are corrected.
* **ADR-090 (Publish a firmware-integrated cumulative DHW water total for the
  Home Assistant Energy dashboard)**: superseded by ADR-093. Two of its
  requirements remain unmet and are carried into this record's Open Questions.
* **ADR-049 (String Class Prohibition in Protocol Paths)**: the live String
  prohibition, replacing ADR-093's citation of the superseded ADR-004.
* **ADR-008 (LittleFS for Configuration Persistence)**: the decision that owns
  the flash-persistence surface this record declines to use.
* **ADR-030 (Heap Memory Monitoring and Emergency Recovery)**: the RAM headroom
  regime that makes an 13-byte cost worth stating.
* **ADR-088 (Republish on-change gated MQTT state when Home Assistant comes
  back online)**: the live head of the JIT discovery lineage (ADR-041 to
  ADR-073 to ADR-088); the announce latch re-arm follows its reconnect
  handling.
* **ADR-066 (MQTT Publish Gating by Source and Per-MsgID Slave-Echo
  Classification)** and **ADR-082 (Surface gateway overrides as distinct
  override state)**: define which frames may feed device state.

## References

* TASK-1091 (implementation), TASK-1092 (the `L/min` unit fix), TASK-1093 (the
  announce-gating and latch fixes recorded here).
* GH (GitHub) issue #675 — the original request and the error log that
  identified the unit rejection:
  <https://github.com/rvdbreemen/OTGW-firmware/issues/675>
* `src/OTGW-firmware/dhwWaterMeter.ino:38,40,48,60,71` — clamp, counter,
  integrator, gap rule, data gate.
* `src/OTGW-firmware/MQTTstuff.ino:882,1146,1148,1554` — latch re-arm, state
  gate, JIT announce, auto-heal scan gate.
* `src/OTGW-firmware/mqtt_configuratie.cpp:653,1127,1451` — sensor count,
  pseudo-ID 243 row, index entry.
* `src/OTGW-firmware/OTGW-firmware.ino:262,353-367` — the 60-second republish
  and the daily discovery auto-heal that exposed the leak.
* Home Assistant reset detection, the 90% rule:
  <https://github.com/home-assistant/core/blob/dev/homeassistant/components/sensor/recorder.py>
* Home Assistant device-class units and Energy water sources:
  <https://github.com/home-assistant/core/blob/dev/homeassistant/components/sensor/const.py>
  and
  <https://github.com/home-assistant/core/blob/dev/homeassistant/components/energy/validate.py>

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "setMQTTConfigPending\\s*\\(\\s*OTGWdhwmeterid",
      "path_glob": "src/OTGW-firmware/**",
      "message": "ADR-094: queue the water-total discovery config only behind a dhwWaterMeterHasData() gate (see publishDHWWaterMeter)."
    },
    {
      "pattern": "\\bdhwWaterTotalL\\b[^\\n]*(LittleFS|writeFile|\\.print|\\.write)",
      "path_glob": "src/OTGW-firmware/**",
      "message": "ADR-094: the DHW water total is RAM-only. Do not persist it to flash."
    },
    {
      "pattern": "(LittleFS\\.open|writeFile)[^\\n]*dhw[Ww]ater",
      "path_glob": "src/OTGW-firmware/**",
      "message": "ADR-094: the DHW water total is RAM-only. Do not persist it to flash."
    }
  ],
  "forbid_import": [],
  "require_pattern": [
    {
      "pattern": "static const uint32_t DHW_METER_MAX_GAP_MS",
      "path_glob": "src/OTGW-firmware/dhwWaterMeter.ino",
      "message": "ADR-094: the gap clamp must remain an explicit named constant."
    },
    {
      "pattern": "dtMs\\s*>\\s*DHW_METER_MAX_GAP_MS",
      "path_glob": "src/OTGW-firmware/dhwWaterMeter.ino",
      "message": "ADR-094: an interval over the clamp must be skipped, not integrated."
    }
  ]
}
```
