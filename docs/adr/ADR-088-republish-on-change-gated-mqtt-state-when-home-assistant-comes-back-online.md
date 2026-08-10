---
id: "ADR-088"
title: "Republish on-change gated MQTT state when Home Assistant comes back online"
status: "Accepted"
date: "2026-08-07"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
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
---

<!-- markdownlint-disable MD025 -->

# ADR-088 Republish on-change gated MQTT state when Home Assistant comes back online

## Status

Accepted, 2026-08-07.

## Status History

```yaml
status_history:
  - date: 2026-08-07
    status: Proposed
    changed_by: Agent (Claude Code)
    reason: Initial proposal from TASK-1058 field report
    changed_via: adr-kit
  - date: 2026-08-07
    status: Accepted
    changed_by: "User: Robert van den Breemen (maintainer)"
    reason: Maintainer accepted in session; supersedes ADR-073 HA-restart trigger row; implements TASK-1058
    changed_via: adr-kit lifecycle
```

## Context and Problem Statement

ADR-073 established that a Home Assistant restart needs no firmware action, because the MQTT (Message Queuing Telemetry Transport) broker retains the discovery configs and Home Assistant re-reads them through its `homeassistant/#` subscription. That reasoning is correct for *discovery*. It does not hold for *state*.

Most MQTT state values in this firmware publish only when they change. The comparison baseline lives in RAM (random-access memory), and the published payloads are not retained on the broker. When Home Assistant restarts it rebuilds the entity from the retained discovery config, subscribes to the state topic, and then waits for a value that the firmware has no reason to send. The entity exists but has no state.

Field report from user nico55 on Discord `#nederlandse-ondersteuning`, 2026-08-06 and 2026-08-07, running 1.7.2+728426c: after every Home Assistant Core update the OpenTherm Gateway (OTGW) climate card shows its mode as "Onbekend" (unknown). A Home Assistant side gateway reset does not recover it. Rebooting the ESP (Espressif microcontroller) does.

The reporter's own capture (`transcript-20260807-211337-1.7.2+728426c-OTGW-otgw-E8DB84DC4538.txt`) contains a complete Home Assistant restart, so the mechanism is observed rather than inferred:

- Telnet 21:18:55 `homeassistant/status = offline`; 21:20:22 `= online`, followed by `Home Assistant went online!` from `src/OTGW-firmware/MQTTstuff.ino:664`.
- The climate discovery config sets `mode_stat_t` to `central_heating/value/otgw-E8DB84DC4538/hvac_mode`.
- That topic was published **0 times** across the 14-minute capture and was **absent from the broker's retained flush**, which runs through `homeassistant/*/config` to `mqtt.log` line 352 and beyond.
- The availability topic `central_heating/value/otgw-E8DB84DC4538` **is** retained with payload `online`. The entity is therefore available, not unavailable: the card renders, current temperature 22.8 °C and target 15.0 °C display correctly because `Tr` and `TrSet` publish on their own cadence, and only the mode is missing. This matches the reporter's screenshot exactly.
- `publishHvacMode()` at `src/OTGW-firmware/OTGW-Core.ino:1654` publishes only on change or on force, caching the last value in the RAM variable `mqttLastHvacMode`. The gateway had been up 1 day 5 hours with a stable mode, so no change ever occurred.
- The reporter's observation that an ESP reboot recovers matches a cold start, where every tracked value publishes as first-seen.

`hvac_mode`, the HVAC (heating, ventilation and air conditioning) mode topic, is the visible symptom because it backs a prominent card, but it is not special. Every on-change gated value has the same exposure: the 128 tracked OpenTherm MsgID (message identifier) slots, the Status and StatusVH (ventilation/heat-recovery status) bit and byte fan-outs, and the ASF (application-specific fault), RBP (remote boiler parameter) and Remote Override fan-outs.

One further constraint shapes the trigger. Home Assistant's birth message is `retain: false` by default, and the capture confirms it: `homeassistant/status` appears only live at `mqtt.log` lines 1990 and 2376, never in the retained flush. But retain is user-configurable. The current handler contains a pre-arm at `src/OTGW-firmware/MQTTstuff.ino:654`:

```cpp
if (!settings.mqtt.bHaRebootDetect) {
  bHAcycle = true;
}
```

With that pre-arm, any `online` payload counts, including a retained birth replayed to the firmware on every MQTT reconnect. A republish wired naively behind it would fire on every reconnect rather than on every Home Assistant restart. This project has a documented history of exactly that failure class: the discovery-verify runaway that exhausted the heap after roughly 80 minutes, fixed in 1.7.2.

## Decision Drivers

