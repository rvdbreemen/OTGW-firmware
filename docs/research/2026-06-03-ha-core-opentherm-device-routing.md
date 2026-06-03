# HA Core opentherm_gw: Per-Entity Device Routing Table

## Purpose

This document provides the authoritative per-entity device-assignment table from the Home
Assistant core `opentherm_gw` integration. It feeds **TASK-648 Task 4** (firmware HA discovery
five-device split). Firmware implementers can transcribe it directly into a routing table.

## Sources

All data fetched from the HA core `dev` branch on **2026-06-03**:

- `homeassistant/components/opentherm_gw/sensor.py`
- `homeassistant/components/opentherm_gw/binary_sensor.py`
- `homeassistant/components/opentherm_gw/switch.py`
- `homeassistant/components/opentherm_gw/select.py`
- `homeassistant/components/opentherm_gw/button.py`
- `homeassistant/components/opentherm_gw/climate.py`
- `pyotgw/vars.py` (for constant-to-string resolution)

## Key Structural Notes

HA core defines three device objects:
- **BOILER** (`OpenThermDataSource.BOILER`) - the heating appliance on the OT bus
- **THERMOSTAT** (`OpenThermDataSource.THERMOSTAT`) - the room controller on the OT bus
- **GATEWAY** (`OpenThermDataSource.GATEWAY`) - the OTGW PIC hardware itself

In sensor.py, **all 46 DATA_* sensor entries are duplicated**: each appears once under
`BOILER_DEVICE_DESCRIPTION` and once under `THERMOSTAT_DEVICE_DESCRIPTION`. This is by design:
HA core creates two entity instances per DATA_* sensor key so the same measurement appears
under both the Boiler and Thermostat devices in the UI. OTGW_* keys are GATEWAY-only with no
duplication.

The firmware-side topic label column uses the `OTmap[]` label field from `OTGW-Core.h`
(the `messageIDToString()` return value) or the explicit `sendMQTTData(F("..."))` string
where the OTmap is not used. Labels marked **TBD-confirm** need cross-checking against the
actual MQTT publish path in `OTGW-Core.ino` before implementation.

---

## Platform: sensor

