# ADR-084 Generic OT-bus state MQTT topics (amends ADR-065)

## Status

Accepted. Date: 2026-04-24. Amends ADR-065 (otgw-pic/ MQTT subtree as stable public topic API). Partially breaks the MQTT API published by firmware 1.4.x and by pre-release 2.0.0 builds.

## Context

The `feature-dev-2.0.0-otgw32-esp32-sat-support` branch introduces a second hardware variant, the OTGW32, which reads the OpenTherm bus directly via the OTDirect driver on an ESP32 instead of through the PIC coprocessor used on the NodoShop OTGW. The two variants are mutually exclusive per binary: board configuration sets either `HAS_PIC=1, HAS_DIRECT_OT=0` (ESP8266 + PIC) or `HAS_PIC=0, HAS_DIRECT_OT=1` (OTGW32). A single device is never both at the same time.

Three values describe what the firmware *observes on the OpenTherm bus*: whether a boiler is present, whether a thermostat is present, and whether the bus itself is online. They are fed by a single source of truth in the state struct:

- `state.otBus.bBoilerState` to `boiler_connected`
- `state.otBus.bThermostatState` to `thermostat_connected`
- `state.otBus.bOnline` to `otgw_connected`

Both hardware paths populate the same state fields. The publish side, however, has evolved independently. Today (pre-patch) these values are emitted in three places per cycle (`src/OTGW-firmware/MQTTstuff.ino:1299-1331`):

1. Unconditionally to the generic namespace: `OTGW/value/<uniqueId>/<label>`.
2. Conditionally to the PIC subtree when `isPICEnabled()`: `OTGW/value/<uniqueId>/otgw-pic/<label>`.
3. Conditionally to the OTDirect subtree when `isOTDirectEnabled()`: `OTGW/value/<uniqueId>/otgw-otdirect/<label>`, with the additional inconsistency that the online flag is named `ot_online` on this path instead of `otgw_connected`.

Home Assistant discovery further amplifies the problem. The two binary_sensor entries for `boiler_connected` and `thermostat_connected` in `src/OTGW-firmware/MQTTHaDiscovery.cpp:1038-1039` carry the flag `MQTT_HA_FLAG_IS_PIC_ENTRY` (0x08). The flag does two things at once: it causes `composeBinSensorPayload()` to prepend `otgw-pic/` to the `stat_t`, and it causes `doAutoConfigure()` at `MQTTstuff.ino:1680` to skip the entry entirely when `!isPICEnabled()`. On an OTGW32 the guard is compiled to `false`, which makes the two entities invisible in Home Assistant even though the firmware still publishes the values to the generic topic.

ADR-065 declared the `otgw-pic/` subtree a stable public topic API and assigned the three keys above to that subtree. At the time there was only one hardware path, so the PIC subtree was the natural home for anything visible on the OT bus. With a second hardware path present, the subtree membership of these three keys is incorrect: they describe the OT bus, not the PIC coprocessor.

## Decision

**Move the three OT-bus presence values out of any hardware-specific subtree and into the generic value namespace**, and simultaneously **amend ADR-065** so that the `otgw-pic/` subtree strictly contains properties of the PIC coprocessor itself.

Concretely:

1. Canonical topics are:
   - `OTGW/value/<uniqueId>/boiler_connected`
   - `OTGW/value/<uniqueId>/thermostat_connected`
   - `OTGW/value/<uniqueId>/otgw_connected`
2. The hardware-specific duplicates (`otgw-pic/boiler_connected`, `otgw-pic/thermostat_connected`, `otgw-pic/otgw_connected`, `otgw-otdirect/boiler_connected`, `otgw-otdirect/thermostat_connected`, `otgw-otdirect/ot_online`) are removed from the publish side in both variants.
3. `otgw-otdirect/ot_online` is retired. The concept lives under the generic `otgw_connected` name so consumers see one name regardless of hardware variant.
4. In `MQTTHaDiscovery.cpp:1038-1039` the `MQTT_HA_FLAG_IS_PIC_ENTRY` flag is cleared on `boiler_connected` and `thermostat_connected`. The flag definition itself (`MQTTstuff.h:232`) stays because genuine PIC-coprocessor entries (version, deviceid, firmwaretype, designer, picavailable, plus the climate `mode_stat_t`) continue to rely on it.
5. A temporary self-healing retained-cleanup block is added to the firmware (see TASK-410, flagged for removal in TASK-409) so that brokers that previously served retained payloads on the six deprecated topics are emptied at the first reconnect of any 2.0.0 firmware. On every MQTT reconnect the firmware briefly subscribes to those six paths, publishes an empty retained payload on any that still carries retained state, and unsubscribes after a short timeout. Idempotent: on brokers that never saw the pre-2.0.0 firmware it is a no-op.

