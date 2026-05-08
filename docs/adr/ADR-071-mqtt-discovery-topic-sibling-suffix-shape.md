# ADR-071: MQTT Discovery Topic Sibling-Suffix Shape (Supersedes ADR-070)

## Status

Accepted, 2026-05-07. Decision Maker: User: Robert van den Breemen (rvdbreemen).

## Context

ADR-070 (Accepted 2026-05-07, commits `7c33e0c9` dev / pending 2.0.0) flipped the **state topic** shape for source-separated values from nested children to sibling-suffix:

| Layer | State topic (post-ADR-070) |
|---|---|
| canonical | `OTGW/value/<id>/TSet` |
| thermostat view | `OTGW/value/<id>/TSet_thermostat` |
| boiler view | `OTGW/value/<id>/TSet_boiler` |

ADR-070 explicitly carved out the **discovery topic** as exempt from the sibling-suffix rule (line 54, line 121 Enforcement carve-out):

> "Discovery topic identifiers stay nested. `homeassistant/sensor/<id>/<label>/<src>/config` is internal to HA — it's an identifier, not a state topic. Keeping it stable means the existing retained discovery configs are *updated in place* with new `stat_t` payloads. HA picks up the change automatically on the next discovery cycle because `unique_id` matches and `subscription.async_prepare_subscribe_topics` handles the topic delta cleanly."

The cited evidence (`subscription.async_prepare_subscribe_topics` handling deltas in-place) is correct in itself, but **not load-bearing for the conclusion**. That function only runs *after* a discovery payload is accepted by the discovery dispatcher. The actual gate on whether HA accepts a discovery topic at all is `discovery.py:TOPIC_MATCHER` — which ADR-070 did not check.

Empirical investigation against Home Assistant `dev` branch source (`homeassistant/components/mqtt/discovery.py`, fetched 2026-05-07):

```python
TOPIC_MATCHER = re.compile(
    r"(?P<component>\w+)/(?:(?P<node_id>[a-zA-Z0-9_-]+)/)"
    r"?(?P<object_id>[a-zA-Z0-9_-]+)/config"
)
```

The `object_id` character class is `[a-zA-Z0-9_-]+` — explicitly **excludes the forward slash**. When `discovery.py:async_discovery_message_received` (line 396) fails to match a topic, the handler at line 397-406 logs:

> "Received message on illegal discovery topic '%s'. The topic contains non allowed characters."

…and silently discards the message. The payload never reaches `subscription.async_prepare_subscribe_topics`.

Empirical regex test against the OTGW topic shapes produced under ADR-070:

| Topic (after stripping `homeassistant/`) | Regex result |
|---|---|
| `sensor/otgw-X/TSet/config` (canonical) | ✓ MATCH (`object_id=TSet`) |
| `sensor/otgw-X/TSet/thermostat/config` (current nested source variant) | ✗ **REJECTED** — discarded by HA, warning logged |
| `sensor/otgw-X/TSet/boiler/config` (current nested source variant) | ✗ **REJECTED** — discarded by HA, warning logged |
| `sensor/otgw-X/TSet_thermostat/config` (sibling) | ✓ MATCH (`object_id=TSet_thermostat`) |
| `sensor/otgw-X/TSet_boiler/config` (sibling) | ✓ MATCH (`object_id=TSet_boiler`) |

**Field consequence on beta.21 / beta.22:** every user with `bSeparateSources=true` is currently seeing the canonical entity register in HA but the thermostat-source and boiler-source variants silently dropped. Mosquitto and MQTT Explorer retain the rejected configs (no broker-side validation), creating the misleading appearance that the configs were delivered. The HA logs contain the warning but most beta testers do not watch those.

This invalidates the ADR-070 carve-out for discovery topics. The sibling-suffix rule must extend to the discovery topic builder as well.

## Decision

Use **sibling-suffix shape for discovery topics** in addition to state topics:

| Layer | Discovery topic |
|---|---|
| canonical | `homeassistant/sensor/<id>/<label>/config` (unchanged) |
| thermostat view | `homeassistant/sensor/<id>/<label>_thermostat/config` (was nested under ADR-070) |
| boiler view | `homeassistant/sensor/<id>/<label>_boiler/config` (was nested under ADR-070) |

State-topic shape from ADR-070 is **unchanged** (already sibling-suffix and accepted by HA without issue).

Concrete code site (1.5.x dev line):

| File | Site | Change |
|---|---|---|
| `mqtt_configuratie.cpp:2140-2146` | `buildSensorDiscoveryTopic` | format string `"%s/sensor/%s/%s/%s/config"` → `"%s/sensor/%s/%s_%s/config"` for the source-variant branch |

The `unique_id` field of the discovery payload is unaffected — it already uses underscore separation (`<nodeId>-<label>_<source>` per `composeSensorPayload`), so HA's identity-tracking continues to deduplicate correctly across the topic-shape migration. ADR-067 boot-time discovery republish delivers the new shape to HA on the next firmware boot.

**Migration of zombie retained configs.** Pre-ADR-071 retained configs at the rejected nested paths (`homeassistant/sensor/<id>/<label>/<src>/config`) linger in the broker after firmware upgrade. They never registered in HA (the regex rejected them), so they are functionally invisible — but mosquitto retains them and topic browsers display them. Two cleanup paths:

1. **Recommended (manual, one-time):** users run the cleanup recipe in `docs/api/MQTT.md` (`mosquitto_pub -t '<topic>' -r -n`) against the nested paths.
2. **Optional firmware-side cleanup (deferred):** firmware could publish empty retained payloads to the old nested paths on first boot after upgrade, mirroring the discovery republish loop. Deferred because: (a) HA never registered the topics so there is no functional impact; (b) the firmware would need to remember historical topic shapes to know which paths to clear, which is a maintenance liability; (c) the manual recipe is well-documented and one-time.

The 2.0.0 line carries the same change under a sibling ADR (ADR-098 Proposed, paired with this one).

## Alternatives Considered