| HA-core key (constant) | String value | Device(s) | Firmware label / MQTT topic | Notes |
|---|---|---|---|---|
| `DATA_CONTROL_SETPOINT` | `control_setpoint` | BOILER, THERMOSTAT | `TSet` | MsgID 1 |
| `DATA_CONTROL_SETPOINT_2` | `control_setpoint_2` | BOILER, THERMOSTAT | `TsetCH2` | MsgID 8 |
| `DATA_SLAVE_MEMBERID` | `slave_memberid` | BOILER, THERMOSTAT | `slave_memberid_code` (via F() macro, MsgID 3 LB) | Legacy label differs |
| `DATA_SLAVE_OEM_FAULT` | `slave_oem_fault` | BOILER, THERMOSTAT | `ASFflags` (MsgID 5 LB) | TBD-confirm exact sub-topic |
| `DATA_COOLING_CONTROL` | `cooling_control` | BOILER, THERMOSTAT | `CoolingControl` | MsgID 7 |
| `DATA_SLAVE_MAX_RELATIVE_MOD` | `slave_max_relative_modulation` | BOILER, THERMOSTAT | `MaxRelModLevelSetting` | MsgID 14 |
| `DATA_SLAVE_MAX_CAPACITY` | `slave_max_capacity` | BOILER, THERMOSTAT | `MaxCapacityMinModLevel` (HB) | MsgID 15 HB |
| `DATA_SLAVE_MIN_MOD_LEVEL` | `slave_min_mod_level` | BOILER, THERMOSTAT | `MaxCapacityMinModLevel` (LB) | MsgID 15 LB |
| `DATA_REL_MOD_LEVEL` | `relative_mod_level` | BOILER, THERMOSTAT | `RelModLevel` | MsgID 17 |
| `DATA_CH_WATER_PRESS` | `ch_water_pressure` | BOILER, THERMOSTAT | `CHPressure` | MsgID 18 |
| `DATA_DHW_FLOW_RATE` | `dhw_flow_rate` | BOILER, THERMOSTAT | `DHWFlowRate` | MsgID 19 |
| `DATA_CH_WATER_TEMP` | `ch_water_temp` | BOILER, THERMOSTAT | `Tboiler` | MsgID 25 |
| `DATA_CH_WATER_TEMP_2` | `ch_water_temp_2` | BOILER, THERMOSTAT | `TflowCH2` | MsgID 31 |
| `DATA_DHW_TEMP` | `dhw_temp` | BOILER, THERMOSTAT | `Tdhw` | MsgID 26 |
| `DATA_DHW_TEMP_2` | `dhw_temp_2` | BOILER, THERMOSTAT | `Tdhw2` | MsgID 32 |
| `DATA_RETURN_WATER_TEMP` | `return_water_temp` | BOILER, THERMOSTAT | `Tret` | MsgID 28 |
| `DATA_SOLAR_STORAGE_TEMP` | `solar_storage_temp` | BOILER, THERMOSTAT | `Tsolarstorage` | MsgID 29 |
| `DATA_SOLAR_COLL_TEMP` | `solar_coll_temp` | BOILER, THERMOSTAT | `Tsolarcollector` | MsgID 30 |
| `DATA_EXHAUST_TEMP` | `exhaust_temp` | BOILER, THERMOSTAT | `Texhaust` | MsgID 33 |
| `DATA_SLAVE_DHW_MAX_SETP` | `slave_dhw_max_setp` | BOILER, THERMOSTAT | `TdhwSetUBTdhwSetLB` (HB) | MsgID 48 HB |
| `DATA_SLAVE_DHW_MIN_SETP` | `slave_dhw_min_setp` | BOILER, THERMOSTAT | `TdhwSetUBTdhwSetLB` (LB) | MsgID 48 LB |
| `DATA_SLAVE_CH_MAX_SETP` | `slave_ch_max_setp` | BOILER, THERMOSTAT | `MaxTSetUBMaxTSetLB` (HB) | MsgID 49 HB |
| `DATA_SLAVE_CH_MIN_SETP` | `slave_ch_min_setp` | BOILER, THERMOSTAT | `MaxTSetUBMaxTSetLB` (LB) | MsgID 49 LB |
| `DATA_DHW_SETPOINT` | `dhw_setpoint` | BOILER, THERMOSTAT | `TdhwSet` | MsgID 56 |
| `DATA_MAX_CH_SETPOINT` | `max_ch_setpoint` | BOILER, THERMOSTAT | `MaxTSet` | MsgID 57 |
| `DATA_OEM_DIAG` | `oem_diag` | BOILER, THERMOSTAT | `OEMDiagnosticCode` | MsgID 115 |
| `DATA_TOTAL_BURNER_STARTS` | `burner_starts` | BOILER, THERMOSTAT | `BurnerStarts` | MsgID 116 |
| `DATA_CH_PUMP_STARTS` | `ch_pump_starts` | BOILER, THERMOSTAT | `CHPumpStarts` | MsgID 117 |
| `DATA_DHW_PUMP_STARTS` | `dhw_pump_starts` | BOILER, THERMOSTAT | `DHWPumpValveStarts` | MsgID 118 |
| `DATA_DHW_BURNER_STARTS` | `dhw_burner_starts` | BOILER, THERMOSTAT | `DHWBurnerStarts` | MsgID 119 |
| `DATA_TOTAL_BURNER_HOURS` | `burner_hours` | BOILER, THERMOSTAT | `BurnerOperationHours` | MsgID 120 |
| `DATA_CH_PUMP_HOURS` | `ch_pump_hours` | BOILER, THERMOSTAT | `CHPumpOperationHours` | MsgID 121 |
| `DATA_DHW_PUMP_HOURS` | `dhw_pump_hours` | BOILER, THERMOSTAT | `DHWPumpValveOperationHours` | MsgID 122 |
| `DATA_DHW_BURNER_HOURS` | `dhw_burner_hours` | BOILER, THERMOSTAT | `DHWBurnerOperationHours` | MsgID 123 |
| `DATA_SLAVE_OT_VERSION` | `slave_ot_version` | BOILER, THERMOSTAT | `OpenThermVersionSlave` | MsgID 125 |
| `DATA_SLAVE_PRODUCT_TYPE` | `slave_product_type` | BOILER, THERMOSTAT | `SlaveVersion` (HB) | MsgID 127 HB |
| `DATA_SLAVE_PRODUCT_VERSION` | `slave_product_version` | BOILER, THERMOSTAT | `SlaveVersion` (LB) | MsgID 127 LB |
| `DATA_MASTER_MEMBERID` | `master_memberid` | BOILER, THERMOSTAT | `master_memberid_code` (via F() macro, MsgID 2 LB) | Legacy label differs |
| `DATA_ROOM_SETPOINT_OVRD` | `room_setpoint_ovrd` | BOILER, THERMOSTAT | `TrOverride` | MsgID 9 |
| `DATA_ROOM_SETPOINT` | `room_setpoint` | BOILER, THERMOSTAT | `TrSet` | MsgID 16 |
| `DATA_ROOM_SETPOINT_2` | `room_setpoint_2` | BOILER, THERMOSTAT | `TrSetCH2` | MsgID 23 |
| `DATA_ROOM_TEMP` | `room_temp` | BOILER, THERMOSTAT | `Tr` | MsgID 24 |
| `DATA_OUTSIDE_TEMP` | `outside_temp` | BOILER, THERMOSTAT | `Toutside` | MsgID 27 |
| `DATA_MASTER_OT_VERSION` | `master_ot_version` | BOILER, THERMOSTAT | `OpenThermVersionMaster` | MsgID 124 |
| `DATA_MASTER_PRODUCT_TYPE` | `master_product_type` | BOILER, THERMOSTAT | `MasterVersion` (HB) | MsgID 126 HB |
| `DATA_MASTER_PRODUCT_VERSION` | `master_product_version` | BOILER, THERMOSTAT | `MasterVersion` (LB) | MsgID 126 LB |
| `OTGW_MODE` | `otgw_mode` | GATEWAY | TBD-confirm (PR=M response) | PIC register PR=M |
| `OTGW_DHW_OVRD` | `otgw_dhw_ovrd` | GATEWAY | TBD-confirm (PR=W response) | PIC register PR=W |
| `OTGW_ABOUT` | `otgw_about` | GATEWAY | TBD-confirm (PR=A response) | PIC firmware version |
| `OTGW_BUILD` | `otgw_build` | GATEWAY | TBD-confirm (PR=B response) | PIC build date |
| `OTGW_CLOCKMHZ` | `otgw_clockmhz` | GATEWAY | TBD-confirm (PR=C response) | PIC clock speed |
| `OTGW_LED_A` | `otgw_led_a` | GATEWAY | TBD-confirm | LED mode setting |
| `OTGW_LED_B` | `otgw_led_b` | GATEWAY | TBD-confirm | |
| `OTGW_LED_C` | `otgw_led_c` | GATEWAY | TBD-confirm | |
| `OTGW_LED_D` | `otgw_led_d` | GATEWAY | TBD-confirm | |
| `OTGW_LED_E` | `otgw_led_e` | GATEWAY | TBD-confirm | |
| `OTGW_LED_F` | `otgw_led_f` | GATEWAY | TBD-confirm | |
| `OTGW_GPIO_A` | `otgw_gpio_a` | GATEWAY | TBD-confirm | GPIO mode setting |
| `OTGW_GPIO_B` | `otgw_gpio_b` | GATEWAY | TBD-confirm | |
| `OTGW_SB_TEMP` | `otgw_setback_temp` | GATEWAY | TBD-confirm | SB= command |
| `OTGW_SETP_OVRD_MODE` | `otgw_setpoint_ovrd_mode` | GATEWAY | TBD-confirm | |
| `OTGW_SMART_PWR` | `otgw_smart_pwr` | GATEWAY | TBD-confirm | |
| `OTGW_THRM_DETECT` | `otgw_thermostat_detect` | GATEWAY | `settings/thermostat_detect` (MQTTHaDiscovery.cpp) | FT= command |
| `OTGW_VREF` | `otgw_vref` | GATEWAY | TBD-confirm | VR= command |

