# SAT topics without a Home Assistant entity: audit (TASK-1486)

Date: 2026-10-29. Source: `dev` at `fc06acb6f` (alpha.535), code read only, no device.
Origin: TASK-1466 decision 6 (Robert, 2026-10-29). Implementation of the additions is a separate task (draft at the end).

## Summary

| | Count |
|---|---|
| Distinct `sat/*` state topics published by `SAT*.ino` (`F("sat/...")` literals) | 178 |
| `sat/*` paths referenced by `MQTTHaDiscovery.cpp` (state, command, retired) | 95 |
| PV-boost topics discovered separately by `MQTTstuff.ino` | 9 |
| Published state topics with no Home Assistant entity | **91** |
| Decision: add an entity | 13 |
| Decision: open, for Robert (recommendation given) | 10 |
| Decision: MQTT-only | 68 |

The task description said 153 published and 63 discovered (2026-10-29 count). This audit uses a different method, so the numbers differ:
- 178 counts every distinct `F("sat/...")` string in `SATcontrol.ino`, `SATble.ino`, `SATpressure.ino` and `SATweather.ino`. That includes conditional topics (PV boost, auto-tune, areas) and the BLE, pressure and weather files.
- 63 counted only the `"sat/..."` labels in the sensor tables. The 13 switches (`streamSatSwitchDiscovery`, `MQTTHaDiscovery.cpp:3373-3460`), the select (`:3466`) and the climate `json_attributes_topic` (`:3108`) also reference `sat/*` paths, so 95 paths in total.
- 97 published topics match no discovery path at all. Three more match only because they share a name with a switch **command** topic: `sat/solar_gain`, `sat/auto_tune` and `sat/simulation`. Command topics live under the subscribe namespace (`cmd_t` = `mqttSubTopic` + path, `:3343-3346`). State topics live under the publish namespace (`stat_t` = `mqttPubTopic` + path, `:3349-3352`). So these three published state topics have no entity either: 97 + 3 = 100.
- A second discovery path, the PV-boost discovery in `MQTTstuff.ino` (TASK-640), covers 9 of those 100 with `stat_t` = `<pub>/sat/pv_*` (`MQTTstuff.ino:2774-2786`):
  - always: switch `pv_boost_enabled` with `1`/`0` payloads, and numbers `pv_boost_threshold_w`, `pv_boost_hold_s`, `pv_boost_delta_c`, `pv_boost_max_indoor_c`, `pv_boost_max_duration_min`;
  - only while PV boost is enabled: sensors `pv_surplus_w` and `pv_boost_applied_c`, and binary sensor `pv_boost_active`.

  100 - 9 = **91**. The same file also discovers the zone entities (`sat/zone/<n>/*`, `:2613`, `:2634`). Those topics are not among the 178 `F()` literals, so the count does not change.

Reproduce:
```bash
cd src/OTGW-firmware
grep -ohE 'F\("sat/[^"]+"\)' SAT*.ino | sed -E 's/F\("(.*)"\)/\1/' | sort -u > pub.txt   # 178
grep -oE 'sat/[a-z0-9_/]+' MQTTHaDiscovery.cpp | sort -u > disc.txt                  # 95
comm -23 pub.txt disc.txt                                                             # 97
# plus sat/solar_gain, sat/auto_tune, sat/simulation (command-path collision, see above)
# minus the 9 sat/pv_* topics discovered in MQTTstuff.ino (see above)
```

## Rules used for the decisions

1. **thermo-nova is the ceiling.** Upstream SAT exposes few entities. Everything else lives in its config flow or as climate attributes. The thermo-nova citations below are `file:line` relative to `custom_components/sat`. They were read from the local copy `other-projects/SAT-releases-thermo-nova/` in the main tree, because the docs worktree has no populated `other-projects`. CLAUDE.md records that copy as identical to the pinned commit `7beaaf531ee9…`; this audit did not re-verify that.
   - Binary sensors, all device class `PROBLEM` except the window sensor: `SatControlSetpointSyncSensor` (`binary_sensor.py:91`), `SatRelativeModulationSyncSensor` (`:125`), `SatCentralHeatingSyncSensor` (`:162`), `SatPressureHealthSensor` (`:200`), `SatDeviceHealthSensor` (`:391`), `SatCycleHealthSensor` (`:414`), and `SatWindowSensor` with class `WINDOW` (`:450`, `:464-466`).
   - Sensors: Requested Setpoint (`sensor.py:71`, temperature °C), PID (`:101`), Current Power (`:184`, kW), Current Consumption (`:219`, `GAS`, m³, only when min and max consumption > 0, `:62-65`), Heating Curve (`:267`), Heating Curve Recommendation (`:299`), Error Value (`:367`), Device Manufacturer (`:402`), Cycle Status (`:421`), Device Status (`:470`).
   - Numbers: hot-water setpoint (`number.py:24`), maximum setpoint (`number.py:77`).
   - Climate `extra_state_attributes` (`climate.py:235`): `pre_custom_temperature`, `pre_activity_temperature`, `rooms`, `current_humidity`, `setpoint`, `summer_simmer_index`, `summer_simmer_perception`, `valves_open`, `heating_curve`, `requested_setpoint`, `outside_temperature`, `solar_gain_active`, `solar_gain_sun_elevation`, `solar_gain_rise_per_hour`, `relative_modulation_value`, `relative_modulation_state`, `pulse_width_modulation_*`.
