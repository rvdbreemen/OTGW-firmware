# ADR-097: MQTT Source-Topic Sibling-Suffix Shape (feature-2.0.0 port of ADR-070)

**Status:** Superseded by ADR-098, 2026-05-07. Original status: Accepted, 2026-05-07 (four verification gates passed: Completeness, Evidence, Clarity, Consistency). Reason for supersession: this ADR's discovery-topic carve-out (mirroring dev ADR-070 line 54) was based on the assumption that HA accepts nested discovery topic identifiers and handles `state_topic` deltas in-place. Empirical investigation against `homeassistant/components/mqtt/discovery.py:TOPIC_MATCHER` showed nested discovery topics (object_id containing `/`) are rejected before reaching the subscription layer. ADR-098 corrects the discovery-topic shape on this branch; the state-topic decision recorded here is preserved and remains in force. See dev sibling: ADR-070 → ADR-071 supersession.
**Date:** 2026-05-07
**Decision Maker:** User: Robert van den Breemen (rvdbreemen)

Ports the decision from `dev`'s **ADR-070** to the `feature-dev-2.0.0-otgw32-esp32-sat-support` branch. Supersedes **ADR-095** (bSeparateSources mutual-exclusion rule, no longer applicable under sibling-suffix shape) and refines **ADR-096** (worldview routing semantics retained; only topic shape changes).

## Context

ADR-096 (Accepted, port of dev's ADR-069) shipped the worldview routing semantics on the 2.0.0 line: each per-source MQTT subtopic shows what that device sees on the OT bus, regardless of which frame type carried the value. ADR-095 (port of dev's ADR-068) made `bSeparateSources` mutually exclusive between base and source-variant entities.

Both ADRs used a **nested topic shape** for the per-source variants:

| Layer | Topic |
|---|---|
| canonical | `<base>/value/<id>/<TSet>` |
| thermostat view | `<base>/value/<id>/<TSet>/thermostat` |
| boiler view | `<base>/value/<id>/<TSet>/boiler` |

This is structurally unusual: the canonical topic carries a payload AND has children. While valid MQTT, it runs against the convention that "leaves carry data, parents don't", and topic-browser tools (mosquitto_sub tree mode, MQTT Explorer) render the parent-with-payload-plus-children pattern in confusing ways.

A field report from beta-tester Andre on 2026-05-07 (against the dev 1.5.x line which had the same shape) captured the consequence:

> "I don't think this nested topology is working. Are you sure HA checks the nested configs?"
> "I haven't noticed any difference between that option enabled and disabled."

The dev line published this finding in **ADR-070** on 2026-05-07. This ADR ports that decision to the 2.0.0 line so the two release lines remain in lockstep on MQTT topic-shape decisions.

Investigation against Home Assistant's source code (verified by the dev-side investigation; cited here for completeness):

- `homeassistant/components/mqtt/sensor.py:289-295` calls `mqtt.async_subscribe` on `state_topic` as a flat literal string. No introspection of topic structure, no recursion, no parent-prefix harvesting.
- `homeassistant/components/mqtt/util.py:254-307` (`valid_topic`, `valid_subscribe_topic`) enforces only "valid topic, no nulls/controls". No constraint on whether the topic is a leaf or has children.
- `homeassistant/components/mqtt/subscription.py:59-97` (`async_prepare_subscribe_topics`) handles `state_topic` deltas in-place: when a discovery payload arrives with the same `unique_id` and discovery topic but a changed `state_topic`, HA unsubscribes from the old topic and subscribes to the new one. This means migrating from nested to sibling-suffix shape is automatic for HA users on the next discovery republish (ADR-094 ensures that fires on boot after OTA on the 2.0.0 line).

So Home Assistant does not "fail to follow nested configs" — but the nested shape creates user doubt and breaks topic-browser UX, and the verification cost of the user's hypothesis ("HA doesn't follow this") is non-trivial. Switching to a sibling-suffix shape eliminates the doubt entirely and aligns the topology with normal MQTT conventions, with no observable cost to HA itself.

