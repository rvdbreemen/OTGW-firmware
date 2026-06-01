# ADR-098: MQTT Discovery Topic Sibling-Suffix Shape (Supersedes ADR-097)

## Status

Accepted, 2026-05-07. Decision Maker: User: Robert van den Breemen (rvdbreemen).

## Context

ADR-097 (Accepted 2026-05-07, the 2.0.0 mirror of dev's ADR-070) flipped the **state topic** shape for source-separated values from nested children to sibling-suffix:

| Layer | State topic (post-ADR-097) |
|---|---|
| canonical | `OTGW/value/<id>/TSet` |
| thermostat view | `OTGW/value/<id>/TSet_thermostat` |
| boiler view | `OTGW/value/<id>/TSet_boiler` |

ADR-097 explicitly carved out the **discovery topic** as exempt from the sibling-suffix rule (line 56 in the body, line 126 in the Enforcement carve-out comment):

> "Discovery topic identifiers stay nested. `homeassistant/sensor/<id>/<label>/<src>/config` is internal to HA — it's an identifier, not a state topic. Keeping it stable means the existing retained discovery configs are *updated in place* with new `stat_t` payloads. HA picks up the change automatically on the next discovery cycle because `unique_id` matches and `subscription.async_prepare_subscribe_topics` handles the topic delta cleanly."

The cited evidence (`subscription.async_prepare_subscribe_topics` handling deltas in-place) is correct in itself, but **not load-bearing for the conclusion**. That function only runs *after* a discovery payload is accepted by the discovery dispatcher. The actual gate on whether HA accepts a discovery topic at all is `discovery.py:TOPIC_MATCHER` — which neither ADR-070 nor ADR-097 checked.

Empirical investigation against Home Assistant `dev` branch source (`homeassistant/components/mqtt/discovery.py`, fetched 2026-05-07 by the dev-side investigation that produced ADR-071):

```python
TOPIC_MATCHER = re.compile(
    r"(?P<component>\w+)/(?:(?P<node_id>[a-zA-Z0-9_-]+)/)"
    r"?(?P<object_id>[a-zA-Z0-9_-]+)/config"
)
```

The `object_id` character class is `[a-zA-Z0-9_-]+` — explicitly **excludes the forward slash**. When `discovery.py:async_discovery_message_received` (line 396) fails to match a topic, the handler at line 397-406 logs:

> "Received message on illegal discovery topic '%s'. The topic contains non allowed characters."

…and silently discards the message. The payload never reaches `subscription.async_prepare_subscribe_topics`.

Empirical regex test against the OTGW topic shapes produced under ADR-097:

| Topic (after stripping `homeassistant/`) | Regex result |
|---|---|
| `sensor/otgw-X/TSet/config` (canonical) | ✓ MATCH (`object_id=TSet`) |
| `sensor/otgw-X/TSet/thermostat/config` (current nested source variant) | ✗ **REJECTED** — discarded by HA, warning logged |
| `sensor/otgw-X/TSet/boiler/config` (current nested source variant) | ✗ **REJECTED** — discarded by HA, warning logged |
| `sensor/otgw-X/TSet_thermostat/config` (sibling) | ✓ MATCH (`object_id=TSet_thermostat`) |
| `sensor/otgw-X/TSet_boiler/config` (sibling) | ✓ MATCH (`object_id=TSet_boiler`) |

The HA-side behaviour is **platform-independent**: the regex check happens inside Home Assistant, not on the firmware. The discovery-topic format the firmware publishes is byte-identical between the ESP8266 and ESP32-S3 (OTGW32) builds of this 2.0.0 line, because both targets share `MQTTHaDiscovery.cpp`'s `buildSensorDiscoveryTopic`. The same fix therefore applies to **every 2.0.0-alpha.5 user**, regardless of firmware target.

**Field consequence on 2.0.0-alpha.5:** every user with `bSeparateSources=true` is currently seeing the canonical entity register in HA but the thermostat-source and boiler-source variants silently dropped — both on ESP8266 and on ESP32-S3 boards. Mosquitto and MQTT Explorer retain the rejected configs (no broker-side validation), creating the misleading appearance that the configs were delivered. The HA logs contain the warning but most alpha testers do not watch those.

This invalidates the ADR-097 carve-out for discovery topics. The sibling-suffix rule must extend to the discovery topic builder as well.

## Decision

Use **sibling-suffix shape for discovery topics** in addition to state topics:

| Layer | Discovery topic |
|---|---|
| canonical | `homeassistant/sensor/<id>/<label>/config` (unchanged) |
| thermostat view | `homeassistant/sensor/<id>/<label>_thermostat/config` (was nested under ADR-097) |
| boiler view | `homeassistant/sensor/<id>/<label>_boiler/config` (was nested under ADR-097) |

State-topic shape from ADR-097 is **unchanged** (already sibling-suffix and accepted by HA without issue).

Concrete code site (2.0.0 feature line):

| File | Site | Change |
|---|---|---|
| `src/OTGW-firmware/MQTTHaDiscovery.cpp:2320-2336` | `buildSensorDiscoveryTopic` | format string `"%s/sensor/%s/%s/%s/config"` (line 2329) → `"%s/sensor/%s/%s_%s/config"` for the source-variant branch. Canonical-branch format (line 2332) unchanged. |

The `uniq_id` field of the discovery payload is unaffected — `composeSensorPayload` (`MQTTHaDiscovery.cpp:2131-2141`) already builds `<nodeId>-<label><sourceSuffix>` with underscore separation under ADR-097, so HA's identity-tracking continues to deduplicate correctly across the topic-shape migration. Likewise the `stat_t` builder (`MQTTHaDiscovery.cpp:2157-2173`) is already sibling-suffix per ADR-097 and is not touched by this ADR. ADR-094 boot-time discovery republish delivers the new shape to HA on the next firmware boot.

**Migration of zombie retained configs.** Pre-ADR-098 retained configs at the rejected nested paths (`homeassistant/sensor/<id>/<label>/<src>/config`) linger in the broker after firmware upgrade. They never registered in HA (the regex rejected them), so they are functionally invisible — but mosquitto retains them and topic browsers display them. Two cleanup paths:

1. **Recommended (manual, one-time):** users run the cleanup recipe in `docs/api/MQTT.md` (`mosquitto_pub -t '<topic>' -r -n`) against the nested paths.
2. **Optional firmware-side cleanup (deferred):** firmware could publish empty retained payloads to the old nested paths on first boot after upgrade, mirroring the discovery republish loop. Deferred because: (a) HA never registered the topics so there is no functional impact; (b) the firmware would need to remember historical topic shapes to know which paths to clear, which is a maintenance liability; (c) the manual recipe is well-documented and one-time.

The dev (1.5.x) line carries the same change under ADR-071; this ADR is the explicit 2.0.0 mirror, deployed on the same calendar day.

## Alternatives Considered

1. **Edit ADR-097 in place to remove the carve-out.** Rejected: violates project ADR immutability rule (CLAUDE.md "NEVER edit an Accepted or Deprecated ADR"). Also destroys the audit trail of *why* the original carve-out was wrong (the misread of HA's subscription delta logic), which is the most valuable architectural lesson here. Supersession preserves the trail.
2. **Flip discovery to sibling-suffix AND revert state topic to nested.** Rejected: state-topic sibling shape is already deployed in 2.0.0-alpha.5 (TASK-554); reverting would churn user dashboards twice without architectural benefit. HA does not care about state-topic structure (any string is accepted), so the state-topic sibling shape is purely a topic-browser ergonomics improvement that should remain.
3. **Use `<id>__<src>` (double underscore) to disambiguate from labels that already contain underscores.** Rejected: HA's `object_id` character class accepts `_` freely, no disambiguation is needed; would diverge from the underscore convention already used in `uniq_id`. Risk of label-source collision is negligible because the firmware's label set is fixed at compile time.
4. **Switch `buildSensorDiscoveryTopic` to use the `node_id` slot for the entity label** (`homeassistant/sensor/<id>_<src>/<label>/config`). Rejected: the `node_id` slot is conventionally used for the *device* identifier (and HA tutorials, third-party tooling, and the OTGW `nodeId` already populate it that way). Repurposing it would break wildcard subscriptions like `homeassistant/sensor/<otgw-id>/+/config`.

## Consequences

**Positive:**

- Source-variant entities (`bSeparateSources=true`) actually register in HA on **both ESP8266 and ESP32-S3 builds**. Restores the feature that ADR-095 → ADR-096 → ADR-097 evolution was building toward on the 2.0.0 line.
- Removes a silent-failure mode: the firmware appeared to publish discovery, the broker retained it, but HA discarded it. With siblings, success is visible end-to-end.
- Discovery-topic shape now matches state-topic shape (both sibling-suffix). Symmetric, easier mental model for users inspecting both trees.
- Eliminates the `subscription.py` carve-out citation that was the load-bearing error in ADR-097 — future maintainers reading the ADR chain see the corrected reasoning.
- 2.0.0-specific: ESP32-S3 (OTGW32) builds get the fix via the same shared `MQTTHaDiscovery.cpp` translation unit, so the change propagates to both targets in one edit.

**Negative:**

- Old retained discovery configs at the nested paths linger in user brokers as zombies on both ESP8266 and ESP32-S3 deployments. They were already invisible to HA (rejected with warning), so the functional impact is nil, but tree browsers will display both old (nested) and new (sibling) configs side-by-side until manually cleared. Documented in `docs/api/MQTT.md`.
- Alpha testers who already verified the canonical-only entities under ADR-097 must re-test with `bSeparateSources=true` on **each target board they care about** to confirm the source variants now appear. One additional validation cycle per branch and per target.
- Users who ran `mosquitto_pub -r -n` cleanup against the rejected nested topics (per ADR-097 migration note) before this ADR landed already cleared the zombies; the current change does not invalidate their cleanup work.

**Neutral:**

- The `composeSensorPayload` `uniq_id` construction (`MQTTHaDiscovery.cpp:2131-2141`) is already underscore-separated and matches the new discovery-topic suffix. No change required there. HA's identity tracking continues to deduplicate cleanly across the migration.
- The `stat_t` builder (`MQTTHaDiscovery.cpp:2157-2173`) is already sibling-suffix per ADR-097. Discovery-topic and state-topic shapes will be symmetrically suffixed after this ADR lands.

## Related Decisions

- **Supersedes ADR-097** (`MQTT Source-Topic Sibling-Suffix Shape`, the 2.0.0 port of ADR-070). After this ADR is Accepted, ADR-097's Status line will be updated to `Superseded by ADR-098, 2026-05-07.` per project supersession protocol. ADR-097's body remains immutable as the historical record of the original (partially-correct) decision.
- **Builds on ADR-094** (HA discovery state reconciliation on OTA upgrade — the trigger that delivers the corrected shape to HA without manual user action on the 2.0.0 line).
- **Preserves ADR-093** (HA discovery retained-config orphan cleanup), **ADR-095** (already superseded by ADR-097; this ADR does not reverse that supersession — additive layout under sibling shape remains the right call), **ADR-096** (worldview routing semantics retained; only topic-string shape changes for discovery, mirroring the state-topic change ADR-097 made).
- **Dev sibling: ADR-071** (`docs/adr/ADR-071-mqtt-discovery-topic-sibling-suffix-shape.md` in the `dev` worktree). Same decision, same calendar day, sibling release line. Both worktrees adopt the discovery sibling-suffix shape on 2026-05-07, mirroring the ADR-070/ADR-097 deployment pattern.
- **Backlog tasks:** TASK-557 (this ADR + 2.0.0 implementation), TASK-554 (the original ADR-097 implementation task whose carve-out is being reversed), and the dev-side TASK-556 (the sibling implementation under ADR-071).

## References

- Home Assistant source: `homeassistant/components/mqtt/discovery.py` lines 63-66 (`TOPIC_MATCHER`), 388-406 (`async_discovery_message_received` with reject-and-warn path). https://github.com/home-assistant/core/blob/dev/homeassistant/components/mqtt/discovery.py
- Empirical regex test executed 2026-05-07 by the dev-side investigation against the live HA `dev`-branch source confirms the rejection of nested discovery paths. The same regex applies to the 2.0.0 firmware because the firmware publishes byte-identical topic strings; output captured in TASK-556 implementation notes (dev) and referenced from TASK-557 (this 2.0.0 work).
- ADR-097 (`docs/adr/ADR-097-mqtt-source-topic-sibling-suffix-shape.md` in this worktree) — the superseded ADR; line 56 (body carve-out) and line 126 (Enforcement carve-out comment) contain the rule being reversed.
- ADR-071 (`docs/adr/ADR-071-mqtt-discovery-topic-sibling-suffix-shape.md` in the `dev` worktree) — the dev-side sibling, same decision and same evidence.
- Firmware code path being changed: `src/OTGW-firmware/MQTTHaDiscovery.cpp:2320-2336` (`buildSensorDiscoveryTopic`).
- Field evidence: 2.0.0-alpha.5 testers report identical symptom to the dev-side beta.21 finding — canonical entity visible in HA, source variants silently absent under `bSeparateSources=true`. Same root cause confirmed by the regex test cited above.

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "PSTR\\(\"%s/sensor/%s/%s/%s/config\"\\)",
      "path_glob": "src/OTGW-firmware/MQTTHaDiscovery.cpp",
      "message": "Discovery topics for source variants must use sibling-suffix shape (`%s/sensor/%s/%s_%s/config`) per ADR-098. Nested-children format is rejected by HA's discovery TOPIC_MATCHER (object_id character class excludes '/')."
    }
  ],
  "llm_judge": false
}
```

The state-topic sibling-suffix Enforcement block from ADR-097 (`PSTR("%s/(thermostat|boiler)")` forbidden in `MQTTstuff.ino`) remains in force — ADR-098 does not amend the state-topic shape.