2. **Health and sync entities follow ON = problem** (TASK-1466 decision 4), binary sensor with device class `problem`.
3. **Setting mirrors stay MQTT-only.** Settings are changed through the web UI, REST or the MQTT command, and thermo-nova keeps them in its config flow. The only settings thermo-nova exposes as entities are its two numbers.
4. **No duplicate entities.** If the value already has an entity (OT entity, SAT sensor, switch state or climate attribute), the topic stays MQTT-only.
5. **Firmware-only safety states have no thermo-nova counterpart.** Adding them is Robert's call, so they are listed as open with a recommendation.

`docs/sat/sergeant-d-corrections.md` was searched for entity, health and sync entries. None of its entries governs which Home Assistant entities exist; they concern control behaviour. No conflict.

Columns: **Gate** = an extra condition on top of "SAT enabled". `satPublishMQTT()` returns early while SAT is disabled (`SATcontrol.ino:3299`). Only `satPublishCoreState()` (`:3248-3271`) publishes while SAT is off: enabled, fallback_active, push_refused, room_temp_lost, ot_link_lost, mode, control_mode and preset. "none" means published whenever SAT is enabled. A topic is published only under its gate. An entity on a gated topic shows "unknown" in HA until the gate opens. **Payload** = what the firmware publishes today.

## 1. Add an entity (13)

| Topic | Published at | Payload | Gate | Entity | thermo-nova | Depends on |
|---|---|---|---|---|---|---|
| `sat/device_health` | `SATcontrol.ino:3646` | `ON`/`OFF` | none | binary_sensor, class `problem`, diagnostic | `binary_sensor.py:391` | **TASK-1485** (today ON = boiler off; must become ON = insufficient data, `binary_sensor.py:404-406`) |
| `sat/cycle_health` | `:3655` | `ON`/`OFF` | none | binary_sensor, `problem`, diagnostic | `binary_sensor.py:414` | none (already ON = problem) |
| `sat/pressure_health` | `:3534` | `ON`/`OFF` | none | binary_sensor, `problem`, diagnostic | `binary_sensor.py:200` | **TASK-1485** (today ON = healthy; an entity before the flip shows inverted) |
| `sat/setpoint_sync` | `:3740` | `ON`/`OFF` | none | binary_sensor, `problem`, diagnostic | `binary_sensor.py:91` | none |
| `sat/modulation_sync` | `:3752` | `ON`/`OFF` | none | binary_sensor, `problem`, diagnostic | `binary_sensor.py:125` | none |
| `sat/ch_sync` | `:3764` | `ON`/`OFF` | none | binary_sensor, `problem`, diagnostic | `binary_sensor.py:162` | none |
| `sat/window_open` | `:3507` | `true`/`false` | none | binary_sensor, class `window` | `binary_sensor.py:450` | none |
| `sat/requested_setpoint` | `:3705` | float °C | none | sensor, `temperature`, °C, `measurement` | `sensor.py:71` | none |
| `sat/heating_curve_recommendation` | `:3565` | string | none | sensor, enum (no device class) | `sensor.py:299` | **TASK-1485** (lower-case values first, so the entity's states don't change after it ships) |
| `sat/manufacturer` | `:3581` | string | none | sensor, diagnostic | `sensor.py:402` | none |
| `sat/consumption` | `:3722` | float | dead today (min/max hard-coded 0) | sensor, `volume_flow_rate`, m³/h, `measurement` | `sensor.py:219` (`GAS`, m³) | **TASK-1484**. Note: thermo-nova uses class `GAS` with unit m³ for a value that is a rate (m³/h). The firmware entity should use `volume_flow_rate` m³/h; HA rejects `gas` with a rate unit. Reviewer: confirm. |
| `sat/max_setpoint` | `:3698` | float °C | none | sensor, `temperature`, °C, diagnostic (read-only) | `number.py:77` (number) | none. No MQTT command exists for it (`kSatMqttCmds`, `MQTTstuff.ino:654`), so it is a sensor, not a number. A number needs a new command: out of scope. |
| `sat/pressure_drop_rate` | `:3531` | float bar/h | none | sensor, no device class, unit `bar/h`, diagnostic | attribute of `binary_sensor.py:200` | none. The input of pressure health; useful when the problem flag goes ON. |