**Amendment to ADR-065**: the `otgw-pic/` subtree is now defined as strictly containing properties of the PIC coprocessor itself. The membership list is: `version`, `deviceid`, `firmwaretype`, `designer`, `picavailable`, `settings/*` (13 subkeys), plus the climate entity `mode_stat_t` which reflects a PIC-provided operating mode. The three keys removed by this ADR are explicitly no longer part of that subtree.

## Alternatives Considered

**A. Dual-publish per ADR-065 migration strategy.**
ADR-065 prescribes, for any subtree-level change, a dual-publish window of at least two minor releases during which both the legacy and the new path are populated, plus a 30-day Discord announcement and retained-cleanup notes in the removal release. Applied here that would mean 2.0.0 and 2.1.0 keep writing to `otgw-pic/*` while also writing to the new generic paths, and only 2.2.0 drops the legacy paths.

Rejected. Three reasons.
- 2.0.0 is a major version bump. Semantic versioning positions major boundaries as the sanctioned point where the MQTT contract may change. ADR-065's dual-publish rule was authored in the context of 1.x minor releases, where there is no consumer expectation of breakage; carrying the rule across a major boundary yields surface-level compliance without its actual reason-to-exist (consumer surprise prevention).
- The self-healing retained cleanup delivers the same user-facing outcome as dual-publish: no ghost state lingering on the broker, no zombie HA entities. Dual-publish is one way to avoid those ghosts; cleanup is another. Functional equivalence, lower firmware footprint.
- 2.0.0 already has a release-notes breaking-change section and a migration guide in `docs/api/MQTT.md` with concrete `mosquitto_pub` commands, which together cover the "documented, announced, actionable" obligations of ADR-065's strategy. The migration guide is in fact richer than the original strategy prescribed.

**B. Fix only the HA-discovery side (strip the flag, keep the publishes).**
Set `MQTT_HA_FLAG_IS_PIC_ENTRY` to 0x00 on the two binary_sensor rows so HA discovers them and points at the generic topic, but leave the hardware-specific publishes intact. That would make HA work on both variants and still satisfy any legacy consumer on `otgw-pic/boiler_connected`.

Rejected. It preserves the design error rather than correcting it. `otgw-pic/boiler_connected` does not describe the PIC; it describes the bus. Keeping the publish there permanently conflates two concepts and leaves OTDirect-only installations with a dead `otgw-pic/*` subtree. It also does not address the naming inconsistency (`ot_online` vs `otgw_connected`).

**C. Keep everything as-is (no change).**
Rejected. Boiler connected and Thermostat connected HA entities are missing on OTGW32, which is a user-facing regression relative to the PIC variant. Publishing the same value to three different topics wastes broker storage and confuses any consumer that does not know which one is authoritative.

## Consequences

**Benefits**

- A single canonical MQTT topic per OT-bus observation. Consumers no longer need to know the hardware variant to subscribe correctly.
- HA binary_sensors `Boiler connected` and `Thermostat connected` now auto-discover on every 2.0.0 device regardless of variant. Their `unique_id` is unchanged, so on existing HA installations the entity keeps its history and automations keep firing; only the `state_topic` changes at the first discovery republish.
- Inconsistent naming resolved: `ot_online` is retired, `otgw_connected` is the single name.
- Publish-side code simplifies. The three hardware-conditional blocks (~30 lines in `MQTTstuff.ino:1299-1331`) are removed.
- ADR-065's subtree contract becomes cleaner and easier to enforce: PIC things live under `otgw-pic/`, nothing else does.

**Trade-offs**

