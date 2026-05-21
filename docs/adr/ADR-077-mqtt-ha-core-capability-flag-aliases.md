# ADR-077: MQTT — Publish HA-Core-Style Self-Describing Aliases for Capability, Type, and State Bits under `settings.mqtt.bPublishHaCoreAliases`

## Status

Proposed

## Context

TASK-649 confirmed semantic parity between this firmware's 13 binary_sensors for OpenTherm MsgID 2 / 3 / 6 (capability flags) and Home Assistant core's `opentherm_gw` integration. A follow-up spec audit (using `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md`) extended scope to all firmware-published bit-flag binary_sensors on MsgID 2, 3, 5, 6, 70, 74, and 101, then classified each by spec-defined semantics.

The user-visible problem is twofold:

1. **Migration friction.** Users moving from HA-core's native `opentherm_gw` integration *to* this firmware see their automations break: they reference entity ids like `binary_sensor.<gw>_supports_hot_water`, but the firmware publishes the same bit as `binary_sensor.<gw>_dhw_present`. Every automation referencing one of the affected bits needs hand-rewriting.

2. **Opaque names.** Several firmware-only labels use OT-spec-derived abbreviations or jargon (`vh_*` for ventilation/heat-recovery, `rbp_*` for Remote Boiler Parameters, `master_configuration_smart_power`, `vh_configuration_bypass`). A user browsing HA's entity registry has no way to know what these mean without consulting the OT spec.

The audit identifies **three distinct alias categories**, each with its own naming convention dictated by what the bit *means* per v4.2:

- **A. Capability bits (`supports_*`)** — the bit answers "does the device have capability X?", with bit=1 meaning "yes, capability is present". Example: MsgID 3 HB0 `dhw_present` → `[dhw not present, dhw is present]` → alias `supports_hot_water`.
- **B. Type / config indicators (plain name)** — the bit distinguishes between two variants of a feature, both of which exist. Aliasing as `supports_*` would mislead because bit=0 doesn't mean "absent". Example: MsgID 3 HB1 `control_type_modulation` → `[modulating, on/off]` → alias `control_type` (HA-core's name).
- **C. State / mode / fault rewrites (plain name)** — the bit reports a runtime state, not a capability. Aliased only for clarity (eliminate `vh_*` jargon). Example: MsgID 70 HB1 `vh_bypass_position` → spec name `Bypass position` → alias `ventilation_bypass_position`.

### v4.2 spec evidence

The categorisation was driven by `OpenTherm Protocol Specification v4.2 §5.3.x`, specifically:

- MsgID 3 HB7 "Heat/cool mode control [Heat/cool mode switching can be done by master, Heat/cool mode switching is done by slave]" — control-locus indicator, *not* a capability. (Both values mean it *is* supported; the bit says *who* does the switching.)
- MsgID 5 LB1 "Lockout-reset [remote reset disabled, rr enabled]" — true capability bit. Note: HA-core uses `DATA_SLAVE_REMOTE_RESET` for the *MsgID 3 bit 6* (which spec calls `Remote water filling function`), so HA-core's `supports_remote_reset` is a mislabel of the water-filling bit; the actual remote-reset bit is here on MsgID 5 LB1.
- MsgID 74 HB2 "Speed control [3-speed, variable]" — type indicator, *not* a capability. (Both values support speed control; the bit distinguishes 3-speed vs variable.)
- MsgID 74 HB0 "System type [0=central exhaust ventilation, 1=heat-recovery ventilation]" — type indicator.

### Mapping table — Category A: Capability aliases (`supports_*`)

13 bits, of which 9 adopt HA-core's translation_key verbatim for migration continuity and 4 are firmware-coined:

| MsgID | Bit | Current firmware label | Alias | Source |
|---|---|---|---|---|
| 2 | HB0 | `master_configuration_smart_power` | `supports_master_smart_power` | coined (no HA-core) |
| 3 | HB0 | `dhw_present` | `supports_hot_water` | HA-core verbatim |
| 3 | HB2 | `cooling_config` | `supports_cooling` | HA-core verbatim |
| 3 | HB4 | `master_low_off_pump_control_function` | `supports_pump_control` | HA-core verbatim |
| 3 | HB5 | `ch2_present` | `supports_ch_2` | HA-core verbatim |
| 3 | HB6 | `remote_water_filling_function` | `supports_remote_reset` | HA-core verbatim (HA-core mislabels this bit; preserving HA-core's name keeps migration continuity) |
| 5 | LB1 | `lockout_reset` | `supports_lockout_reset` | coined (no HA-core; spec calls this the true remote-reset capability) |
| 6 | HB0 | `rbp_dhw_setpoint` | `supports_hot_water_setpoint_transfer` | HA-core verbatim |
| 6 | HB1 | `rbp_max_ch_setpoint` | `supports_central_heating_setpoint_transfer` | HA-core verbatim |
| 6 | LB0 | `rbp_rw_dhw_setpoint` | `supports_hot_water_setpoint_writing` | HA-core verbatim |
| 6 | LB1 | `rbp_rw_max_ch_setpoint` | `supports_central_heating_setpoint_writing` | HA-core verbatim |
| 74 | HB1 | `vh_configuration_bypass` | `supports_ventilation_bypass` | coined (no HA-core; spec `[not present, present]`) |

### Mapping table — Category B: Type / config indicators (plain name)

4 bits where the spec defines distinct variants on both sides of the bit; `supports_*` would mislead:

| MsgID | Bit | Current firmware label | Alias | Spec value-set |
|---|---|---|---|---|
| 3 | HB1 | `control_type_modulation` | `control_type` | `[modulating, on/off]` |
| 3 | HB3 | `dhw_config` | `hot_water_config` | `[instantaneous or not-specified, storage tank]` |
| 74 | HB0 | `vh_configuration_system_type` | `ventilation_system_type` | `[central exhaust ventilation, heat-recovery ventilation]` |
| 74 | HB2 | `vh_configuration_speed_control` | `ventilation_speed_control_type` | `[3-speed, variable]` |

### Mapping table — Category C: Clarity rewrites for state / mode / fault bits (plain name)

11 firmware-only bits where the current label uses an opaque prefix (`vh_*`, redundant `_slave_indicator`). Aliases keep the spec's own wording (`ventilation_*`, `solar_storage_*`) but in full words:

| MsgID | Bit | Current firmware label | Alias | Spec name |
|---|---|---|---|---|
| 70 | HB0 | `vh_ventilation_enabled` | `ventilation_enabled` | Ventilation enable |
| 70 | HB1 | `vh_bypass_position` | `ventilation_bypass_position` | Bypass position (only bypass manual mode) |
| 70 | HB2 | `vh_bypass_mode` | `ventilation_bypass_mode` | Bypass mode |
| 70 | HB3 | `vh_free_ventilation_mode` | `ventilation_free_mode` | Free ventilation mode |
| 70 | LB0 | `vh_fault` | `ventilation_fault` | Fault indication |
| 70 | LB1 | `vh_ventilation_mode` | `ventilation_active` | Ventilation mode `[not active, active]` — value-set name preferred over spec name "mode" since the bit is boolean active/inactive, not an enum |
| 70 | LB2 | `vh_bypass_status` | `ventilation_bypass_status` | Bypass status |
| 70 | LB3 | `vh_bypass_automatic_status` | `ventilation_bypass_automatic` | Bypass automatic status |
| 70 | LB4 | `vh_free_ventliation_status` | `ventilation_free_status` | Free ventilation status — *also* corrects the typo `ventliation` → `ventilation` non-breakingly |
| 70 | LB6 | `vh_diagnostic_indicator` | `ventilation_diagnostic` | Diagnostic indication |
| 101 | LB0 | `solar_storage_slave_fault_indicator` | `solar_storage_fault` | "Bit 0: fault indication" — drops the redundant `_slave_indicator` qualifier |

### Bits explicitly NOT aliased

| MsgID | Bit | Current firmware label | Why no alias |
|---|---|---|---|
| 3 | HB7 | `heat_cool_mode_control` | Spec value-set is `[switching by master, switching by slave]` — neither variant means "doesn't support". Current name is already clear; aliasing would add no information. |
| 113 | – | `solar_storage_system_type` | Type indicator with spec value-set `[DHW preheat system, DHW parallel system]`. Current name is already self-describing. |

### Out of scope for this ADR (future expansion candidates surfaced by the audit)

The audit found additional name divergences that are *not* addressed here. Each would expand scope beyond capability/type/clarity and is left for a separate ADR if needed:

- **MsgID 5 fault-name HA-core divergences** — `service_request` vs HA-core `service_required`, `gas_flame_fault` vs `gas_fault`, `water_over_temperature` vs `water_overtemperature`. Three aliases for HA-core compat on fault bits.
- **MsgID 100 override-priority HA-core divergences** — `remote_override_manual_change_priority` vs HA-core `override_manual_change_prio`, similar for `_program_`. Two aliases on state bits.
- **MsgID 0 status-bit HA-core divergences** — `central_heating`, `cooling`, `flame`, `dhw_enable`, etc. all have HA-core counterparts with different translation_keys. Around ten aliases on the primary status fan-out; would significantly expand discovery footprint when toggled.

These are flagged here so a future maintainer doesn't have to re-do the audit.

### Constraints

- **No breaking renames.** ADR-070 (sibling-suffix source-separated discovery) is already accepted; the existing labels are part of users' live automations across thousands of deployments. The firmware must keep publishing all current labels untouched.
- **ESP8266 memory.** Adding 28 PROGMEM label strings + 28 friendly-name strings + 28 struct rows costs roughly 2.2 KB of flash (PROGMEM) and 0 bytes of RAM at rest. The active discovery + publish path adds 28 extra `sendMQTTData()` calls (per MsgID 2 / 3 / 5 / 6 / 70 / 74 / 101 update) when the setting is on.
- **Broker hygiene.** When the setting is *off*, the broker must not see any alias discovery configs or value publishes. When toggled *on*, broker sees 28 new retained discovery configs.
- **HA-discovery contract.** The new entries must conform to the existing `MqttHaBinSensorCfg` shape (the dev struct has no `deviceClass` or `payload` fields; the composer emits no `device_class` and relies on HA's default `"ON"`/`"OFF"` payloads — same as the existing entries).
- **Default off.** Existing users who do not toggle the setting see no behavioural change at all.

## Decision

Add a single bool to the persisted MQTT settings sub-section and a corresponding flag bit to `MqttHaBinSensorCfg`:

1. **Settings field** — add `bool bPublishHaCoreAliases;` (default `false`) to the `mqtt` sub-section of `OTGWSettings` (parallel to `bHaRebootDetect`, `bSeparateSources`, `bDiscoveryAutoVerify`). Serialise to LittleFS JSON via the existing `settingStuff.ino` reader/writer. Expose a web UI toggle on the MQTT settings page.

2. **Flag bit** — define `#define MQTT_HA_FLAG_IS_HA_CORE_ALIAS 0x10` in `MQTTstuff.h` (the existing flags use the 0x01..0x08 range; 0x10 is the next unused bit).

3. **Discovery table** — add 28 new rows to `mqttHaBinSensors[]` in `mqtt_configuratie.cpp`, each carrying the new flag bit, appended after the corresponding MsgID's existing rows. Each new row mirrors the icon and entity_category of its current-name sibling. The friendly-name PROGMEM string follows the same `Title_Case` convention used elsewhere in this array.

4. **Discovery gating** — `streamBinarySensorDiscovery()` checks `settings.mqtt.bPublishHaCoreAliases` before publishing any row whose `cfg.flags & MQTT_HA_FLAG_IS_HA_CORE_ALIAS` is set. When the setting is off, alias rows are silently skipped; the iteration index still advances. No discovery topic is published, no retained config lands on the broker.

5. **Publisher gating** — each of the 28 affected `sendMQTTData(F("<current_label>"), val)` call sites — 1 in `print_mastermemberid()` (MsgID 2), 7 in `print_slavememberid()` (MsgID 3; skipping HB7), 1 in `print_asfflags()` / equivalent (MsgID 5 LB1), 4 in `publishRBPFlagsState()` (MsgID 6), 10 in the MsgID-70 publishers `publishMasterStatusVHState()` + `publishSlaveStatusVHState()`, 2 in the MsgID-74 publisher block (`vh_configuration_system_type` and `_speed_control`; bypass already covered in Category A), 1 in the MsgID-101 publisher — gains a guarded second call:
   ```cpp
   sendMQTTData(F("dhw_present"), val);
   if (settings.mqtt.bPublishHaCoreAliases) {
       sendMQTTData(F("supports_hot_water"), val);
   }
   ```
   Order: current-label publish first, alias publish second. The current-label path is *never* affected by the setting.

6. **No cleanup on toggle-off.** When the user disables the setting, the firmware stops publishing alias discovery and alias state, but does not actively deregister the entities. Retained discovery configs and the last retained state values stay in the broker until manually purged (e.g. via `mosquitto_pub -t .../config -n -r`). This is the conscious trade-off; see Alternatives.

7. **Naming-convention rule baked into the ADR.** Future maintainers adding new binary_sensors must classify them per the three categories:
   - True yes/no capability with bit=0 meaning "feature absent" → `supports_*` alias.
   - Two-variant type/config indicator → plain spec-derived name, no `supports_*` prefix.
   - State / mode / fault — alias *only* if the current name uses an opaque prefix worth rewriting; otherwise leave it.

### Why this choice

- **Minimal migration friction.** Users moving from the HA-core integration to this firmware can flip one toggle and have their automations work unchanged for the 9 HA-core-matched capability bits and 2 HA-core-matched type indicators (11 of 13 audited).
- **Coherent naming.** All capability bits use `supports_*` consistently; all type/state bits use plain spec-faithful names. No one-off naming exceptions.
- **Spec-correct categorisation.** Bits are aliased according to what the v4.2 spec actually says they mean, not based on superficial similarity to other capability bits. The audit catches three misclassifications the original 11-row scope would have shipped.
- **No risk to existing users.** Default-off means the contract for installed deployments is unchanged. A user who never toggles the setting cannot notice the new code path.
- **Confined surface area.** Additive: one settings bool, one flag bit, 28 new discovery rows, 28 guarded second-publish call sites. No existing data structures are reshaped. No existing topic loses its name.
- **Reuses the existing discovery flag-bit pattern.** `MQTT_HA_FLAG_IS_PIC_ENTRY` (ADR-065) already gates topic prefixing via a flag bit on the same struct; this ADR follows the same shape.

## Alternatives Considered

### Alternative 1 — Rename the 28 existing labels to the new alias names

Drop `dhw_present` and publish `supports_hot_water` only; do the same for all 28 bits.

**Rejected:** breaks every existing user's automations referring to one of the 28 topics. ADR-070's source-separated layer is already widely deployed; another forced rename would burn user trust. Documentation alone cannot reach every deployment.

### Alternative 2 — Documentation-only mapping (extend the TASK-649 audit doc)

Publish nothing new. Extend `docs/audits/2026-05-21-ha-capability-flags-dev.md` into a complete name-mapping reference so users can rewrite their automations manually.

**Rejected:** does not solve the migration problem (every automation referencing an affected bit still has to be hand-rewritten). The complete mapping should land alongside this ADR — documentation and runtime feature are complementary, not alternatives.

### Alternative 3 — Always-on aliases, no setting

Skip the setting entirely; always publish both names. Cuts ~15 lines of code.

**Rejected:** permanently doubles the discovery footprint and per-update MQTT chatter for these 28 bits for every deployment, even those that will never use the HA-core names. ESP8266 broker payload budget and HA entity registry are both finite. The 1-bit settings field is a cheap toggle.

### Alternative 4 — Default-on aliases with the setting present

Setting defaults to `true`; users can opt out.

**Rejected:** silently introduces 28 new HA entities on the next firmware upgrade for every existing deployment. Existing users would discover unexplained `binary_sensor.*_supports_*` / `binary_sensor.*_ventilation_*` entries appearing in HA without context. Default-off is strictly safer; new users can flip the toggle once they understand what it does.

### Alternative 5 — Clean discovery deregistration on toggle-off

Same as Decision, but on toggle-off, publish empty retained payloads to all 28 alias discovery topics so HA removes the entities cleanly.

**Rejected for v1** (may be revisited in a follow-up ADR): adds ~50 lines of cleanup machinery (alias label registry, dispatcher on settings-change, retry on MQTT reconnect) for a corner case. Users toggling off after toggling on can manually purge retained topics with `mosquitto_pub -n -r`. If field testing shows this is a frequent operation, a future ADR can add the automated cleanup.

### Alternative 6 — Alias only the 11 HA-core-matched bits (Category A subset + Category B HA-core entries)

Skip the firmware-coined supports_* (smart_power, lockout_reset, ventilation_bypass), the firmware-coined type indicators (ventilation_system_type, ventilation_speed_control_type), and all of Category C. Cuts to 11 aliases.

**Rejected:** mixes naming conventions inside one logical group. A user enabling the toggle sees the 11 HA-core-matched aliases alongside `binary_sensor.<gw>_master_configuration_smart_power`, `binary_sensor.<gw>_vh_configuration_bypass`, etc. — multiple styles in one capability-flag set, harder to scan. The flash and bandwidth cost of the additional aliases is negligible; coherent naming is worth more.

### Alternative 7 — Split into three ADRs, one per category

Author ADR-077 for capability-only, ADR-N for type indicators, ADR-M for clarity rewrites. Each gets its own toggle and own controversy budget.

**Rejected:** the three categories share a single user-visible mechanism (one MQTT topic per alias, one HA discovery entry, one publisher gating pattern). Splitting the ADR would force the user to flip three settings to get coherent behaviour and would triple the maintenance surface. Three categories under one ADR with one toggle is the natural grouping.

## Consequences

### Positive

- HA-core integration users can migrate to this firmware without rewriting any of their 11 HA-core-matched capability + type-indicator automations.
- Three additional `supports_*` aliases (smart_power, lockout_reset, ventilation_bypass) bring full HA-core-style coverage of the firmware's true capability bits.
- All ventilation/heat-recovery state/mode/fault bits get self-describing `ventilation_*` aliases under the toggle — no more `vh_*` jargon for users who opt in.
- The solar fault entity gets a clean `solar_storage_fault` alias dropping the redundant `_slave_indicator`.
- Spec-correct categorisation is baked into the ADR: capability vs type vs state. Future binary_sensors get classified the same way.
- The 28-row mapping is codified in firmware source, not just documentation footnotes — easier for future maintainers to audit against new HA-core releases.

### Negative

- When the setting is on, broker discovery payload count for the 28 affected bits doubles (from 28 to 56 retained discovery configs across MsgID 2 / 3 / 5 / 6 / 70 / 74 / 101), and per-update MQTT publish volume for those frames doubles for the affected bits.
- On toggle-off, retained discovery configs + last retained state values for the 28 aliases linger in the broker until manually purged. HA will mark the entities `unavailable` once the firmware stops publishing the retained availability heartbeat, but the entities themselves remain in HA's entity registry until the user removes them.
- The four firmware-coined Category-A aliases (`supports_master_smart_power`, `supports_lockout_reset`, `supports_ventilation_bypass`) and two firmware-coined Category-B aliases (`ventilation_system_type`, `ventilation_speed_control_type`) are *coined* by this firmware, not by HA-core. If HA-core later adds these bits with different names, the firmware aliases will diverge. The fix at that point would be either (a) accept the divergence, or (b) add a *third* alias matching HA-core's new name (handled by another small ADR; this one does not pre-decide).
- The `supports_remote_reset` alias preserves HA-core's mislabel of the MsgID 3 bit 6 (`remote_water_filling_function`) for migration continuity, while the *actual* remote-reset capability (MsgID 5 LB1, per spec) gets its own `supports_lockout_reset` alias. This means a user reading both aliases needs to know they refer to *different* spec bits. The mismatch is documented in the audit doc and in this ADR's Context section, but it is a foot-gun for HA-core-naive users.
- The new flag bit (`MQTT_HA_FLAG_IS_HA_CORE_ALIAS`) consumes one of the eight available bits in `MqttHaBinSensorCfg::flags`. Two other bits (`MQTT_HA_FLAG_IS_PIC_ENTRY` = 0x08, plus the existing 0x01/0x02/0x04) are already taken; this leaves four for future use.

### Risk

The risk to the existing fan-out path is low because the changes are additive — the current-label publish is identical to today's behaviour. The new alias publish is conditional and skipped entirely when the setting is off. The discovery flag bit is checked once per discovery iteration; no hot-path impact.

## Related Decisions

- **ADR-052** — MQTT publish eligibility contract (first-seen / value-changed / stale-refresh). The alias publishers reuse this contract verbatim via `sendMQTTData()`.
- **ADR-065** — `MQTT_HA_FLAG_IS_PIC_ENTRY` flag-bit-based discovery customisation. This ADR follows the same pattern.
- **ADR-070** — Sibling-suffix source-separated discovery (the dev equivalent of 2.0.0's ADR-098). Out of scope for this ADR; alias topics use the canonical (no-source) shape, same as the existing 28 binary_sensors being aliased.
- **ADR-074** — Retained availability heartbeat. Alias entities inherit the same availability topic, so HA correctly marks them unavailable when the firmware stops publishing.
- **ADR-076** — MQTT status fan-out per-slot heartbeat. The MsgID 70 alias publishers in Category C ride on the same per-slot gating as their current-name siblings.
- **ADR-105** — sibling on the 2.0.0 feature line (`feature-dev-2.0.0-otgw32-esp32-sat-support`). Same decision, different file layout (`MQTTHaDiscovery.cpp` instead of `mqtt_configuratie.cpp`; `MqttHaBinSensorCfg` has explicit `deviceClass` and `payload` fields that default to `nullptr` / `HaBinaryPayload::on_off`, so the alias rows omit them just like the existing rows do).

## References

- `docs/audits/2026-05-21-ha-capability-flags-dev.md` — TASK-649 audit confirming semantic parity for the 13 MsgID-2/3/6 capability flags.
- `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md` — primary spec source for the audit. Specifically §5.3.1 (Data-IDs 0 / 70 / 101 status), §5.3.2 (Data-IDs 2 / 3 / 74 / 103 configuration), and the Application-specific fault sections (5 / 72 / 102).
- `src/OTGW-firmware/mqtt_configuratie.cpp:1103` — current `mqttHaBinSensors[]` array; alias rows append here.
- `src/OTGW-firmware/MQTTstuff.h:205` — `MqttHaBinSensorCfg` struct (no `deviceClass` field; alias rows match the existing shape).
- `src/OTGW-firmware/OTGW-Core.ino` — publishers:
  - `print_mastermemberid()` MsgID-2 (1 site: `master_configuration_smart_power` → `supports_master_smart_power`).
  - `print_slavememberid()` MsgID-3 (7 sites; `heat_cool_mode_control` is not aliased).
  - `print_asfflags()` / MsgID-5 LB publishers (1 site: `lockout_reset` → `supports_lockout_reset`).
  - `publishRBPFlagsState()` MsgID-6 (4 sites).
  - `publishMasterStatusVHState()` + `publishSlaveStatusVHState()` MsgID-70 (10 sites).
  - MsgID-74 publishers around `OTGW-Core.ino:2371-2373` (3 sites: bypass in Category A, system_type + speed_control in Category B).
  - MsgID-101 publisher (1 site: `solar_storage_slave_fault_indicator` → `solar_storage_fault`).
- Home Assistant core `homeassistant/components/opentherm_gw/binary_sensor.py` — source of the HA-core translation_key names.
- pyotgw `vars.py` — defines the `DATA_SLAVE_*` / `DATA_REMOTE_*` Python constants behind the HA-core entities.

## Enforcement

```json
{
  "forbid_pattern": [],
  "require_pattern": [],
  "llm_judge": true
}
```

The Decision is structural (settings field + flag bit + guarded publishes across three naming categories) and not easily encoded as a regex. Sonnet judge in the pre-commit hook reviews any diff touching `mqtt_configuratie.cpp`, `OTGW-Core.ino`, or `OTGWSettings.h` against this ADR's Decision text to catch:

(a) Alias publishes not guarded by `settings.mqtt.bPublishHaCoreAliases`.
(b) Alias rows in `mqttHaBinSensors[]` that omit `MQTT_HA_FLAG_IS_HA_CORE_ALIAS`.
(c) Renames of any of the 28 current labels (which would be a breaking change).
(d) New `supports_*` aliases coined for non-capability bits (mis-categorisation; e.g. naming a type indicator with the `supports_*` prefix).
(e) New non-capability aliases (Category B / C) given the `supports_*` prefix.
(f) New capability aliases given a plain non-`supports_*` name (the converse miscategorisation).