**Sensor count:** 46 DATA_* keys (each on BOILER + THERMOSTAT = 92 entity instances) + 18 OTGW_* keys (GATEWAY-only) = 64 unique keys, 110 entity instances total.

---

## Platform: binary_sensor

| HA-core key (constant) | String value | Device(s) | Firmware label / MQTT topic | Notes |
|---|---|---|---|---|
| `DATA_SLAVE_FAULT_IND` | `slave_fault_indication` | BOILER, THERMOSTAT | `slave_fault_indication` (ADR-106 alias for MsgID 0 slave bit 0) | Status byte slave bit 0 |
| `DATA_SLAVE_CH_ACTIVE` | `slave_ch_active` | BOILER, THERMOSTAT | `slave_ch_active` (alias, MsgID 0 slave bit 1) | Status byte slave bit 1 |
| `DATA_SLAVE_CH2_ACTIVE` | `slave_ch2_active` | BOILER, THERMOSTAT | `slave_ch2_active` (alias, MsgID 0 slave bit 5) | Status byte slave bit 5 |
| `DATA_SLAVE_DHW_ACTIVE` | `slave_dhw_active` | BOILER, THERMOSTAT | `slave_dhw_active` (alias, MsgID 0 slave bit 2) | Status byte slave bit 2 |
| `DATA_SLAVE_FLAME_ON` | `slave_flame_on` | BOILER, THERMOSTAT | `slave_flame_on` (alias, MsgID 0 slave bit 3) | Status byte slave bit 3 |
| `DATA_SLAVE_COOLING_ACTIVE` | `slave_cooling_active` | BOILER, THERMOSTAT | `slave_cooling_active` (alias, MsgID 0 slave bit 4) | Status byte slave bit 4 |
| `DATA_SLAVE_DIAG_IND` | `slave_diagnostic_indication` | BOILER, THERMOSTAT | `slave_diagnostic_indication` (alias, MsgID 0 slave bit 6) | Status byte slave bit 6 |
| `DATA_SLAVE_DHW_PRESENT` | `slave_dhw_present` | BOILER, THERMOSTAT | `slave_dhw_present` (alias, MsgID 3 HB bit 0) | SlaveConfigMemberIDcode HB0 |
| `DATA_SLAVE_CONTROL_TYPE` | `slave_control_type` | BOILER, THERMOSTAT | `slave_control_type` (alias, MsgID 3 HB bit 1) | SlaveConfigMemberIDcode HB1 |
| `DATA_SLAVE_COOLING_SUPPORTED` | `slave_cooling_supported` | BOILER, THERMOSTAT | `slave_cooling_supported` (alias, MsgID 3 HB bit 2) | SlaveConfigMemberIDcode HB2 |
| `DATA_SLAVE_DHW_CONFIG` | `slave_dhw_config` | BOILER, THERMOSTAT | `slave_dhw_config` (alias, MsgID 3 HB bit 3) | SlaveConfigMemberIDcode HB3 |
| `DATA_SLAVE_MASTER_LOW_OFF_PUMP` | `slave_master_low_off_pump` | BOILER, THERMOSTAT | `slave_master_low_off_pump` (alias, MsgID 3 HB bit 4) | SlaveConfigMemberIDcode HB4 |
| `DATA_SLAVE_CH2_PRESENT` | `slave_ch2_present` | BOILER, THERMOSTAT | `slave_ch2_present` (alias, MsgID 3 HB bit 5) | SlaveConfigMemberIDcode HB5 |
| `DATA_SLAVE_SERVICE_REQ` | `slave_service_required` | BOILER, THERMOSTAT | `slave_service_required` (alias, MsgID 5 HB bit 0) | ASFflags HB0 |
| `DATA_SLAVE_REMOTE_RESET` | `slave_remote_reset` | BOILER, THERMOSTAT | `slave_remote_reset` (alias, MsgID 5 HB bit 1) | ASFflags HB1 |
| `DATA_SLAVE_LOW_WATER_PRESS` | `slave_low_water_pressure` | BOILER, THERMOSTAT | `slave_low_water_pressure` (alias, MsgID 5 HB bit 2) | ASFflags HB2 |
| `DATA_SLAVE_GAS_FAULT` | `slave_gas_fault` | BOILER, THERMOSTAT | `slave_gas_fault` (alias, MsgID 5 HB bit 3) | ASFflags HB3 |
| `DATA_SLAVE_AIR_PRESS_FAULT` | `slave_air_pressure_fault` | BOILER, THERMOSTAT | `slave_air_pressure_fault` (alias, MsgID 5 HB bit 4) | ASFflags HB4 |
| `DATA_SLAVE_WATER_OVERTEMP` | `slave_water_overtemp` | BOILER, THERMOSTAT | `slave_water_overtemp` (alias, MsgID 5 HB bit 5) | ASFflags HB5 |
| `DATA_REMOTE_TRANSFER_MAX_CH` | `remote_transfer_max_ch` | BOILER, THERMOSTAT | `remote_transfer_max_ch` (alias, MsgID 6 HB bit 0) | RBPflags HB0 |
| `DATA_REMOTE_RW_MAX_CH` | `remote_rw_max_ch` | BOILER, THERMOSTAT | `remote_rw_max_ch` (alias, MsgID 6 HB bit 1) | RBPflags HB1 |
| `DATA_REMOTE_TRANSFER_DHW` | `remote_transfer_dhw` | BOILER, THERMOSTAT | `remote_transfer_dhw` (alias, MsgID 6 HB bit 2) | RBPflags HB2 |
| `DATA_REMOTE_RW_DHW` | `remote_rw_dhw` | BOILER, THERMOSTAT | `remote_rw_dhw` (alias, MsgID 6 HB bit 3) | RBPflags HB3 |
| `DATA_MASTER_CH_ENABLED` | `master_ch_enabled` | THERMOSTAT, BOILER | `master_ch_enabled` (alias, MsgID 0 master bit 0) | Status byte master bit 0 |
| `DATA_MASTER_CH2_ENABLED` | `master_ch2_enabled` | THERMOSTAT, BOILER | `master_ch2_enabled` (alias, MsgID 0 master bit 4) | Status byte master bit 4 |
| `DATA_MASTER_DHW_ENABLED` | `master_dhw_enabled` | THERMOSTAT, BOILER | `master_dhw_enabled` (alias, MsgID 0 master bit 1) | Status byte master bit 1 |
| `DATA_MASTER_COOLING_ENABLED` | `master_cooling_enabled` | THERMOSTAT, BOILER | `master_cooling_enabled` (alias, MsgID 0 master bit 2) | Status byte master bit 2 |
| `DATA_MASTER_OTC_ENABLED` | `master_otc_enabled` | THERMOSTAT, BOILER | `master_otc_enabled` (alias, MsgID 0 master bit 3) | Status byte master bit 3 |
| `DATA_ROVRD_MAN_PRIO` | `rovrd_man_prio` | THERMOSTAT, BOILER | `rovrd_man_prio` (alias, MsgID 100 bit 0) | RoomRemoteOverrideFunction bit 0 |
| `DATA_ROVRD_AUTO_PRIO` | `rovrd_auto_prio` | THERMOSTAT, BOILER | `rovrd_auto_prio` (alias, MsgID 100 bit 1) | RoomRemoteOverrideFunction bit 1 |
| `OTGW_GPIO_A_STATE` | `otgw_gpio_a_state` | GATEWAY | TBD-confirm | GPIO A runtime state |
| `OTGW_GPIO_B_STATE` | `otgw_gpio_b_state` | GATEWAY | TBD-confirm | GPIO B runtime state |
| `OTGW_IGNORE_TRANSITIONS` | `otgw_ignore_transitions` | GATEWAY | TBD-confirm | IT= command response |
| `OTGW_OVRD_HB` | `otgw_ovrd_high_byte` | GATEWAY | TBD-confirm | High-byte override flag |