* Home Assistant entities must carry real state after a Home Assistant restart, without the user rebooting the gateway.
* The fix must cover every on-change gated value, not only the two HVAC topics.
* The JIT (just-in-time) discovery behaviour established by ADR-073 must survive unchanged; no discovery bulk republish may be reintroduced.
* The trigger must not fire on ordinary MQTT reconnects, under any broker or Home Assistant retain configuration.
* ESP8266 (the Espressif microcontroller this firmware targets) heap is roughly 40 KB (kilobytes) total; the reporter's capture shows 18 to 19 KB free with a largest block near 14 KB. Any republish must be paced, not bursted.

## Considered Options

* **Option A**: trigger `requestMQTTRepublishAll()` on a genuine `offline` to `online` transition of `homeassistant/status`.
* **Option B**: keep ADR-073's no-action rule and publish `hvac_mode` and `hvac_action` retained instead.
* **Option C**: trigger on every `online` payload, rate-limited to once per `MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS`.
* **Option D**: force-publish all values periodically on a timer, independent of Home Assistant state.

## Decision Outcome

Chosen option: **Option A**, because it repairs every gated value through a mechanism the firmware already ships and already exercises in production, while a transition requirement makes the trigger immune to a replayed retained birth message.

On an observed `offline` to `online` transition of `homeassistant/status`, the handler calls `requestMQTTRepublishAll()`.

No new force mechanism is introduced. `requestMQTTRepublishAll()` at `src/OTGW-firmware/OTGW-Core.ino:1359` already covers every on-change gate found in a full audit of the publish paths:

- `resetMqttTrackedState()` clears `mqttlastsent[128]`, the Status and StatusVH bit and byte slots, and the ASF, RBP and Remote Override slots.
- `requestMQTTStatusRepublish()` sets the four `mqttForceNext*StatusPublish` flags.
- `hvac_mode` and `hvac_action` are covered transitively: `publishMasterStatusState` and `publishSlaveStatusState` pass their `forcePublish` argument into `publishHvacMode` and `publishHvacAction` at `src/OTGW-firmware/OTGW-Core.ino:1725` and `:1770`.

The resulting republish is demand-driven, not a flood. `resetMqttTrackedState()` only clears timers; each value republishes when its MsgID next arrives on the OpenTherm bus, at roughly one message per second. The identical path already runs in production on the `offlineMs > MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS` reconnect branch introduced by ADR-073 section 3.

Discovery behaviour is explicitly untouched. This decision does not call `clearMQTTConfigDone()`, `clearMQTTConfigPending()`, `publishNonOTDiscoveryConfigs()` or `markAllMQTTConfigPending()` from the Home Assistant status handler. ADR-073's JIT discovery decision, its trigger table rows for boot, top-topic change and reconnect, and its force path all stand. This ADR replaces exactly one row of that table, the `HA restart (homeassistant/status → online)` row, and the Alternative 4 that justified it.

The pre-arm at `src/OTGW-firmware/MQTTstuff.ino:654` to `:658` is removed, so `bHAcycle` is set only by an observed `offline`. A retained `online` replayed on reconnect then finds `bHAcycle == false` and does nothing.

Removing the pre-arm leaves `settings.mqtt.bHaRebootDetect` with no behavioural effect, because `MQTTstuff.ino:654` is its only functional read. The setting is therefore deprecated: it continues to be parsed and written so existing `settings.ini` files load unchanged, it is removed from the web UI (user interface), and it no longer influences any code path. It is not removed from the settings struct in this decision, so no settings-file migration is required.

### Confirmation

Verified against a live Home Assistant restart on a gateway that has been running long enough for values to be stable:

1. Observe `homeassistant/status` go `offline` then `online` in the telnet debug log.
2. Confirm `central_heating/value/<uniqueid>/hvac_mode` is published within one OpenTherm bus cycle afterwards, with no ESP reboot.
3. Confirm the Home Assistant climate entity shows a real mode instead of unknown.
4. Confirm an ordinary firmware MQTT reconnect, with no preceding `offline`, produces no republish.

## Decision Contract

### Must

* The `homeassistant/status` handler calls `requestMQTTRepublishAll()` when, and only when, an `online` payload follows an observed `offline` payload.
* `bHAcycle` is set to `true` only by an observed `offline` payload.
* `settings.mqtt.bHaRebootDetect` continues to be parsed from and written to the settings file.

### Must Not

* The `homeassistant/status` handler must not call `clearMQTTConfigDone()`, `clearMQTTConfigPending()`, `publishNonOTDiscoveryConfigs()` or `markAllMQTTConfigPending()`.
* No new per-value force flag or parallel republish mechanism may be introduced; `requestMQTTRepublishAll()` is the single entry point.
* `settings.mqtt.bHaRebootDetect` must not gate any behaviour.

