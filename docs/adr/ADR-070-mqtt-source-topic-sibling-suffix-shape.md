# ADR-070: MQTT Source-Topic Sibling-Suffix Shape

## Status

Accepted, 2026-05-07 (four verification gates passed: Completeness, Evidence, Clarity, Consistency). Classification: structural amendment to ADR-069 and supersession of ADR-068. Decision Maker: User: Robert van den Breemen (rvdbreemen).

## Context

ADR-069 (Accepted 2026-05-07, commit cbc21af6) shipped the **worldview** routing semantics: each per-source MQTT subtopic shows what that device sees on the OT bus, regardless of which frame type carried the value. That fixed a real data-loss bug — thermostat-side values were being dropped during gateway override — and is uncontroversial at the routing-logic level.

ADR-069 used a **nested topic shape** for the per-source variants:

| Layer | Topic |
|---|---|
| canonical | `<base>/value/<id>/<TSet>` |
| thermostat view | `<base>/value/<id>/<TSet>/thermostat` |
| boiler view | `<base>/value/<id>/<TSet>/boiler` |

This is structurally unusual: the canonical topic carries a payload AND has children. While valid MQTT, it runs against the convention that "leaves carry data, parents don't", and topic-browser tools (mosquitto_sub tree mode, MQTT Explorer) render the parent-with-payload-plus-children pattern in confusing ways.

A field report from beta-tester Andre on 2026-05-07 captured the consequence:

> "I don't think this nested topology is working. Are you sure HA checks the nested configs?"
> "I haven't noticed any difference between that option enabled and disabled."

That last sentence is a real symptom (likely caused by discovery not refreshing on a runtime `bSeparateSources` toggle without reboot — separate concern, deferred). The first sentence is a *perception* problem rooted in the unusual topology.

Investigation against Home Assistant's source code:

- `homeassistant/components/mqtt/sensor.py:289-295` calls `mqtt.async_subscribe` on `state_topic` as a flat literal string. No introspection of topic structure, no recursion, no parent-prefix harvesting.
- `homeassistant/components/mqtt/util.py:254-307` (`valid_topic`, `valid_subscribe_topic`) enforces only "valid topic, no nulls/controls". No constraint on whether the topic is a leaf or has children.
- `homeassistant/components/mqtt/subscription.py:59-97` (`async_prepare_subscribe_topics`) handles `state_topic` deltas in-place: when a discovery payload arrives with the same `unique_id` and discovery topic but a changed `state_topic`, HA unsubscribes from the old topic and subscribes to the new one. This means migrating from nested to sibling-suffix shape is automatic for HA users on the next discovery republish (ADR-067 ensures that fires on boot after OTA).

So Home Assistant does not "fail to follow nested configs" — but the nested shape creates user doubt and breaks topic-browser UX, and the verification cost of the user's hypothesis ("HA doesn't follow this") is non-trivial. Switching to a sibling-suffix shape eliminates the doubt entirely and aligns the topology with normal MQTT conventions, with no observable cost to HA itself.

ADR-068 (Accepted 2026-05-03) made `bSeparateSources` mutually exclusive between the base entity and the source-variant entities under the nested shape, because `OTGW/value/0/TSet` and its children `…/thermostat`, `…/boiler` felt semantically duplicated when stacked. With the sibling shape (`TSet`, `TSet_thermostat`, `TSet_boiler`), the three topics are clearly distinct entities with non-overlapping `state_topic`s, so the mutual-exclusion rule is no longer warranted. Keeping the canonical entity always advertised means dashboards referencing `sensor.<host>_TSet` keep working when users opt into source separation — purely additive layout.

## Decision

Use **sibling-suffix topic shape** (`<id>_<view>`) instead of nested children (`<id>/<view>`) for source-separated state topics:

| Layer | Topic |
|---|---|
| canonical | `<base>/value/<id>/<TSet>` (unchanged) |
| thermostat view | `<base>/value/<id>/<TSet>_thermostat` (new shape) |
| boiler view | `<base>/value/<id>/<TSet>_boiler` (new shape) |

All three are sibling leaves. Each is a normal MQTT topic with no structural surprise.

**ADR-068's mutual exclusion rule is dropped.** With siblings, the canonical entity coexists with the source variants without semantic duplication — they advertise non-overlapping `state_topic`s. The base entity is now emitted unconditionally, and `expandAndStreamSensorSources` emits two source variants (`thermostat`, `boiler`) instead of three (the previous canonical-row entry is removed, since the base entity already carries the canonical worldview).

**ADR-069's worldview routing logic is unchanged.** Same `switch` statement in `publishToSourceTopic` (`MQTTstuff.ino:1200`), same `bGatewaySubstituted` flag semantics. Only the topic-string shape changes.

**Discovery topic identifiers stay nested.** `homeassistant/sensor/<id>/<label>/<src>/config` is internal to HA — it's an identifier, not a state topic. Keeping it stable means the existing retained discovery configs are *updated in place* with new `stat_t` payloads. HA picks up the change automatically on the next discovery cycle because `unique_id` matches and `subscription.async_prepare_subscribe_topics` handles the topic delta cleanly.

Concrete code sites (1.5.x dev line):

| File | Site | Change |
|---|---|---|
| `MQTTstuff.ino:1242` | publish path, thermostat | `PSTR("%s/thermostat")` → `PSTR("%s_thermostat")` |
| `MQTTstuff.ino:1246` | publish path, boiler | `PSTR("%s/boiler")` → `PSTR("%s_boiler")` |
| `mqtt_configuratie.cpp:2011` | `composeSensorPayload` `stat_t` separator | `'/'` → `'_'` |
| `mqtt_configuratie.cpp:1488-1489, 1554-1558` | base-suppression branches | removed (ADR-068 supersession) |
| `mqtt_configuratie.cpp:2406-2408` | source-variants table | canonical row removed |

