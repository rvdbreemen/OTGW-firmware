# Audit: HA capability-flag binary_sensors (MsgID 2/3/6) vs HA core opentherm_gw — feature-2.0.0 branch

**Date:** 2026-05-21
**Branch:** `feature-dev-2.0.0-otgw32-esp32-sat-support` (OTGW-firmware 2.0.0 feature line)
**Backlog task:** TASK-650 (sibling of TASK-649 on `dev`)
**Scope:** Confirm parity between OTGW-firmware's HA discovery for the 13
capability-flag binary_sensors derived from OpenTherm MsgID 2 (master
configuration), MsgID 3 (slave configuration), and MsgID 6 (RBP transfer/RW
flags) and the semantics published by Home Assistant core's `opentherm_gw`
integration.

**Non-breaking only.** Topic-label renames, new bit decoders, MsgID handler
changes, ADR-098 source-separated layer changes (the 2.0.0 equivalent of
dev's ADR-070), and gating/cadence logic changes are explicitly out of scope.

## Conclusion

**Parity-confirmed; 1 fix applied (clarifying comment at MsgID 3 bit-6 decode site, ported from dev TASK-649 / commit `de5de6de`).**

All 13 binary_sensor discovery configs publish with `entity_category=diagnostic`,
no `device_class` asserted (matching HA core's `None`), and `retain=true`. The
struct `MqttHaBinSensorCfg` in `MQTTstuff.h:277-287` has `deviceClass` and
`payload` fields on this branch (unlike dev, which has no such fields), but
none of the 13 entries override either:

- `cfg.deviceClass` default-initialises to `nullptr` → `composeBinSensorPayload()`
  (MQTTHaDiscovery.cpp:2316-2319) only emits the `dev_cla` JSON key when this
  field is non-null. Net effect: matches dev (no device_class emitted).
- `cfg.payload` default-initialises to `HaBinaryPayload::on_off` (= 0; the
  first enum value at `MQTTstuff.h:220-225`). In the composer switch
  (MQTTHaDiscovery.cpp:2320-2333), `case on_off:` falls through to `default:`
  with no emission. Net effect: no `payload_on`/`payload_off` keys in JSON, so
  HA's defaults `"ON"`/`"OFF"` apply, which match the literal strings
  emitted by the publishers in `OTGW-Core.ino`.

## Method

1. Read the 13 entries in `mqttHaBinSensors[]` at
   `src/OTGW-firmware/MQTTHaDiscovery.cpp:1277-1298`.
2. Read the discovery payload composer `composeBinSensorPayload()` at
   `src/OTGW-firmware/MQTTHaDiscovery.cpp:2252-2340` to determine which JSON
   keys are emitted under default field values.
3. Read `streamBinarySensorDiscovery()` at
   `src/OTGW-firmware/MQTTHaDiscovery.cpp:2429-2459` to confirm
   `client.beginPublish(topic, byteCount, /*retained=*/true)` at line 2446.
4. Read the publishers in `src/OTGW-firmware/OTGW-Core.ino`:
   - `print_slavememberid()` (MsgID 3) at lines around 2317-2337
   - `print_mastermemberid()` (MsgID 2)
   - `publishRBPFlagsState()` (MsgID 6)
5. Confirmed each publisher emits literal `"ON"` / `"OFF"` via `sendMQTTData(F("..."), expr ? "ON" : "OFF")`.
6. Verified `MqttHaBinSensorCfg` struct definition at `src/OTGW-firmware/MQTTstuff.h:277-287` and confirmed the 13 entries supply only 7 of the 9 fields (omitting `deviceClass` and `payload`).
7. Verified `HaBinaryPayload` enum at `MQTTstuff.h:220-225` — `on_off = 0` is the first value, matching default-initialisation.

## Per-entry findings

Legend:
- `entity_category` column: value emitted in the discovery JSON `entity_category` key.
- `device_class`: not emitted (struct field present but defaults to `nullptr`; composer only writes when non-null).
- `payload_on/off`: not emitted (struct field present but defaults to `HaBinaryPayload::on_off`; composer's switch emits nothing for that case). HA applies defaults `"ON"`/`"OFF"`, matching publisher output.
- `retain` (discovery): always `true` (`beginPublish(..., true)`).
- All entries emit `avty_t = <mqttPubTopic>` (LWT-backed availability topic at device root).

| MsgID | label (topic) | friendly name | icon | entity_category | device_class | payload_on/off (publisher) | retain (disc.) | HA-core counterpart |
|---|---|---|---|---|---|---|---|---|
| 2 | `master_configuration_smart_power` | `master_configuration_smart_power` | mdi:checkbox-marked-circle | diagnostic | (none) | "ON" / "OFF" | true | (firmware-extra; no HA-core flag) |
| 3 | `ch2_present` | `CH2_present` | mdi:checkbox-marked-circle | diagnostic | (none) | "ON" / "OFF" | true | `DATA_SLAVE_CH2_PRESENT` |
| 3 | `control_type_modulation` | `control_type_modulation` | mdi:checkbox-marked-circle | diagnostic | (none) | "ON" / "OFF" | true | `DATA_SLAVE_CONTROL_TYPE` |
| 3 | `cooling_config` | `Cooling_configs` | mdi:snowflake | diagnostic | (none) | "ON" / "OFF" | true | `DATA_SLAVE_COOLING_SUPPORTED` |
| 3 | `dhw_config` | `DHW_config` | mdi:water-boiler | diagnostic | (none) | "ON" / "OFF" | true | `DATA_SLAVE_DHW_CONFIG` |
| 3 | `dhw_present` | `DHW_present` | mdi:water-boiler | diagnostic | (none) | "ON" / "OFF" | true | `DATA_SLAVE_DHW_PRESENT` |
| 3 | `heat_cool_mode_control` | `heat_cool_mode_control` | mdi:checkbox-marked-circle | diagnostic | (none) | "ON" / "OFF" | true | (firmware-extra; no HA-core flag) |
| 3 | `master_low_off_pump_control_function` | `Master_low_off_pump_control_function` | mdi:checkbox-marked-circle | diagnostic | (none) | "ON" / "OFF" | true | `DATA_SLAVE_MASTER_LOW_OFF_PUMP` |
| 3 | `remote_water_filling_function` | `remote_water_filling_function` | mdi:checkbox-marked-circle | diagnostic | (none) | "ON" / "OFF" | true | `DATA_SLAVE_REMOTE_RESET` (same wire bit, different name; see notes) |
| 6 | `rbp_dhw_setpoint` | `RBP_DHW_setpoint` | mdi:water-boiler | diagnostic | (none) | "ON" / "OFF" | true | `DATA_REMOTE_TRANSFER_DHW` |
| 6 | `rbp_max_ch_setpoint` | `RBP_max_CH_setpoint` | mdi:checkbox-marked-circle | diagnostic | (none) | "ON" / "OFF" | true | `DATA_REMOTE_TRANSFER_MAX_CH` |
| 6 | `rbp_rw_dhw_setpoint` | `RBP_rw_DHW_setpoint` | mdi:water-boiler | diagnostic | (none) | "ON" / "OFF" | true | `DATA_REMOTE_RW_DHW` |
| 6 | `rbp_rw_max_ch_setpoint` | `RBP_rw_max_CH_setpoint` | mdi:checkbox-marked-circle | diagnostic | (none) | "ON" / "OFF" | true | `DATA_REMOTE_RW_MAX_CH` |

## Fixes applied

### Fix 1: bit-6 ambiguity comment (per TASK-650 AC#7; ported from dev TASK-649)

`src/OTGW-firmware/OTGW-Core.ino` — `print_slavememberid()` around line 2327
(the `// 5:` / `// 6:` / `// 7:` bit-list comment block).

Added a multi-line code comment at the MsgID 3 bit-6 decode site clarifying
that pyotgw / HA core's `opentherm_gw` integration names this bit
`DATA_SLAVE_REMOTE_RESET`, while the OpenTherm 2.2 spec and this firmware's
label call it `remote_water_filling_function`. Both refer to the same physical
wire bit (MsgID 3, HB bit 6). Cross-references this audit document by path.

```diff
     // 5:  CH2 present  [CH2 not present, CH2 present]
     // 6:  Remote water filling function
+    //     NOTE: pyotgw / HA core's opentherm_gw integration names this bit
+    //     DATA_SLAVE_REMOTE_RESET. The OpenTherm 2.2 spec and this firmware's
+    //     label call it "remote_water_filling_function". Both refer to the
+    //     same wire bit (MsgID 3, HB bit 6); HA-side discovery parity is
+    //     audited in docs/audits/2026-05-21-ha-capability-flags-feature-2.0.0.md (TASK-650).
     // 7:  Heat/cool mode control
```

No behavioural change. Comment only. Mirrors dev commit `de5de6de` (TASK-649).

## Items noted but not addressed (off-limits per TASK-650 AC#9)

Same set as dev TASK-649. These items were observed during the audit and
intentionally left as-is because they fall outside the non-breaking scope:

- **Friendly-name casing inconsistencies.** Some entries Title-Case the
  friendly name (`CH2_present`, `DHW_config`, `Master_low_off_pump_control_function`,
  `Cooling_configs`, `RBP_DHW_setpoint`, …) while others stay all-lowercase
  (`control_type_modulation`, `heat_cool_mode_control`,
  `master_configuration_smart_power`, `remote_water_filling_function`). This
  is a style inconsistency, not a typo. `composeBinSensorPayload()`'s
  `writeFriendlyName()` helper converts `_` → space + Title Case before
  display, so the rendered HA entity names are sane regardless. A
  normalisation pass would be cosmetic and is deferred.
- **"Cooling_configs" plural.** The friendly name `ha_name_cooling_configs`
  ("Cooling_configs", note the trailing 's') is slightly awkward English vs
  the singular topic label `cooling_config`. Renaming touches `ha_name_*`
  PROGMEM but not `ha_lbl_*`, so technically non-breaking; however this is
  a naming-style change, not a typo (per AC#6 scope). Filed mentally; not
  changed.
- **`payload_on` / `payload_off` not explicitly emitted.** HA defaults these
  to `"ON"` / `"OFF"`, which is exactly what the publishers emit. The 2.0.0
  composer has structural support for explicit emission (via
  `HaBinaryPayload::on_off` / `true_false` / `one_zero`), but using the
  default `on_off` enum value is the correct choice for these entries.
- **Firmware-extras not in HA core (`master_configuration_smart_power`,
  `heat_cool_mode_control`).** Audit-only. These remain published; HA users
  see them as additional diagnostic entities.

## Differences from dev (TASK-649) — structural only, behaviourally identical

| Aspect | dev (TASK-649) | feature-2.0.0 (TASK-650) |
|---|---|---|
| Discovery file | `mqtt_configuratie.cpp` | `MQTTHaDiscovery.cpp` (renamed) |
| `MqttHaBinSensorCfg` struct | 7 fields, no `deviceClass`, no `payload` | 9 fields, adds `deviceClass: PGM_P` and `payload: HaBinaryPayload` |
| Composer device_class emission | structurally impossible (no field) | conditionally emitted when `cfg.deviceClass != nullptr`; none of the 13 entries set it |
| Composer payload emission | not present (defaults always apply) | switch on `cfg.payload`; `on_off = 0` (default) emits nothing → defaults apply |
| Source-separated ADR | ADR-070 sibling-suffix | ADR-098 sibling-suffix (supersedes earlier nested ADR-097) |
| Net HA discovery behaviour for these 13 | identical | identical |

The 13 entries themselves are line-for-line equivalent (same labels, friendly
names, icons, categories, defaults). Net wire-level discovery output is
indistinguishable between branches for these capability flags.

## Verification gates

- `python build.py --firmware` — see commit log.
- `python evaluate.py --quick` — see commit log.
- No `ha_lbl_*` strings touched.
- No new bit decoders added.
- No MsgID handler logic changed (only the bit-6 comment block extended).
- No ADR-098 source-separated layer touched.
- No gating/cadence logic touched.

## Related code paths (feature-2.0.0)

- `src/OTGW-firmware/MQTTHaDiscovery.cpp:1250` — `mqttHaBinSensors[]` array head.
- `src/OTGW-firmware/MQTTHaDiscovery.cpp:1277-1298` — MsgID 2/3/6 rows.
- `src/OTGW-firmware/MQTTHaDiscovery.cpp:2252-2340` — `composeBinSensorPayload()`.
- `src/OTGW-firmware/MQTTHaDiscovery.cpp:2429-2459` — `streamBinarySensorDiscovery()` (retain flag at line 2446).
- `src/OTGW-firmware/MQTTstuff.h:277-287` — `MqttHaBinSensorCfg` struct.
- `src/OTGW-firmware/MQTTstuff.h:220-225` — `HaBinaryPayload` enum.
- `src/OTGW-firmware/OTGW-Core.ino` — bit-6 comment site (around line 2327).

## Related backlog

- **TASK-649** — dev sibling. Done. Audit doc at `docs/audits/2026-05-21-ha-capability-flags-dev.md`. Commit `de5de6de` on `dev`.
- **TASK-648** — v2.1.0 three-device split (deferred from v2.0.0). HA discovery topology realignment with HA core's 2024.10 boiler/gateway/thermostat split. Topic names unchanged; only the device: block in discovery payloads changes.