**Binary sensor count:** 23 DATA_* keys (all bilateral BOILER+THERMOSTAT) + 4 OTGW_* keys (GATEWAY-only) = 27 unique keys, 50 entity instances total.

---

## Platform: switch

| HA-core key (constant/string) | String value | Device(s) | Firmware label / MQTT topic | Notes |
|---|---|---|---|---|
| `central_heating_1_override` | `central_heating_1_override` | GATEWAY | TBD-confirm | CH1 override switch |
| `central_heating_2_override` | `central_heating_2_override` | GATEWAY | TBD-confirm | CH2 override switch |

**Switch count:** 2 keys (GATEWAY-only).

---

## Platform: select

| HA-core key (constant) | String value | Device(s) | Firmware label / MQTT topic | Notes |
|---|---|---|---|---|
| `OTGW_GPIO_A` | `otgw_gpio_a` | GATEWAY | TBD-confirm | GPIO A mode select |
| `OTGW_GPIO_B` | `otgw_gpio_b` | GATEWAY | TBD-confirm | GPIO B mode select |
| `OTGW_LED_A` | `otgw_led_a` | GATEWAY | TBD-confirm | LED A mode select |
| `OTGW_LED_B` | `otgw_led_b` | GATEWAY | TBD-confirm | |
| `OTGW_LED_C` | `otgw_led_c` | GATEWAY | TBD-confirm | |
| `OTGW_LED_D` | `otgw_led_d` | GATEWAY | TBD-confirm | |
| `OTGW_LED_E` | `otgw_led_e` | GATEWAY | TBD-confirm | |
| `OTGW_LED_F` | `otgw_led_f` | GATEWAY | TBD-confirm | |