## 2. Open: Robert decides (10)

The first seven are firmware safety or fault states with no thermo-nova counterpart. The last three are control surfaces.

| Topic | Published at | Payload | Recommendation | Reason |
|---|---|---|---|---|
| `sat/fallback_active` | `SATcontrol.ino:3255` | `true`/`false` | **Add**: binary_sensor `problem`, diagnostic | Tells the user SAT handed control back. Today only visible over MQTT or in the UI. |
| `sat/room_temp_lost` | `:3257` | `true`/`false` | **Add**: binary_sensor `problem`, diagnostic | Root cause of a fallback; worth an automation. |
| `sat/ot_link_lost` | `:3258` | `true`/`false` | **Add**: binary_sensor `problem`, diagnostic | Same. |
| `sat/push_refused` | `:3256` | `true`/`false` | **Add**: binary_sensor `problem`, diagnostic | Same. |
| `sat/pressure_alarm` | `:3532` | `true`/`false` | **MQTT-only** | Overlaps `pressure_health` (ON = problem after TASK-1485). Two problem entities for one fault confuse users. |
| `sat/setpoint_mismatch` | `:3542` | `true`/`false` | **MQTT-only** | A second, separately debounced setpoint check: `satCheckSetpointSync()` (`:4205-4228`), with a 1.0 °C threshold, 60 s debounce and no `bActive` term. `setpoint_sync` (`:3729-3740`) uses 0.5 °C and requires `bActive`. Two problem entities for one fault confuse users, so the entity is `setpoint_sync`, which matches thermo-nova. Whether two checks should exist at all is a separate cleanup question. |
| `sat/modulation_reliable` | `:3541` | `true`/`false` | **MQTT-only** | ON = good, the opposite of the problem convention. An entity would need an inverted template. Low user value. |
| `sat/enabled` | `:3252` | `true`/`false` | **Add**: switch, command `enabled` (exists in `kSatMqttCmds`) | The only SAT on/off without an HA entity. It reuses `streamSatBoolSwitch` (true/false payloads match). In thermo-nova this is the climate entity's hvac mode; here the climate entity is the OT thermostat, so a switch is the equivalent. Turning SAT on from HA is a behaviour surface, so this is Robert's decision. |
| `sat/preset` | `:3271` | string | **Add**: select, command `preset` (exists) | thermo-nova shows presets as climate `preset_mode`. Our climate entity does not carry SAT presets. A select reuses `streamSatSelectDiscovery`. |
| `sat/control_mode` | `:3270` | `auto`/`continuous`/`pwm` | **MQTT-only** | The actual mode is the discovered `sat/mode`. The requested mode is a tuning knob, and thermo-nova has no entity for it. |

If Robert accepts every recommendation, the additions grow from 13 to 19 (4 binaries, 1 switch, 1 select).

## 3. MQTT-only (68)

### 3a. Duplicates of an existing entity or of another topic (14)

| Topic | Published at | Duplicates |
|---|---|---|
| `sat/pressure` | `:3530` | `sat/ch_pressure` (discovered, `MQTTHaDiscovery.cpp:333`). This one is the smoothed value. |
| `sat/humidity` | `:3600` | `sat/ble_humidity` (discovered) and the climate attribute `current_humidity` (`climate.py:235`) |
| `sat/humidity_valid` | `:3601` | Validity flag for the above |
| `sat/current_modulation` | `:3540` | OT RelModLevel entity |
| `sat/auto_tune` | `:3612` | `ON`/`OFF` duplicate of `sat/auto_tune_enable` (switch state) |
| `sat/simulation` | `:3606` | `ON`/`OFF` duplicate of `sat/simulation_enable` (switch state) |
| `sat/dhw_setpoint` | `:3697` | The DHW setpoint is controllable through the OT `dhw_control` climate entity (`MQTTHaDiscovery.cpp:2960`); `TdhwSet` (`:105`) is its read-only sensor. thermo-nova's number (`number.py:24`) maps to the same boiler value. |
| `sat/ssi` | `:3663` | Same value as `sat/summer_simmer_index`; **TASK-1485 removes it** |
| `sat/curve_recommendation` | `:3552` | Same information as `sat/heating_curve_recommendation` (added in §1); TASK-1485 keeps both topics |
| `sat/curve_recommendation_attributes` | `:3562` | JSON attributes; see the implementation note (attach to the §1 recommendation entity) |
| `sat/manufacturer_id` | `:3797` | Numeric form of `sat/manufacturer` (added in §1) |
| `sat/heating_curve_coeff` | `:3773` | Setting mirror. TASK-1485 renames its command; the computed curve is the discovered `sat/heating_curve` |
| `sat/estimated_room` | `:3586` | Thermal-model estimate; the room temperature entity is `sat/room_temp` |
| `sat/sim/last_cmd` | `:3610` | Debug output while simulating (gate: simulation on, not retained) |

