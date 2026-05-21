# ADR-105: MQTT — Publish HA-Core Capability-Flag Aliases under `settings.mqtt.bPublishHaCoreAliases`

## Status

Proposed

## Context

TASK-650 confirmed that the 13 capability-flag binary_sensors this firmware publishes for OpenTherm MsgID 2 (master config), MsgID 3 (slave config), and MsgID 6 (RBP transfer/RW flags) are at semantic parity with Home Assistant core's `opentherm_gw` integration on the 2.0.0 feature line. The firmware additionally publishes capability-style binary_sensors on MsgID 74 (ventilation/heat-recovery configuration) that HA-core does not surface at all. Together this gives a 15-bit capability-flag set across MsgID 2 / 3 / 6 / 74 — 11 with direct HA-core counterparts, and 4 firmware-extras with no HA-core upstream.

The **names** differ for 11 of the 13 audited bits and for all 4 firmware-extras — the firmware uses OT-spec-derived labels, HA-core uses self-describing translation_keys with a `supports_*` verb prefix for capability bits.

The current name mismatch becomes a real-world friction point in one specific scenario: a user migrating *from* HA-core's native `opentherm_gw` integration *to* this firmware. Their HA automations are pinned to entity ids like `binary_sensor.<gateway>_supports_hot_water`, but this firmware exposes that same boiler bit as `binary_sensor.<gateway>_dhw_present` — every automation referencing one of the 11 affected bits has to be rewritten by hand. There is no in-firmware mechanism to ease that migration today.

This ADR is the sibling of dev's **ADR-077**, applied to the `feature-dev-2.0.0-otgw32-esp32-sat-support` branch. The architectural decision is identical; the implementation differs because the 2.0.0 file layout and struct shape have diverged from dev:

- The HA discovery code lives in `MQTTHaDiscovery.cpp` on 2.0.0 (renamed from dev's `mqtt_configuratie.cpp`).
- `MqttHaBinSensorCfg` (`MQTTstuff.h:277-287`) has two additional fields on 2.0.0: `PGM_P deviceClass` and `HaBinaryPayload payload`. None of the existing 15 capability-flag rows override either; the alias rows added by this ADR also omit both, defaulting `deviceClass` to `nullptr` (no `device_class` emitted) and `payload` to `HaBinaryPayload::on_off = 0` (no `payload_on`/`payload_off` emitted; HA's defaults `"ON"`/`"OFF"` apply, which match the publisher output).
- The discovery composer is `composeBinSensorPayload()` at `MQTTHaDiscovery.cpp:2252` and the streaming function `streamBinarySensorDiscovery()` at `MQTTHaDiscovery.cpp:2429`. Both have additional source-separation logic from ADR-098 (the 2.0.0 equivalent of dev's ADR-070).

The mapping (extracted from HA-core's `homeassistant/components/opentherm_gw/binary_sensor.py` against the dev `branch`):

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

The firmware also publishes four capability bits that HA-core's `opentherm_gw` integration does *not* expose. These get HA-core-style `supports_*` aliases coined for consistency (no upstream name to align with, so the name is chosen by this ADR):

| MsgID | Current firmware label | Proposed alias | Rationale |
|---|---|---|---|
| 2 (bit 0) | `master_configuration_smart_power` | `supports_master_smart_power` | MsgID 2 HB bit 0: master-side smart-power capability. The `master_` qualifier keeps the source side (thermostat vs boiler) explicit in the entity name. |
| 3 (bit 7) | `heat_cool_mode_control` | `supports_heat_cool_mode_control` | MsgID 3 HB bit 7: slave-side heat/cool-mode-control capability. Full bit name preserved under the `supports_*` prefix because there is no natural shorter form. |
| 74 (bit 1) | `vh_configuration_bypass` | `supports_ventilation_bypass` | MsgID 74 HB bit 1: ventilation/heat-recovery unit bypass capability. Full `ventilation_*` word preferred over the firmware's `vh_*` abbreviation for self-descriptiveness. |
| 74 (bit 2) | `vh_configuration_speed_control` | `supports_ventilation_speed_control` | MsgID 74 HB bit 2: ventilation/heat-recovery unit speed-control capability. |

This brings the total alias set to **15** — every capability-flag binary_sensor the firmware publishes gets a self-describing alias under the same toggle.

### Firmware-extras intentionally NOT aliased

The firmware publishes additional binary_sensors that are *also* firmware-extras (HA-core has no equivalent) but do **not** fit the `supports_*` capability-bit pattern. They are intentionally excluded from this ADR's alias scheme:

| MsgID | Firmware label | Why not aliased |
|---|---|---|
| 5 (bit 1) | `lockout_reset` | Control bit (asserts a reset action), not a capability indicator. |
| 70 (all 10 entries) | `vh_bypass_automatic_status`, `vh_bypass_mode`, `vh_bypass_position`, `vh_bypass_status`, `vh_diagnostic_indicator`, `vh_fault`, `vh_free_ventilation_mode`, `vh_free_ventliation_status`, `vh_ventilation_enabled`, `vh_ventilation_mode` | All represent runtime *state*, *mode*, or *fault*, not capability. A `supports_*` prefix would mislead. |
| 74 (bit 0) | `vh_configuration_system_type` | Type / enum-value indicator (system type 0 vs 1), not a capability ("does it support X?"). Aliasing as `supports_*` would misrepresent the semantics. |
| 101 | `solar_storage_slave_fault_indicator` | Fault state, not capability. |
| 113 | `solar_storage_system_type` | Type / enum-value indicator, not capability. |

Aliasing these under `supports_*` would conflate semantics and confuse users. They remain published under their current names only; if any are later judged to deserve self-describing aliases under a different scheme (e.g. `*_active`, `*_fault`, `*_type`), a separate ADR can address them.

### Constraints

- **No breaking renames.** ADR-098 (sibling-suffix source-separated discovery) is accepted on this branch; the 15 existing capability-flag labels are part of the contract with alpha-channel deployments. The firmware must keep publishing all current labels untouched.
- **ESP8266 + ESP32-S3 memory.** Adding 15 PROGMEM label strings + 15 friendly-name strings + 15 struct rows costs roughly 1.2 KB of flash on both targets. RAM cost at rest is 1 bit in the settings struct.
- **Broker hygiene.** When the setting is *off*, the broker must not see any alias discovery configs or value publishes. When toggled *on*, broker sees 15 new retained discovery configs.
- **HA-discovery contract.** The new entries must conform to `MqttHaBinSensorCfg`'s 9-field shape on this branch. The alias rows use positional initialisation with the same 7 fields the existing capability-flag rows use; `deviceClass` and `payload` fall through to their default-initialised values, exactly as the existing entries do.
- **Default off.** Existing alpha-channel users who do not toggle the setting see no behavioural change.

## Decision

Add a single bool to the persisted MQTT settings sub-section and a corresponding flag bit to `MqttHaBinSensorCfg`:

1. **Settings field** — add `bool bPublishHaCoreAliases;` (default `false`) to the `mqtt` sub-section of `OTGWSettings` (parallel to the existing `bHaRebootDetect`, `bSeparateSources`, `bDiscoveryAutoVerify`). Serialise to LittleFS JSON via the existing settings reader/writer. Expose a web UI toggle on the MQTT settings page.

2. **Flag bit** — define `#define MQTT_HA_FLAG_IS_HA_CORE_ALIAS 0x10` in `MQTTstuff.h`. The existing flags use the 0x01..0x08 range; 0x10 is the next unused bit. (Confirm against the canonical flag list at the time of implementation in case 2.0.0 has consumed an additional bit since this ADR was drafted.)

3. **Discovery table** — add 15 new rows to `mqttHaBinSensors[]` in `MQTTHaDiscovery.cpp:1250`, each carrying the new flag bit. The MsgID-2/3/6 aliases append after the MsgID-2/3/6 rows; the MsgID-74 ventilation aliases append after the MsgID-74 rows. Each new row mirrors the icon and entity_category of its current-name sibling. The alias rows omit `deviceClass` and `payload` (positional initialiser stops at the 7th field, same as the existing rows do). The four firmware-extra aliases (`supports_master_smart_power`, `supports_heat_cool_mode_control`, `supports_ventilation_bypass`, `supports_ventilation_speed_control`) inherit the `mdi:checkbox-marked-circle` icon and `diagnostic` category of their current-name siblings.

4. **Discovery gating** — `streamBinarySensorDiscovery()` checks `settings.mqtt.bPublishHaCoreAliases` before publishing any row whose `cfg.flags & MQTT_HA_FLAG_IS_HA_CORE_ALIAS` is set. When the setting is off, alias rows are silently skipped. The check sits alongside the existing source-separation predicate from ADR-098; the alias check applies regardless of source variant (the canonical no-source shape is used for all 15 alias topics, same as the existing capability flags).

5. **Publisher gating** — each of the 15 affected `sendMQTTData(F("<current_label>"), val)` call sites — 1 in `print_mastermemberid()` (MsgID 2), 8 in `print_slavememberid()` (MsgID 3), 4 in `publishRBPFlagsState()` (MsgID 6), and 2 in the MsgID-74 publisher block at `OTGW-Core.ino:2372-2373` (`vh_configuration_bypass` / `vh_configuration_speed_control`; `vh_configuration_system_type` at line 2371 is skipped as a non-capability type indicator), all in `OTGW-Core.ino` — gains a guarded second call:
   ```cpp
   sendMQTTData(F("dhw_present"), val);
   if (settings.mqtt.bPublishHaCoreAliases) {
       sendMQTTData(F("supports_hot_water"), val);
   }
   ```
   Order: current-label publish first, alias publish second. The current-label path is *never* affected by the setting. The MQTT source-separation layer (ADR-098) wraps both publishes uniformly — if `settings.mqtt.bSeparateSources` is also on, alias topics get the same sibling-suffix treatment as current-label topics.

6. **No cleanup on toggle-off.** When the user disables the setting, the firmware stops publishing alias discovery and alias state, but does not actively deregister the entities. Retained discovery configs and the last retained state values stay in the broker until manually purged. This is the conscious trade-off; see Alternatives.

7. **Scope is all 15 capability-flag bits the firmware publishes** across MsgID 2 / 3 / 6 / 74. Eleven aliases adopt HA-core's translation_key verbatim for migration continuity; the four firmware-extras (`master_configuration_smart_power`, `heat_cool_mode_control`, `vh_configuration_bypass`, `vh_configuration_speed_control`) get HA-core-style `supports_*` aliases coined in this ADR. Naming the firmware-extras under the same `supports_*` convention keeps the entity-id namespace coherent — a user enabling the toggle sees a uniformly-named `binary_sensor.<gw>_supports_*` set rather than a mix of `supports_*` and `master_configuration_smart_power`. Five other firmware-only binary_sensors (MsgID 5 `lockout_reset`, all 10 MsgID 70 ventilation status/mode/fault entries, MsgID 74 `vh_configuration_system_type`, MsgID 101 solar fault, MsgID 113 solar system type) are explicitly *not* aliased — see "Firmware-extras intentionally NOT aliased" above.

### Why this choice

- **Minimal migration friction.** Users moving from the HA-core integration to this firmware can flip one toggle and have their automations work unchanged for the 11 HA-core-matched capability bits.
- **Coherent naming across the full set.** The four firmware-extras pick up `supports_master_smart_power`, `supports_heat_cool_mode_control`, `supports_ventilation_bypass`, and `supports_ventilation_speed_control` aliases under the same toggle, so the user-facing namespace under toggle-on is uniformly `supports_*` — no one-off naming exceptions.
- **Symmetric with dev (ADR-077).** Same architectural decision on both branches; users with mixed deployments (some on dev, some on 2.0.0) get identical alias behaviour on both.
- **No risk to existing users.** Default-off means the contract for installed deployments is unchanged.
- **Confined surface area.** Additive only: one settings bool, one flag bit, 15 new discovery rows, 15 guarded second-publish call sites.
- **Reuses the existing discovery flag-bit pattern.** `MQTT_HA_FLAG_IS_PIC_ENTRY` already gates topic-prefixing via a flag bit on the same struct; this ADR follows the same shape.

## Alternatives Considered

### Alternative 1 — Rename the 15 existing labels to the new alias names

Drop `dhw_present` and publish `supports_hot_water` only; do the same for all 15 bits, including the firmware-extras.

**Rejected:** breaks every existing user's automations referring to one of the 15 topics. The 2.0.0 branch is in alpha and has fewer deployments than dev, but rename churn this close to a stable 2.0.0 release would burn early-adopter trust. Documentation alone cannot reach every deployment.

### Alternative 2 — Documentation-only mapping (extend the TASK-650 audit doc)

Publish nothing new. Extend `docs/audits/2026-05-21-ha-capability-flags-feature-2.0.0.md` into a complete name-mapping reference.

**Rejected:** does not solve the migration problem (every automation still needs hand-rewriting). The complete mapping should land alongside this ADR — documentation and runtime feature are complementary, not alternatives.

### Alternative 3 — Always-on aliases, no setting

Skip the setting entirely; always publish both names. Cuts ~15 lines.

**Rejected:** permanently doubles the discovery footprint and per-update MQTT chatter for these 15 bits for every deployment. ESP8266 broker payload budget and HA entity registry are both finite. The 1-bit settings field is cheap.

### Alternative 4 — Default-on aliases with the setting present

Setting defaults to `true`; users can opt out.

**Rejected:** silently introduces 15 new HA entities on the next firmware upgrade for every existing deployment. Default-off is strictly safer.

### Alternative 5 — Clean discovery deregistration on toggle-off

Same as Decision, but on toggle-off, publish empty retained payloads to all 15 alias discovery topics so HA removes the entities cleanly.

**Rejected for v1** (may be revisited): adds ~50 lines of cleanup machinery for a corner case. Users toggling off after toggling on can manually purge retained topics with `mosquitto_pub -n -r`. If field testing on the alpha channel surfaces this as a frequent operation, a future ADR can add automated cleanup.

### Alternative 6 — Alias only the 11 HA-core-matched bits; leave the 4 firmware-extras alone

Skip the firmware-extras (`master_configuration_smart_power`, `heat_cool_mode_control`, `vh_configuration_bypass`, `vh_configuration_speed_control`); only alias the 11 bits that have an HA-core counterpart. Saves 4 PROGMEM rows.

**Rejected:** mixes naming conventions inside one logical group. A user enabling the toggle sees `binary_sensor.<gw>_supports_hot_water` alongside `binary_sensor.<gw>_master_configuration_smart_power`, `binary_sensor.<gw>_vh_configuration_bypass`, etc. — multiple styles in one capability-flag set, harder to scan. The flash and bandwidth cost of four extra aliases is negligible; coherent naming is worth more.

### Alternative 7 — Wait until v2.1.0's three-device split lands, then design aliases on top of it

TASK-648 (deferred to v2.1.0) splits HA discovery into boiler/gateway/thermostat devices to match HA core's 2024.10 layout. The aliases would be more natural in that world (HA-core's `supports_hot_water` is published on the boiler device, not the gateway).

**Rejected:** the user-visible benefit of aliases is independent of which HA device they appear under. The 15 aliases can land now on the existing gateway-only device topology and be re-parented to the boiler device in v2.1.0 along with the rest of the discovery split (TASK-648 already covers re-parenting; this ADR's aliases ride along).

## Consequences

### Positive

- HA-core integration users can migrate to this firmware without rewriting any of their 11-bit capability-flag automations (the 11 HA-core-matched aliases).
- The four firmware-extras gain `supports_master_smart_power`, `supports_heat_cool_mode_control`, `supports_ventilation_bypass`, and `supports_ventilation_speed_control` aliases, so the toggle-on experience is uniformly `supports_*`-prefixed across all 15 capability bits — easier for users to discover and reason about.
- The 11 HA-core names are more self-descriptive than the existing OT-spec-derived names (`supports_central_heating_setpoint_transfer` vs `rbp_max_ch_setpoint`); new users discovering this firmware also benefit.
- The settings field gives a clean opt-in story.
- Symmetric with dev ADR-077 — single mental model across both maintenance lines.

### Negative

- When the setting is on, broker discovery payload count for capability flags doubles (from 15 to 30 retained discovery configs across MsgID 2 / 3 / 6 / 74), and per-update MQTT publish volume for those frames doubles for the affected bits.
- When the setting is *also* used with `settings.mqtt.bSeparateSources=true` (ADR-098), the broker sees source-separated *and* aliased entries — boiler-source and thermostat-source aliases. Multiplicatively larger discovery footprint; documented but not prevented by the design.
- On toggle-off, retained alias discovery configs and state values linger in the broker until manually purged.
- The new flag bit (`MQTT_HA_FLAG_IS_HA_CORE_ALIAS = 0x10`) consumes one of the eight available bits in `MqttHaBinSensorCfg::flags`.
- A future HA-core rename of a translation_key would require a firmware change to track.
- The four firmware-extra aliases (`supports_master_smart_power`, `supports_heat_cool_mode_control`, `supports_ventilation_bypass`, `supports_ventilation_speed_control`) are *coined* by this firmware, not by HA-core. If HA-core later adds these bits with a different name, the firmware aliases will diverge from HA-core's canonical names. The fix at that point would be either (a) accept the divergence, or (b) add a *third* alias matching HA-core's new name (handled by another small ADR; this one does not pre-decide).

### Risk

The risk to the existing fan-out path is low because the changes are additive. The new alias publish is conditional and skipped entirely when the setting is off. The discovery flag bit is checked once per discovery iteration. Both ESP8266 and ESP32-S3 targets receive the same code; the flash/RAM cost is identical on both.

## Related Decisions

- **ADR-052** — MQTT publish eligibility contract.
- **ADR-065** — `MQTT_HA_FLAG_IS_PIC_ENTRY` flag-bit-based discovery customisation. This ADR follows the same pattern.
- **ADR-098** — Sibling-suffix source-separated discovery (the 2.0.0 evolution of dev's ADR-070, after the failed nested ADR-097 form was superseded). Aliases inherit the same source-separation treatment when `bSeparateSources` is on.
- **ADR-074** — Retained availability heartbeat. Alias entities inherit the same availability topic.
- **ADR-103** / **ADR-104** — Proxy-answer routing and pending-slot scoping. Out of scope for this ADR; alias publishers sit on the same `sendMQTTData()` path the existing capability-flag publishers use.
- **ADR-077** — sibling on the `dev` branch. Same decision, simpler struct shape (no `deviceClass` / `payload` fields), file at `mqtt_configuratie.cpp`.
- **TASK-648** — v2.1.0 three-device discovery split. Future ADR (v2.1.0 milestone) will re-parent these aliases to the boiler device alongside the rest of the capability-flag entities.

## References

- `docs/audits/2026-05-21-ha-capability-flags-feature-2.0.0.md` — TASK-650 audit confirming semantic parity for the 13 capability flags on this branch.
- `src/OTGW-firmware/MQTTHaDiscovery.cpp:1250` — current `mqttHaBinSensors[]` array; alias rows append here.
- `src/OTGW-firmware/MQTTHaDiscovery.cpp:2252` — `composeBinSensorPayload()` (the discovery composer; unchanged by this ADR).
- `src/OTGW-firmware/MQTTHaDiscovery.cpp:2429` — `streamBinarySensorDiscovery()` (gains the new flag-bit check).
- `src/OTGW-firmware/MQTTstuff.h:277` — `MqttHaBinSensorCfg` struct (9 fields including `deviceClass` and `payload`; alias rows positional-initialise only the first 7 like the existing rows do).
- `src/OTGW-firmware/MQTTstuff.h:220` — `HaBinaryPayload` enum; `on_off = 0` is the default-initialised value used by all existing + 15 alias rows.
- `src/OTGW-firmware/OTGW-Core.ino:2284` — `print_slavememberid()` MsgID-3 publishers (8 sites gain the guarded second call, including `heat_cool_mode_control` → `supports_heat_cool_mode_control`).
- `src/OTGW-firmware/OTGW-Core.ino:1945` — `publishRBPFlagsState()` MsgID-6 publishers (4 sites gain the guarded second call).
- `src/OTGW-firmware/OTGW-Core.ino` — `print_mastermemberid()` MsgID-2 publisher (1 site gains the guarded second call: `master_configuration_smart_power` → `supports_master_smart_power`).
- `src/OTGW-firmware/OTGW-Core.ino:2372-2373` — MsgID-74 publishers for `vh_configuration_bypass` → `supports_ventilation_bypass` and `vh_configuration_speed_control` → `supports_ventilation_speed_control`. The sibling `vh_configuration_system_type` publisher at line 2371 is *not* aliased (type/enum indicator).
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

The Decision is structural (settings field + flag bit + guarded publishes) and not easily encoded as a regex. Sonnet judge in the pre-commit hook reviews any diff touching `MQTTHaDiscovery.cpp`, `OTGW-Core.ino`, `OTGWSettings.h`, or `MQTTstuff.h` against this ADR's Decision text to catch: (a) alias publishes that are not guarded by `settings.mqtt.bPublishHaCoreAliases`, (b) alias rows in `mqttHaBinSensors[]` that omit `MQTT_HA_FLAG_IS_HA_CORE_ALIAS`, (c) renames of any of the 15 current labels (which would be a breaking change), (d) alias rows that incorrectly override `deviceClass` or `payload` (capability bits should keep HA-core's defaulted-None / defaulted-on_off semantics), (e) coining a new firmware-extra alias outside the `supports_*` convention, (f) accidentally adding aliases for state/mode/fault/type bits that this ADR explicitly excludes.