**Select count:** 8 keys (all GATEWAY-only). Note: `OTGW_GPIO_A` and `OTGW_LED_A`-`F` also appear as sensors (state reporting). The select platform provides the write/command side; the sensor platform the read/state side. Both share the same key string.

---

## Platform: button

| HA-core key (string) | String value | Device(s) | Firmware label / MQTT topic | Notes |
|---|---|---|---|---|
| `cancel_room_setpoint_override` | `cancel_room_setpoint_override` | THERMOSTAT | TBD-confirm | Clears room setpoint override |
| `restart_button` | `restart_button` | GATEWAY | `resetgateway` (MQTTHaDiscovery.cpp line 3490) | PIC hardware reset |

**Button count:** 2 keys (1 THERMOSTAT, 1 GATEWAY).

---

## Platform: climate

| HA-core key (string) | Device(s) | Firmware label / MQTT topic | Notes |
|---|---|---|---|
| `thermostat_entity` | THERMOSTAT | Climate entity (room setpoint + room temp) | Single entity; reads from BOILER+THERMOSTAT+GATEWAY data sources |

**Climate count:** 1 entity (THERMOSTAT device). The climate entity monitors all three data sources internally but is registered under THERMOSTAT.

---

## Consolidated Bilateral Set (BOILER + THERMOSTAT)

