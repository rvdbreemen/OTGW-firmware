# ADR-077: MQTT — Publish HA-Core Capability-Flag Aliases under `settings.mqtt.bPublishHaCoreAliases`

## Status

Proposed

## Context

TASK-649 confirmed that the 13 capability-flag binary_sensors this firmware publishes for OpenTherm MsgID 2 (master config), MsgID 3 (slave config), and MsgID 6 (RBP transfer/RW flags) are at semantic parity with Home Assistant core's `opentherm_gw` integration. However, the **names** differ for 11 of the 13 bits — the firmware uses OT-spec-derived labels, HA-core uses self-describing translation_keys with a `supports_*` verb prefix for capability bits.

The current name mismatch becomes a real-world friction point in one specific scenario: a user migrating *from* HA-core's native `opentherm_gw` integration *to* this firmware. Their HA automations are pinned to entity ids like `binary_sensor.<gateway>_supports_hot_water`, but this firmware exposes that same boiler bit as `binary_sensor.<gateway>_dhw_present` — every automation referencing one of the 11 affected bits has to be rewritten by hand. There is no in-firmware mechanism to ease that migration today.

The mapping was extracted directly from HA-core's source (`homeassistant/components/opentherm_gw/binary_sensor.py`):

| MsgID | Current firmware label | HA-core translation_key |
|---|---|---|
| 3 | `dhw_present` | `supports_hot_water` |
| 3 | `control_type_modulation` | `control_type` |
| 3 | `cooling_config` | `supports_cooling` |
| 3 | `dhw_config` | `hot_water_config` |
| 3 | `master_low_off_pump_control_function` | `supports_pump_control` |
| 3 | `ch2_present` | `supports_ch_2` |
| 3 | `remote_water_filling_function` | `supports_remote_reset` |
| 6 | `rbp_dhw_setpoint` | `supports_hot_water_setpoint_transfer` |
| 6 | `rbp_max_ch_setpoint` | `supports_central_heating_setpoint_transfer` |
| 6 | `rbp_rw_dhw_setpoint` | `supports_hot_water_setpoint_writing` |
| 6 | `rbp_rw_max_ch_setpoint` | `supports_central_heating_setpoint_writing` |

The firmware also publishes two capability bits that HA-core's `opentherm_gw` integration does *not* expose. These get HA-core-style `supports_*` aliases coined for consistency (no upstream name to align with, so the name is chosen by this ADR):

| MsgID | Current firmware label | Proposed alias | Rationale |
|---|---|---|---|
| 2 | `master_configuration_smart_power` | `supports_smart_power` | MsgID 2 HB bit 0: master-side smart-power capability. Concise self-describing form. |
| 3 (bit 7) | `heat_cool_mode_control` | `supports_heat_cool_mode_control` | MsgID 3 HB bit 7: slave-side heat/cool-mode-control capability. Full bit name preserved under the `supports_*` prefix because there is no natural shorter form. |

This brings the total alias set to **13** — every capability-flag binary_sensor the firmware publishes gets a self-describing alias under the same toggle.

### Constraints

