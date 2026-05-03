# ADR-094: Home Assistant discovery state reconciliation on OTA upgrade (feature-2.0.0 port of ADR-067)

## Status

Accepted (guideline-level per ADR-080; reviewed at PR, no CI gate).
Date: 2026-05-03.
Accepted: 2026-05-03 (four verification gates passed: Completeness, Evidence, Clarity, Consistency).
Ports the decision from `dev`'s **ADR-067** to the `feature-dev-2.0.0-otgw32-esp32-sat-support` branch.
Complementary to **ADR-093** (per-entity, user-driven removal); this ADR is the per-device, firmware-driven removal half of the same lifecycle.

## Context

Home Assistant (HA) caches every MQTT auto-discovery config it has ever seen in `core.entity_registry`, keyed primarily by `unique_id`, and keeps those records indefinitely. A retained config on the broker drives entity creation; the *removal* of a retained config does not drive entity removal. HA happily holds the cached entity even after the publisher stops announcing it. The firmware therefore cannot fix a discovery mistake by simply not publishing the bad shape on the next boot.

Three classes of template change make this visible to users:

1. **`unique_id` change** for an existing concept (e.g. label sanitization differences, or a renamed source-suffix). HA registers the new id as a fresh entity and keeps the old one as a shadow with the same friendly name. Two entities, same value.
2. **Friendly-name or `object_id` change** that does not change `unique_id`. The old entity stays under its old name; the new shape may or may not collapse into it depending on HA's matching heuristics.
3. **Entity-set change**, where a MsgID that previously published a single base entity now publishes three source-separated variants under ADR-040 (`bSeparateSources`), or vice versa. The retired shape persists until it is wiped. This case is exactly what ADR-095 (port of ADR-068) introduces on this branch when `bSeparateSources = true` flips from "additive" to "exclusive" semantics.

The original trigger was a field report from `_reuzenpanda_` on `dev`'s 1.5.0-beta.5: two `OTGW_Room_Temperature` entities side by side in HA, one stale, one live, indistinguishable at a glance. The same root cause affects this branch the moment 2.0.0 ships ADR-095's `bSeparateSources` semantic shift, plus the additional 2.0.0-only entity types (SAT switches, select, BLE roster) carry their own future shape-evolution risk.

ADR-062 already provides a daily *self-heal* loop for the steady state (subscribe to the discovery wildcard, count what is retained, re-publish anything missing). That loop assumes the published shape has not changed; it cannot detect "this retained config is stale because we now publish a different topic for the same concept", because it has no record of the previous shape. ADR-062 closes broker-side loss; it does not close firmware-version transitions.

ADR-093 already provides a *per-entity* removal path (zero-byte retained payload via the streaming primitives) for user-driven lifecycle events (BLE roster slot forget). That path is symmetric with the per-entity publish; it does not cover the cross-version-transition case where the publisher cannot enumerate which previously-published topics no longer exist in the new firmware.

The constraint that bounds the design space:

- Existing devices in the field have a `settings.ini` written by an older firmware. There is no migration-version marker and no schema version field. Any solution must work on devices whose persisted settings predate this ADR.
- The OTA upload handler is the single chokepoint that knows a firmware-or-filesystem update has just succeeded and a reboot is imminent. On 2.0.0 this chokepoint is dual-platform: ESP8266 path in `OTGW-ModUpdateServer-impl.h` and ESP32 path in `OTGW-ModUpdateServer-esp32.h`. Both paths reach `Update.end(true)` (impl.h:301, esp32.h:279) before the imminent reboot; the wipe-flag must be set on both.
- The wipe step writes *many* MQTT messages in a row. On 2.0.0 the matrix is 306 sensors + 53 binary sensors + 2 climate + 1 number + 14 SAT switches + 1 select = **377 base topics**, plus 3 source-variant topics for each ANY_SOURCE-flagged sensor (defensive: emitted regardless of current `bSeparateSources` setting). It must coexist with the cooperative ESP8266 watchdog and must not run while another autoconfig path holds the shared scratch state covered by ADR-090's `MQTTAutoConfigSessionLock` re-entry guard at `MQTTstuff.ino:75-89`.
- The wipe loop must reuse the **streaming MQTT primitives** specified by ADR-077 and ADR-093 (`beginMqttPublish(topic, /*len=*/0, /*retain=*/true)` followed immediately by `MQTTclient.endPublish()` with no chunk writes in between), so retain semantics, payload framing, and TCP behavior are byte-identical to the publish side and to ADR-093's per-entity unpublish. No buffered alternative; no `mqttClient.publish(topic, "", true)` shortcut. The streaming path is the only path. This is the only material contract change versus dev's ADR-067.