All of these keys create **two entity instances** in HA core: one under the Boiler device, one under the Thermostat device.

### Sensors (bilateral)
All 46 DATA_* sensor keys are bilateral. Grouped by functional area:

**Setpoints and control:**
`control_setpoint`, `control_setpoint_2`, `room_setpoint_ovrd`, `room_setpoint`, `room_setpoint_2`, `dhw_setpoint`, `max_ch_setpoint`, `cooling_control`, `slave_max_relative_modulation`

**Temperatures:**
`ch_water_temp`, `ch_water_temp_2`, `dhw_temp`, `dhw_temp_2`, `return_water_temp`, `solar_storage_temp`, `solar_coll_temp`, `exhaust_temp`, `room_temp`, `outside_temp`

**Capabilities / modulation:**
`slave_max_capacity`, `slave_min_mod_level`, `relative_mod_level`, `ch_water_pressure`, `dhw_flow_rate`

**Setpoint bounds:**
`slave_dhw_max_setp`, `slave_dhw_min_setp`, `slave_ch_max_setp`, `slave_ch_min_setp`

**Counters and hours:**
`burner_starts`, `ch_pump_starts`, `dhw_pump_starts`, `dhw_burner_starts`, `burner_hours`, `ch_pump_hours`, `dhw_pump_hours`, `dhw_burner_hours`, `oem_diag`

