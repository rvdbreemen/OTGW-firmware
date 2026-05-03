# ADR-095: bSeparateSources Makes Base and Source-Variant Entities Mutually Exclusive (feature-2.0.0 port of ADR-068)

**Status:** Accepted (amends ADR-040; structural-level per ADR-080)
**Date:** 2026-05-03
**Accepted:** 2026-05-03 (four verification gates passed: Completeness, Evidence, Clarity, Consistency)
**Decision Maker:** User: Rob van den Breemen (rvdbreemen)

Ports the decision from `dev`'s **ADR-068** to the `feature-dev-2.0.0-otgw32-esp32-sat-support` branch. Companion to **ADR-094** (the broker-side cleanup mechanism that makes this semantic flip safe across the upgrade boundary on this branch).

## Context

ADR-040 introduced `bSeparateSources` (originally `settingMQTTSeparateSources`, migrated into `settings.mqtt.bSeparateSources` per ADR-051; on this branch the field lives at `MQTTstuff.h:59` per ADR-079 / ADR-081) as an opt-in setting that adds per-source MQTT topics (`/thermostat`, `/boiler`, `/gateway`) for OpenTherm message IDs where source-attribution is meaningful. The original implementation was strictly **additive**: when the setting was enabled, the firmware kept publishing the legacy base entity for those MsgIDs and added three source-variant entities on top, expanded from a separate set of HA discovery configs flagged with `MQTT_HA_FLAG_ANY_SOURCE` (cfg flag `0x07`, defined at `src/OTGW-firmware/MQTTstuff.h:233`).

For a source-templated MsgID like 24 (Tr / room temperature), this meant Home Assistant ended up with **four** entities for the same conceptual quantity:

- `OTGW_Room_Temperature` (base, cfg flag `0x00`)
- `OTGW_Room_Temperature Thermostat` (source variant)
- `OTGW_Room_Temperature Boiler` (source variant)
- `OTGW_Room_Temperature Gateway` (source variant)

In normal operation (no PIC modification active), the base entity and the Thermostat-source variant carry the same value, and HA renders them with friendly names that differ only by a trailing source word. A user (`_reuzenpanda_`, Discord `#beta-testing`, 2026-04-30) reported seeing two `OTGW_Room_Temperature 21.9 °C` entries in HA's device list. The duplicate-name UX collision is structural: both entities are correctly populated by the firmware, both pass HA discovery validation, and both show the same value most of the time. Renaming alone cannot fix this; the user is staring at semantic redundancy.

The field report originated on `dev`'s 1.5.0-beta.5; the same code-path exists on this branch (the ANY_SOURCE flag is `0x07` in both branches, the publish loops have the same shape, the `expandAndStreamSensorSources` expansion helper at `MQTTHaDiscovery.cpp:2285` is unchanged). The fix must therefore land on both lines. ADR-068 captured the decision on `dev` after the implementation shipped (TASK-522, commit `4c95acd8`); this ADR carries the contract forward to the 2.0.0 line so the implementation can be ported with the design already settled.

The ADR-066 master-topic gating work (and its TASK-483 PS=1 amendment) already cleaned up *flapping* on the base topic for non-echo MsgIDs. That fix made the base entity a stable representation of the thermostat-side intent. It did not, however, address the question of whether the base entity should exist at all when the user has explicitly opted into source separation.

### Constraints

- **Backward compatibility for default users (`bSeparateSources = false`):** the default behaviour must remain exactly as ADR-040 specified pre-fix. No entity removal, no entity_id change, no MQTT topic change.
- **No HA discovery `unique_id` churn within a mode:** users who keep `bSeparateSources` at one setting across upgrades must not see any entity appear or disappear because of this change. The disappearance only happens when the user toggles the setting, or when ADR-094's wipe-on-OTA fires once across the upgrade boundary that introduces the new semantics.
- **Memory-safe:** the new lookup must be O(1) at call time and must not balloon static buffers. ESP8266 RAM budget per ADR-004 / ADR-044; ESP32 OTGW32 has more headroom but uses the same code path.
- **Re-entrancy:** the publish-loops in `doAutoConfigure()` (`MQTTstuff.ino:1754`) and `doAutoConfigureMsgid()` (`MQTTstuff.ino:1836`) already run under `MQTTAutoConfigSessionLock` (`MQTTstuff.ino:75-89`, ADR-090). Any new helper consulted from inside those loops must be safe under that guard.

## Decision

**`bSeparateSources` becomes a binary toggle: either base entities or source-variant entities are published for source-templated MsgIDs, never both.** Identical contract to `dev`'s ADR-068; identical implementation shape adapted to feature-2.0.0 line numbers.