The 2.0.0 line carries the same change under ADR-097 (port).

## Alternatives Considered

1. **Keep nested shape; add documentation.** Rejected: doesn't solve the convention problem; topic-browser UX still confusing; doesn't address the user's perception that HA isn't following nested configs. The fix is to make the topology unambiguously normal, not to argue from docs that the unusual shape is fine.
2. **Sibling shape but keep ADR-068 mutual exclusion** (suppress the canonical entity when `bSeparateSources=true`). Rejected by maintainer 2026-05-07: dashboards referencing `sensor.<host>_TSet` would break the moment a user toggles source separation on. Additive layout matches user mental model better and avoids hidden-state surprises in HA dashboards.
3. **Separate top-level prefix per view** (`<base>/thermostat/value/<id>`, `<base>/boiler/value/<id>`). Rejected: more invasive; breaks the "one canonical topic tree per metric" model that downstream consumers (Home Assistant tutorials, third-party gateways, Zabbix dashboards) rely on; complicates wildcard subscriptions for power users (`<base>/value/+/+` no longer enumerates everything).
4. **Use a non-underscore separator** (`TSet.thermostat`, `TSet:thermostat`). Rejected: `_` is conventional for MQTT topic suffixes; matches HA's `object_id` sanitization (which already replaces non-alphanumerics with `_`); aligns with the suffix already used in `unique_id` field of discovery payloads (`<nodeId>-<label>_<source>` per `composeSensorPayload` at `mqtt_configuratie.cpp:1980-1981`).

## Consequences

**Positive:**

- Clean leaf topology; mosquitto_sub, MQTT Explorer, and tree-style topic browsers render the topic tree without surprise.
- Canonical entity stays available — dashboards remain stable when users opt into source separation (`bSeparateSources=true` adds entities, never removes the canonical one).
- Three additive entities per dual-source MsgID under `bSeparateSources=true` matches user mental model: "the boiler view, the thermostat view, and the canonical view".
- Removes one whole class of user doubt: "is HA following nested configs?" stops being a question because there's nothing nested.
- Migration is automatic for HA auto-discovery users: HA's `subscription.async_prepare_subscribe_topics` handles the `state_topic` delta in-place, so no manual sensor reconfiguration is needed.

**Negative:**

- Old retained values at `<base>/value/<id>/<view>` linger in the broker as orphans (the firmware no longer publishes there). Users with mosquitto can clear with `mosquitto_pub -t '<topic>' -r -n`. HA itself ignores them after discovery refresh (it has unsubscribed). Documented in `docs/api/MQTT.md` under the migration note.
- With `bSeparateSources=true`, three entities per dual-source MsgID slightly increases the HA entity registry size (vs. ADR-068's two). Acceptable since opt-in.
- Users who manually configured HA sensors against the nested topics (e.g. wrote a YAML config for `sensor.boiler_setpoint` pointing at `<base>/value/0/TSet/boiler`) need to re-point them at the suffixed shape. The auto-discovery path handles itself; manual configs are documented as a one-time migration step.

## Related

- **Supersedes ADR-068** (`bSeparateSources` mutual-exclusion rule no longer applies under sibling shape). ADR-068's status line will be updated to `Superseded by ADR-070, 2026-05-07.` after this ADR is Accepted.
- **Refines ADR-069** (worldview routing semantics retained; only topic shape changes). ADR-069 stays Accepted; this ADR amends it without superseding.
- Preserves ADR-065 (otgw-pic/ subtree as public API), ADR-066 (slave-echo gate on `/boiler`), ADR-067 (boot-time discovery republish — the trigger that delivers the new shape to HA without manual user intervention).
- 2.0.0 mirror: ADR-097 (Proposed, sibling task TASK-553). Both worktrees adopt the sibling-suffix shape on the same calendar day.
- Backlog tasks: TASK-549 (parent worldview work, dev), TASK-550 (parent worldview work, 2.0.0), TASK-551 (this ADR), TASK-552 (dev impl), TASK-553 (2.0.0 ADR), TASK-554 (2.0.0 impl).

## References

- Beta channel: Andre, 2026-05-07 (verbatim quotes in Context).
- Home Assistant source: `homeassistant/components/mqtt/sensor.py:289-295`, `homeassistant/components/mqtt/util.py:254-307`, `homeassistant/components/mqtt/subscription.py:59-97`. https://github.com/home-assistant/core/tree/dev/homeassistant/components/mqtt
- Home Assistant docs: https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery, https://www.home-assistant.io/integrations/sensor.mqtt/
- Firmware code paths: `src/OTGW-firmware/MQTTstuff.ino:1178-1250`, `src/OTGW-firmware/mqtt_configuratie.cpp:1488-1560` and `:1999-2013` and `:2367-2436`.

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "PSTR\\(\"%s/(thermostat|boiler)\"\\)",
      "path_glob": "src/OTGW-firmware/MQTTstuff.ino",
      "message": "Source state topics use sibling-suffix shape (`%s_thermostat`, `%s_boiler`) per ADR-070. Nested children pattern was retired."
    }
  ],
  "llm_judge": false
}
```

The discovery topic builder (`mqtt_configuratie.cpp` `buildSensorDiscoveryTopic`, lines 2130-2145) is intentionally exempt from this rule — its `homeassistant/.../<label>/<src>/config` slashes are HA-internal identifiers, not state topics, and must remain stable to support in-place discovery updates.