## Decision

Add a one-shot wipe-and-republish pass over Home Assistant discovery topics, scoped to `nodeId` (= `settings.mqtt.sUniqueid`), triggered on the first boot after a firmware or filesystem OTA, idempotent across crashes within that boot, and emitted via the streaming primitives that ADR-077 and ADR-093 already use on this branch.

Four moving parts:

1. **Persistent flag with default-true struct-init.**
   `settings.mqtt.bWipeHaDiscoveryOnBoot` is a new `bool` field added to `MQTTSettingsSection` at `src/OTGW-firmware/MQTTstuff.h:41` (per ADR-079 / ADR-081, MQTT settings types live in `MQTTstuff.h`, not `OTGW-firmware.h` — this is the only structural delta versus dev's ADR-067 placement). C++ struct-default `= true`. `readSettings` updates only fields whose JSON key is present, so existing `settings.ini` files written by older firmware silently inherit the struct-default. On first boot of the new firmware the flag is `true`; the wipe runs once; the flag is then cleared and persisted. From that point on the field is serialized with whatever value it ended up holding.

2. **OTA-finish hook on both platforms.**
   `requestHaDiscoveryWipeOnNextBoot()` (declared in `MQTTstuff.h`, body in `MQTTstuff.ino`) sets the flag and calls `writeSettings(false)` if `LittleFSmounted`. It is called from **both** OTA paths after their respective `Update.end(true)` returns true:
   - ESP8266 path: `OTGW-ModUpdateServer-impl.h` (success branch after line 304's `[OTA] post-end` log probe and the LittleFS-OTA branch's `writeSettings(false)` call at line 311).
   - ESP32 path: `OTGW-ModUpdateServer-esp32.h` (success branch after line 282's `[OTA] post-end` log probe and the LittleFS-OTA branch's `writeSettings(false)` call at line 289).
   Both paths must call the helper. The helper is idempotent: setting an already-set flag and rewriting `settings.ini` is wasted I/O but harmless. A future contributor adding a third OTA path (USB-DFU, OTA-via-MQTT, etc.) inherits the same one-line hook.

3. **Boot-time handler in `setup()`.**
   `runPendingDiscoveryWipeIfRequested()` (in `MQTTstuff.ino`) runs at most once per boot, gated by a function-static `sDiscoveryWipeRanThisBoot` flag. It is called from `OTGW-firmware.ino:186` immediately after `startMQTT();`. It returns early if MQTT is disabled or if `bWipeHaDiscoveryOnBoot == false`. Because `startMQTT()` is asynchronous, the handler then polls `MQTTclient.connected()` for up to ~5 seconds (`deadline = millis() + 5000`, `handleMQTT()` + `feedWatchDog()` + `delay(50)` per iteration). If MQTT does not connect within that window the handler returns without clearing the flag, so the wipe will be retried on the next boot.

4. **Wipe loop with shared sanitizer and streaming primitives.**
   `wipeHaDiscoveryTopicsForNode()` (in `MQTTstuff.ino`) acquires the `MQTTAutoConfigSessionLock` RAII guard from ADR-090 (`MQTTstuff.ino:75-89`), refuses to run if free heap is below `MQTT_DISCOVERY_HEAP_MIN`, and walks `mqttHaSensors[]` (306 entries; `MQTT_HA_SENSOR_COUNT` at `MQTTHaDiscovery.cpp:583`, declaration at `MQTTstuff.h:284`), `mqttHaBinSensors[]` (53; `MQTTHaDiscovery.cpp:1020`), the hardcoded climate (2) and number (1) topics, the SAT switch object-ids (14; including conditional `dhw_enable` defensively), and the SAT select (1; `sat_heating_system`). For every sensor with `MQTT_HA_FLAG_ANY_SOURCE` (constant at `MQTTstuff.h:233`) the loop emits all three source-variant topics (`thermostat`, `boiler`, `gateway`) regardless of the current `bSeparateSources` setting, because a previous firmware may have published them under a different mode.

   Each topic gets cleared via the streaming primitives:

   ```cpp
   if (!beginMqttPublish(topic, /*len=*/0, /*retain=*/true)) continue;
   MQTTclient.endPublish();
   feedWatchDog();
   ```

   Identical shape to ADR-093's `satBLEUnpublishDiscovery` per-entity removal. The broker treats a zero-byte retained payload as deletion of the retained config. `feedWatchDog()` is called on every iteration. Topic construction reuses `sanitizeHaObjectId()` (`MQTTHaDiscovery.cpp:1847`), which **must be de-static-ed during TASK-524 implementation** — this is currently a `static void` and must become file-scope-visible (declaration in `MQTTstuff.h` next to the other shared helpers) so wipe-side and publish-side topic construction stay byte-for-byte identical. The de-static is the same change ADR-067 specified for dev; on this branch it lands as part of TASK-524.

After the wipe loop returns, the handler clears the flag and persists it *before* triggering republish:

```
settings.mqtt.bWipeHaDiscoveryOnBoot = false;
if (LittleFSmounted) writeSettings(false);
markAllMQTTConfigPending();
sDiscoveryWipeRanThisBoot = true;
```

The ordering is deliberate. See "Crash recovery" below.

### Sub-pattern: default-true struct-init as forced one-shot migration

This is the second adoption (after dev's TASK-523) of default-true struct-init as a deliberate migration mechanism, and the first on this branch. The pattern:

- Add a new `bool` field to a settings struct with `= true` as the C++ struct-default.
- Anchor a one-shot action behind that flag at boot.
- Clear and persist the flag inside the action.

The pattern works because of two properties of the existing settings layer:

- The struct is constructed with C++ defaults *before* `readSettings` runs.
- `readSettings` only overwrites fields whose JSON key is present in `settings.ini`. A field that did not exist in the previous firmware is absent from the file; the struct keeps its default.

The result is a forced one-shot action on first boot of the new firmware, requiring zero user intervention and zero migration-version bookkeeping. The cost is one persistent `bool` per migration that this codebase chooses to do this way. ADR-067 first documented the shape on dev; this ADR adopts it on 2.0.0 unchanged. Future features that need a one-shot post-upgrade action (purge a stale cache, re-derive a key, rebuild an index file, force a SAT recalibration on first boot of a new PID) can reuse the same shape: add a default-true bool, anchor the action behind it, clear and persist inside the action. If the codebase ever accumulates several such flags, that is the right time to consolidate them into a `state.migration.*` block; until then the cost stays where the benefit is.

### Why not the alternatives

**A1, OTA-hook only, no default-true.** The flag would be `false` by default, set to `true` only at the OTA-finish hook, then cleared on the next boot. Simpler, but it leaves out every device that already had a discovery-shape mismatch baked in by an earlier firmware: those devices have already booted past their "first boot of new firmware" moment and the OTA hook never re-fires. Default-true catches the broader case at no extra ceremony cost. Same trade-off as ADR-067; same conclusion.

**A2, OTA-hook plus git-hash compare.** Persist the firmware git hash in settings, compare on boot, run the wipe only when the hash changed. More targeted, fewer republishes over the life of the device. Rejected for this MVP because A1's default-true field already covers the user-visible problem on first boot of the new firmware, and A2's hash-compare introduces a second persistent field, a comparator that has to keep working across version transitions, and a subtle failure mode where a git-hash bookkeeping bug silently disables the wipe for everyone. If a future feature needs version-aware migration logic, this ADR is not in the way.

**A3, per-topic identity tracking via persisted topic list.** Persist the list of discovery topics each firmware published, diff at boot, wipe only what is no longer in the new list. Most efficient on the wire. Rejected because it requires persisting and diffing a list of 377+ strings on every boot, which costs DRAM, flash, and review effort for a path that runs once per OTA. The 2.0.0 entity matrix is bigger than dev's 1.5.x matrix, which makes A3 strictly more expensive on this branch without solving anything A1 does not already solve.

**A4 (rejected, branch-specific): merge into ADR-093.** ADR-093 is the per-entity, user-driven removal pattern (BLE roster forget). This ADR is the per-device, firmware-driven removal pattern (OTA upgrade). Both publish zero-byte retained payloads via the streaming primitives, but the trigger (single user click vs. firmware lifecycle event), the scope (one entity's 4 configs vs. all 377+ topics for the device's nodeId), and the persistence model (no flag vs. struct-default migration flag) differ enough that merging would dilute both. They reference each other as companion lifecycle events and remain independently revisitable.

## Consequences

### Benefits

- **Closes the field-reported regression on this branch** before it can ship. The `bSeparateSources` semantic flip ADR-095 introduces on 2.0.0 would otherwise reproduce dev's two-`OTGW_Room_Temperature` regression on every upgrade from a pre-ADR-095 build; this ADR ensures the broker is reconciled across that exact transition.
- **No user action required on existing devices.** Default-true struct-init runs the wipe on first boot of the new firmware regardless of what is in `settings.ini`. Users who have never opened the settings page still get the migration.
- **Compatible with all OTA paths on 2.0.0.** ESP8266 firmware OTA (LittleFS still mounted from before upload), ESP8266 filesystem OTA (LittleFS just remounted), ESP32 firmware OTA, and ESP32 filesystem OTA all reach `requestHaDiscoveryWipeOnNextBoot()` after their respective `Update.end(true)` succeeds.
- **Reuses ADR-077 / ADR-093 streaming primitives end-to-end.** The wipe path uses `beginMqttPublish + endPublish` exactly as the per-entity unpublish does, so the heap-floor gate, retain semantics, and TCP framing are guaranteed identical. There is no second code path to maintain or audit.
- **Symmetric with ADR-093.** A reader of either ADR can predict the other: ADR-093 wipes one entity's configs on user action; ADR-094 wipes the device's configs on firmware action. Same primitives, same gates, same `feedWatchDog()` cadence.
- **Cleanup of pre-2.0.0 OTGW32 source-variant orphans.** Users upgrading 1.4.x or 1.5.x firmware to 2.0.0 carry with them whatever discovery shape the older firmware published; the wipe sweeps the broker on first boot of 2.0.0 and the new shape is the only remaining truth.

### Costs

- **One full wipe + full republish per OTA per device.** On a 2.0.0 device, the wipe path is ~377 + (3 × ANY_SOURCE-flagged) MQTT publishes; the drip-republish that follows is ~377 more. At the drip's 2 s spacing the user sees full HA-side reconciliation in roughly 13 minutes (slightly longer than dev's ~12 minutes due to the extra 2.0.0 entities). Acceptable for a once-per-upgrade event; would not be acceptable as a continuous loop.
- **One persistent `bool` per migration.** Cheap, but the codebase pays this for every future feature that opts into the same shape. Cap is implicit (settings-struct size).
- **Wipe runs even when the new firmware did not change discovery shape.** A no-op upgrade (e.g. a bugfix in the OT parser) still triggers the wipe. Cost is ~13 minutes of redundant traffic on the broker; benefit is that we never have to remember to gate the wipe on whether the shape changed.
- **Default-true means a fresh install also runs the wipe on its very first boot, against an empty broker.** The wipe loop is a no-op against topics that do not exist (broker treats empty-retained on a non-existent topic as a no-op), so this is wasted MQTT round-trips, not a correctness issue.
- **De-static-ing `sanitizeHaObjectId` is a TU-spanning change.** It moves the symbol from internal-linkage to external-linkage and adds a declaration in `MQTTstuff.h`. Risk is low — there are no name collisions in the project — but the change has to land atomically with the wipe loop to avoid a half-implementation where the wipe loop calls a still-static helper and fails to link.

### Crash recovery

The handler is designed so that any single crash leaves a state from which the next boot recovers without user intervention.

| Failure point | State at next boot | Outcome |
|---|---|---|
| Crash during wipe loop | `bWipeHaDiscoveryOnBoot = true`; broker has partial wipe applied | Wipe re-runs; empty-retained on already-deleted topic is a broker no-op; loop completes; flag clears. |
| Crash between wipe end and `writeSettings(false)` | `bWipeHaDiscoveryOnBoot = true`; broker fully wiped | Wipe re-runs (full no-op); flag clears. |
| Crash between flag-clear-persist and `markAllMQTTConfigPending()` | `bWipeHaDiscoveryOnBoot = false`; broker fully wiped; in-RAM pending bitmap empty | `startMQTT()` on next boot rebuilds the pending bitmap from scratch (first-publish-after-boot is one of ADR-062's three baseline recovery paths); drip-republish resumes naturally. No second wipe. |
| Crash during drip-republish | Pending bitmap is RAM-only; reset on boot; `startMQTT()` re-triggers drip | Drip resumes from a clean bitmap. |

The "persist flag-clear *before* `markAllMQTTConfigPending`" ordering is the only non-obvious step, identical to ADR-067's reasoning: if the order were reversed, a crash between the bitmap mark and the persist would leave the flag still true, and the next boot would wipe again on top of an already-republishing broker, doubling the traffic for no benefit. Persisting first means the worst case after a mid-republish crash is a clean re-drip, never a re-wipe.

### Limits

- **Cannot detect orphan discovery configs from a previous nodeId.** If the device's `sUniqueid` changed between firmware versions (it usually does not), the wipe cleans only the *current* nodeId's topics. Foreign-nodeId orphans are ADR-062's territory (`iLastOrphanCount` metric) and are intentionally not auto-deleted there.
- **Cannot recover from a broker that disconnects mid-wipe.** If MQTT drops between the wipe loop's first and last publish, the broker has a partial-wipe state. This is identical to "crash during wipe loop" in the table above; the next boot re-runs the wipe and finishes the job.
- **Wipe set is hardcoded against the publish set.** The wipe loop must walk the same arrays and emit the same topic shapes as `MQTTHaDiscovery.cpp`'s publish helpers. The two paths share `sanitizeHaObjectId` to keep object-id sanitization aligned, but the wipe loop's topic templates are duplicated by hand. If a new entity type is added to publish without a matching wipe entry, the wipe will not clean it. There is currently no CI gate for this; it is the structural reason this ADR is guideline-level rather than binding (see ADR-080 classification below). On 2.0.0 the entity-type set is broader (sensors, binary sensors, climate, number, switches, select, BLE roster), which raises the maintenance attention this rule needs.
- **BLE roster topics are out-of-scope by design.** Per ADR-093, BLE roster discovery is published per-MAC at runtime via `<HaPrefix>/sensor/<uniqueId>_ble_<macCompact>_<kind>/config` and removed per-MAC via the per-entity unpublish helper at `MQTTstuff.ino:2257`. The OTA wipe does **not** enumerate BLE topics: the runtime roster is RAM-only and is empty at boot before any BLE adverts have been observed, so an enumeration would be empty anyway. After the OTA, the first BLE advert seen on-air re-populates the roster and republishes its discovery configs through the existing flow. Pre-OTA orphans on BLE topics from a forgotten-and-not-cleared slot would persist across the OTA; that case is rare (ADR-093 cleans on forget) and not regression-relevant. If a future shape change to BLE discovery topics introduces broker-side stale configs across firmware versions, a follow-up ADR can extend the wipe to enumerate the configured BLE roster slots from `settings.sat.ble[]` (a persistent surface) rather than from the runtime roster.
- **No telemetry surface for wipe outcome.** The wipe logs `[wipe] cleared %u HA discovery topics for node=%s` to telnet but does not expose a count via REST or MQTT. If users report stale entities after an upgrade, support has to read the telnet log to confirm the wipe ran. Acceptable for first ship; a follow-up could thread the count through `state.discovery.iLastWipeCount`.

### Per-device topic matrix wiped (2.0.0 scope)

| Topic shape | Count | Source |
|---|---|---|
| `<haPrefix>/sensor/<nodeId>/<sanitized_label>/config` | 306 | `mqttHaSensors[]` (`MQTTHaDiscovery.cpp:585`) |
| `<haPrefix>/sensor/<nodeId>/<sanitized_label>/{thermostat,boiler,gateway}/config` | 3 per ANY_SOURCE-flagged sensor (defensive: emitted regardless of current `bSeparateSources`) | `expandAndStreamSensorSources` (`MQTTHaDiscovery.cpp:2285`) |
| `<haPrefix>/binary_sensor/<nodeId>/<sanitized_label>/config` | 53 | `mqttHaBinSensors[]` (`MQTTHaDiscovery.cpp:1020`) |
| `<haPrefix>/climate/<nodeId>/{climate,dhw_control}/config` | 2 | hardcoded in `MQTTstuff.ino` |
| `<haPrefix>/number/<nodeId>/Toutside_override/config` | 1 | hardcoded in `MQTTstuff.ino` |
| `<haPrefix>/switch/<nodeId>/<obj_id>/config` (SAT switches; defensively all 14 including conditional `dhw_enable`) | 14 | hardcoded loop in `MQTTstuff.ino` |
| `<haPrefix>/select/<nodeId>/sat_heating_system/config` | 1 | hardcoded in `MQTTstuff.ino` |

Source variants are emitted **always** regardless of current `bSeparateSources` value: an older firmware on this device may have published them under a different mode, and the wipe must be defensive across the version-transition boundary. Empty-publish on a non-existent broker topic is a no-op.

### Classification per ADR-080

This ADR is **guideline-level**. It is a multi-step lifecycle pattern (settings flag → OTA hook → boot-time wipe → republish) whose correctness depends on the interaction between five files (`MQTTstuff.h`, `MQTTstuff.ino`, `OTGW-ModUpdateServer-impl.h`, `OTGW-ModUpdateServer-esp32.h`, `OTGW-firmware.ino`) and a runtime ordering invariant that only manifests under crash. A static `evaluate.py`-style grep cannot reasonably check that the boot handler runs after `startMQTT()`, that the flag-clear is persisted before `markAllMQTTConfigPending()`, that both OTA hooks set the flag, or that the wipe-side topic templates match the publish-side helpers. An integration-test rig that boots the firmware, fakes an OTA, and watches the broker would cover this; that rig does not exist yet. Until it does, the contract is reviewed at PR. Promotion to pattern-level becomes worthwhile when a third lifecycle event (factory-reset over MQTT, force-rediscover REST endpoint) adopts the same default-true-flag-with-persisted-clear shape.

## Related

- **ADR-040**: MQTT source-specific topics. Amended by **ADR-095** on this branch (the `bSeparateSources` exclusivity flip is the trigger that makes this wipe ADR load-bearing for the 2.0.0 release).
- **ADR-062**: retained-discovery verification. ADR-062 is the *steady-state* heal loop (daily, count-based, recovers broker-side loss). This ADR is the *firmware-version-transition* heal (one-shot, full wipe, recovers shape changes). The two are complementary; they share `markAllMQTTConfigPending()` as the republish trigger.
- **ADR-066**: MQTT publish gating by source and slave-echo. Affects which topics a given firmware version publishes; a future change to ADR-066's per-MsgID classification table is exactly the kind of shape transition this ADR reconciles.
- **ADR-067 (on `dev`)**: original decision this ADR ports forward. The structural shape (default-true struct-init, OTA hook, boot-time wipe, persist-clear-then-republish) is unchanged. The only material differences on this branch are: streaming-primitive emission per ADR-077/ADR-093, dual-platform OTA hooks, and the broader topic matrix that includes SAT switches and the SAT select.
- **ADR-077**: streaming MQTT HA discovery architecture. The wipe loop emits zero-byte retained payloads through the same `beginMqttPublish + endPublish` primitives the publish loop uses, so heap gates and TCP framing are identical.
- **ADR-080**: binding ADR rules must have a CI gate. This ADR is explicitly guideline-level under that meta-rule; classification rationale documented above.
- **ADR-090**: re-entrancy guard pattern for shared scratch buffers. The wipe loop acquires `MQTTAutoConfigSessionLock` (`MQTTstuff.ino:75-89`) so it cannot interleave with `doAutoConfigure()` mid-stream.
- **ADR-093**: HA discovery retained-config orphan cleanup (per-entity, user-driven). Companion lifecycle event: ADR-093 covers per-entity removal on user action ("Forget" a BLE roster slot); this ADR covers per-device removal on firmware action (OTA upgrade). Both publish zero-byte retained via the streaming primitives. Together they form the complete HA discovery removal lifecycle on 2.0.0.
- **ADR-095**: `bSeparateSources` exclusivity (port of ADR-068). The trigger that makes ADR-094's wipe load-bearing for the 2.0.0 release: without ADR-094, ADR-095's semantic flip would reproduce dev's two-`OTGW_Room_Temperature` regression on every upgrade.
- **TASK-524**: implementation task that lands the 2.0.0 mechanism (`backlog/tasks/task-524 - feat-2.0.0-extend-HA-discovery-wipe-on-OTA-to-SAT-switches-and-select-entities-port-forward-delta-from-TASK-523.md`).

## References

- Anchor points on `feature-dev-2.0.0-otgw32-esp32-sat-support`:
  - `src/OTGW-firmware/MQTTstuff.h:41` — `MQTTSettingsSection` (per ADR-079/081); `bWipeHaDiscoveryOnBoot` field added next to `bDiscoveryAutoVerify` (line 50) and `bSeparateSources` (line 59).
  - `src/OTGW-firmware/MQTTstuff.h:233` — `MQTT_HA_FLAG_ANY_SOURCE = 0x07`.
  - `src/OTGW-firmware/MQTTstuff.h:284` — `MQTT_HA_SENSOR_COUNT` declaration.
  - `src/OTGW-firmware/MQTTstuff.h:437` — `expandAndStreamSensorSources` declaration.
  - `src/OTGW-firmware/MQTTstuff.ino:75-89` — `MQTTAutoConfigSessionLock` (ADR-090).
  - `src/OTGW-firmware/MQTTstuff.ino:1604` — `markAllMQTTConfigPending` body.
  - `src/OTGW-firmware/MQTTstuff.ino:1754` / `1836` — `doAutoConfigure` / `doAutoConfigureMsgid` (publish loops; wipe runs alongside, never within).
  - `src/OTGW-firmware/MQTTstuff.ino:2257` — `satBLEUnpublishDiscovery` (ADR-093 reference template for the streaming-primitive pattern reused by the wipe loop).
  - `src/OTGW-firmware/MQTTHaDiscovery.cpp:583` / `:585` / `:1020` / `:1847` / `:2078` / `:2285` — `MQTT_HA_SENSOR_COUNT` / `mqttHaSensors[]` / `mqttHaBinSensors[]` / `sanitizeHaObjectId` (currently `static`, must be de-static-ed during TASK-524) / `streamSensorDiscovery` / `expandAndStreamSensorSources`.
  - `src/OTGW-firmware/OTGW-ModUpdateServer-impl.h:281-311` — ESP8266 `_handleUploadEnd`; OTA-finish hook insertion site after `Update.end(true)` success.
  - `src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h:276-289` — ESP32 `_handleUploadEnd`; OTA-finish hook insertion site after `Update.end(true)` success.
  - `src/OTGW-firmware/OTGW-firmware.ino:186` — `startMQTT();` in `setup()`; boot-time wipe handler insertion site immediately after.
- Home Assistant MQTT discovery removal contract: <https://www.home-assistant.io/integrations/mqtt/#removing-devices-and-entities-from-the-mqtt-integration> (zero-byte retained payload to the discovery topic).
- Field report (root cause on `dev`): Discord `#beta-testing`, user `_reuzenpanda_`, 2026-04-30. Dev fix shipped under TASK-523 / ADR-067; this ADR ports the contract forward.
