# ADR-105: MQTT — Publish HA-Core-Style Self-Describing Aliases for Capability, State, Type, and Fault Bits under `settings.mqtt.bPublishHaCoreAliases`

## Status

Accepted, 2026-05-21.

## Context

TASK-650 confirmed semantic parity between this firmware's 13 binary_sensors for OpenTherm MsgID 2 / 3 / 6 (capability flags) on the 2.0.0 feature line and Home Assistant core's `opentherm_gw` integration. A follow-up audit of every firmware-published bit-flag binary_sensor against `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md` and HA-core's `homeassistant/components/opentherm_gw/binary_sensor.py` extended scope to MsgID 0 (status), 2, 3, 5 (app-specific faults), 6, 70 (VH status), 74 (VH configuration), 100 (remote override priority), and 101 (solar fault). The audit cross-referenced each bit's v4.2 semantics and HA-core translation_key to classify it for the alias scheme.

This ADR is the sibling of dev's **ADR-077**, applied to the `feature-dev-2.0.0-otgw32-esp32-sat-support` branch. The architectural decision is identical; the implementation differs because the 2.0.0 file layout and struct shape have diverged from dev:

- The HA discovery code lives in `MQTTHaDiscovery.cpp` on 2.0.0 (renamed from dev's `mqtt_configuratie.cpp`).
- `MqttHaBinSensorCfg` (`MQTTstuff.h:277-287`) has two additional fields on 2.0.0: `PGM_P deviceClass` and `HaBinaryPayload payload`. None of the existing capability-flag rows override either; the alias rows added by this ADR also omit both, defaulting `deviceClass` to `nullptr` (no `device_class` emitted) and `payload` to `HaBinaryPayload::on_off = 0` (no `payload_on`/`payload_off` emitted; HA's defaults `"ON"`/`"OFF"` apply, which match the publisher output).
- The discovery composer is `composeBinSensorPayload()` at `MQTTHaDiscovery.cpp:2252` and the streaming function `streamBinarySensorDiscovery()` at `MQTTHaDiscovery.cpp:2429`. Both have additional source-separation logic from ADR-098 (the 2.0.0 equivalent of dev's ADR-070).

The user-visible problem is twofold:

1. **Migration friction.** Users moving from HA-core's native `opentherm_gw` integration *to* this firmware see their automations break: they reference entity ids like `binary_sensor.<gw>_supports_hot_water` or `binary_sensor.<gw>_fault_indication`, but the firmware publishes those same bits as `dhw_present` / `fault`. Every automation referencing one of the affected bits needs hand-rewriting.

2. **Opaque names.** Several firmware-only labels use OT-spec-derived abbreviations or jargon (`vh_*` for ventilation/heat-recovery, `rbp_*` for Remote Boiler Parameters, `master_configuration_smart_power`, `vh_configuration_bypass`, `solar_storage_slave_fault_indicator`). A user browsing HA's entity registry has no way to know what these mean without consulting the OT spec.

The audit identifies **three alias categories**, with two orthogonal axes:

- **Naming convention** — `supports_*` for true capabilities, plain name for everything else (state, mode, fault, type, configuration).
- **Source** — HA-core verbatim translation_key (migration aid) vs firmware-coined (clarity / coverage where no HA-core upstream exists).

Crossed against the available bits, three categories emerge:

- **A. Capability `supports_*`** — bit answers "does the device have capability X?", with bit=1 meaning "yes, capability is present". Examples: `dhw_present` → `supports_hot_water` (HA-core verbatim), `master_configuration_smart_power` → `supports_master_smart_power` (firmware-coined).
- **B. Plain-name HA-core verbatim** — bit is a state, type, or fault indicator; HA-core has a counterpart entity but uses a translation_key without the `supports_*` prefix. Aliased verbatim from HA-core for migration continuity. Example: `fault` (slave LB0) → `fault_indication`.
- **C. Plain-name firmware-coined** — bit has no HA-core counterpart and either uses an opaque firmware label or is a firmware-extra. Aliased with a self-describing name based on the v4.2 spec wording. Example: `vh_bypass_position` → `ventilation_bypass_position`.

### v4.2 spec evidence

The categorisation was driven by `OpenTherm Protocol Specification v4.2`. Key bits worth flagging:

- **MsgID 3 HB7** "Heat/cool mode control [Heat/cool mode switching can be done by master, Heat/cool mode switching is done by slave]" — control-locus indicator, *not* a capability. (Both values mean it *is* supported; the bit says *who* does the switching.) Not aliased.
- **MsgID 5 LB1** "Lockout-reset [remote reset disabled, rr enabled]" — true capability bit. Note: HA-core uses `DATA_SLAVE_REMOTE_RESET` for the *MsgID 3 bit 6* (which spec calls `Remote water filling function`), so HA-core's `supports_remote_reset` is a mislabel of the water-filling bit; the actual remote-reset bit is here on MsgID 5 LB1, aliased as `supports_lockout_reset`.
- **MsgID 74 HB2** "Speed control [3-speed, variable]" — type indicator, *not* a capability. (Both values support speed control; the bit distinguishes 3-speed vs variable.)
- **MsgID 74 HB0** "System type [0=central exhaust ventilation, 1=heat-recovery ventilation]" — type indicator.
- **MsgID 100 LB0/LB1** "Manual / Program change priority [disable overruling..., enable overruling...]" — capability-style per spec, but HA-core treats them as plain-name state indicators (`override_manual_change_prio`, `override_program_change_prio`). This ADR follows HA-core's naming for migration continuity.

### Mapping table — Category A: Capability aliases (`supports_*`) — 12 entries

9 adopt HA-core's translation_key verbatim for migration continuity; 3 are firmware-coined for bits HA-core does not publish:

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

### Mapping table — Category B: Plain-name HA-core verbatim — 12 entries

State / fault / type indicators that HA-core publishes with a plain-name translation_key. Aliased verbatim from HA-core for migration continuity:

| MsgID | Bit | Current firmware label | Alias | Spec name / HA-core context |
|---|---|---|---|---|
| 0 | LB0 | `fault` | `fault_indication` | spec "fault indication"; HA-core `DATA_SLAVE_FAULT_IND` |
| 0 | LB1 | `centralheating` | `central_heating` | spec "CH mode"; HA-core `DATA_SLAVE_CH_ACTIVE` (HA-core's `_n` suffix dropped — our single-device topology can't parameterise CH1/CH2 the way HA-core's two-device split does) |
| 0 | LB2 | `domestichotwater` | `hot_water` | spec "DHW mode"; HA-core `DATA_SLAVE_DHW_ACTIVE` |
| 0 | LB5 | `centralheating2` | `central_heating_2` | spec "CH2 mode"; HA-core `DATA_SLAVE_CH2_ACTIVE` (same `_n` note as LB1) |
| 0 | LB6 | `diagnostic_indicator` | `diagnostic_indication` | spec "diagnostic/service indication"; HA-core `DATA_SLAVE_DIAG_IND` |
| 3 | HB1 | `control_type_modulation` | `control_type` | type indicator `[modulating, on/off]`; HA-core `DATA_SLAVE_CONTROL_TYPE` |
| 3 | HB3 | `dhw_config` | `hot_water_config` | type indicator `[instantaneous or not-specified, storage tank]`; HA-core `DATA_SLAVE_DHW_CONFIG` |
| 5 | HB0 | `service_request` | `service_required` | spec "Service request"; HA-core `DATA_SLAVE_SERVICE_REQ` (HA-core uses past-participle form) |
| 5 | HB3 | `gas_flame_fault` | `gas_fault` | spec "Gas/flame fault"; HA-core `DATA_SLAVE_GAS_FAULT` (HA-core drops `flame`) |
| 5 | HB5 | `water_over_temperature` | `water_overtemperature` | spec "Water over-temp"; HA-core `DATA_SLAVE_WATER_OVERTEMP` (HA-core drops the underscore between `over` and `temperature`) |
| 100 | LB0 | `remote_override_manual_change_priority` | `override_manual_change_prio` | spec "Manual change priority" (capability-style per spec but HA-core treats as plain-name); HA-core `DATA_ROVRD_MAN_PRIO` |
| 100 | LB1 | `remote_override_program_change_priority` | `override_program_change_prio` | spec "Program change priority"; HA-core `DATA_ROVRD_AUTO_PRIO` |

### Mapping table — Category C: Plain-name firmware-coined — 13 entries

Bits with no HA-core counterpart. Aliases chosen for clarity (dropping `vh_*` abbreviation and redundant `_slave_indicator` qualifier), based on the v4.2 spec's own wording:

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
| 74 | HB0 | `vh_configuration_system_type` | `ventilation_system_type` | type indicator `[central exhaust ventilation, heat-recovery ventilation]` |
| 74 | HB2 | `vh_configuration_speed_control` | `ventilation_speed_control_type` | type indicator `[3-speed, variable]` |
| 101 | LB0 | `solar_storage_slave_fault_indicator` | `solar_storage_fault` | "Bit 0: fault indication" — drops the redundant `_slave_indicator` qualifier |

### Bits explicitly NOT aliased

| MsgID | Bit | Current firmware label | Why no alias |
|---|---|---|---|
| 0 | HB0..HB6 | `ch_enable`, `dhw_enable`, `cooling_enable`, `otc_active`, `ch2_enable`, `summerwintertime`, `dhw_blocking` | Master-side enable commands. HA-core publishes corresponding `DATA_MASTER_*_ENABLED` entries that *share* translation_keys with the slave-side `DATA_SLAVE_*_ACTIVE` entries (e.g. master CH-enable and slave CH-active both → `central_heating_n`). HA-core distinguishes them via *separate HA devices* (thermostat vs boiler); our single-device topology cannot replicate that pattern without colliding with the Category B slave-side aliases. Current firmware names are reasonably clear; leaving them as-is until the v2.1.0 three-device split (TASK-648) re-parents them to the thermostat device and allows shared translation_keys without collision. |
| 0 | LB3 | `flame` | Firmware name already matches HA-core's `DATA_SLAVE_FLAME_ON` translation_key (`flame`). No alias needed. |
| 0 | LB4 | `cooling` | Firmware name already matches HA-core's `DATA_SLAVE_COOLING_ACTIVE` translation_key (`cooling`). No alias needed. |
| 0 | LB7 | `electric_production` | No clear HA-core counterpart; current firmware name is already self-describing. |
| 3 | HB7 | `heat_cool_mode_control` | Spec value-set is `[switching by master, switching by slave]` — neither variant means "doesn't support". Current name is already clear; aliasing would add no information. |
| 5 | HB2 | `low_water_pressure` | Firmware name matches HA-core's `DATA_SLAVE_LOW_WATER_PRESS` translation_key (`low_water_pressure`). No alias needed. |
| 5 | HB4 | `air_pressure_fault` | Firmware name matches HA-core's `DATA_SLAVE_AIR_PRESS_FAULT` translation_key (`air_pressure_fault`). No alias needed. |
| 70 | LB5 | – | Reserved bit, not published. |
| 113 | – | `solar_storage_system_type` | Type indicator with spec value-set `[DHW preheat system, DHW parallel system]`. Current firmware name is already self-describing. |

### Constraints

- **No breaking renames.** ADR-098 (sibling-suffix source-separated discovery) is accepted on this branch; the 37 existing labels are part of the contract with alpha-channel deployments. The firmware must keep publishing all current labels untouched.
- **ESP8266 + ESP32-S3 memory.** Adding 37 PROGMEM label strings + 37 friendly-name strings + 37 struct rows costs roughly 2.9 KB of flash on both targets. RAM cost at rest is 1 bit in the settings struct. The active discovery + publish path adds 37 extra `sendMQTTData()` calls (across MsgID 0 / 2 / 3 / 5 / 6 / 70 / 74 / 100 / 101 updates) when the setting is on.
- **Broker hygiene.** When the setting is *off*, the broker must not see any alias discovery configs or value publishes. When toggled *on*, broker sees 37 new retained discovery configs.
- **HA-discovery contract.** The new entries must conform to `MqttHaBinSensorCfg`'s 9-field shape on this branch. The alias rows use positional initialisation with the same 7 fields the existing capability-flag rows use; `deviceClass` and `payload` fall through to their default-initialised values, exactly as the existing entries do.
- **Default off.** Existing alpha-channel users who do not toggle the setting see no behavioural change.

## Decision

Add a single bool to the persisted MQTT settings sub-section and a corresponding flag bit to `MqttHaBinSensorCfg`:

1. **Settings field** — add `bool bPublishHaCoreAliases;` (default `false`) to the `mqtt` sub-section of `OTGWSettings` (parallel to the existing `bHaRebootDetect`, `bSeparateSources`, `bDiscoveryAutoVerify`). Serialise to LittleFS JSON via the existing settings reader/writer. Expose a web UI toggle on the MQTT settings page.

2. **Flag bit** — define `#define MQTT_HA_FLAG_IS_HA_CORE_ALIAS 0x10` in `MQTTstuff.h`. The existing flags use the 0x01..0x08 range; 0x10 is the next unused bit. (Confirm against the canonical flag list at the time of implementation in case 2.0.0 has consumed an additional bit since this ADR was drafted.)

3. **Discovery table** — add 37 new rows to `mqttHaBinSensors[]` in `MQTTHaDiscovery.cpp:1250`, each carrying the new flag bit, appended after the corresponding MsgID's existing rows. Each new row mirrors the icon and entity_category of its current-name sibling. The alias rows omit `deviceClass` and `payload` (positional initialiser stops at the 7th field, same as the existing rows do).

4. **Discovery gating** — `streamBinarySensorDiscovery()` checks `settings.mqtt.bPublishHaCoreAliases` before publishing any row whose `cfg.flags & MQTT_HA_FLAG_IS_HA_CORE_ALIAS` is set. When the setting is off, alias rows are silently skipped. The check sits alongside the existing source-separation predicate from ADR-098; the alias check applies regardless of source variant (the canonical no-source shape is used for all 37 alias topics, same as the existing capability flags).

5. **Publisher gating** — each of the 37 affected publisher call sites — 5 in `publishSlaveStatusState()` (MsgID 0 slave LB at lines ~1799-1806; skipping LB3/LB4/LB7), 1 in `print_mastermemberid()` (MsgID 2), 7 in `print_slavememberid()` (MsgID 3; skipping HB7), 4 in `print_asfflags()` / equivalent (MsgID 5; skipping HB2/HB4), 4 in `publishRBPFlagsState()` (MsgID 6), 10 in `publishMasterStatusVHState()` + `publishSlaveStatusVHState()` (MsgID 70), 3 in the MsgID-74 publisher block at `OTGW-Core.ino:2371-2373` (bypass in Category A, system_type + speed_control in Category C), 2 in the MsgID-100 publisher (LB0/LB1), 1 in the MsgID-101 publisher — all in `OTGW-Core.ino` — gains a guarded second call:
   ```cpp
   sendMQTTData(F("dhw_present"), val);
   if (settings.mqtt.bPublishHaCoreAliases) {
       sendMQTTData(F("supports_hot_water"), val);
   }
   ```
   Order: current-label publish first, alias publish second. The current-label path is *never* affected by the setting. The MQTT source-separation layer (ADR-098) wraps both publishes uniformly — if `settings.mqtt.bSeparateSources` is also on, alias topics get the same sibling-suffix treatment as current-label topics.

6. **No cleanup on toggle-off.** When the user disables the setting, the firmware stops publishing alias discovery and alias state, but does not actively deregister the entities. Retained discovery configs and the last retained state values stay in the broker until manually purged. This is the conscious trade-off; see Alternatives.

7. **Naming-convention rule baked into the ADR.** Future maintainers adding new binary_sensors must classify them per the three categories:
   - True yes/no capability with bit=0 meaning "feature absent" → Category A, `supports_*` alias.
   - State / fault / type indicator with an HA-core counterpart → Category B, HA-core translation_key verbatim.
   - State / fault / type indicator with no HA-core counterpart, or with an opaque firmware label → Category C, firmware-coined plain name from v4.2 spec wording.
   - Bits where the spec value-set has both variants supported (e.g. master vs slave control-locus) → no alias; current label stays.
   - Bits where firmware label already matches HA-core → no alias needed.

### Why this choice

- **Comprehensive migration friction reduction.** Users moving from the HA-core integration to this firmware can flip one toggle and have their automations work unchanged for the 21 HA-core-matched bits (9 capability + 12 state/fault/type).
- **Coherent naming.** All capability bits use `supports_*` consistently; all state/type/fault bits use plain spec-faithful names. Category B preserves HA-core's choices; Category C fills the gaps for VH/solar bits HA-core doesn't publish.
- **Spec-correct categorisation.** Bits are aliased according to what the v4.2 spec actually says they mean, not based on superficial similarity. The audit catches three misclassifications that earlier scope drafts would have shipped.
- **Symmetric with dev (ADR-077).** Same architectural decision on both branches; users with mixed deployments (some on dev, some on 2.0.0) get identical alias behaviour on both.
- **No risk to existing users.** Default-off means the contract for installed deployments is unchanged.
- **Confined surface area.** Additive: one settings bool, one flag bit, 37 new discovery rows, 37 guarded second-publish call sites.
- **Reuses the existing discovery flag-bit pattern.** `MQTT_HA_FLAG_IS_PIC_ENTRY` already gates topic-prefixing via a flag bit on the same struct; this ADR follows the same shape.

## Alternatives Considered

### Alternative 1 — Rename the 37 existing labels to the new alias names

Drop `dhw_present` and publish `supports_hot_water` only; do the same for all 37 bits.

**Rejected:** breaks every existing user's automations referring to one of the 37 topics. The 2.0.0 branch is in alpha and has fewer deployments than dev, but rename churn this close to a stable 2.0.0 release would burn early-adopter trust. Documentation alone cannot reach every deployment.

### Alternative 2 — Documentation-only mapping (extend the TASK-650 audit doc)

Publish nothing new. Extend `docs/audits/2026-05-21-ha-capability-flags-feature-2.0.0.md` into a complete name-mapping reference.

**Rejected:** does not solve the migration problem (every automation still needs hand-rewriting). The complete mapping should land alongside this ADR — documentation and runtime feature are complementary, not alternatives.

### Alternative 3 — Always-on aliases, no setting

Skip the setting entirely; always publish both names. Cuts ~15 lines.

**Rejected:** permanently doubles the discovery footprint and per-update MQTT chatter for these 37 bits for every deployment. ESP8266 broker payload budget and HA entity registry are both finite. The 1-bit settings field is cheap.

### Alternative 4 — Default-on aliases with the setting present

Setting defaults to `true`; users can opt out.

**Rejected:** silently introduces 37 new HA entities on the next firmware upgrade for every existing deployment. Default-off is strictly safer.

### Alternative 5 — Clean discovery deregistration on toggle-off

Same as Decision, but on toggle-off, publish empty retained payloads to all 37 alias discovery topics so HA removes the entities cleanly.

**Rejected for v1** (may be revisited): adds ~50 lines of cleanup machinery for a corner case. Users toggling off after toggling on can manually purge retained topics with `mosquitto_pub -n -r`. If field testing on the alpha channel surfaces this as a frequent operation, a future ADR can add automated cleanup.

### Alternative 6 — Alias only the 21 HA-core-matched bits

Skip the 3 firmware-coined `supports_*` (smart_power, lockout_reset, ventilation_bypass) and all of Category C. Cuts to 21 aliases.

**Rejected:** mixes naming conventions inside one logical group. A user enabling the toggle sees `supports_hot_water` alongside `master_configuration_smart_power`, `vh_configuration_bypass`, `vh_fault`, etc. — multiple styles in one capability/state set, harder to scan. The flash and bandwidth cost of the additional aliases is negligible; coherent naming is worth more.

### Alternative 7 — Split into three ADRs, one per category

Author this ADR for capability-only, ADR-N for HA-core-matched state/fault, ADR-M for firmware-coined clarity. Each gets its own toggle.

**Rejected:** the three categories share a single user-visible mechanism (one MQTT topic per alias, one HA discovery entry, one publisher gating pattern). Splitting the ADR would force the user to flip three settings to get coherent behaviour and would triple the maintenance surface.

### Alternative 8 — Also alias the MsgID 0 master HB enable bits

Add aliases for `ch_enable`, `dhw_enable`, `cooling_enable`, `otc_active`, `ch2_enable`, etc. (5 more aliases).

**Rejected for v1:** HA-core publishes master and slave bits with the *same* translation_keys (`central_heating_n` for both `DATA_MASTER_CH_ENABLED` and `DATA_SLAVE_CH_ACTIVE`), distinguishing them only by device. Our single-device topology cannot replicate this — a master alias `central_heating` would collide with the slave alias `central_heating`. Inventing artificial disambiguation suffixes (`central_heating_enable_command`, `central_heating_active_status`) would defeat the migration purpose. The right fix is to wait for the v2.1.0 three-device split (TASK-648), which re-parents master bits to the thermostat device — then HA-core's translation_keys can be reused without collision. Current firmware names (`ch_enable`, etc.) are reasonably clear in the meantime.

### Alternative 9 — Wait until v2.1.0's three-device split lands, then design aliases on top of it

TASK-648 (deferred to v2.1.0) splits HA discovery into boiler/gateway/thermostat devices to match HA core's 2024.10 layout. The aliases would be more natural in that world (HA-core's `supports_hot_water` is published on the boiler device, not the gateway).

**Rejected:** the user-visible benefit of aliases is independent of which HA device they appear under. The 37 aliases can land now on the existing gateway-only device topology and be re-parented to the boiler/thermostat devices in v2.1.0 along with the rest of the discovery split (TASK-648 already covers re-parenting; this ADR's aliases ride along, and the v2.1.0 milestone is where the deferred master HB enable bits — per Alternative 8 — can also be aliased).

## Consequences

### Positive

- HA-core integration users can migrate to this firmware without rewriting any of their 21 HA-core-matched automations (9 capability + 12 state/fault/type).
- Three additional `supports_*` aliases (smart_power, lockout_reset, ventilation_bypass) bring full HA-core-style coverage of the firmware's true capability bits.
- All ventilation/heat-recovery state/mode/fault bits get self-describing `ventilation_*` aliases under the toggle — no more `vh_*` jargon for users who opt in.
- The solar fault entity gets a clean `solar_storage_fault` alias dropping the redundant `_slave_indicator`.
- MsgID 0 slave-side status bits (CH/DHW/CH2/fault/diagnostic) and MsgID 5 fault bits (service, gas, water-over-temp) get HA-core-verbatim aliases for migration continuity.
- MsgID 100 override-priority bits also get HA-core-verbatim aliases.
- Spec-correct categorisation is baked into the ADR: capability vs HA-core verbatim plain-name vs firmware-coined plain-name. Future binary_sensors get classified the same way.
- The 37-row mapping is codified in firmware source, not just documentation footnotes — easier for future maintainers to audit against new HA-core releases.
- Symmetric with dev ADR-077 — single mental model across both maintenance lines.

### Negative

- When the setting is on, broker discovery payload count for the 37 affected bits doubles (from 37 to 74 retained discovery configs across MsgID 0 / 2 / 3 / 5 / 6 / 70 / 74 / 100 / 101), and per-update MQTT publish volume for those frames doubles for the affected bits. MsgID 0 is the highest-frequency MsgID in the firmware (every status fan-out), so this is the dominant fan-out cost.
- When the setting is *also* used with `settings.mqtt.bSeparateSources=true` (ADR-098), the broker sees source-separated *and* aliased entries — boiler-source and thermostat-source aliases. Multiplicatively larger discovery footprint; documented but not prevented by the design.
- On toggle-off, retained alias discovery configs and state values linger in the broker until manually purged.
- The Category A firmware-coined aliases (`supports_master_smart_power`, `supports_lockout_reset`, `supports_ventilation_bypass`) and Category C firmware-coined aliases (`ventilation_*`, `solar_storage_fault`) are *coined* by this firmware, not by HA-core. If HA-core later adds these bits with different names, the firmware aliases will diverge. The fix at that point would be either (a) accept the divergence, or (b) add a *third* alias matching HA-core's new name (handled by another small ADR; this one does not pre-decide).
- The `supports_remote_reset` alias preserves HA-core's mislabel of the MsgID 3 bit 6 (`remote_water_filling_function`) for migration continuity, while the *actual* remote-reset capability (MsgID 5 LB1, per spec) gets its own `supports_lockout_reset` alias. This means a user reading both aliases needs to know they refer to *different* spec bits. The mismatch is documented here and in the audit doc, but it is a foot-gun for HA-core-naive users.
- The Category B HA-core verbatim aliases for MsgID 0 (`central_heating`, `hot_water`, `central_heating_2`) drop HA-core's `_n` translation_key parameterisation — necessary because our single-device topology cannot host both master and slave bits without name collision. HA-core users migrating will see `binary_sensor.<gw>_central_heating` instead of HA-core's `binary_sensor.<boiler>_central_heating_1`. Close enough for migration; not identical. The v2.1.0 three-device split (TASK-648) closes this gap.
- The MsgID 0 master HB enable bits (5 entries) are intentionally not aliased; users see them under firmware names (`ch_enable`, etc.) until v2.1.0.
- The new flag bit (`MQTT_HA_FLAG_IS_HA_CORE_ALIAS = 0x10`) consumes one of the eight available bits in `MqttHaBinSensorCfg::flags`.

### Risk

The risk to the existing fan-out path is low because the changes are additive. The new alias publish is conditional and skipped entirely when the setting is off. The discovery flag bit is checked once per discovery iteration. Both ESP8266 and ESP32-S3 targets receive the same code; the flash/RAM cost is identical on both. The MsgID 0 fan-out is high-frequency, so any bug in the alias publishers would have higher visibility — recommend extra care reviewing the MsgID 0 site changes during code review.

## Related Decisions

- **ADR-052** — MQTT publish eligibility contract.
- **ADR-065** — `MQTT_HA_FLAG_IS_PIC_ENTRY` flag-bit-based discovery customisation. This ADR follows the same pattern.
- **ADR-098** — Sibling-suffix source-separated discovery (the 2.0.0 evolution of dev's ADR-070, after the failed nested ADR-097 form was superseded). Aliases inherit the same source-separation treatment when `bSeparateSources` is on.
- **ADR-074** — Retained availability heartbeat. Alias entities inherit the same availability topic.
- **ADR-103** / **ADR-104** — Proxy-answer routing and pending-slot scoping. Out of scope for this ADR; alias publishers sit on the same `sendMQTTData()` path the existing capability-flag publishers use.
- **ADR-077** — sibling on the `dev` branch. Same decision, simpler struct shape (no `deviceClass` / `payload` fields), file at `mqtt_configuratie.cpp`.
- **TASK-648** — v2.1.0 three-device discovery split. The MsgID 0 master HB enable bits are deliberately not aliased here; TASK-648 will re-parent them to the thermostat device, removing the single-device naming collision and allowing HA-core's `_n` translation_keys to be reused faithfully.

## References

- `docs/audits/2026-05-21-ha-capability-flags-feature-2.0.0.md` — TASK-650 audit confirming semantic parity for the 13 MsgID-2/3/6 capability flags on this branch.
- `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md` — primary spec source for the audit. Specifically §5.3.1 (Data-IDs 0 / 70 / 101 status), §5.3.2 (Data-IDs 2 / 3 / 74 / 103 configuration), §5.3.7.3 (Data-ID 100 remote override priority), and the Application-specific fault sections (5 / 72 / 102).
- `src/OTGW-firmware/MQTTHaDiscovery.cpp:1250` — current `mqttHaBinSensors[]` array; alias rows append here.
- `src/OTGW-firmware/MQTTHaDiscovery.cpp:2252` — `composeBinSensorPayload()` (the discovery composer; unchanged by this ADR).
- `src/OTGW-firmware/MQTTHaDiscovery.cpp:2429` — `streamBinarySensorDiscovery()` (gains the new flag-bit check).
- `src/OTGW-firmware/MQTTstuff.h:277` — `MqttHaBinSensorCfg` struct (9 fields including `deviceClass` and `payload`; alias rows positional-initialise only the first 7 like the existing rows do).
- `src/OTGW-firmware/MQTTstuff.h:220` — `HaBinaryPayload` enum; `on_off = 0` is the default-initialised value used by all existing + 37 alias rows.
- `src/OTGW-firmware/OTGW-Core.ino` — publishers:
  - `publishSlaveStatusState()` MsgID-0 LB at lines ~1799-1806 (5 sites: fault, centralheating, domestichotwater, centralheating2, diagnostic_indicator; skipping flame, cooling, electric_production).
  - `print_mastermemberid()` MsgID-2 (1 site: `master_configuration_smart_power` → `supports_master_smart_power`).
  - `print_slavememberid()` MsgID-3 (7 sites; `heat_cool_mode_control` is not aliased).
  - MsgID-5 publishers (4 sites: service_request, gas_flame_fault, water_over_temperature, lockout_reset; skipping low_water_pressure + air_pressure_fault which already match HA-core).
  - `publishRBPFlagsState()` MsgID-6 (4 sites).
  - `publishMasterStatusVHState()` + `publishSlaveStatusVHState()` MsgID-70 (10 sites).
  - MsgID-74 publishers at `OTGW-Core.ino:2371-2373` (3 sites: bypass in Category A, system_type + speed_control in Category C).
  - MsgID-100 publishers (2 sites: manual + program change priority).
  - MsgID-101 publisher (1 site: `solar_storage_slave_fault_indicator` → `solar_storage_fault`).
- Home Assistant core `homeassistant/components/opentherm_gw/binary_sensor.py` — source of the HA-core translation_key names.
- pyotgw `vars.py` — defines the `DATA_SLAVE_*` / `DATA_MASTER_*` / `DATA_REMOTE_*` / `DATA_ROVRD_*` Python constants behind the HA-core entities.

## Enforcement

```json
{
  "forbid_pattern": [],
  "require_pattern": [],
  "llm_judge": true
}
```

The Decision is structural (settings field + flag bit + guarded publishes across three naming categories) and not easily encoded as a regex. Sonnet judge in the pre-commit hook reviews any diff touching `MQTTHaDiscovery.cpp`, `OTGW-Core.ino`, `OTGWSettings.h`, or `MQTTstuff.h` against this ADR's Decision text to catch:

(a) Alias publishes not guarded by `settings.mqtt.bPublishHaCoreAliases`.
(b) Alias rows in `mqttHaBinSensors[]` that omit `MQTT_HA_FLAG_IS_HA_CORE_ALIAS`.
(c) Renames of any of the 37 current labels (which would be a breaking change).
(d) New `supports_*` aliases coined for non-capability bits (mis-categorisation; e.g. naming a type indicator with the `supports_*` prefix).
(e) New non-capability aliases (Category B / C) given the `supports_*` prefix.
(f) New capability aliases given a plain non-`supports_*` name (the converse miscategorisation).
(g) Aliases for the MsgID 0 master HB enable bits that collide with the Category B slave-side aliases — see Alternative 8.
(h) Alias rows that incorrectly override `deviceClass` or `payload` (the aliased bits should keep HA-core's defaulted-None / defaulted-on_off semantics, matching the existing rows).
