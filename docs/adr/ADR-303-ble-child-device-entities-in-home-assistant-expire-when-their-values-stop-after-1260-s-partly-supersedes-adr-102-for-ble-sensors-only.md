---
id: "ADR-303"
title: "BLE child-device entities in Home Assistant expire when their values stop, after 1260 s (partly supersedes ADR-102 for BLE sensors only)"
status: "Proposed"
date: "2026-11-27"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "home assistant availability"
  - "ble sensors"
  - "expire_after"
aliases:
  - "ble expire after"
  - "H5 ble availability"
components:
  - "satBLEPublishOneDiscovery"
symbols:
  - "BLE_HA_EXPIRE_AFTER_S"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-303 BLE child-device entities in Home Assistant expire when their values stop, after 1260 s (partly supersedes ADR-102 for BLE sensors only)

## Status

Proposed, 2026-11-27.

## Status History

```yaml
status_history:
  - date: 2026-11-27
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

Home Assistant (HA) marks an entity unavailable through an availability topic (`avty_t`) or, when the discovery config sets
`expire_after` (`exp_aft`), when no new value arrives within that many seconds.

Accepted ADR-102 decides: "HA entity availability MUST reflect only the ESP↔MQTT link". Its Alternative 3, `expire_after` on
discovery configs, was rejected because it "makes entities go `unavailable` when a *value* stops updating: wrong for slow OT IDs".
ADR-255 already partly superseded ADR-102 for one entity, the simulation switch.

The HA review of 2026-11-20 (TASK-2970, finding H5) found that a dead BLE (Bluetooth Low Energy) probe keeps its last value in HA
for ever. ADR-148 makes each BLE probe its own HA child device. Commit c24417857 (TASK-2971.1, beta.434, on origin/dev since
2026-11-25) added two things to every BLE entity:

- the gateway availability topic, which agrees with ADR-102;
- `exp_aft` 1260 s, which does not.

So the code on dev is ahead of its ADR. otgw-architect found this while recording TASK-2971 AC1.

Facts (origin/dev, `src/OTGW-firmware/MQTTstuff.ino`, read by otgw-architect):

- `BLE_HA_EXPIRE_AFTER_S = 660UL + 2UL * 300UL` (`:3918`), used in both BLE composers (`:3976`, `:4005`).
- The reasoning in the comment at `:3910-3917`: a BLE state topic is published on change and at least every
  `SAT_HEARTBEAT_MAX_MS` (660 s, `SATmqttPublish.h:58`). The BLE scan interval is at most 300 s (`settingStuff.ino:1331`), and
  `BLE_STALE_MS` is 300 s (`SATble.ino:42`). So the longest gap while a probe is alive is 660 + 2 x 300 = 1260 s.
- `tests/test_ble_discovery_availability.py` fails when the three sources outgrow the constant.

## Decision

BLE child-device entities (temperature, humidity, battery and signal strength of each probe) carry, next to the gateway
availability topic of ADR-102, an `expire_after` of 1260 s. That is the longest gap between two values while the probe is alive.
A probe that stops reporting turns unavailable in HA after at most about 21 minutes.

This partly supersedes ADR-102 for these entities only. Every other entity keeps ADR-102 as written. ADR-102's reason for
rejecting `expire_after`, slow OT IDs whose values do not change, does not apply here: a BLE probe's values are refreshed at a
known maximum interval by the gateway's own heartbeat.

## Decision Contract

### Must

- Set `exp_aft` only on the BLE child-device entities, from one constant derived from the heartbeat and the scan interval.
- Keep the gateway availability topic (`avty_t`) on those entities as well (ADR-102).
- Fail a host test when the heartbeat or the scan interval grows past the constant.

### Must Not

- Put `expire_after` on any OpenTherm, SAT, Dallas or gateway entity (ADR-102 Alternative 3 stays rejected there).
- Change topics or `uniq_id` of the BLE entities.

### Exceptions

- None.

### Verification

- `tests/test_ble_discovery_availability.py` (the payload carries `avty_t` and `exp_aft` 1260; red on the code before
  c24417857, per the TASK-2971.1 notes; not re-run by otgw-architect).
- Bench by otgw-bench-tester (TASK-2971.1 AC4): after an HA rediscovery no entity is lost, and a stopped BLE probe turns
  unavailable within 1260 s.

## Alternatives Considered

- **Keep ADR-102 strictly: revert `exp_aft`.** Rejected (recommendation): a dead probe would show a stale temperature in HA, and
  SAT can regulate on a BLE probe. A stale value that looks fresh is the worse failure.
- **A per-probe availability topic published by the gateway** (like ADR-255's second topic). Rejected: more retained topics and
  firmware state, for the same effect that `expire_after` gives with no code on the gateway.
- **A shorter expiry.** Rejected: values that do not change are only republished by the 660 s heartbeat, so a shorter value
  would flap a live probe.

## Consequences

**Positive:**

- A dead BLE probe is visible in HA within about 21 minutes instead of never.
- No new topics or firmware state.

**Negative:**

- A live probe can show unavailable if the heartbeat or the scan interval is raised past the constant. *Mitigation:* the host
  test fails first.
- During a broker outage longer than 1260 s the BLE entities expire, as all entities do through the LWT (last will and
  testament) under ADR-102.

## Open Questions

- [ ] Robert: accept `expire_after` 1260 s on the BLE entities as a partial supersession of ADR-102, or revert it.
  Recommendation: accept.

## Related Decisions

- **ADR-102**: availability reflects only the ESP-MQTT link. This ADR partly supersedes it, for BLE child-device entities only.
- **ADR-255**: the earlier partial supersession of ADR-102, for the simulation switch. Same form.
- **ADR-148**: BLE probes as HA child devices.
- **ADR-142** (Rejected): marking unsupported MsgIDs unavailable. Not affected.

## References

- c24417857, `src/OTGW-firmware/MQTTstuff.ino:3910-3918`, `:3976`, `:4005` (origin/dev).
- TASK-2970 HA review, finding H5 (`docs/reviews/2026-11-20_ux-ha-entities/README.md`); TASK-2971 AC1 note; TASK-2971.1.
- Home Assistant MQTT sensor `expire_after`: https://www.home-assistant.io/integrations/sensor.mqtt/