1. **Edit ADR-070 in place to remove the carve-out.** Rejected: violates project ADR immutability rule (CLAUDE.md "NEVER edit an Accepted or Deprecated ADR"). Also destroys the audit trail of *why* the original carve-out was wrong (the misread of HA's subscription delta logic), which is the most valuable architectural lesson here. Supersession preserves the trail.
2. **Flip discovery to sibling-suffix AND revert state topic to nested.** Rejected: state-topic sibling shape is already deployed in beta.21+ (TASK-552); reverting would churn user dashboards twice without architectural benefit. HA does not care about state-topic structure (any string is accepted), so the state-topic sibling shape is purely a topic-browser ergonomics improvement that should remain.
3. **Use `<id>__<src>` (double underscore) to disambiguate from labels that already contain underscores.** Rejected: HA's `object_id` character class accepts `_` freely, no disambiguation is needed; would diverge from the underscore convention already used in `unique_id`. Risk of label-source collision is negligible because the firmware's label set is fixed at compile time.
4. **Switch `buildSensorDiscoveryTopic` to use the `node_id` slot for the entity label** (`homeassistant/sensor/<id>_<src>/<label>/config`). Rejected: the `node_id` slot is conventionally used for the *device* identifier (and HA tutorials, third-party tooling, and the OTGW `nodeId` already populate it that way). Repurposing it would break wildcard subscriptions like `homeassistant/sensor/<otgw-id>/+/config`.

## Consequences

**Positive:**

- Source-variant entities (`bSeparateSources=true`) actually register in HA. Restores the feature that ADR-068 → ADR-069 → ADR-070 evolution was building toward.
- Removes a silent-failure mode: the firmware appeared to publish discovery, the broker retained it, but HA discarded it. With siblings, success is visible end-to-end.
- Discovery-topic shape now matches state-topic shape (both sibling-suffix). Symmetric, easier mental model for users inspecting both trees.
- Eliminates the `subscription.py` carve-out citation that was the load-bearing error in ADR-070 — future maintainers reading the ADR chain see the corrected reasoning.

**Negative:**

- Old retained discovery configs at the nested paths linger in user brokers as zombies. They were already invisible to HA (rejected with warning), so the functional impact is nil, but tree browsers will display both old (nested) and new (sibling) configs side-by-side until manually cleared. Documented in `docs/api/MQTT.md`.
- Beta testers who already verified the canonical-only entities under ADR-070 must re-test with `bSeparateSources=true` to confirm the source variants now appear. One additional validation cycle per branch.
- Users who ran `mosquitto_pub -r -n` cleanup against the rejected nested topics (per ADR-070 migration note) before this ADR landed already cleared the zombies; the current change does not invalidate their cleanup work.

**Neutral:**

- The `composeSensorPayload` `unique_id` construction (`<nodeId>-<label>_<source>` at `mqtt_configuratie.cpp:1980-1981`) is already underscore-separated and matches the new discovery-topic suffix. No change required there. HA's identity tracking continues to deduplicate cleanly across the migration.

## Related Decisions

- **Supersedes ADR-070** (`MQTT Source-Topic Sibling-Suffix Shape`). After this ADR is Accepted, ADR-070's Status line will be updated to `Superseded by ADR-071, 2026-05-07.` per project supersession protocol. ADR-070's body remains immutable as the historical record of the original (partially-correct) decision.
- **Builds on ADR-067** (boot-time discovery republish — the trigger that delivers the corrected shape to HA without manual user action).
- **Preserves ADR-065** (`otgw-pic/` subtree as stable public API), **ADR-066** (slave-echo gate on `/boiler`), **ADR-068** (already superseded by ADR-070; this ADR does not reverse that supersession — additive layout under sibling shape remains the right call).
- **Refines ADR-069** (worldview routing semantics retained; only topic-string shape changes for discovery, mirroring the state-topic change ADR-070 made).
- **2.0.0 mirror:** sibling ADR (ADR-098 Proposed, paired backlog task to be created). Both worktrees adopt the discovery-topic sibling-suffix shape on the same calendar day, mirroring the ADR-070/ADR-097 deployment pattern.
- **Backlog tasks:** TASK-XXX (this ADR + dev impl), TASK-YYY (2.0.0 port). Numbers assigned at task creation.

## References

- Home Assistant source: `homeassistant/components/mqtt/discovery.py` lines 63-66 (`TOPIC_MATCHER`), 388-406 (`async_discovery_message_received` with reject-and-warn path). https://github.com/home-assistant/core/blob/dev/homeassistant/components/mqtt/discovery.py
- Empirical regex test executed 2026-05-07 against the live HA `dev`-branch source confirms the rejection of nested discovery paths (output captured in TASK-XXX implementation notes).
- ADR-070 (`docs/adr/ADR-070-mqtt-source-topic-sibling-suffix-shape.md`) — the superseded ADR, lines 54 and 121 contain the carve-out being reversed.
- Firmware code path being changed: `src/OTGW-firmware/mqtt_configuratie.cpp:2132-2148` (`buildSensorDiscoveryTopic`).
- Field evidence: beta.21 user MQTT Explorer screenshot 2026-05-07 confirms the nested discovery topic is being published by the firmware (and silently dropped by HA per the regex finding above).

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "PSTR\\(\"%s/sensor/%s/%s/%s/config\"\\)",
      "path_glob": "src/OTGW-firmware/mqtt_configuratie.cpp",
      "message": "Discovery topics for source variants must use sibling-suffix shape (`%s/sensor/%s/%s_%s/config`) per ADR-071. Nested-children format is rejected by HA's discovery TOPIC_MATCHER (object_id character class excludes '/')."
    }
  ],
  "llm_judge": false
}
```

The state-topic sibling-suffix Enforcement block from ADR-070 (`PSTR("%s/(thermostat|boiler)")` forbidden in `MQTTstuff.ino`) remains in force — ADR-071 does not amend the state-topic shape.
