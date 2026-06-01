# ADR-093 Home Assistant Discovery Retained-Config Orphan Cleanup

## Status

Accepted (guideline-level per ADR-080: one publisher today; promote to pattern-level with a CI gate when a second discoverable component adopts the pattern). Date: 2026-05-01.

**Complemented by:** ADR-094 (2026-05-03) — per-device, firmware-driven removal on OTA upgrade. ADR-093 covers per-entity, user-driven removal (BLE roster forget); ADR-094 covers per-device, firmware-driven removal (OTA upgrade). Both publish zero-byte retained payloads via the same `beginMqttPublish + endPublish` streaming primitives. Together they form the complete HA discovery removal lifecycle on this branch.

## Context

Home Assistant MQTT auto-discovery (ADR-006, ADR-077) advertises every entity by publishing a retained JSON config to a deterministic topic of the form `<HaPrefix>/sensor/<uniqueId>_<scope>_<key>/config`. The retained flag is essential: it lets HA reconstruct the entity set after a restart without needing the publisher to be online. The downside is symmetric. Once a retained config is on the broker, it stays there forever unless the publisher actively removes it. If the publisher merely stops emitting the entity, HA keeps the orphan in its registry and the user has to delete it manually via the HA UI or by hand-clearing the topic with MQTT Explorer. Bad UX, and worse, it accumulates silently.

Until TASK-508 the firmware never had a lifecycle event where it removed a previously advertised entity, so the question did not come up. TASK-508 introduced a self-discovering BLE roster: every BLE temperature sensor seen on-air enters an 8-slot roster, the user names slots ("Woonkamer", "Slaapkamer"), and selects one as the active SAT input. The "Forget" button drops a slot from the roster. At that point the roster slot's four retained HA configs (temperature, humidity, battery, RSSI) become orphans on the broker, with no publisher behind them. The cleanup is the publisher's responsibility, and Home Assistant has a specific contract for how to do it.

The contract is documented in the HA MQTT discovery spec: publish an empty (zero-byte) retained payload to the same config topic. HA interprets that as "device removed" and unregisters the entity. Any other shape (a `"removed": true` payload, a non-retained delete, a different topic) does nothing useful. The orphan stays.

## Decision

When the firmware terminates the lifecycle of an entity that was previously advertised via retained HA discovery, it MUST publish a zero-byte retained payload to each of that entity's discovery config topics, using the same MQTT streaming primitives and the same heap and connectivity gates as the publish path.