### 3b. Runtime telemetry that thermo-nova keeps as climate attribute or internal state (18)

These are diagnostic values for logs and Grafana. An entity per value would add noise to every user's HA. Moving the thermo-nova attribute set into `sat/climate_attributes` (which the climate entity already carries) is the cheaper way to reach parity, and is listed as an optional item in the draft task.

| Topic | Published at | Gate | thermo-nova |
|---|---|---|---|
| `sat/pre_custom_temperature` | `:3501` | > 0 | climate attribute |
| `sat/pre_activity_temperature` | `:3504` | > 0 | climate attribute |
| `sat/summer_simmer_index` | `:3664` | fresh humidity | climate attribute |
| `sat/summer_simmer_perception` | `:3665` | fresh humidity | climate attribute |
| `sat/solar_gain` | `:3591` | none | climate attribute `solar_gain_active` |
| `sat/solar_gain_sun_elevation` | `:3594` | sun elevation valid | climate attribute |
| `sat/indoor_rise_rate` | `:3592` | none | climate attribute `solar_gain_rise_per_hour` |
| `sat/modulation_state` | `:3684` | none | climate attribute `relative_modulation_state` |
| `sat/pwm_state` | `:3691` | none | climate attribute `pulse_width_modulation_state` |
| `sat/summer_active` | `:3597` | none | internal (`summer_simmer.py`) |
| `sat/summer_hours_above` | `:3598` | none | internal |
| `sat/error_mean` | `:3567` | none | internal (curve recommendation input) |
| `sat/error_stddev` | `:3568` | none | internal |
| `sat/thermal_drop_rate` | `:3584` | none | firmware-only |
| `sat/thermal_model_valid` | `:3585` | none | firmware-only |
| `sat/auto_tune_score` | `:3614` | auto-tune on | firmware-only |
| `sat/auto_tune_active` | `:3616` | auto-tune on | firmware-only |
| `sat/pv_surplus_valid` | `:3514` | PV boost on | firmware-only; its siblings `pv_surplus_w` and `pv_boost_active` already have entities (`MQTTstuff.ino:2783-2786`) |

### 3c. More runtime values (4)

| Topic | Published at | Gate | Reason |
|---|---|---|---|
| `sat/area/0`, `sat/area/1`, `sat/area/2`, `sat/area/3` (4 topics) | `:3637-3640` | multi-area on, count | The inputs come from HA sensors, so mirroring them back adds nothing |

### 3d. Setting mirrors (32)

These mirror a persisted setting so MQTT clients can read it. thermo-nova keeps all of them in its config flow, not as entities. Changing them goes through the UI, REST or the MQTT command.

`sat/boiler_capacity` `:3781`, `sat/boiler_efficiency` `:3784`, `sat/boiler_rated_kw` `:3783`, `sat/control_interval` `:3775`, `sat/cycles_per_hour` `:3625`, `sat/deadband` `:3774`, `sat/dhw_capacity` `:3782`, `sat/error_monitoring` `:3623`, `sat/flame_off_offset` `:3777`, `sat/flow_offset` `:3778`, `sat/heating_mode` `:3624`, `sat/humidity_timeout_s` `:3802`, `sat/max_modulation` `:3776`, `sat/max_pressure` `:3788`, `sat/max_pressure_drop` `:3789`, `sat/min_pressure` `:3787`, `sat/mod_sup_delay` `:3779`, `sat/mod_sup_offset` `:3780`, `sat/preset_activity` `:3794`, `sat/preset_away` `:3792`, `sat/preset_comfort` `:3790`, `sat/preset_eco` `:3791`, `sat/preset_home` `:3795`, `sat/preset_sleep` `:3793`, `sat/sensor_max_age` `:3622`, `sat/solar_freeze_integral` `:3627`, `sat/summer_threshold` `:3785`, `sat/target_temp_step` `:3786`, `sat/thermal_coeff` `:3583`, `sat/thermal_comfort` `:3801`, `sat/valve_offset` `:3626`, `sat/auto_tune_rate` `:3615` (gate: auto-tune on).

