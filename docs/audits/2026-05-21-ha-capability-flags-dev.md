# Audit: HA capability-flag binary_sensors (MsgID 2/3/6) vs HA core opentherm_gw — dev branch

**Date:** 2026-05-21
**Branch:** `dev` (OTGW-firmware 1.6.x prerelease line)
**Backlog task:** TASK-649
**Scope:** Confirm parity between OTGW-firmware's HA discovery for the 13
capability-flag binary_sensors derived from OpenTherm MsgID 2 (master
configuration), MsgID 3 (slave configuration), and MsgID 6 (RBP transfer/RW
flags) and the semantics published by Home Assistant core's `opentherm_gw`
integration.

**Non-breaking only.** Topic-label renames, new bit decoders, MsgID handler
changes, ADR-070 source-separated layer changes, and gating/cadence logic
changes are explicitly out of scope.

## Conclusion

**parity-confirmed; 1 fix applied (clarifying comment at MsgID 3 bit-6 decode site).**

All 13 binary_sensor discovery configs publish with `entity_category=diagnostic`,
no `device_class` asserted (matching HA core's `None`), and `retain=true`. The
struct `MqttHaBinSensorCfg` in `MQTTstuff.h` lines 205-213 structurally has no
`deviceClass` field, so no binary_sensor entry can wrongly assert one. Payload
values published by `OTGW-Core.ino`'s decoders match the HA discovery default
`payload_on="ON"` / `payload_off="OFF"` (firmware does not override these in the
discovery JSON, so HA's defaults apply).

## Method

1. Read the 13 entries in `mqttHaBinSensors[]` at
   `src/OTGW-firmware/mqtt_configuratie.cpp:1124-1147`.
2. Read the discovery payload composer `composeBinSensorPayload()` at
   `src/OTGW-firmware/mqtt_configuratie.cpp:2089-2157` to determine which JSON
   keys are emitted.
3. Read the publisher functions in `src/OTGW-firmware/OTGW-Core.ino`:
   - `print_mastermemberid()` (MsgID 2)
   - `print_slavememberid()` (MsgID 3)
   - `print_RBPflags()` / `publishRBPFlagsState()` (MsgID 6)
4. Verified `publishMQTTOnOff()` (`MQTTstuff.ino:1179-1185`) emits the literal
   string `"ON"` or `"OFF"`.
5. Verified `streamBinarySensorDiscovery()` (`mqtt_configuratie.cpp:2246-2275`)
   calls `client.beginPublish(topic, byteCount, /*retained=*/true)` on line 2262.

## Per-entry findings

Legend:
- `entity_category` column: value emitted in the discovery JSON `entity_category` key.
- `device_class`: HA core uses `None` for all capability flags. The firmware
  struct has no `deviceClass` field at all, so the key is never emitted (= `None`).
- `payload_on/off`: discovery JSON omits these keys, so HA applies defaults
  `"ON"` / `"OFF"`. The firmware publishers emit exactly those strings.
- `retain` (discovery): always `true` (`beginPublish(..., true)`).
- All entries emit `avty_t = <mqttPubTopic>` (the LWT-backed availability topic
  at the device root).

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

### Fix 1: bit-6 ambiguity comment (per AC#7)

`src/OTGW-firmware/OTGW-Core.ino` — `print_slavememberid()` around the bit-6
decode site.

Added a multi-line code comment at the MsgID 3 bit-6 decode site clarifying
that pyotgw / HA core's `opentherm_gw` integration names this bit
`DATA_SLAVE_REMOTE_RESET`, while the OpenTherm 2.2 spec and this firmware's
label call it `remote_water_filling_function`. Both refer to the same physical
wire bit (MsgID 3, HB bit 6). Cross-references this audit document by path.

Diff snippet:

```diff
     // 6:  Remote water filling function
+    //     NOTE: pyotgw / HA core's opentherm_gw integration names this bit
+    //     DATA_SLAVE_REMOTE_RESET. The OpenTherm 2.2 spec and this firmware's
+    //     label call it "remote_water_filling_function". Both refer to the
+    //     same wire bit (MsgID 3, HB bit 6); HA-side discovery parity is
+    //     audited in docs/audits/2026-05-21-ha-capability-flags-dev.md (TASK-649).
     // 7:  Heat/cool mode control
```

No behavioural change. Comment only.

## Items noted but not addressed (off-limits per AC#9)

These items were observed during the audit and intentionally left as-is
because they fall outside the non-breaking scope:

- **Friendly-name casing inconsistencies.** Some entries Title-Case the
  friendly name (`CH2_present`, `DHW_config`, `Master_low_off_pump_control_function`,
  `Cooling_configs`, `RBP_DHW_setpoint`, …) while others stay all-lowercase
  (`control_type_modulation`, `heat_cool_mode_control`,
  `master_configuration_smart_power`, `remote_water_filling_function`). This
  is a style inconsistency, not a typo, and `composeBinSensorPayload()`'s
  `writeFriendlyName()` helper already converts `_` → space and applies title
  case before display, so the rendered HA entity names are sane regardless.
  A normalisation pass would be cosmetic and is left for a separate task if
  ever desired.
- **"Cooling_configs" plural.** The friendly name `ha_name_cooling_configs`
  ("Cooling_configs", note the trailing 's') is slightly awkward English vs
  the singular topic label `cooling_config`. Renaming touches the `ha_name_*`
  PROGMEM string but not the `ha_lbl_*` topic, so technically non-breaking;
  however, this falls into "naming-style change" territory per AC#6, which
  scopes only clear typos. Filed mentally; not changed.
- **Topic-label/friendly-name underscore vs space.** HA discovery best practice
  often uses spaces in `name`, but `composeBinSensorPayload()` post-processes
  these via `writeFriendlyName()` to swap underscores for spaces, so the
  emitted JSON `name` field already renders correctly in HA. No change needed.
- **`payload_on` / `payload_off` not explicitly emitted.** HA defaults these
  to `"ON"` / `"OFF"`, which is exactly what the publishers emit. Explicit
  emission would be redundant. No change.
- **Firmware-extras not in HA core (`master_configuration_smart_power`,
  `heat_cool_mode_control`).** Audit-only. These remain published; HA users
  see them as additional diagnostic entities. No action required.

## Verification gates

- `python build.py --firmware` — exit 0 (1.6.0-beta.11).
- `python evaluate.py --quick` — no new findings vs pre-change baseline.
- No `ha_lbl_*` strings touched.
- No new bit decoders added.
- No MsgID handler logic changed (only a comment block added inside
  `print_slavememberid()`).
- No ADR-070 source-separated layer touched.
- No gating/cadence logic touched.

## Related code paths

- `src/OTGW-firmware/mqtt_configuratie.cpp:1124-1147` — `mqttHaBinSensors[]` rows for MsgID 2/3/6.
- `src/OTGW-firmware/mqtt_configuratie.cpp:2089-2157` — `composeBinSensorPayload()`.
- `src/OTGW-firmware/mqtt_configuratie.cpp:2246-2275` — `streamBinarySensorDiscovery()` (retain flag).
- `src/OTGW-firmware/MQTTstuff.h:205-213` — `MqttHaBinSensorCfg` struct (no `deviceClass` field).
- `src/OTGW-firmware/MQTTstuff.ino:1179-1185` — `publishMQTTOnOff()` (payload "ON"/"OFF").
- `src/OTGW-firmware/OTGW-Core.ino` — `publishRBPFlagsState()` (MsgID 6 emitters).
- `src/OTGW-firmware/OTGW-Core.ino` — `print_slavememberid()` (MsgID 3 emitters; bit-6 comment fix).
- `src/OTGW-firmware/OTGW-Core.ino` — `print_mastermemberid()` (MsgID 2 emitter).