ADR-095 made `bSeparateSources` mutually exclusive between the base entity and the source-variant entities under the nested shape, because `OTGW/value/0/TSet` and its children `…/thermostat`, `…/boiler` felt semantically duplicated when stacked. With the sibling shape (`TSet`, `TSet_thermostat`, `TSet_boiler`), the three topics are clearly distinct entities with non-overlapping `state_topic`s, so the mutual-exclusion rule is no longer warranted. Keeping the canonical entity always advertised means dashboards referencing `sensor.<host>_TSet` keep working when users opt into source separation — purely additive layout.

## Decision

Use **sibling-suffix topic shape** (`<id>_<view>`) instead of nested children (`<id>/<view>`) for source-separated state topics:

| Layer | Topic |
|---|---|
| canonical | `<base>/value/<id>/<TSet>` (unchanged) |
| thermostat view | `<base>/value/<id>/<TSet>_thermostat` (new shape) |
| boiler view | `<base>/value/<id>/<TSet>_boiler` (new shape) |

All three are sibling leaves. Each is a normal MQTT topic with no structural surprise.

**ADR-095's mutual exclusion rule is dropped.** With siblings, the canonical entity coexists with the source variants without semantic duplication — they advertise non-overlapping `state_topic`s. The base entity is now emitted unconditionally, and `expandAndStreamSensorSources` emits two source variants (`thermostat`, `boiler`) instead of three (the previous canonical-row entry is removed, since the base entity already carries the canonical worldview).

**ADR-096's worldview routing logic is unchanged.** Same `switch` statement in `publishToSourceTopic` (`MQTTstuff.ino:1552`), same `bGatewaySubstituted` flag semantics. Only the topic-string shape changes.

**Discovery topic identifiers stay nested.** `homeassistant/sensor/<id>/<label>/<src>/config` is internal to HA — it's an identifier, not a state topic. Keeping it stable means the existing retained discovery configs are *updated in place* with new `stat_t` payloads. HA picks up the change automatically on the next discovery cycle because `unique_id` matches and `subscription.async_prepare_subscribe_topics` handles the topic delta cleanly.

Concrete code sites (2.0.0 feature line):

| File | Site | Change |
|---|---|---|
| `MQTTstuff.ino:1594` | publish path, thermostat | `PSTR("%s/thermostat")` → `PSTR("%s_thermostat")` |
| `MQTTstuff.ino:1598` | publish path, boiler | `PSTR("%s/boiler")` → `PSTR("%s_boiler")` |
| `MQTTHaDiscovery.cpp:2169` | `composeSensorPayload` `stat_t` separator | `'/'` → `'_'` |
| `MQTTstuff.ino:1953-1966, 2039-2049` | base-suppression branches | removed (ADR-095 supersession) |
| `MQTTstuff.ino:1906-1925` | `msgIdHasAnySourceEntry` helper | removed (dead code) |
| `MQTTHaDiscovery.cpp:2583-2598` | source-variants table | canonical row removed |

The dev (1.5.x) line carries the same change under ADR-070; this ADR is the explicit 2.0.0 mirror.

## Alternatives Considered

1. **Keep nested shape; add documentation.** Rejected: doesn't solve the convention problem; topic-browser UX still confusing; doesn't address the user's perception that HA isn't following nested configs. The fix is to make the topology unambiguously normal, not to argue from docs that the unusual shape is fine.
2. **Sibling shape but keep ADR-095 mutual exclusion** (suppress the canonical entity when `bSeparateSources=true`). Rejected by maintainer 2026-05-07: dashboards referencing `sensor.<host>_TSet` would break the moment a user toggles source separation on. Additive layout matches user mental model better and avoids hidden-state surprises in HA dashboards.
3. **Separate top-level prefix per view** (`<base>/thermostat/value/<id>`, `<base>/boiler/value/<id>`). Rejected: more invasive; breaks the "one canonical topic tree per metric" model that downstream consumers rely on; complicates wildcard subscriptions for power users (`<base>/value/+/+` no longer enumerates everything).
4. **Use a non-underscore separator** (`TSet.thermostat`, `TSet:thermostat`). Rejected: `_` is conventional for MQTT topic suffixes; matches HA's `object_id` sanitization (which already replaces non-alphanumerics with `_`); aligns with the suffix already used in `unique_id` field of discovery payloads (`<nodeId>-<label>_<source>`).

## Consequences

**Positive:**