The PV-boost settings are not in this list: `MQTTstuff.ino` already exposes them as a switch and numbers (see Summary).

Check: 14 + 18 + 4 + 32 = 68. 13 + 10 + 68 = 91.

## Cost estimate for the additions

Measured on `build/OTGW-firmware-esp32-combo-2.0.0-alpha.535+fc06acb.elf` with `xtensa-esp-elf-nm -S` (no build):

| Item | Measured | Per entity |
|---|---|---|
| `mqttHaSensors` | 0x2b18 = 11032 B for 394 rows | 28 B per row |
| `mqttHaBinSensors` | 0x930 = 2352 B for 73 rows | 32 B per row |
| `ha_lbl_sat_*` + `ha_name_sat_*` strings | 2175 B for 122 strings | ~18 B per string, 2 per entity |
| `streamSatBoolSwitch` | shared; a new case is code only | ~40 B per case (estimate, not measured) |
| `streamSatSelectDiscovery` | 0x191 = 401 B for one select | a second select case ~150-250 B (estimate) |

| Scope | Flash |
|---|---|
| §1 only: 7 binaries × ~68 B + 6 sensors × ~64 B | **~0.9 KB** |
| §1 + every §2 "add" (4 binaries, 1 switch, 1 select) | **~1.5 KB** |
| Optional: extend `sat/climate_attributes` with the §3b thermo-nova attribute set | ~0.3-0.5 KB format strings; static buffer `climAttrBuf[512]` (`SATcontrol.ino:3819`) may need to grow, which costs static RAM |

Heap: about 0 at rest. Discovery is streamed (`MqttJsonWriter`), and the state topics already exist with their on-change shadows. The cost is runtime: each entity adds one retained discovery publish (~0.5-0.7 KB JSON) to the discovery burst at MQTT (re)connect and HA restart. That is 13-19 more publishes, inside the ADR-088 burst window. The flash numbers are estimates from the table sizes. The implementation task must measure them with a before/after build.

## Draft implementation task (for the lead to create)

**Title:** HA entities for SAT health, sync, window and setpoint topics (TASK-1486 audit §1, §2)

**Description:**
TASK-1486 audited the 91 published `sat/*` state topics without a Home Assistant entity (`docs/sat/sat-ha-entity-audit.md`). Add the 13 §1 entities to the discovery tables in `MQTTHaDiscovery.cpp`:
- 7 `SAT_BIN` rows: device_health, cycle_health, pressure_health, setpoint_sync, modulation_sync and ch_sync (class `problem`, diagnostic, `on_off` payload); window_open (class `window`, `true_false`).
- 6 sensor rows: requested_setpoint, heating_curve_recommendation, manufacturer, consumption, max_setpoint and pressure_drop_rate (classes and units per the audit).

Also add the §2 items Robert accepts. Ordering: pressure_health, device_health and heating_curve_recommendation ship with or after TASK-1485; consumption ships after TASK-1484. Update the entity counts in `docs/api/MQTT.md` (SAT Discovery Entities), the CHANGELOG, and any host test that asserts discovery row counts. Read-only checks first: `python evaluate.py --quick`, then the discovery host tests.

**Acceptance criteria:**
1. Each new entity appears in the discovery output with the class, unit, category and payload from the audit. Evidence: a host test that streams the discovery for the new rows; it fails on HEAD and passes after.
2. A bench check on a running unit (`.63` or `.61`): `mosquitto_sub -t 'homeassistant/#' -v` shows each new config, and HA shows each entity with a state (gated topics excepted, as the audit lists them).
3. Health and sync entities read OFF on a healthy bench and ON when the fault is forced, for at least one sync and one health entity (simulation).
4. Flash and static RAM delta measured from a before/after esp32-combo build and recorded; expected ~0.9 KB flash for §1, ~1.5 KB with all §2 adds, and 0 B static RAM.
5. `docs/api/MQTT.md` entity counts and the CHANGELOG are updated.

**Depends on:** TASK-1485 (health polarity, recommendation case), TASK-1484 (consumption). **Labels:** sat, mqtt, ha. **Priority:** low. **Milestone:** 2.0.0.