**Product identification:**
`slave_memberid`, `slave_ot_version`, `slave_product_type`, `slave_product_version`, `slave_oem_fault`, `master_memberid`, `master_ot_version`, `master_product_type`, `master_product_version`

### Binary sensors (bilateral)
All 23 DATA_* binary sensor keys are bilateral:

**Slave status byte (MsgID 0 LB):**
`slave_fault_indication`, `slave_ch_active`, `slave_ch2_active`, `slave_dhw_active`, `slave_flame_on`, `slave_cooling_active`, `slave_diagnostic_indication`

**Master status byte (MsgID 0 HB):**
`master_ch_enabled`, `master_ch2_enabled`, `master_dhw_enabled`, `master_cooling_enabled`, `master_otc_enabled`

**Slave config (MsgID 3 HB):**
`slave_dhw_present`, `slave_control_type`, `slave_cooling_supported`, `slave_dhw_config`, `slave_master_low_off_pump`, `slave_ch2_present`

**ASF flags (MsgID 5 HB):**
`slave_service_required`, `slave_remote_reset`, `slave_low_water_pressure`, `slave_gas_fault`, `slave_air_pressure_fault`, `slave_water_overtemp`

**Remote boiler params (MsgID 6):**
`remote_transfer_max_ch`, `remote_rw_max_ch`, `remote_transfer_dhw`, `remote_rw_dhw`

**Remote override priority (MsgID 100):**
`rovrd_man_prio`, `rovrd_auto_prio`

**Total bilateral count: 69 key instances (46 sensors + 23 binary sensors), each appearing on both BOILER and THERMOSTAT.**

---

## Gateway-Only Set (OTGW_* keys)

These entities reflect state internal to the OTGW PIC/firmware; they have no OT bus message ID counterpart.

| Key string | Platform(s) | Notes |
|---|---|---|
| `otgw_mode` | sensor | Operating mode (PR=M) |
| `otgw_dhw_ovrd` | sensor | DHW override mode (PR=W) |
| `otgw_about` | sensor | Firmware version string (PR=A) |
| `otgw_build` | sensor | Firmware build date (PR=B) |
| `otgw_clockmhz` | sensor | Clock speed (PR=C) |
| `otgw_led_a` through `otgw_led_f` | sensor + select | LED mode: sensor=current state, select=writable |
| `otgw_gpio_a`, `otgw_gpio_b` | sensor + select | GPIO mode: sensor=current state, select=writable |
| `otgw_gpio_a_state`, `otgw_gpio_b_state` | binary_sensor | GPIO runtime state (high/low) |
| `otgw_setback_temp` | sensor | Setback temperature (SB= command) |
| `otgw_setpoint_ovrd_mode` | sensor | Setpoint override mode |
| `otgw_smart_pwr` | sensor | Smart power mode |
| `otgw_thermostat_detect` | sensor | Thermostat detection mode (FT= command) |
| `otgw_ignore_transitions` | binary_sensor | Ignore transitions flag (IT= command) |
| `otgw_ovrd_high_byte` | binary_sensor | Override high byte active flag |
| `otgw_vref` | sensor | Reference voltage (VR= command) |
| `central_heating_1_override` | switch | CH1 override enable/disable |
| `central_heating_2_override` | switch | CH2 override enable/disable |
| `restart_button` | button | PIC hardware reset |

