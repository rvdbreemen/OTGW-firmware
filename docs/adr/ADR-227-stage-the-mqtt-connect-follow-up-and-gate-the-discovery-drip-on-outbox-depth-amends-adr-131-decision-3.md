---
id: "ADR-227"
title: "Stage the MQTT connect follow-up and gate the discovery drip on outbox depth (amends ADR-131 decision 3)"
status: "Proposed"
date: "2026-11-10"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-131"
topics:
  - "mqtt-connect-burst"
  - "heap-dip"
  - "discovery-drip"
aliases:
  - "staged connect follow-up"
  - "connect burst staging"
  - "drip outbox gate"
components:
  - "MQTTstuff"
symbols:
  - "mqttConnectStageStart"
  - "mqttConnectStageTick"
  - "discoveryOutboxAllows"
  - "discoveryOutboxWaitReset"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-227 Stage the MQTT connect follow-up and gate the discovery drip on outbox depth (amends ADR-131 decision 3)

## Status

Proposed, 2026-11-10.

## Status History

```yaml
status_history:
  - date: 2026-11-10
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-10
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-131
    changed_via: adr-kit lifecycle
```

## Context

**Decision Maker:** User: Robert van den Breemen (chose options C and B on 2026-11-10 from a list of six options, TASK-1757).

