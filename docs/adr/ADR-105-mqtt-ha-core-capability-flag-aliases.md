# ADR-105: MQTT — Publish HA-Core Capability-Flag Aliases under `settings.mqtt.bPublishHaCoreAliases`

## Status

Proposed

## Context

TASK-650 confirmed that the 13 capability-flag binary_sensors this firmware publishes for OpenTherm MsgID 2 (master config), MsgID 3 (slave config), and MsgID 6 (RBP transfer/RW flags) are at semantic parity with Home Assistant core's `opentherm_gw` integration on the 2.0.0 feature line. However, the **names** differ for 11 of the 13 bits — the firmware uses OT-spec-derived labels, HA-core uses self-describing translation_keys with a `supports_*` verb prefix for capability bits.

The current name mismatch becomes a real-world friction point in one specific scenario: a user migrating *from* HA-core's native `opentherm_gw` integration *to* this firmware. Their HA automations are pinned to entity ids like `binary_sensor.<gateway>_supports_hot_water`, but this firmware exposes that same boiler bit as `binary_sensor.<gateway>_dhw_present` — every automation referencing one of the 11 affected bits has to be rewritten by hand. There is no in-firmware mechanism to ease that migration today.

This ADR is the sibling of dev's **ADR-077**, applied to the `feature-dev-2.0.0-otgw32-esp32-sat-support` branch. The architectural decision is identical; the implementation differs because the 2.0.0 file layout and struct shape have diverged from dev:

- The HA discovery code lives in `MQTTHaDiscovery.cpp` on 2.0.0 (renamed from dev's `mqtt_configuratie.cpp`).
- `MqttHaBinSensorCfg` (`MQTTstuff.h:277-287`) has two additional fields on 2.0.0: `PGM_P deviceClass` and `HaBinaryPayload payload`. None of the existing 13 capability-flag rows override either; the alias rows added by this ADR also omit both, defaulting `deviceClass` to `nullptr` (no `device_class` emitted) and `payload` to `HaBinaryPayload::on_off = 0` (no `payload_on`/`payload_off` emitted; HA's defaults `"ON"`/`"OFF"` apply, which match the publisher output).
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

`master_configuration_smart_power` (MsgID 2) and `heat_cool_mode_control` (MsgID 3 bit 7) have no HA-core equivalent and are out of scope.

### Constraints

- **No breaking renames.** ADR-098 (sibling-suffix source-separated discovery) is accepted on this branch; the existing 13 labels are part of the contract with alpha-channel deployments. The firmware must keep publishing all current labels untouched.
- **ESP8266 + ESP32-S3 memory.** Adding 11 PROGMEM label strings + 11 friendly-name strings + 11 struct rows costs roughly 860 bytes of flash on both targets. RAM cost at rest is 1 bit in the settings struct.
- **Broker hygiene.** When the setting is *off*, the broker must not see any alias discovery configs or value publishes. When toggled *on*, broker sees 11 new retained discovery configs.
- **HA-discovery contract.** The new entries must conform to `MqttHaBinSensorCfg`'s 9-field shape on this branch. The alias rows use positional initialisation with the same 7 fields the existing 13 rows use; `deviceClass` and `payload` fall through to their default-initialised values, exactly as the existing 13 entries do.
- **Default off.** Existing alpha-channel users who do not toggle the setting see no behavioural change.

## Decision

Add a single bool to the persisted MQTT settings sub-section and a corresponding flag bit to `MqttHaBinSensorCfg`:

1. **Settings field** — add `bool bPublishHaCoreAliases;` (default `false`) to the `mqtt` sub-section of `OTGWSettings` (parallel to the existing `bHaRebootDetect`, `bSeparateSources`, `bDiscoveryAutoVerify`). Serialise to LittleFS JSON via the existing settings reader/writer. Expose a web UI toggle on the MQTT settings page.

2. **Flag bit** — define `#define MQTT_HA_FLAG_IS_HA_CORE_ALIAS 0x10` in `MQTTstuff.h`. The existing flags use the 0x01..0x08 range; 0x10 is the next unused bit. (Confirm against the canonical flag list at the time of implementation in case 2.0.0 has consumed an additional bit since this ADR was drafted.)

3. **Discovery table** — add 11 new rows to `mqttHaBinSensors[]` in `MQTTHaDiscovery.cpp:1250`, each carrying the new flag bit, appended after the existing MsgID-2/3/6 rows. Each new row mirrors the icon and entity_category of its current-name sibling. The alias rows omit `deviceClass` and `payload` (positional initialiser stops at the 7th field, same as the existing rows do).

4. **Discovery gating** — `streamBinarySensorDiscovery()` checks `settings.mqtt.bPublishHaCoreAliases` before publishing any row whose `cfg.flags & MQTT_HA_FLAG_IS_HA_CORE_ALIAS` is set. When the setting is off, alias rows are silently skipped. The check sits alongside the existing source-separation predicate from ADR-098; the alias check applies regardless of source variant (the canonical no-source shape is used for all 11 alias topics, same as the existing 13 capability flags).

5. **Publisher gating** — each of the 11 affected `sendMQTTData(F("<current_label>"), val)` call sites in `print_slavememberid()` (MsgID 3) and `publishRBPFlagsState()` (MsgID 6), both in `OTGW-Core.ino`, gains a guarded second call:
   ```cpp
   sendMQTTData(F("dhw_present"), val);
   if (settings.mqtt.bPublishHaCoreAliases) {
       sendMQTTData(F("supports_hot_water"), val);
   }
   ```
   Order: current-label publish first, alias publish second. The current-label path is *never* affected by the setting. The MQTT source-separation layer (ADR-098) wraps both publishes uniformly — if `settings.mqtt.bSeparateSources` is also on, alias topics get the same sibling-suffix treatment as current-label topics.

6. **No cleanup on toggle-off.** When the user disables the setting, the firmware stops publishing alias discovery and alias state, but does not actively deregister the entities. Retained discovery configs and the last retained state values stay in the broker until manually purged. This is the conscious trade-off; see Alternatives.

7. **Scope is strictly the 11 HA-core-matched bits.** The two firmware-extras (`master_configuration_smart_power`, `heat_cool_mode_control`) are not aliased — HA-core has no equivalent.

### Why this choice

- **Minimal migration friction.** Users moving from the HA-core integration to this firmware can flip one toggle and have their automations work unchanged for the 11 capability bits.
- **Symmetric with dev (ADR-077).** Same architectural decision on both branches; users with mixed deployments (some on dev, some on 2.0.0) get identical alias behaviour on both.
- **No risk to existing users.** Default-off means the contract for installed deployments is unchanged.
- **Confined surface area.** Additive only: one settings bool, one flag bit, 11 new discovery rows, 11 guarded second-publish call sites.
- **Reuses the existing discovery flag-bit pattern.** `MQTT_HA_FLAG_IS_PIC_ENTRY` already gates topic-prefixing via a flag bit on the same struct; this ADR follows the same shape.

## Alternatives Considered

### Alternative 1 — Rename the 11 existing labels to HA-core names

Drop `dhw_present` and publish `supports_hot_water` only; do the same for all 11 bits.

**Rejected:** breaks every existing user's automations referring to one of the 11 topics. The 2.0.0 branch is in alpha and has fewer deployments than dev, but rename churn this close to a stable 2.0.0 release would burn early-adopter trust. Documentation alone cannot reach every deployment.

### Alternative 2 — Documentation-only mapping (extend the TASK-650 audit doc)

Publish nothing new. Extend `docs/audits/2026-05-21-ha-capability-flags-feature-2.0.0.md` into a complete name-mapping reference.

**Rejected:** does not solve the migration problem (every automation still needs hand-rewriting). The complete mapping should land alongside this ADR — documentation and runtime feature are complementary, not alternatives.

### Alternative 3 — Always-on aliases, no setting

Skip the setting entirely; always publish both names. Cuts ~15 lines.

**Rejected:** permanently doubles the discovery footprint and per-update MQTT chatter for these 11 bits for every deployment. ESP8266 broker payload budget and HA entity registry are both finite. The 1-bit settings field is cheap.

### Alternative 4 — Default-on aliases with the setting present

Setting defaults to `true`; users can opt out.

**Rejected:** silently introduces 11 new HA entities on the next firmware upgrade for every existing deployment. Default-off is strictly safer.

### Alternative 5 — Clean discovery deregistration on toggle-off

Same as Decision, but on toggle-off, publish empty retained payloads to all 11 alias discovery topics so HA removes the entities cleanly.

**Rejected for v1** (may be revisited): adds ~50 lines of cleanup machinery for a corner case. Users toggling off after toggling on can manually purge retained topics with `mosquitto_pub -n -r`. If field testing on the alpha channel surfaces this as a frequent operation, a future ADR can add automated cleanup.

### Alternative 6 — Wait until v2.1.0's three-device split lands, then design aliases on top of it

TASK-648 (deferred to v2.1.0) splits HA discovery into boiler/gateway/thermostat devices to match HA core's 2024.10 layout. The aliases would be more natural in that world (HA-core's `supports_hot_water` is published on the boiler device, not the gateway).

**Rejected:** the user-visible benefit of aliases is independent of which HA device they appear under. The 11 aliases can land now on the existing gateway-only device topology and be re-parented to the boiler device in v2.1.0 along with the rest of the discovery split (TASK-648 already covers re-parenting; this ADR's aliases ride along).

## Consequences

### Positive

- HA-core integration users can migrate to this firmware without rewriting any of their 11-bit capability-flag automations.
- The 11 HA-core names are more self-descriptive than the existing OT-spec-derived names (`supports_central_heating_setpoint_transfer` vs `rbp_max_ch_setpoint`); new users discovering this firmware also benefit.
- The settings field gives a clean opt-in story.
- Symmetric with dev ADR-077 — single mental model across both maintenance lines.

### Negative

- When the setting is on, broker discovery payload count for capability flags doubles (from 13 to 24 retained discovery configs), and per-update MQTT publish volume for MsgID-3 / MsgID-6 frames doubles for the affected bits.
- When the setting is *also* used with `settings.mqtt.bSeparateSources=true` (ADR-098), the broker sees source-separated *and* aliased entries — boiler-source and thermostat-source aliases. Multiplicatively larger discovery footprint; documented but not prevented by the design.
- On toggle-off, retained alias discovery configs and state values linger in the broker until manually purged.
- The new flag bit (`MQTT_HA_FLAG_IS_HA_CORE_ALIAS = 0x10`) consumes one of the eight available bits in `MqttHaBinSensorCfg::flags`.
- A future HA-core rename of a translation_key would require a firmware change to track.

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
- `src/OTGW-firmware/MQTTstuff.h:220` — `HaBinaryPayload` enum; `on_off = 0` is the default-initialised value used by all 13 existing + 11 alias rows.
- `src/OTGW-firmware/OTGW-Core.ino:2284` — `print_slavememberid()` MsgID-3 publishers (8 sites gain the guarded second call).
- `src/OTGW-firmware/OTGW-Core.ino:1945` — `publishRBPFlagsState()` MsgID-6 publishers (4 sites gain the guarded second call).
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

The Decision is structural (settings field + flag bit + guarded publishes) and not easily encoded as a regex. Sonnet judge in the pre-commit hook reviews any diff touching `MQTTHaDiscovery.cpp`, `OTGW-Core.ino`, `OTGWSettings.h`, or `MQTTstuff.h` against this ADR's Decision text to catch: (a) alias publishes that are not guarded by `settings.mqtt.bPublishHaCoreAliases`, (b) alias rows in `mqttHaBinSensors[]` that omit `MQTT_HA_FLAG_IS_HA_CORE_ALIAS`, (c) renames of any of the 11 current labels (which would be a breaking change), (d) alias rows that incorrectly override `deviceClass` or `payload` (capability bits should keep HA-core's defaulted-None / defaulted-on_off semantics).
