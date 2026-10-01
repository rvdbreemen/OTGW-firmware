---
id: "ADR-182"
title: "Announce the DHW water total just in time, after the first MsgID 19 sample of a boot"
status: "Accepted"
date: "2026-10-01"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-176"
topics:
  - "water-meter"
  - "mqtt-discovery"
  - "home-assistant"
  - "dhw"
aliases:
  - "dhw_water_total announce"
  - "faux id 241"
  - "just-in-time discovery"
  - "unknown water sensor"
components:
  - "Discovery queueing of faux id 241 (queueNonOTDiscoveryIds, markAllMQTTConfigPending, publishDHWWaterMeter)"
symbols:
  - "queueNonOTDiscoveryIds"
  - "markAllMQTTConfigPending"
  - "publishDHWWaterMeter"
  - "dhwWaterMeterHasData"
  - "OTGWdhwmeterid"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-182 Announce the DHW water total just in time, after the first MsgID 19 sample of a boot

## Status

Accepted, 2026-10-01.

**Decision Maker:** User: Robert van den Breemen (maintainer), 2026-10-01, chose the
just-in-time announce of the 1.x line over keeping the boot announce.

## Status History

```yaml
status_history:
  - date: 2026-10-01
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-01
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: "ADR-182 amends ADR-176's boot-publish clause: the water total is announced just in time"
    changed_via: adr-kit lifecycle
  - date: 2026-10-01
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Maintainer accepted in session 2026-10-01 after the acceptance packet
    changed_via: adr-kit lifecycle
```

## Context

ADR-176 publishes the domestic hot water (DHW) total as `dhw_water_total`, a Home
Assistant entity under the faux discovery id 241 (`OTGWdhwmeterid`). Its Decision
Contract requires the entity to be registered in the boot-publish path, "or it will
be absent in Home Assistant until the first value arrives" (ADR-176, line 167). The
firmware therefore queued 241 at boot, in `queueNonOTDiscoveryIds()`, and on every
full republish. Its state waits for data: nothing is published until a MsgID 19
sample was taken on the current boot (`dhwWaterMeterHasData()`).

The 1.x line announces the entity just in time instead: its `publishDHWWaterMeter()`
queues the discovery config on the first publish after a sample (1.x ADR-093). A
1.x gateway that never sees MsgID 19 has no water entity at all.

The boot announce leaves an entity that never gets a value on every installation
that never samples MsgID 19. After ADR-181 that includes every gateway whose PIC
(Microchip PIC microcontroller) runs in PS=1 mode, which decodes no MsgID 19 frame. Home Assistant shows such an entity as
unknown for good.

## Decision

The DHW water total is announced just in time, after the first MsgID 19 sample of
a boot, as on the 1.x line:

- `queueNonOTDiscoveryIds()` queues 241 only when `dhwWaterMeterHasData()` is true.
  It stays the single place that decides 241 for both the boot path and a full
  republish.
- `markAllMQTTConfigPending()` skips 241 in its walk over the discovery table, so it
  does not queue 241 behind the helper's back.
- `publishDHWWaterMeter()`, on the 60 s task, queues 241 while the drip has not yet
  published it (`getMQTTConfigDone()`), behind an MQTT-enabled check. A published
  config is not queued again every minute.

This amends ADR-176's Decision Contract item "Register the new entity in the
boot-publish path": the entity is registered after the first sample of a boot, and
again on every republish after one. Every other clause of ADR-176 stays in force.

## Decision Contract

### Must

- Queue faux id 241 only after a MsgID 19 sample was taken on the current boot.
- Decide 241's queueing in `queueNonOTDiscoveryIds()` for the boot path and every
  full republish.
- Queue 241 from the 60 s publish while its config has not been published, and only
  while MQTT (Message Queuing Telemetry Transport) is enabled.

### Must Not

- Queue 241 from the table walk in `markAllMQTTConfigPending()`.
- Queue a published 241 config again from the 60 s publish.

### Exceptions

- None.

### Verification

- `python test/host/test_dhw_water_meter.py`, discovery cases on the real code
  sliced from the sources: D4 (boot path without a sample: 241 not queued), D5
  (markAll without a sample: not queued), D6 (a restored total alone: not queued),
  D7 (after a sample the 60 s publish queues 241, and both republish paths do too),
  D8 (a republish queues it again), D9 (MQTT disabled: nothing queued) and D12 (a
  published config is not queued again). Mutants M4, M5, M15, M16, M17 and M27 each
  fail their case.

## Alternatives Considered

- **Keep the boot announce (ADR-176 as accepted).** Every installation without
  MsgID 19, and after ADR-181 every PIC gateway in PS=1 mode, keeps an entity that
  never gets a value.
- **Announce at boot and remove the entity when no sample comes.** There is no
  moment at which "no sample" is final: a thermostat can start asking for MsgID 19
  at any time. The retained config would have to be cleared and published again,
  which costs broker traffic and makes the entity flap.

## Consequences

**Positive:**

- No permanently unknown water entity on gateways that never see MsgID 19.
- Both firmware lines announce the same way, so the "Announce timing" difference
  between them is gone.

**Negative:**

- The entity appears in Home Assistant only after the first sample, up to one 60 s
  tick plus the discovery drip later. The first state publish can reach Home
  Assistant before the config, which drops it; the next 60 s publish fills the
  entity.
- A gateway that ran a build with the boot announce left a retained config on the
  broker. Home Assistant keeps that entity until it is removed there. Only 2.0.0
  alpha builds announced at boot.
- `queueNonOTDiscoveryIds()`, the single source for the non-OT ids (ADR-171), now
  has one conditional entry.

## Open Questions

- None.

## Related Decisions

- **ADR-176 (Publish a firmware-integrated cumulative DHW water total for the Home
  Assistant Energy dashboard)**: this ADR amends its boot-publish clause; the rest
  of ADR-176 stands.
- **ADR-181 (Count only boiler Read-Ack frames into the DHW water total, not the PIC
  PS=1 summary)**: the reason a PIC gateway in PS=1 mode never samples.
- **ADR-171 (single source for the non-OT discovery ids)**: the helper that now
  decides 241 conditionally.

## References

- `docs/adr/ADR-176-publish-a-firmware-integrated-cumulative-dhw-water-total-for-the-home-assistant-energy-dashboard.md`,
  line 167 (the clause this ADR amends).
- `src/OTGW-firmware/MQTTstuff.ino`: `queueNonOTDiscoveryIds()`,
  `markAllMQTTConfigPending()` and `publishDHWWaterMeter()`.
- The 1.x line's `publishDHWWaterMeter()` (`MQTTstuff.ino` on `otgw-1.x.x`), which
  announces on the first publish after a sample.
- `test/host/test_dhw_water_meter.py` and `test/host/test_dhw_water_discovery.cpp`.
- TASK-1123 (the open announce question) and TASK-1190 (this change).