Concretely, for the BLE roster (today's sole adopter):

1. **Symmetric topic set.** `satBLEUnpublishDiscovery(macCompact)` in `src/OTGW-firmware/MQTTstuff.ino:2257` walks the same four kind keys (`temp`, `rh`, `bat`, `rssi`) that `satBLEPublishOneDiscovery` uses, and constructs the same topic via the same `snprintf_P(... PSTR("%s/sensor/%s_ble_%s_%s/config") ...)` template at line 2274. Any drift between publish and unpublish topic templates leaves orphans on the broker, so the kind list and topic format live next to each other and reference the same comment trail.

2. **Zero-byte retained payload via streaming primitives.** The cleanup uses `beginMqttPublish(topic, /*len=*/0, /*retain=*/true)` followed immediately by `MQTTclient.endPublish()` with no chunk writes in between. This reuses the streaming compose path from ADR-077 so retain semantics, payload framing, and TCP behavior are byte-identical to the publish side. No buffered alternative; no `mqttClient.publish(topic, "", true)` fallback. The streaming path is the only path.

3. **Heap and connectivity gates match the publish side.** Cleanup runs only when `settings.mqtt.bEnable && state.mqtt.bConnected && isValidIP(MQTTbrokerIP) && canPublishMQTT()` (lines 2260-2262). The implicit `MQTT_DISCOVERY_HEAP_MIN` floor is enforced by the streaming primitives themselves. If gates fail, the cleanup silently no-ops and the orphans remain on the broker; the alternative (forcing the publish under low-heap conditions) risks an OOM at exactly the moment the user is trying to remove a sensor.

4. **Caller gates on `bDiscoveryPublished`.** The cleanup is invoked from `satBLERosterForget()` in `src/OTGW-firmware/SATble.ino:844` only when the slot's runtime `bDiscoveryPublished` flag is true. A slot whose discovery configs were never published has nothing to wipe; calling the cleanup unconditionally would still be correct (zero-byte retained on a non-existent topic is a no-op) but would generate four pointless network round-trips per forget.

5. **`feedWatchDog()` between publishes.** Even though the four publishes are tiny, the loop yields the watchdog after each (line 2285). Cooperative ESP8266 hygiene: the cleanup is on the same critical path as everything else.

This ADR is **guideline-level** under ADR-080. There is one adopter today (BLE roster). The pattern is small enough that documenting it is more valuable than gating it. When a second discoverable component (Dallas dynamic sensors, S0 pulse counter on/off, OTGW-PIC binary sensors, etc.) adopts the same lifecycle, a follow-up ADR can promote it to pattern-level and add an `evaluate.py` check that any function named `*PublishOneDiscovery` has a sibling `*UnpublishDiscovery` with a matching kind list.

## Alternatives Considered

### Alternative A: Leave orphans for the user to clean up manually

Do nothing on forget. The retained configs sit on the broker; HA shows the entities as "Unavailable"; the user clears them via HA's MQTT integration UI or by deleting the retained topic with an external tool. Rejected: the BLE roster is explicitly a self-discovering UX; the whole point of TASK-508 is that the user does not have to touch the broker. Leaving orphans behind every "Forget" click defeats the design goal and accumulates broker state that no one owns. It is also surprising: the publish side is automatic, so the user reasonably expects the unpublish side to be automatic too.

### Alternative B: Publish a custom "removed" payload

Send a non-empty retained JSON like `{"removed": true}` or `{"availability": false}` to the same config topic, on the assumption that some downstream consumer (HA, a rule, a dashboard) might key off it. Rejected on two counts. First, it does not work: HA's MQTT discovery contract specifically requires zero-byte payload as the removal signal, and any non-empty retained payload is interpreted as a (malformed) config update, which leaves the entity registered with broken metadata. Second, even if it worked, inventing a custom shape for an interoperability protocol invites every other consumer of the topic to invent its own. The retained-empty-payload contract is the documented HA behavior; we follow it.

### Alternative C: Publish via the lower-level `mqttClient.publish(topic, "", true)` shortcut instead of the streaming primitives

Bypass `beginMqttPublish` / `endPublish` and use the PubSubClient one-shot API, since the payload is empty. Rejected because the streaming path enforces the heap-floor and connectivity gates that ADR-077 set up; bypassing them creates a low-heap OOM hole at the exact moment the user is removing a sensor (the broker round-trip during forget could fire under heap pressure if the ESP is also doing other work). Using the same primitives also keeps the publish and unpublish paths visually parallel, which makes drift between the two easier to catch in code review.

## Consequences

**Benefits**

- HA entity registry stays consistent with the firmware's view of which devices are actively published. No silent broker drift.
- The forget path is symmetric with the publish path: same topic template, same streaming primitives, same gates. A future maintainer who reads either side can predict the other.
- Reusable for future discoverable components without inventing a new shape.

**Trade-offs**

- Adds ~30 lines of code (the helper itself plus the `bDiscoveryPublished` callsite gate). Marginal but non-zero flash and RAM cost.
- The unpublish runs on the loop task; under MQTT disconnect at the moment of forget, the cleanup silently no-ops and the orphans persist. The current implementation does not retry on reconnect. Acceptable today (the user can re-add and re-forget), but a future reconnect-time replay queue is one of the obvious extensions.
- The kind list is duplicated between `satBLEPublishOneDiscovery` and `satBLEUnpublishDiscovery`. A static `constexpr` table shared between both would remove the duplication; left for a follow-up because today the four kinds are stable enough that the duplication is cheap insurance against accidental drift on either side.

**Risks and mitigations**

- *Risk*: kind list or topic template drifts between publish and unpublish, leaving partial orphans (e.g. `temp` cleaned but `rssi` left). *Mitigation*: comment trail at `MQTTstuff.ino:2267-2269` explicitly names the publish-side function as the contract anchor; both sites use the same `PSTR` topic template. Future addition or rename of a kind requires touching both, and code review catches it.
- *Risk*: cleanup runs while MQTT is offline and silently no-ops, leaving orphans. *Mitigation*: gated on `state.mqtt.bConnected` plus `canPublishMQTT()`; documented as an accepted limitation. The user can re-issue forget after reconnect.
- *Risk*: low-heap OOM during the unpublish loop. *Mitigation*: streaming primitives enforce `MQTT_DISCOVERY_HEAP_MIN`; on failure each iteration logs through `PrintMQTTError()` and `feedWatchDog()` runs unconditionally between iterations.
- *Risk*: future contributor adds a new BLE entity kind to the publish path but forgets to extend the unpublish path. *Mitigation*: today, code review and the side-by-side comment. If a second adopter shows up, an `evaluate.py` rule pairing `*PublishOneDiscovery` kind lists with their sibling unpublish becomes worth writing (promote to pattern-level under ADR-080).

## Related Decisions

- **ADR-006 (MQTT Integration Pattern)**: foundational MQTT and HA discovery design; this ADR adds the symmetric removal half of the lifecycle.
- **ADR-077 (Streaming MQTT HA Discovery Architecture)**: the publish side. Unpublish reuses the same streaming primitives (`beginMqttPublish` / `endPublish`) and the same heap floor (`MQTT_DISCOVERY_HEAP_MIN`). Topic templates on the unpublish side must mirror the publish side byte-for-byte.
- **ADR-041 (Just-In-Time Home Assistant MQTT Discovery)**: governs when discovery is first emitted (after a real OT message). This ADR governs when discovery is removed (on explicit user-driven forget).
- **ADR-085 (SAT Integration)**: the SAT subsystem is the first adopter through the BLE roster.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is explicitly guideline-level until a second adopter justifies a CI check.

## References

- TASK-508 (`backlog/tasks/task-508 - feat-satble-self-discovering-ble-roster-with-labels-auto-select-and-ha-propagation.md`).
- Implementation: `src/OTGW-firmware/MQTTstuff.ino:2257-2289` (`satBLEUnpublishDiscovery`), invoked from `src/OTGW-firmware/SATble.ino:844-851` (`satBLERosterForget`), forward-declared in `src/OTGW-firmware/OTGW-firmware.h:269`.
- Commit `b20f5770` on `feature-dev-2.0.0-otgw32-esp32-sat-support`.
- Home Assistant MQTT discovery removal contract: <https://www.home-assistant.io/integrations/mqtt/#removing-devices-and-entities-from-the-mqtt-integration> (zero-byte retained payload to the discovery topic).