- Clean leaf topology; mosquitto_sub, MQTT Explorer, and tree-style topic browsers render the topic tree without surprise.
- Canonical entity stays available — dashboards remain stable when users opt into source separation (`bSeparateSources=true` adds entities, never removes the canonical one).
- Three additive entities per dual-source MsgID under `bSeparateSources=true` matches user mental model: "the boiler view, the thermostat view, and the canonical view".
- Removes one whole class of user doubt: "is HA following nested configs?" stops being a question because there's nothing nested.
- Migration is automatic for HA auto-discovery users: HA's `subscription.async_prepare_subscribe_topics` handles the `state_topic` delta in-place; ADR-094's boot-time discovery republish triggers it.
- 2.0.0-specific note: ESP32 (OTGW32) targets have substantially more flash and RAM headroom than ESP8266, so the additional canonical-always-present entity per dual-source MsgID has negligible cost. The `msgIdHasAnySourceEntry` helper removal also recovers ~32 bytes of static RAM.

**Negative:**

- Old retained values at `<base>/value/<id>/<view>` linger in the broker as orphans (the firmware no longer publishes there). Users with mosquitto can clear with `mosquitto_pub -t '<topic>' -r -n`. HA itself ignores them after discovery refresh (it has unsubscribed). Documented in `docs/api/MQTT.md` under the migration note.
- With `bSeparateSources=true`, three entities per dual-source MsgID slightly increases the HA entity registry size (vs. ADR-095's two). Acceptable since opt-in.
- Users who manually configured HA sensors against the nested topics need to re-point them at the suffixed shape. The auto-discovery path handles itself; manual configs are documented as a one-time migration step.

## Related

- **Supersedes ADR-095** (`bSeparateSources` mutual-exclusion rule no longer applies under sibling shape). ADR-095's status is updated to `Superseded by ADR-097, 2026-05-07.`
- **Refines ADR-096** (worldview routing semantics retained; only topic shape changes). ADR-096 stays Accepted; this ADR amends it without superseding.
- Preserves ADR-093 (HA discovery retained-config orphan cleanup), ADR-094 (HA discovery state reconciliation on OTA upgrade — the trigger that delivers the new shape to HA without manual user intervention).
- **Cross-references dev-side ADR-070** (same decision, same calendar day, sibling release line). Both worktrees adopt the sibling-suffix shape on 2026-05-07.
- Backlog tasks: TASK-549 (parent worldview work, dev), TASK-550 (parent worldview port, 2.0.0), TASK-551 (dev ADR-070), TASK-552 (dev impl), TASK-553 (this ADR), TASK-554 (2.0.0 impl).

## References

- Beta channel: Andre, 2026-05-07 (verbatim quotes in Context).
- Home Assistant source: `homeassistant/components/mqtt/sensor.py:289-295`, `homeassistant/components/mqtt/util.py:254-307`, `homeassistant/components/mqtt/subscription.py:59-97`. https://github.com/home-assistant/core/tree/dev/homeassistant/components/mqtt
- Home Assistant docs: https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery, https://www.home-assistant.io/integrations/sensor.mqtt/
- Firmware code paths: `src/OTGW-firmware/MQTTstuff.ino:1525-1610` (publish path), `:1906-2050` (discovery dispatch), `src/OTGW-firmware/MQTTHaDiscovery.cpp:2155-2173` (`stat_t` builder) and `:2553-2630` (`expandAndStreamSensorSources`).
- Dev-side mirror: `docs/adr/ADR-070-mqtt-source-topic-sibling-suffix-shape.md` in the `dev` worktree.

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "PSTR\\(\"%s/(thermostat|boiler)\"\\)",
      "path_glob": "src/OTGW-firmware/MQTTstuff.ino",
      "message": "Source state topics use sibling-suffix shape (`%s_thermostat`, `%s_boiler`) per ADR-097. Nested children pattern was retired."
    }
  ],
  "llm_judge": false
}
```

The discovery topic builder (`MQTTHaDiscovery.cpp` `buildSensorDiscoveryTopic`) is intentionally exempt from this rule — its `homeassistant/.../<label>/<src>/config` slashes are HA-internal identifiers, not state topics, and must remain stable to support in-place discovery updates.