- **No breaking renames.** ADR-070 (sibling-suffix source-separated discovery) is already accepted; the existing 13 labels are part of users' live automations across thousands of deployments. The firmware must keep publishing all current labels untouched.
- **ESP8266 memory.** Adding 13 PROGMEM label strings + 13 friendly-name strings + 13 struct rows costs roughly 1 KB of flash (PROGMEM) and 0 bytes of RAM at rest. The active discovery + publish path adds 13 extra `sendMQTTData()` calls per MsgID 2 / MsgID 3 / MsgID 6 update when the setting is on.
- **Broker hygiene.** When the setting is *off*, the broker must not see any alias discovery configs or value publishes. When toggled *on*, broker sees 13 new retained discovery configs.
- **HA-discovery contract.** The new entries must conform to the existing `MqttHaBinSensorCfg` shape (the dev struct has no `deviceClass` or `payload` fields; the composer emits no `device_class` and relies on HA's default `"ON"`/`"OFF"` payloads — same as the existing 13 entries).
- **Default off.** Existing users who do not toggle the setting see no behavioural change at all.

## Decision

Add a single bool to the persisted MQTT settings sub-section and a corresponding flag bit to `MqttHaBinSensorCfg`:

1. **Settings field** — add `bool bPublishHaCoreAliases;` (default `false`) to the `mqtt` sub-section of `OTGWSettings` (parallel to `bHaRebootDetect`, `bSeparateSources`, `bDiscoveryAutoVerify`). Serialise to LittleFS JSON via the existing `settingStuff.ino` reader/writer. Expose a web UI toggle on the MQTT settings page.

2. **Flag bit** — define `#define MQTT_HA_FLAG_IS_HA_CORE_ALIAS 0x10` in `MQTTstuff.h` (the existing flags use the 0x01..0x08 range; 0x10 is the next unused bit).

3. **Discovery table** — add 13 new rows to `mqttHaBinSensors[]` in `mqtt_configuratie.cpp`, each carrying the new flag bit, after the existing MsgID-2/3/6 rows. Each new row mirrors the icon and entity_category of its current-name sibling (so aliased `supports_hot_water` keeps `mdi:water-boiler`, aliased `supports_cooling` keeps `mdi:snowflake`, etc.). The friendly-name PROGMEM string follows the same `Title_Case` convention used elsewhere in this array (`Supports_hot_water`, `Supports_central_heating_setpoint_transfer`, …). The two firmware-extra aliases (`supports_smart_power`, `supports_heat_cool_mode_control`) inherit the `mdi:checkbox-marked-circle` icon and `diagnostic` category of their current-name siblings.

4. **Discovery gating** — `streamBinarySensorDiscovery()` checks `settings.mqtt.bPublishHaCoreAliases` before publishing any row whose `cfg.flags & MQTT_HA_FLAG_IS_HA_CORE_ALIAS` is set. When the setting is off, alias rows are silently skipped; the iteration index still advances. No discovery topic is published, no retained config lands on the broker.

5. **Publisher gating** — each of the 13 affected `sendMQTTData(F("<current_label>"), val)` call sites — 1 in `print_mastermemberid()` (MsgID 2), 8 in `print_slavememberid()` (MsgID 3), and 4 in `publishRBPFlagsState()` (MsgID 6), all in `OTGW-Core.ino` — gains a guarded second call:
   ```cpp
   sendMQTTData(F("dhw_present"), val);
   if (settings.mqtt.bPublishHaCoreAliases) {
       sendMQTTData(F("supports_hot_water"), val);
   }
   ```
   Order: current-label publish first, alias publish second. The current-label path is *never* affected by the setting.

6. **No cleanup on toggle-off.** When the user disables the setting, the firmware stops publishing alias discovery and alias state, but does not actively deregister the entities. Retained discovery configs and the last retained state values stay in the broker until manually purged (e.g. via `mosquitto_pub -t .../config -n -r`). This is the conscious trade-off; see Alternatives.

7. **Scope is all 13 capability-flag bits the firmware publishes.** Eleven aliases adopt HA-core's translation_key verbatim for migration continuity; the two firmware-extras (`master_configuration_smart_power`, `heat_cool_mode_control`) get HA-core-style `supports_*` aliases coined in this ADR. Naming the firmware-extras under the same `supports_*` convention keeps the entity-id namespace coherent — a user enabling the toggle sees a uniformly-named `binary_sensor.<gw>_supports_*` set rather than a mix of `supports_*` and `master_configuration_smart_power`.

### Why this choice

- **Minimal migration friction.** Users moving from the HA-core integration to this firmware can flip one toggle and have their automations work unchanged for the 11 HA-core-matched capability bits.
- **Coherent naming across the full set.** The two firmware-extras pick up `supports_smart_power` / `supports_heat_cool_mode_control` aliases under the same toggle, so the user-facing namespace under toggle-on is uniformly `supports_*` — no one-off naming exceptions.
- **No risk to existing users.** Default-off means the contract for installed deployments is unchanged. A user who never toggles the setting cannot notice the new code path.
- **Confined surface area.** The change is additive: one settings bool, one flag bit, 13 new discovery rows, 13 guarded second-publish call sites. No existing data structures are reshaped. No existing topic loses its name.
- **Reuses the existing discovery flag-bit pattern.** `MQTT_HA_FLAG_IS_PIC_ENTRY` (ADR-065) already gates topic prefixing via a flag bit on the same struct; this ADR follows the same shape.

## Alternatives Considered

### Alternative 1 — Rename the 13 existing labels to the new alias names

Drop `dhw_present` and publish `supports_hot_water` only; do the same for all 13 bits, including the firmware-extras.

**Rejected:** breaks every existing user's automations referring to one of the 13 topics. ADR-070's source-separated layer is already widely deployed; another forced rename so soon would burn user trust. Documentation alone cannot reach every deployment.

### Alternative 2 — Documentation-only mapping (extend the TASK-649 audit doc)

Publish nothing new. Extend `docs/audits/2026-05-21-ha-capability-flags-dev.md` into a complete name-mapping reference so users searching for `supports_hot_water` find that the firmware exposes the same bit as `dhw_present` and can rewrite their automations manually.

**Rejected:** does not solve the migration problem (every automation referencing a capability bit still has to be hand-rewritten). The complete mapping should land alongside this ADR as well — the documentation and the runtime feature are complementary, not alternatives.

### Alternative 3 — Always-on aliases, no setting

Skip the setting entirely; always publish both names. Cuts ~15 lines of code.

**Rejected:** permanently doubles the discovery footprint and per-update MQTT chatter for these 13 bits for every deployment, even those that will never use the HA-core names. ESP8266 broker payload budget and HA entity registry are both finite. The 1-bit settings field is a cheap toggle.

### Alternative 4 — Default-on aliases with the setting present

Setting defaults to `true`; users can opt out.

**Rejected:** silently introduces 13 new HA entities on the next firmware upgrade for every existing deployment. Existing users would discover unexplained `binary_sensor.*_supports_*` entries appearing in HA without context, then have to read release notes to figure out how to disable them. Default-off is strictly safer; new users can flip the toggle once they understand what it does.

### Alternative 5 — Clean discovery deregistration on toggle-off

Same as Decision, but on toggle-off, publish empty retained payloads to all 13 alias discovery topics so HA removes the entities cleanly.

**Rejected for v1** (may be revisited in a follow-up ADR): adds ~50 lines of cleanup machinery (alias label registry, dispatcher on settings-change, retry on MQTT reconnect) for a corner case. Users toggling off after toggling on can manually purge retained topics with `mosquitto_pub -n -r`. If field testing shows this is a frequent operation, a future ADR can add the automated cleanup.

### Alternative 6 — Alias only the 11 HA-core-matched bits; leave the 2 firmware-extras alone

Skip the firmware-extras (`master_configuration_smart_power`, `heat_cool_mode_control`); only alias the 11 bits that have an HA-core counterpart. Saves 2 PROGMEM rows.

**Rejected:** mixes naming conventions inside one logical group. A user enabling the toggle sees `binary_sensor.<gw>_supports_hot_water` alongside `binary_sensor.<gw>_master_configuration_smart_power` and `binary_sensor.<gw>_heat_cool_mode_control` — three styles in one capability-flag set, harder to scan. The flash and bandwidth cost of two extra aliases is negligible; coherent naming is worth more.

## Consequences

### Positive

- HA-core integration users can migrate to this firmware without rewriting any of their 11-bit capability-flag automations (the 11 HA-core-matched aliases).
- The two firmware-extras gain `supports_smart_power` and `supports_heat_cool_mode_control` aliases, so the toggle-on experience is uniformly `supports_*`-prefixed across all 13 capability bits — easier for users to discover and reason about.
- The 11 HA-core names are more self-descriptive than the existing OT-spec-derived names (`supports_central_heating_setpoint_transfer` vs `rbp_max_ch_setpoint`), so new HA users discovering this firmware also benefit.
- The settings field gives a clean opt-in story: this firmware does not impose dual-publishing on anyone who hasn't explicitly asked for it.
- The 13-row mapping table is now codified in firmware source, not just a documentation footnote — easier for future maintainers to update if HA-core renames a key.

### Negative

- When the setting is on, broker discovery payload count for capability flags doubles (from 13 to 26 retained discovery configs), and per-update MQTT publish volume for MsgID-2 / MsgID-3 / MsgID-6 frames doubles for the affected bits.
- On toggle-off, retained discovery configs + last retained state values for the 13 aliases linger in the broker until manually purged. HA will mark the entities `unavailable` once the firmware stops publishing the retained availability heartbeat, but the entities themselves remain in HA's entity registry until the user removes them.
- The two firmware-extra aliases (`supports_smart_power`, `supports_heat_cool_mode_control`) are *coined* by this firmware, not by HA-core. If HA-core later adds these bits with a different name, the firmware aliases will diverge from HA-core's canonical names. The fix at that point would be either (a) accept the divergence (no real harm; users of this firmware already see the names), or (b) add a *third* alias matching HA-core's new name (handled by another small ADR; this one does not pre-decide).
- The new flag bit (`MQTT_HA_FLAG_IS_HA_CORE_ALIAS`) consumes one of the eight available bits in `MqttHaBinSensorCfg::flags`. Two other bits (`MQTT_HA_FLAG_IS_PIC_ENTRY` = 0x08, plus the existing 0x01/0x02/0x04) are already taken; this leaves four for future use.
- A future HA-core rename of a translation_key would require a firmware change to track. The audit doc (TASK-649) captures the mapping; reverse-checking on HA releases is operational work.

### Risk

The risk to the existing fan-out path is low because the changes are additive — the current-label publish is identical to today's behaviour. The new alias publish is conditional and skipped entirely when the setting is off. The discovery flag bit is checked once per discovery iteration; no hot-path impact.

## Related Decisions

- **ADR-052** — MQTT publish eligibility contract (first-seen / value-changed / stale-refresh). The alias publishers reuse this contract verbatim via `sendMQTTData()`.
- **ADR-065** — `MQTT_HA_FLAG_IS_PIC_ENTRY` flag-bit-based discovery customisation. This ADR follows the same pattern.
- **ADR-070** — Sibling-suffix source-separated discovery (the dev equivalent of 2.0.0's ADR-098). Out of scope for this ADR; alias topics use the canonical (no-source) shape, same as the existing 13 capability flags.
- **ADR-074** — Retained availability heartbeat. Alias entities inherit the same availability topic, so HA correctly marks them unavailable when the firmware stops publishing.
- **ADR-076** — MQTT status fan-out per-slot heartbeat. Alias publishers do not enter the status fan-out; they sit on the same `sendMQTTData()` path the existing capability-flag publishers use.
- **ADR-105** — sibling on the 2.0.0 feature line (`feature-dev-2.0.0-otgw32-esp32-sat-support`). Same decision, different file layout (`MQTTHaDiscovery.cpp` instead of `mqtt_configuratie.cpp`; `MqttHaBinSensorCfg` has explicit `deviceClass` and `payload` fields that default to `nullptr` / `HaBinaryPayload::on_off`, so the alias rows omit them just like the existing rows do).

## References

- `docs/audits/2026-05-21-ha-capability-flags-dev.md` — TASK-649 audit confirming semantic parity for the 13 capability flags.
- `src/OTGW-firmware/mqtt_configuratie.cpp:1103` — current `mqttHaBinSensors[]` array; alias rows append here.
- `src/OTGW-firmware/MQTTstuff.h:205` — `MqttHaBinSensorCfg` struct (no `deviceClass` field; alias rows match the existing shape).
- `src/OTGW-firmware/OTGW-Core.ino:2284` — `print_slavememberid()` MsgID-3 publishers (8 sites gain the guarded second call, including `heat_cool_mode_control` → `supports_heat_cool_mode_control`).
- `src/OTGW-firmware/OTGW-Core.ino:1945` — `publishRBPFlagsState()` MsgID-6 publishers (4 sites gain the guarded second call).
- `src/OTGW-firmware/OTGW-Core.ino` — `print_mastermemberid()` MsgID-2 publisher (1 site gains the guarded second call: `master_configuration_smart_power` → `supports_smart_power`).
- Home Assistant core `homeassistant/components/opentherm_gw/binary_sensor.py` — source of the 11 translation_key names; consulted via the `dev` branch at https://github.com/home-assistant/core.
- pyotgw `vars.py` — defines the `DATA_SLAVE_*` / `DATA_REMOTE_*` Python constants behind the HA-core entities.

## Enforcement

```json
{
  "forbid_pattern": [],
  "require_pattern": [],
  "llm_judge": true
}
```

The Decision is structural (settings field + flag bit + guarded publishes) and not easily encoded as a regex. Sonnet judge in the pre-commit hook reviews any diff touching `mqtt_configuratie.cpp`, `OTGW-Core.ino`, or `OTGWSettings.h` against this ADR's Decision text to catch: (a) alias publishes that are not guarded by `settings.mqtt.bPublishHaCoreAliases`, (b) alias rows in `mqttHaBinSensors[]` that omit `MQTT_HA_FLAG_IS_HA_CORE_ALIAS`, (c) renames of any of the 13 current labels (which would be a breaking change), (d) coining a new firmware-extra alias outside the `supports_*` convention.
