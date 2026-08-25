---
id: "ADR-174"
title: "Republish on-change gated MQTT state when Home Assistant comes back online"
status: "Accepted"
date: "2026-08-07"
binding: false
gate: null
documents_shipped: false
verified_in: []
superseded_by: null
topics:
  - "mqtt"
  - "home-assistant"
  - "discovery"
  - "state-republish"
  - "availability"
aliases:
  - "HA restart republish"
  - "hvac_mode unknown after HA restart"
  - "homeassistant/status online trigger"
components:
  - "MQTT HA status handler"
  - "MQTT publish gating"
symbols:
  - "bHAcycle"
  - "requestMQTTRepublishAll"
  - "resetMqttTrackedState"
  - "requestMQTTStatusRepublish"
  - "publishHvacMode"
  - "publishHvacAction"
  - "bHaRebootDetect"
context_scope: "selective"
format: "madr"
supersedes:
  - "ADR-100"
---

<!-- markdownlint-disable MD025 -->

# ADR-174 Republish on-change gated MQTT state when Home Assistant comes back online

## Status

Accepted, 2026-08-07.

## Status History

```yaml
status_history:
  - date: 2026-08-07
    status: Proposed
    changed_by: Agent (Claude Code)
    reason: Port of otgw-1.x.x ADR-088; supersedes ADR-100 HA-restart trigger row
    changed_via: adr-kit
  - date: 2026-08-07
    status: Accepted
    changed_by: "User: Robert van den Breemen (maintainer)"
    reason: Maintainer accepted in session; ports otgw-1.x.x ADR-088, supersedes ADR-100 HA-restart trigger row
    changed_via: adr-kit lifecycle
```

## Context and Problem Statement

This ADR ports the decision from `otgw-1.x.x` ADR-088 to the 2.0.0 worktree, exactly as ADR-100 ported ADR-073. The context and evidence are identical and are not restated in full here; see ADR-088 for the complete field report and capture analysis.

Short summary. ADR-100 established that a Home Assistant restart needs no firmware action, because the MQTT (Message Queuing Telemetry Transport) broker retains the discovery configs and Home Assistant re-reads them through its `homeassistant/#` subscription. That reasoning is correct for *discovery* but not for *state*.

Most MQTT state values publish only on change. The comparison baseline lives in RAM (random-access memory) and the payloads are not retained on the broker. After a Home Assistant restart the entity is rebuilt from the retained discovery config, subscribes to its state topic, and then waits for a value the firmware has no reason to send. The entity exists but carries no state.

Field evidence, from a 1.7.2 ESP8266 gateway captured by user nico55 on Discord `#nederlandse-ondersteuning`, 2026-08-06 and 2026-08-07:

- The capture contains a complete Home Assistant restart: `homeassistant/status = offline` at 21:18:55, `= online` at 21:20:22.
- The climate discovery config sets `mode_stat_t` to `<toptopic>/value/<uniqueid>/hvac_mode`. That topic was published 0 times in 14 minutes and was absent from the broker's retained flush.
- The availability topic is retained `online`, so the entity is available, not unavailable. The card renders with correct temperatures and only the mode missing.
- `publishHvacMode()` publishes only on change or force, caching the last value in RAM.

The 2.0.0 branch carries the identical handler and the identical gating machinery, so it carries the identical defect. Verified by reading both trees: `src/OTGW-firmware/MQTTstuff.ino:800` to `:818` here versus `:651` to `:670` on `otgw-1.x.x`, and `requestMQTTRepublishAll()` at `src/OTGW-firmware/OTGW-Core.ino:1788` here versus `:1359` there.

The pre-arm that shapes the trigger is also identical, at `src/OTGW-firmware/MQTTstuff.ino:802`:

```cpp
if (!settings.mqtt.bHaRebootDetect) {
  bHAcycle = true;
}
```

With it, any `online` payload counts, including a retained birth message replayed on every MQTT reconnect.

## Decision Drivers

* Home Assistant entities must carry real state after a Home Assistant restart, without the user rebooting the gateway.
* The fix must cover every on-change gated value, not only the two HVAC (heating, ventilation and air conditioning) topics.
* The JIT (just-in-time) discovery behaviour established by ADR-100 must survive unchanged.
* The trigger must not fire on ordinary MQTT reconnects, under any broker or Home Assistant retain configuration.
* Both firmware lines must agree on `homeassistant/status` semantics, because it is a Home Assistant side contract and a divergence would make the two branches behave differently against the same broker.

## Considered Options

* **Option A**: mirror ADR-088 exactly — trigger `requestMQTTRepublishAll()` on a genuine `offline` to `online` transition.
* **Option B**: leave 2.0.0 on the ADR-100 no-action rule and fix only the 1.x line.
* **Option C**: design a different trigger for 2.0.0, taking advantage of the async MQTT path.

## Decision Outcome

Chosen option: **Option A**, because `homeassistant/status` is a Home Assistant side contract and the two firmware lines must not disagree about it.

On an observed `offline` to `online` transition of `homeassistant/status`, the handler calls `requestMQTTRepublishAll()`. The `!settings.mqtt.bHaRebootDetect` pre-arm at `src/OTGW-firmware/MQTTstuff.ino:802` to `:806` is removed, so `bHAcycle` is set only by an observed `offline`.

`requestMQTTRepublishAll()` at `src/OTGW-firmware/OTGW-Core.ino:1788` already covers every on-change gate, and `hvac_mode` and `hvac_action` are covered transitively through the `forcePublish` argument that `publishMasterStatusState` and `publishSlaveStatusState` pass into `publishHvacMode` and `publishHvacAction`. No new mechanism is introduced.