Specifically, for any MsgID whose sensor table contains at least one entry with `cfg.flags & MQTT_HA_FLAG_ANY_SOURCE`:

1. **`bSeparateSources = false`** (default): publish only the base entity (`cfg.flags == 0x00`). No source-variants. Behaviour is identical to ADR-040 pre-fix and to v1.4.x.
2. **`bSeparateSources = true`**: suppress the base entity. Publish only the three source-variants (Thermostat / Boiler / Gateway) expanded by `expandAndStreamSensorSources()` (`MQTTstuff.h:437`, body in `MQTTHaDiscovery.cpp:2285`). The user gets exactly three entities per source-templated MsgID, no fourth duplicate.

MsgIDs that have **no** ANY_SOURCE-flagged pair in `mqttHaSensors[]` (e.g. MsgID 27, outside_temperature) are unaffected: their single base entity is published in both modes, identically.

### Implementation primitives

- A new helper `msgIdHasAnySourceEntry(uint8_t id)` will be added to `MQTTstuff.ino`, near the publish loops, maintaining a lazy-built 32-byte (8 × `uint32_t`) bitmap of MsgIDs that have at least one ANY_SOURCE-flagged entry in the sensor table. Built on first call, idempotent on subsequent calls. The bitmap is `static` inside the function; the build loop runs once per boot and walks `MQTT_HA_SENSOR_COUNT` (306 entries on this branch, declared at `MQTTHaDiscovery.cpp:583`).

- Both publish-loops gain a single `else if` branch:

```cpp
if (cfg.flags & MQTT_HA_FLAG_ANY_SOURCE) {
  if (settings.mqtt.bSeparateSources) {
    expandAndStreamSensorSources(MQTTclient, cfg, ctx);
  }
  // skip source-template entries when separate sources disabled
} else if (settings.mqtt.bSeparateSources && msgIdHasAnySourceEntry(cfg.id)) {
  // skip base entity; source-variants cover this MsgID under bSeparateSources
} else {
  streamSensorDiscovery(MQTTclient, cfg, ctx);
}
setMQTTConfigDone(cfg.id);
```

- The `setMQTTConfigDone(cfg.id)` call stays **outside** the if/else-if/else chain. This is deliberate: a deliberately-skipped base entity is still considered "configured" for the purposes of the JIT discovery state machine (ADR-041). Without this, the skip would be classified as "not yet emitted" and `doAutoConfigureMsgid()` would retry on the next OT message arrival, producing a drip-retry loop that never terminates.

The two call-sites on this branch:

- `doAutoConfigure()` at `MQTTstuff.ino:1773-1790` (publish loop body; ANY_SOURCE check currently at line 1780).
- `doAutoConfigureMsgid()` at `MQTTstuff.ino:1857-1870` (publish loop body; ANY_SOURCE check currently at line 1860).

`expandAndStreamSensorSources()` itself is unchanged.

## Alternatives Considered

### Alternative A: Disambiguate the base entity by suffixing its friendly name with `(combined)` or `(canonical)`

Keeps the four-entity model but tags the base friendly_name to communicate "this is the legacy combined value, the others are per-source". `entity_id` and `unique_id` stay unchanged, so existing HA automations bound to the base entity keep working without migration.

**Rejected** because it fixes the *symptom* (the visible name collision) and not the *cause* (the conceptual redundancy). Users still see four entities for the same physical quantity, three of which carry near-identical values. The `(combined)` qualifier needs a documentation paragraph to explain what makes it different from the Thermostat-source variant, and that paragraph is hard to write because for the most common case (no PIC modification active) the values truly *are* identical. The disambiguation reads as "we know this is confusing, here is a label that admits it" rather than "we removed the confusion".

### Alternative B (chosen): Suppress the base entity when `bSeparateSources = true`

Eliminates the conceptual overlap. `bSeparateSources` becomes a true binary toggle: either the base set or the source set, never both.

**Trade-off accepted:** users who toggle `bSeparateSources` between OFF and ON will see entity_ids change. Automations bound to the disappearing entity_ids break at the moment of toggle. This is mitigated, but not eliminated, by ADR-094's wipe-on-OTA logic which republishes a coherent discovery set across upgrade boundaries; the wipe cleans up HA's view automatically, but it does not migrate user-authored automations.

