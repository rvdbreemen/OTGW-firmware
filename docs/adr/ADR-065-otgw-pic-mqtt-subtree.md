# ADR-065 — otgw-pic/ MQTT subtree as stable public topic API

## Status

Accepted

## Context

The OTGW-firmware MQTT namespace has, since v1.3.0, included a dedicated sub-namespace `otgw-pic/` beneath the per-device publish root `<topTopic>/value/<uniqueid>/`. Topics under this subtree describe state and configuration of the PIC microcontroller on the NodoShop OpenTherm Gateway board: the boiler/thermostat connectivity status, the PIC firmware identification, the PIC settings readback (setpoint override, setback, DHW override, GPIO, tweaks, LED pattern, temperature sensor, smart-power, thermostat-detect, build date, clock speed, reset cause, standalone interval, voltage reference), plus the gateway-mode and OTGW-online flags.

The string `otgw-pic/` currently appears as a hardcoded F()-literal on 24 publish call-sites across `MQTTstuff.ino` and `OTGW-Core.ino` (see "Call-site inventory" below), and on 2 discovery-generator locations in `mqtt_configuratie.cpp`. Up through v1.3.5 the discovery side was driven by `src/OTGW-firmware/data/mqttha.cfg` and explicitly encoded `stat_t = "%mqtt_pub_topic%/otgw-pic/<label>"` for both `boiler_connected` and `thermostat_connected`; the runtime publish side matched this exactly.

During the mqttha.cfg → `mqtt_configuratie.cpp` takeover (commits `bc9bd6a2`, `3e1872ce`, 1.4.0 timeframe) the structured discovery generator introduced a flag bit `MQTT_HA_FLAG_IS_PIC_ENTRY = 0x08` that was set on the two binary_sensor entries for `boiler_connected` and `thermostat_connected` (`mqtt_configuratie.cpp:1035-1036`), but the flag was consumed only as a skip-filter in `MQTTstuff.ino:1365, 1385` ("omit entry when PIC disabled"). The discovery payload generators `composeBinSensorPayload` and `composeSensorPayload` never read the flag when composing `stat_t`. Result: HA-discovery announced `stat_t = <pub>/boiler_connected` while the firmware kept publishing to `<pub>/otgw-pic/boiler_connected`. The binary_sensor entities were permanently `unavailable` in Home Assistant for multiple point releases. Reported by `the_royal_fortune` on Discord `#nederlandse-ondersteuning`, 2026-04-23.

TASK-388 fixes the discovery side by:
- introducing a single PROGMEM constant `kPicSubtreePrefix = "otgw-pic/"` declared extern in `MQTTstuff.h`, defined once in `MQTTstuff.ino`
- honouring `MQTT_HA_FLAG_IS_PIC_ENTRY` in both `composeBinSensorPayload` and `composeSensorPayload` (writes the prefix between `mqttPubTopic/` and `label`)
- replacing the hardcoded `/otgw-pic/thermostat_connected` literal in the climate `mode_stat_t` generator with `writeChar('/')` + `writeProgmem(kPicSubtreePrefix)` + `writeProgmem(PSTR("thermostat_connected\""))`

The publish side remains unchanged; existing consumers are unaffected.

Beyond the immediate fix, this ADR asks the question: what is the **contract** for the `otgw-pic/` subtree, and how do we keep it from drifting in the future?

## Decision

**The `otgw-pic/` MQTT subtree is a stable public topic API.** Any published topic path under this subtree is considered part of the user-facing interface of the firmware, on the same footing as a REST API endpoint or an HA discovery `uniq_id`. It shall not be renamed, restructured, split, nor deprecated without a coordinated migration strategy (see Consequences).

Operational consequences of this decision:

1. **Single source of truth for the subtree name.** `kPicSubtreePrefix` in `MQTTstuff.h` / `MQTTstuff.ino` is the only authoritative string for the subtree prefix. The 26 existing hardcoded literals remain as-is under TASK-388 but are targeted for migration to a `sendMQTTDataPic()` helper in TASK-390 so that a future rename is a one-line change.

2. **Flag-driven discovery contract.** Every HA discovery entry whose publish path lives under the `otgw-pic/` subtree MUST set `MQTT_HA_FLAG_IS_PIC_ENTRY` in its config-table row. The discovery generators use this flag to emit a `stat_t` that matches the publish path. Adding a new PIC-scoped entry to the discovery tables without the flag is a bug; the reviewer must catch it.

3. **No silent subtree changes.** A rename, split, or removal requires a new ADR that supersedes this one, a dual-publish migration (see below), and release notes that call out the breaking change.

## Alternatives Considered

<!-- TODO: document at least 2 alternatives that were considered and rejected, with reasoning. -->

## Consequences

**Benefits:**