Discovery behaviour is untouched: this decision does not call `clearMQTTConfigDone()`, `clearMQTTConfigPending()`, `publishNonOTDiscoveryConfigs()` or `markAllMQTTConfigPending()` from the Home Assistant status handler. ADR-100's JIT discovery decision, its other trigger-table rows, and its force path all stand. This ADR replaces exactly one row, the `HA restart (homeassistant/status → online)` row.

`settings.mqtt.bHaRebootDetect` is deprecated on this branch on the same terms as ADR-088: still parsed and written so existing settings files load unchanged, removed from the web UI (user interface), and gating nothing.

Platform note for this branch. 2.0.0 targets the ESP32-S3 with an async web stack, where request handlers run on the `async_tcp` task rather than the Arduino loop. The republish path is unchanged by that: `resetMqttTrackedState()` only clears timers, and each value republishes when its OpenTherm message identifier next arrives on the bus. The implementing task must confirm the republish burst introduces no re-entrancy hazard on the async MQTT path before the ADR is treated as shipped.

### Confirmation

Same as ADR-088, executed on 2.0.0 hardware:

1. Observe `homeassistant/status` go `offline` then `online` in the telnet debug log.
2. Confirm `<toptopic>/value/<uniqueid>/hvac_mode` is published within one OpenTherm bus cycle, with no reboot.
3. Confirm the Home Assistant climate entity shows a real mode instead of unknown.
4. Confirm an ordinary firmware MQTT reconnect, with no preceding `offline`, produces no republish.

## Decision Contract

### Must

* The `homeassistant/status` handler calls `requestMQTTRepublishAll()` when, and only when, an `online` payload follows an observed `offline` payload.
* `bHAcycle` is set to `true` only by an observed `offline` payload.
* `settings.mqtt.bHaRebootDetect` continues to be parsed from and written to the settings file.
* This branch's behaviour matches `otgw-1.x.x` ADR-088; any intentional divergence requires its own ADR.

### Must Not

* The `homeassistant/status` handler must not call `clearMQTTConfigDone()`, `clearMQTTConfigPending()`, `publishNonOTDiscoveryConfigs()` or `markAllMQTTConfigPending()`.
* No new per-value force flag or parallel republish mechanism may be introduced.
* `settings.mqtt.bHaRebootDetect` must not gate any behaviour.

### Exceptions

* None.

### Verification

* `src/OTGW-firmware/MQTTstuff.ino`, the `homeassistant/status` handler.
* Field validation on 2.0.0 hardware, tracked in the sibling backlog task of TASK-1058.

## Consequences

### Positive

* Every on-change gated value recovers after a Home Assistant restart, matching the 1.x line.
* The two firmware lines agree on `homeassistant/status` semantics, so a user moving between them sees identical behaviour.
* One call site. No new state, timer, or configuration surface.

### Negative

* If the firmware does not observe Home Assistant's last-will `offline`, the following `online` does not republish. Recovery remains the force endpoint, the Serial force command, or a reboot.
* A republish burst follows every Home Assistant restart. Mitigation: it is paced by OpenTherm bus arrival, and the implementing task must confirm no re-entrancy hazard on the async path.
* `MQTTharebootdetection` becomes stored but ignored. Mitigation: removed from the web UI; this ADR records why the field remains.

## Pros and Cons of the Options

### Option A: mirror ADR-088

* Good, because it keeps a Home Assistant side contract identical across both firmware lines.
* Good, because the mechanism already exists on this branch and needs one call site.
* Bad, because a missed last-will `offline` means no republish on the following `online`.

### Option B: fix only the 1.x line

* Good, because it is less work now.
* Bad, because the two branches would disagree about `homeassistant/status` against the same broker, and 2.0.0 users would keep reporting a defect already fixed elsewhere.

### Option C: a different, async-specific trigger for 2.0.0

* Good, because it could exploit the async stack.
* Bad, because no evidence suggests the async path needs a different trigger, and divergence on a Home Assistant contract costs more than it buys.

## Open Questions

* None.

## Related Decisions

* **ADR-100 (JIT HA Discovery with Smart Reconnect, Port of dev ADR-073)**: superseded by this ADR. Only the `HA restart (homeassistant/status → online)` trigger-table row is replaced; every other ADR-100 decision remains in force.
* **`otgw-1.x.x` ADR-088**: the originating decision that this ADR ports. The two must stay in agreement.

## References

* `otgw-1.x.x` ADR-088, Accepted 2026-08-07.
* Backlog task TASK-1058 (the `otgw-1.x.x` task) and its 2.0.0 sibling.
* Field report: Discord `#nederlandse-ondersteuning`, user nico55, 2026-08-06 and 2026-08-07, message 1535369849235054738.
* Capture: `transcript-20260807-211337-1.7.2+728426c-OTGW-otgw-E8DB84DC4538.txt`.
* `src/OTGW-firmware/MQTTstuff.ino:800` to `:818` — the `homeassistant/status` handler on this branch.
* `src/OTGW-firmware/OTGW-Core.ino:1788` — `requestMQTTRepublishAll()` on this branch.
* `src/OTGW-firmware/OTGW-Core.ino:2095` — `publishHvacMode()` on this branch.
* Home Assistant MQTT integration, birth and will message defaults: <https://www.home-assistant.io/integrations/mqtt/>

## Enforcement

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": true
}
```