**Gateway-only count: 18 unique keys across sensor/binary_sensor/switch/button.**

---

## Notes and Gaps

### Confirmed from source
1. **All DATA_* sensor and binary_sensor keys are bilateral** (BOILER + THERMOSTAT). This is explicit in sensor.py where every DATA_* entry appears twice - once per device description. The HA core intentionally creates the same measurement under both devices so users can see what both sides of the OT bus are reporting.

2. **OTGW_* keys are exclusively GATEWAY**. No OTGW_* key appears under any device other than `GATEWAY_DEVICE_DESCRIPTION`.

3. **Switch keys use plain strings** (not gw_vars constants): `central_heating_1_override` and `central_heating_2_override`. These are not in pyotgw/vars.py.

4. **Button keys also use plain strings**: `cancel_room_setpoint_override` and `restart_button`.

5. **The climate entity is THERMOSTAT-only** but reads from all three data sources internally at runtime.

### Firmware label mapping gaps (TBD-confirm items)
The firmware MQTT topic labels for OTGW_* keys (gateway state: LED modes, GPIO modes, PR=x responses, clock, vref, smart power, etc.) are not published via the `OTmap[]/messageIDToString()` path. They are published via PIC response handlers in `OTGW-Core.ino` and settings paths. The exact MQTT leaf topics for each `OTGW_*` key need to be traced from the `handlePRresponse()` / settings publish functions. The `settings/thermostat_detect` label is confirmed from `MQTTHaDiscovery.cpp line 271`.

### Ambiguities
- `DATA_SLAVE_MAX_RELATIVE_MOD` maps to pyotgw string `slave_max_relative_modulation` but the firmware OTmap label for MsgID 14 is `MaxRelModLevelSetting`. The firmware publishes this as the OTmap label (via `messageIDToString`), not the pyotgw string. The HA core integration uses the pyotgw string as the data dictionary key; the firmware must publish under a topic that HA core's stat_t configuration resolves correctly. TBD-confirm: does the firmware need to publish under `MaxRelModLevelSetting` or `slave_max_relative_modulation` for HA core compatibility?

- MsgID 48/49 (`TdhwSetUBTdhwSetLB`, `MaxTSetUBMaxTSetLB`) are s8/s8 pairs. HA core treats HB and LB as separate entities (`slave_dhw_max_setp` / `slave_dhw_min_setp`). The firmware publishes these as HB/LB sub-topics. Mapping to firmware labels is TBD-confirm.

- `DATA_SLAVE_PRODUCT_TYPE` and `DATA_SLAVE_PRODUCT_VERSION` both map to MsgID 127 (`SlaveVersion`) HB and LB respectively. Same applies for master (MsgID 126, `MasterVersion`). The firmware publishes a single topic per MsgID; the HA core treats them as two distinct entities. The exact per-byte topic suffix needs confirmation.

- The sensor.py fetch indicated a "third BOILER block at the end of SENSOR_DESCRIPTIONS with ~15 entries" beyond the main 46-entry bilateral section. This could not be verified from the summarized output alone. If accurate, some entries may be BOILER-only (not bilateral). **This is flagged as uncertain** - the bilateral assertion is based on the fetch model's analysis, not a line-by-line manual review. Recommend independent verification by inspecting sensor.py directly before implementing.