- Breaking for consumers that subscribed to `otgw-pic/boiler_connected`, `otgw-pic/thermostat_connected`, `otgw-pic/otgw_connected`, `otgw-otdirect/boiler_connected`, `otgw-otdirect/thermostat_connected`, or `otgw-otdirect/ot_online`. Custom Node-RED flows, openHAB bridges, Prometheus exporters, Grafana queries, and hand-written HA YAML against these paths need their topic pattern updated. The migration is well within reach: it is a one-line topic change per consumer, documented in `docs/api/MQTT.md` with copy-pasteable examples.
- Conscious deviation from ADR-065's prescribed dual-publish migration strategy. This ADR treats the major version boundary as the alternative mitigation, paired with self-healing cleanup and a migration guide. ADR-065 remains in force for any future changes to the `otgw-pic/` subtree *within* a major release line: those still require dual-publish.
- Temporary firmware code (the retained-cleanup block) must be removed once it is no longer earning its keep. Tracked in TASK-409 with a practical target of 2.3.0 or 3.0.0, whichever comes first. The code carries an explicit header comment stating when it may be removed.

**Risks and Mitigation**

- *Risk*: a user runs 2.0.0 firmware briefly, rolls back to 1.4.x for an unrelated reason, and encounters broker state that the 2.0.0 cleanup has emptied. Impact: `otgw-pic/boiler_connected` retained would be missing on restart until 1.4.x republishes on its next cycle. *Mitigation*: 1.4.x repopulates these retained topics at normal publish cadence (typically within tens of seconds); impact is observable only in the brief window. Release notes mention the cleanup behaviour explicitly so the operator is not surprised.
- *Risk*: a consumer that still subscribes to the old `otgw-pic/*` paths sees no values after upgrading and has no error to diagnose from. *Mitigation*: the migration section in `docs/api/MQTT.md` is prominent (breaking-change banner at the top of the document), and the release notes explicitly name each moved key.
- *Risk*: someone adds a new HA entity with a publish path under `otgw-pic/` but forgets the `MQTT_HA_FLAG_IS_PIC_ENTRY` flag, and the discovery `stat_t` points at the wrong location. *Mitigation*: unchanged from ADR-065. The flag-driven discovery contract remains the enforcement mechanism for the now-narrower subtree; code review catches missing flags.
- *Risk*: ADR-065 superseding logic becomes confused because this ADR amends rather than supersedes. *Mitigation*: ADR-065's Status line is not changed by this ADR (it stays Accepted); ADR-065's Decision is narrowed by reference to this ADR-084. Future ADRs touching the subtree should reference both.

## Related Decisions

- **ADR-065 (`otgw-pic/` MQTT subtree as stable public topic API)**: amended by this ADR. The subtree definition is narrowed; the stability contract and the flag-driven discovery contract remain in force for the remaining entries.
- **ADR-040 (MQTT source-specific topics for OpenTherm values)**: unchanged. Source-specific topics (`<metric>/thermostat`, `<metric>/boiler`, `<metric>/gateway`) describe direction on the OT bus and are orthogonal to this change.
- **ADR-077 (Streaming MQTT HA Discovery Architecture)**: unchanged. The streaming discovery pipeline already supports flag-gated `stat_t` prefixing; clearing the flag on the two rows is a data change, not an architectural one.
- **ADR-063 (OTGW32 hardware support)**: context. This ADR resolves one of the MQTT-surface gaps between the two hardware variants introduced by ADR-063.
- **ADR-060 (PIC availability guard pattern)**: context. `isPICEnabled()` remains the authoritative guard for PIC-specific code paths; this ADR simply removes two entries from the set of things it guards.

## References

- Implementation task: TASK-410 (firmware side).
- Documentation task: TASK-411 (`docs/api/MQTT.md` migration section, `docs/releases/RELEASE_GITHUB_2.0.0.md` breaking-change entry).
- Cleanup task: TASK-409 (remove the temporary retained-cleanup block in a future release).
- Related repo-hygiene task: TASK-412 (resolve duplicate ADR numbers 062, 064, 065; does not affect this ADR's number but is adjacent).
- Source files touched by the implementation:
  - `src/OTGW-firmware/MQTTstuff.ino` (publish sites near :1299-1331, migration helpers, callback wiring)
  - `src/OTGW-firmware/MQTTHaDiscovery.cpp:1038-1039` (flag clear)
  - `src/OTGW-firmware/OTGW-Core.ino` (duplicate publish sites per ADR-065 inventory at :3743, :3750, :3758 if still present)
- State of truth: `state.otBus.bBoilerState`, `state.otBus.bThermostatState`, `state.otBus.bOnline`.
- Home Assistant discovery protocol: <https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery> (entity `unique_id` stability contract).