### Exceptions

* None.

### Verification

* `src/OTGW-firmware/MQTTstuff.ino`, the `homeassistant/status` handler.
* Field validation by the reporting user across a Home Assistant Core update, tracked in backlog TASK-1058.

## Consequences

### Positive

* Every on-change gated value recovers after a Home Assistant restart: the 128 tracked MsgID slots, Status and StatusVH bits and bytes, ASF, RBP and Remote Override fan-outs, and `hvac_mode` and `hvac_action`.
* Users stop needing to reboot the gateway after a Home Assistant Core update.
* One call site per branch. No new state, no new timer, no new configuration surface.
* The trigger is immune to broker and Home Assistant retain configuration, because it requires a transition rather than a payload.

### Negative

* If the firmware does not observe Home Assistant's last-will `offline`, for example because the firmware was itself disconnected at that moment, the following `online` does not republish. The user's recovery paths remain the existing force endpoint `POST /api/v2/otgw/discovery` (an HTTP POST request), the Serial `F` command, or a reboot. This trade-off was chosen deliberately over a rate limit, which would still republish on a replayed retained birth, merely less often.
* A republish burst follows every Home Assistant restart. Mitigation: the burst is paced by OpenTherm bus arrival rather than emitted synchronously, and the same path already runs on the reconnect branch without reported heap problems.
* `MQTTharebootdetection` becomes a setting that is stored but ignored. Mitigation: it is removed from the web UI so no user can set an expectation it will not meet, and this ADR is the record of why the stored field remains.

## Pros and Cons of the Options

### Option A: republish on a genuine offline to online transition

* Good, because it covers every on-change gated value through one existing, production-exercised helper.
* Good, because requiring a transition makes a replayed retained birth message a no-op.
* Good, because it leaves ADR-073's discovery decision completely intact.
* Bad, because a missed last-will `offline` means no republish on the following `online`.

### Option B: keep no action, publish hvac_mode and hvac_action retained

* Good, because the broker would answer Home Assistant directly with no firmware trigger at all.
* Bad, because it repairs only two topics and leaves every other gated value stale after a restart, so the reporter's class of complaint would recur on the next entity anyone noticed.
* Bad, because it grows the broker's retained set for state that is already reconstructible from the bus.

### Option C: trigger on every online, rate-limited

* Good, because it also fires when the last-will `offline` was missed.
* Bad, because a retained birth message replayed on every reconnect still triggers republishes, merely throttled; the failure mode is reduced, not removed.
* Bad, because it adds a timer and a tuning constant to reach a weaker guarantee than the transition check.

### Option D: periodic force-publish on a timer

* Good, because it is independent of any Home Assistant signal.
* Bad, because it reintroduces exactly the periodic bulk traffic that the on-change and JIT design exists to avoid.
* Bad, because this project has a documented history of republish loops exhausting the heap, fixed in 1.7.2.

## Open Questions

* None.

## Related Decisions

* **ADR-073 (JIT HA Discovery with Smart Reconnect)**: superseded by this ADR. ADR-073's trigger-table row `HA restart (homeassistant/status → online) → No action` and its Alternative 4 are replaced. All other ADR-073 decisions, including JIT discovery for OpenTherm message identifiers, the non-OT config set, and the five-minute broker-restart heuristic, remain in force and are restated here by reference rather than re-decided.
* **ADR-074**: the availability topic carries MQTT connection liveness, not OpenTherm bus state. This ADR relies on that: the entity stays available across a Home Assistant restart, which is why the symptom is a missing state rather than an unavailable entity.
* A sibling ADR on the 2.0.0 line will mirror this decision for the `dev` branch, superseding ADR-100 there.

## References

* Backlog task TASK-1058 (backlog task identifier).
* Field report: Discord `#nederlandse-ondersteuning`, user nico55, 2026-08-06 and 2026-08-07, message 1535369849235054738.
* Capture: `transcript-20260807-211337-1.7.2+728426c-OTGW-otgw-E8DB84DC4538.txt`, telnet lines for 21:18:55 and 21:20:22, `mqtt.log` lines 1990 and 2376, retained flush through line 352.
* `src/OTGW-firmware/MQTTstuff.ino:651` to `:670` — the `homeassistant/status` handler.
* `src/OTGW-firmware/OTGW-Core.ino:1359` — `requestMQTTRepublishAll()`.
* `src/OTGW-firmware/OTGW-Core.ino:438` — `resetMqttTrackedState()`.
* `src/OTGW-firmware/OTGW-Core.ino:1654` — `publishHvacMode()`.
* `src/OTGW-firmware/OTGW-Core.ino:1725` and `:1770` — `forcePublish` propagation into the HVAC publishers.
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