- Users who wrote HA YAML snippets, NodeRED flows, Prometheus rules, Grafana queries, or custom HACS components against topics under `otgw-pic/` (at any point since v1.3.0, roughly 3+ years of installed base) have an explicit guarantee of stability.
- The flag-driven discovery contract means new PIC-scoped entries are correctly routed to the subtree without each contributor having to remember the convention.
- A future refactor to rename/split the subtree has a well-defined touch surface: one constant, plus whatever call-sites have not yet been migrated to the helper (tracked in TASK-390).

**Trade-offs:**

- Callers wishing to reorganise the topic namespace for stylistic reasons (e.g. flatten `otgw-pic/settings/*` to `pic_settings/*`) must accept either the migration cost or leave the layout as-is. This is the intended outcome of treating the subtree as public API.
- Any new entry under `otgw-pic/` commits us to long-term support of that specific topic path.

**Migration strategy, if ever required:**

Should a future design require renaming, splitting, or deprecating part of the subtree, the following process applies:

1. **Announce** via ADR that supersedes this one, plus release-notes breaking-change section and a Discord post at least 30 days before the first release carrying the change.
2. **Dual-publish** from the first release of the change: firmware publishes to both the legacy `otgw-pic/<label>` path AND the new path, with retain=true on both. Discovery generators announce only the new path. Legacy consumers keep working during the deprecation window.
3. **Deprecation window**: minimum two minor releases with dual-publish active (e.g. if v1.6.0 introduces the change, v1.7.0 still dual-publishes, v1.8.0 may drop the legacy path).
4. **Retained-topic cleanup** instructions in the release notes of the removal release, explaining to users how to clear stale retained messages on the broker if they upgrade.
5. **HA discovery entities** keep stable `uniq_id` values across the migration to avoid HA creating duplicate entities.

This process is deliberately heavier than a normal feature change. The asymmetry is intentional: breaking user-facing MQTT contracts quietly has historically (see this very ADR's context) cost users real troubleshooting time.

## Call-site inventory (informative, as of 2026-04-23)

Publish side (24 call-sites, unchanged by TASK-388):

- `MQTTstuff.ino:980-985` — PIC metadata: `version`, `deviceid`, `firmwaretype`, `designer`, `picavailable`
- `MQTTstuff.ino:1045-1050` — state: `boiler_connected`, `thermostat_connected`, `gateway_mode`, `otgw_connected`
- `OTGW-Core.ino:691` — `gateway_mode` (duplicate publish path)
- `OTGW-Core.ino:707-749` — 13 PIC-settings subtopics assigned into `mqttTopic` variable (switch-case)
- `OTGW-Core.ino:778-794` — corresponding publishes for `picSettings` block
- `OTGW-Core.ino:3743, 3750, 3758` — state publish at process-OT time: `boiler_connected`, `thermostat_connected`, `otgw_connected`

Discovery side (2 locations, modified by TASK-388):

- `mqtt_configuratie.cpp:1896-1911` — `composeSensorPayload`, flag-gated prefix on `stat_t`
- `mqtt_configuratie.cpp:1989-2000` — `composeBinSensorPayload`, flag-gated prefix on `stat_t`
- `mqtt_configuratie.cpp:2411-2415` — `composeClimatePayload`, climate `mode_stat_t` uses constant for consistency

Flag table entries with `MQTT_HA_FLAG_IS_PIC_ENTRY` set:

- `mqtt_configuratie.cpp:1035` — `boiler_connected`
- `mqtt_configuratie.cpp:1036` — `thermostat_connected`

Currently **no sensor table entries** carry the flag. The PIC-settings topics (`otgw-pic/settings/*`) are published but have no HA discovery. Adding discovery for them is out of scope for TASK-388 but is enabled by this ADR: future work sets the flag on the relevant sensor entries and the generator handles the prefix automatically.

## Related Decisions

- **TASK-388** — the immediate bug fix that introduces `kPicSubtreePrefix` and activates the flag in the discovery generators. Links this ADR from source comments at `MQTTstuff.h:176`, `MQTTstuff.ino:55`, `mqtt_configuratie.cpp:1897, 1990, 2412`.
- **TASK-389** — the task that authors this ADR and tracks its progression from Proposed to Accepted.
- **TASK-390** — publish-side helper `sendMQTTDataPic()` and migration of the 24 call-sites to it. Operationalises the single-source-of-truth principle stated in this ADR.
- **ADR-004** — "No String class in hot paths". The `kPicSubtreePrefix` definition and the helper planned in TASK-390 both use PROGMEM-correct `char[]` patterns consistent with ADR-004.
- **v1.3.0 release** — the release in which the `otgw-pic/` subtree was introduced. Tag `v1.3.0`; see `mqttha.cfg` entries for `boiler_connected` and `thermostat_connected` with `stat_t = %mqtt_pub_topic%/otgw-pic/<label>` as the original design.
- **mqttha.cfg archive** — `docs/archive/mqttha-generator/` holds the retired `mqttha.cfg` and generator scripts. The historical `stat_t` literals there are the authoritative reference for the pre-takeover contract.

## References

<!-- TODO: populate from inline citations or external sources cited in the body. -->