When the gateway connects to the MQTT (Message Queuing Telemetry Transport) broker, `onMqttConnect()` queued the whole connect follow-up in one callback: the birth, 8 SUBSCRIBEs (2 command topics and 6 V2-migration topics, ADR-084), 6 version-info publishes, the PIC (the OpenTherm Gateway's microcontroller) settings, `command_retry_exceeded` and `unsupported_msgids`. On an OT-Direct board that is 17 packets. ADR-131 decision 3 put this work in `onMqttConnect()` so it runs when the broker's CONNACK (connect acknowledgement) lands.

A connect-window heap trace on the bench (TASK-1757, unit .61, OT-Direct board without PSRAM (external RAM), firmware 2.0.0-beta.171) showed where the deepest boot heap dip comes from. Queueing the 17 packets cost about 4 KB (free heap 71.7 KB to 67.6 KB, t = 13 to 32 ms after CONNACK). Transmitting them all in the next 50 to 150 ms cost a further 30 to 50 KB (67.6 KB to 34.6 KB to 24.1 KB in a boot without a browser). With two browser tabs loading the web UI at the same moment, the free heap fell to 2,916 B, below the CRITICAL tier of 3,072 B (ADR-030 ladder, ADR-167). The likely cost is the transmit buffers per TCP (Transmission Control Protocol) segment in lwIP and the WiFi driver; that split was not measured separately. Robert put TASK-1757 back as a 2.0.0 stable gate after this measurement.

## Decision

The MQTT connect follow-up is sent in small portions over later loop passes, and the HA (Home Assistant) discovery drip waits while the MQTT outbox is deep.

1. `onMqttConnect()` sends only the birth (`publishBirthOnline()`), the decision to republish after more than 5 minutes offline (unchanged), and the two command SUBSCRIBEs (`<namespace>/set/<node>/#` and `homeassistant/status`), then calls `mqttConnectStageStart()`.
2. `mqttConnectStageTick()`, called from `handleMQTT()` in `MQTT_STATE_IS_CONNECTED`, runs one step per pass: (1) `mqttV2MigrationOnConnect()`, (2) `sendMQTTversioninfo()`, (3) `publishAllPICsettings()`, (4) `publishCommandDroppedState()` + `publishBoilerUnsupportedMsgids()`. A step runs only when the outbox holds at most 2 packets (`MQTT_CONNECT_STAGE_MAX_DEPTH`; the two command SUBSCRIBEs stay queued until their SUBACK) or after 3 s (`MQTT_CONNECT_STAGE_MAX_WAIT_MS`). A disconnect stops the sequence; the next connect starts it again.
3. `discoveryCanPublishNow()` also requires `discoveryOutboxAllows()`: false while the outbox holds more than 8 packets, so the drip retries the same slot on the next tick and nothing is dropped. After 10 s of continuous waiting one publish is let through and the wait starts again (one discovery config per 10 s at most under a congested link). The wait is reset on every connect and disconnect.

This amends ADR-131 decision 3 for the version info, the PIC settings, the V2-migration window, `command_retry_exceeded` and `unsupported_msgids`: they now run after the connect, from the staged tick. The birth, the command subscriptions and the republish decision stay in `onMqttConnect()` as ADR-131 decided.

## Decision Contract

### Must

- `onMqttConnect()` queues at most the birth, the two command SUBSCRIBEs and the republish decision's work; every other connect follow-up runs from `mqttConnectStageTick()`.
- Each staged step waits until the outbox depth is at most 2 packets, bounded by 3 s.
- The discovery drip gate refuses while the outbox depth is above 8, retries the same slot, and releases at most one publish per 10 s while the depth stays above 8.
- The drip wait is reset on connect and disconnect.

### Must Not

- Drop a staged publish or a discovery config because of the outbox depth.
- Move the birth or the command SUBSCRIBEs out of `onMqttConnect()`.

### Exceptions

- The republish after more than 5 minutes offline (`requestMQTTRepublishAll()`, `publishNonOTDiscoveryConfigs()`) keeps its ADR-131 place in `onMqttConnect()`; it is not on the boot path.

### Verification

- `tests/test_mqtt_connect_staged.py`: source order of `onMqttConnect()` and `handleMQTT()`, and the real `mqttConnectStageStart/Tick` and `discoveryOutboxAllows/discoveryOutboxWaitReset` host-compiled with stubs (order, depth gate, 3 s bound, disconnect stop, drip gate release, re-arm and reset). Red on the previous `MQTTstuff.ino` (4 of 4), green on this change (4 of 4).
- Bench (pending): the TASK-1757 runner on .61, boots with two real browser tabs, old (2.0.0-beta.171) versus this change; the `[HEAP] connect-trace` lines must show the connect-window minimum clearly above CRITICAL.

## Alternatives Considered

- **A. A global cap of 8 packets on every publish.** Rejected: most publish callers have no retry, so a refused publish is lost, and normal operation already sees 0 to 35 packets in the outbox, so it would drop status values outside the boot too. It would also need an ADR that partly supersedes ADR-167.
- **D. Start the discovery drip 60 s after connect.** Rejected: it only moves the first discovery config out of the connect window; the 17-packet connect burst, which the trace shows is the cost, stays.
- **E. 3 s instead of 1 s between discovery configs.** Rejected: it does not change the connect burst either, makes the drip take about 10 minutes instead of about 3.5, and reverses the TASK-1699 decision.
- **Do nothing.** Rejected: two real browser tabs reached 2,916 B, below CRITICAL, on the bench.

## Consequences

**Positive:**

- The connect burst leaves in portions of at most 6 packets (version info) on OT-Direct instead of 17 at once; the transmit peak is expected to shrink accordingly (to be measured, see Verification).
- The discovery drip no longer stacks a config on top of a deep outbox after a reconnect or a status burst; nothing is dropped.
- Birth and command subscriptions stay immediate, so Home Assistant availability and MQTT control do not change.

**Negative:**

- After a (re)connect the retained version, hardware and `command_retry_exceeded` topics, and the non-retained reboot count and reason, are refreshed a few seconds later than before; until then the broker still serves the previous retained values. Mitigation: bounded by 4 steps of at most 3 s.
- The V2-migration cleanup window (ADR-084) now opens at step 1, not at connect; its 5 s window is measured from its own subscribe, so its behaviour does not change.
- On a congested broker link the discovery drip slows to one config per 10 s. Mitigation: bounded release instead of a stall.
- The just-in-time announce of the DHW water total (ADR-182) marks its config pending and leaves it to the drip, so under a deep outbox the config can appear up to about 10 s after the first MsgID 19 sample instead of on the next drip tick. It is never dropped; ADR-182 sets no deadline.

## Open Questions

- [ ] Does the bench comparison (two real tabs, beta.171 versus this change) show the connect-window minimum clearly above CRITICAL? Acceptance waits for this measurement.

## Related Decisions

- **ADR-131 (MQTT engine on espMqttClient, async connect)**: amended; decision 3 listed version info and PIC settings as running from `onMqttConnect()`.
- **ADR-088 (MQTT status burst windowing and cooldown)**: consistent; the drip still consults `isDripDeferred()`, the outbox gate comes on top.
- **ADR-174 (republish when Home Assistant comes back online)**: consistent; the `homeassistant/status` SUBSCRIBE stays immediate and `bHAcycle` is unchanged.
- **ADR-084 (V2 migration of deprecated topics)**: its window now starts at staged step 1.
- **ADR-182 (announce the DHW water total just in time)**: consistent; the announce still follows the first MsgID 19 sample, the drip outbox gate can delay the config by up to about 10 s.

## References

- TASK-1757 notes (AC2 connect-window trace, 2026-11-10; Robert's decisions C + B and "gate back").
- `src/OTGW-firmware/MQTTstuff.ino:1672` (`onMqttConnect`), `:1754` (`mqttConnectStageTick`), `:2783` (`discoveryOutboxAllows`).
- `tests/test_mqtt_connect_staged.py`.
- `docs/c4/c4-code-mqtt.md` (connect sequence and drip outbox gate).

## Enforcement

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [
    {
      "pattern": "mqttConnectStageStart\\(\\);",
      "path_glob": "src/OTGW-firmware/MQTTstuff.ino",
      "message": "ADR-227: onMqttConnect() hands the connect follow-up to the staged tick."
    }
  ],
  "llm_judge": false,
  "llm_judge_reason": "The behavioural contract is covered by tests/test_mqtt_connect_staged.py; a per-commit model call adds cost without extra coverage."
}
```