The trade-off is judged acceptable because:
- Toggling `bSeparateSources` is rare. Users typically pick a mode at install and stay.
- The pre-fix four-entity state was actively misleading to new users (the duplicate friendly_name in HA's device list is the first thing they see).
- 2.0.0 release notes will document the toggle as a one-way migration; users who need to flip the setting know to re-bind their automations.
- On this branch the upgrade path from any 1.4.x or 1.5.x firmware to 2.0.0 already crosses a major-version boundary; ADR-094's wipe is the correct vehicle for cleaning up the resulting orphans, and ADR-095 is the cause that makes that wipe materially load-bearing rather than just defensive.

The user's framing during the original `dev` review was: "duplicate-name disambiguation moet echt met prioriteit opgelost worden" — symptom-fighting via naming was not enough, structurally removing the overlap was the right call. Captured here so a future maintainer cannot reflexively flip back to Alternative A under the impression that "less invasive" automatically equals "better".

### Alternative C: Rename source-suffixes to clearer forms like `Thermostat-side`, `Boiler-side`, `Gateway-side`

Keeps the four-entity model and tries to make the source-variants visually distinct enough that the duplication is tolerable.

**Rejected** for the same reason as Alternative A: this is naming polish on top of a structural redundancy. Two entities still carry the same value most of the time. The longer suffix makes the friendly name harder to read in HA's narrow device-list column without solving the underlying "why are there two of these" question. It also introduces a churn cost (every existing source-variant entity_id changes) without a corresponding semantic gain.

### Alternative D: Do nothing

Leave the four-entity model and document the duplication as expected behaviour.

**Rejected** because the report came from a regular user, not a power user inspecting topic structure. The four-entity layout is the first thing a new HA user sees after enabling source separation, and it actively reduces trust in the integration. "Document as expected" is not a fix for a UX collision in the most prominent surface of the integration.

### Alternative E (rejected, branch-specific): Defer to dev's ADR-068 alone, do not port a separate ADR

Treat the contract as established on dev and let the implementation port land without a 2.0.0 ADR. Cheaper paperwork.

**Rejected** because the 2.0.0 line has its own ADR family (highest is ADR-093), independent numbering, and a different topic matrix (SAT switches, select, BLE roster). A future reader on the 2.0.0 branch trying to understand "why is `bSeparateSources` exclusive on this branch" should not have to context-switch to a different branch's ADR set to find the decision. The port preserves traceability.

## Trade-offs

- **Cost:** users who toggle `bSeparateSources` change-of-state (OFF → ON or ON → OFF) experience a one-time entity_id churn for source-templated MsgIDs. Automations bound to the disappearing entity_ids stop working until rebound. The wipe-on-OTA mechanism in ADR-094 cleans HA's stale view; it does not migrate automations.
- **Benefit:** zero conceptual overlap. `bSeparateSources` is now a clean binary toggle. New users see exactly the entities relevant to their chosen mode; the firmware no longer publishes "and also here is the legacy version of the same thing" silently.
- **Memory:** the bitmap is 32 bytes of static RAM (`uint32_t bitmap[8]` inside `msgIdHasAnySourceEntry`). The build loop runs once per boot. Per-call cost is two shifts, one mask, one bit test.
- **Mitigation for the toggle-churn cost:** documented in the 2.0.0 release notes as a one-way migration, and the upgrade-from-1.x case is automatically handled by ADR-094's wipe on the very first 2.0.0 boot.

## Consequences

### Positive

- **No more duplicate-name HA entities** for source-templated MsgIDs when `bSeparateSources = true`. The reported collision (`OTGW_Room_Temperature` vs `OTGW_Room_Temperature Thermostat`) is structurally removed on 2.0.0 from the moment the implementation lands.
- **`bSeparateSources` semantics become explainable in one sentence:** "publishes either the base set or the per-source set, never both". The pre-fix semantics required two paragraphs and a footnote.
- **Default users see no change.** With `bSeparateSources = false` the publish-path takes the `else` branch unchanged, the bitmap is built once and never consulted in the hot path.
- **Drip-retry loop avoided.** Because `setMQTTConfigDone(cfg.id)` stays outside the if-chain, deliberately-skipped base entities are correctly recorded as configured. The JIT discovery state machine (ADR-041) does not re-fire on subsequent OT message arrivals for the same MsgID.
- **Branch parity with dev.** The fix is the same shape on both lines, so a future contributor moving between branches encounters identical publish-loop logic. No bifurcation tax.

### Negative

- **One-time entity_id churn on toggle.** As described above. Mitigated by ADR-094 + release-notes migration guidance.
- **Coupling between publish-loops and a per-MsgID lookup.** The bitmap helper introduces an O(1) check at call site, but the conceptual coupling is non-trivial: a future contributor adding a new ANY_SOURCE-flagged sensor must understand that the bitmap auto-rebuilds on first call after boot, and that the rebuild is bounded to `MQTT_HA_SENSOR_COUNT` iterations. Documentation in the helper's leading comment block makes this explicit.
- **Asymmetric default behaviour across firmware-version boundary.** Pre-2026, `bSeparateSources` was "additive". Post-2026 it is "exclusive". A user reading the v1.4.x release notes alongside the v2.0.0 release notes will need to internalize this change. The setting name itself does not communicate the change; only the documentation does.

### Neutral

- **MsgIDs without an ANY_SOURCE pair** (e.g. outside_temperature MsgID 27) publish their base entity in both modes, exactly as before. The `msgIdHasAnySourceEntry()` check returns `false` and the `else` branch fires.
- **`expandAndStreamSensorSources()` itself is unchanged.** The behaviour change lives entirely in the caller's branch logic. The expansion helper is auditable in isolation.

## Synergy with ADR-094

ADR-094 (HA discovery wipe-on-OTA, port of ADR-067) is the broker-side cleanup mechanism that makes this semantic flip safe across the upgrade boundary on 2.0.0:

- **ADR-095 fixes the cause** (the firmware was publishing redundant entities; now it does not).
- **ADR-094 cleans up the consequences** (existing users on a pre-fix firmware had base+source entities published; after the upgrade those base entities become orphans on the broker; the OTA wipe removes them automatically on the very next firmware upgrade, without user intervention).

Without ADR-094, users upgrading from a pre-fix firmware with `bSeparateSources = true` would see the source variants continue to update while the orphaned base entities froze at their last retained value. With ADR-094 in place, the upgrade sweeps the broker and republishes the post-fix discovery set, leaving HA in a coherent state.

## Verification gates (per ADR-080 process for Proposed → Accepted)

1. **Completeness:** Status / Context / Decision / Alternatives Considered / Trade-offs / Consequences / Related sections populated. Cross-references to ADR-040 (amended), ADR-041 (JIT discovery state machine), ADR-051 (settings encapsulation), ADR-066 (master-topic gating, prior cleanup), ADR-068 (originating decision on `dev`), ADR-079 / ADR-081 (settings-types-in-MQTTstuff.h placement), ADR-080 (classification rule), ADR-090 (re-entrancy guard), ADR-094 (companion). Implementation tracking will be a port-forward task (referenced as TASK-528 in this branch's backlog when created); the port should land alongside TASK-524's wipe so users never observe an interim state where the redundancy is removed but the broker still carries pre-fix orphans.
2. **Evidence:** field report from `_reuzenpanda_` on Discord `#beta-testing` 2026-04-30 (duplicate `OTGW_Room_Temperature 21.9 °C` entries in HA device list). Originating `dev` implementation: commit `4c95acd8 fix(mqtt-ha): drop redundant base sensor when bSeparateSources publishes source-variants`. Code references on this branch: helper to be added near `MQTTstuff.ino:1773` and `MQTTstuff.ino:1857`; cfg flag at `MQTTstuff.h:233`; expansion helper unchanged at `MQTTHaDiscovery.cpp:2285`. Sketch-size impact on this branch is expected to be on the same order as `dev`'s 23-insert / 0-delete diff, though the absolute flash footprint is larger because 2.0.0 carries the SAT subsystem.
3. **Clarity:** the decision is implementable from the text alone. Function names, branch shape, and the placement-of-`setMQTTConfigDone` invariant are spelled out. The bitmap layout (8 × uint32 = 32 bytes, indexed by `(id >> 5) & 0x07` and bit-tested by `1U << (id & 0x1F)`) is given concretely. Default vs opt-in behaviour stated in one sentence each. The `else if` insertion sites on this branch are pinpointed by line number.
4. **Consistency:** does not contradict ADR-040; amends its "additive" property explicitly. Refines ADR-066's master-topic invariant by removing one half of the redundancy ADR-066 stabilised. Aligned with ADR-051 / ADR-079 / ADR-081 (`settings.mqtt.bSeparateSources` access pattern; settings struct lives in `MQTTstuff.h`). Consistent with ADR-041 (JIT discovery state-machine integrity preserved by keeping `setMQTTConfigDone` outside the if-chain). Companion to ADR-094 (broker-side cleanup makes the semantic flip safe). Per ADR-080: structural classification, no CI gate required; reviewers should confirm at PR-time that (a) the `else if` ordering keeps `setMQTTConfigDone(cfg.id)` outside the chain in both loops, (b) the bitmap-build is lazy and idempotent, (c) `expandAndStreamSensorSources()` itself is unchanged.

## Related

- **ADR-040:** MQTT Source-Specific Topics for OpenTherm Values. **This ADR amends ADR-040** by changing the "base topic / base entity always published" property to "base entity is suppressed for source-templated MsgIDs when `bSeparateSources = true`". ADR-040's status block on this branch carries an "Amended by: ADR-095" annotation alongside the existing Updated-line history.
- **ADR-041:** JIT HA Discovery. The `setMQTTConfigDone(cfg.id)` placement decision (kept outside the if-chain) preserves the JIT state-machine invariant that every iterated MsgID is recorded as configured exactly once.
- **ADR-051:** Dual Encapsulating Structs. `settings.mqtt.bSeparateSources` access pattern.
- **ADR-066:** MQTT Publish Gating by Source and Per-MsgID Slave-Echo Classification. Companion gating logic on the publish (live OT-bus and PS=1) side; this ADR addresses the orthogonal redundancy on the discovery side.
- **ADR-068 (on `dev`):** original decision this ADR ports forward. Implementation shape, alternatives analysis, and the "Alternative B chosen" reasoning are all carried over; the only material differences are file:line citations, the explicit naming of ADR-094 as the companion (rather than ADR-067), and the addition of Alternative E rejecting "do not port at all".
- **ADR-079 / ADR-081:** per-component types live in `<Component>stuff.h`. `bSeparateSources` is on this branch at `MQTTstuff.h:59` rather than `OTGW-firmware.h` as on dev; the `bWipeHaDiscoveryOnBoot` field that ADR-094 introduces lands in the same struct.
- **ADR-080:** Binding pattern-level ADRs require a CI gate. This ADR is structural, not pattern-level: no CI gate, reviewed manually at PR.
- **ADR-090:** Re-entrancy guard pattern for shared scratch buffers. The publish loops modified by this ADR run under the existing `MQTTAutoConfigSessionLock` (`MQTTstuff.ino:75-89`); the new `else if` branch and the bitmap helper are safe under that guard.
- **ADR-094:** HA Discovery State Reconciliation on OTA Upgrade (companion). Cleans up the broker-side consequences of the pre-fix four-entity publication on the very first 2.0.0 boot after upgrade from a 1.x firmware.
- **TASK-528 (port-forward):** to be created in this branch's backlog. Lands the bitmap helper and the `else if` branches at the line numbers cited above. Should land alongside TASK-524 (the wipe-on-OTA implementation) so users do not observe an interim state where the base entity is suppressed but the broker still carries the pre-fix orphan.
- **Implementation commit on `dev`:** `4c95acd8 fix(mqtt-ha): drop redundant base sensor when bSeparateSources publishes source-variants` — the reference shape for the 2.0.0 port.

## References

- Helper insertion site (to be added during port-forward): `src/OTGW-firmware/MQTTstuff.ino`, near the publish loops at lines `1754` (`doAutoConfigure`) and `1836` (`doAutoConfigureMsgid`).
- Publish loop branch insertion site (full discovery): `src/OTGW-firmware/MQTTstuff.ino:1773-1790` (`doAutoConfigure`); ANY_SOURCE check currently at line 1780.
- Publish loop branch insertion site (per-MsgID JIT): `src/OTGW-firmware/MQTTstuff.ino:1857-1870` (`doAutoConfigureMsgid`); ANY_SOURCE check currently at line 1860.
- Source-variant expansion helper (unchanged): `src/OTGW-firmware/MQTTstuff.h:437`, body at `src/OTGW-firmware/MQTTHaDiscovery.cpp:2285`.
- Cfg flag definition: `src/OTGW-firmware/MQTTstuff.h:233` (`MQTT_HA_FLAG_ANY_SOURCE = 0x07`).
- Re-entrancy guard: `src/OTGW-firmware/MQTTstuff.ino:75-89` (`MQTTAutoConfigSessionLock`, ADR-090).
- Sensor table size: `src/OTGW-firmware/MQTTHaDiscovery.cpp:583` (`MQTT_HA_SENSOR_COUNT = 306`).
- Settings field on this branch: `src/OTGW-firmware/MQTTstuff.h:59` (`bool bSeparateSources = false; // ADR-040: publish source-specific topics`).
- Field report (root cause on `dev`): Discord `#beta-testing`, user `_reuzenpanda_`, 2026-04-30. Dev fix shipped under TASK-522 / ADR-068; this ADR ports the contract forward to 2.0.0.
