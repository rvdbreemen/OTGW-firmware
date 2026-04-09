# SAT Python Feature Inventory

Source: `other-projects/SAT-releases-thermo-nova/custom_components/sat/`
Date: 2026-04-09
Audit phase: Phase A, Task-87

---

## 1. Top-level Module Files

### `__init__.py`
Entry point for the HA integration.

**Key functions:**
- `async_setup_entry(hass, entry)` — boots the integration: creates SatConfig, SatDataUpdateCoordinator, SatHeatingControl, SatEntryData; registers platforms; optionally initialises Sentry
- `async_unload_entry(hass, entry)` — tears down the integration cleanly
- `async_reload_entry(hass, entry)` — unload + setup cycle
- `async_migrate_entry(hass, entry)` — config-version migration (v1 through v11)
- `async_migrate_identifiers(hass, entry, config)` — migrates entity/device unique-IDs from name-based to entry-ID-based scheme
- `initialize_sentry()` — initialises Sentry error-monitoring client

**HA-specific:** Entirely HA lifecycle. Not portable to firmware.

---

### `const.py`
All integration-wide constants and defaults.

**Key groups:**
- Timing constants: DEADBAND (0.1), BOILER_DEADBAND (2), DHW_OVERSHOOT_GUARD_SECONDS (300), OVERSHOOT_SUSTAIN_SECONDS (60), UNDERHEAT_SUSTAIN_SECONDS (180), SATURATION_SUSTAIN_SECONDS (300), SOLAR_GAIN_HOLD_SECONDS (600)
- PWM margins: PWM_ENABLE_MARGIN_CELSIUS (0.5), PWM_DISABLE_MARGIN_CELSIUS (1.5), PWM_ENABLE_LOW_MODULATION_PERCENT (10), PWM_DISABLE_LOW_MODULATION_PERCENT (30)
- Setpoint bounds: COLD_SETPOINT (22), MINIMUM_SETPOINT (10), MAXIMUM_SETPOINT (65)
- Config keys: ~50 CONF_* constants
- OPTIONS_DEFAULTS dict: all default values for user-configurable settings (PID gains P=45 I=0 D=6000, cycles_per_hour=4, heating_curve_coefficient=2.0, etc.)
- Overshoot protection setpoints: HEAT_PUMP=40, RADIATORS=62, UNDERFLOOR=45
- Service names: SERVICE_RESET_INTEGRAL, SERVICE_PULSE_WIDTH_MODULATION, SERVICE_SET_OVERSHOOT_PROTECTION_VALUE, SERVICE_START_OVERSHOOT_PROTECTION_CALCULATION
- HA event names: EVENT_SAT_CYCLE_STARTED, EVENT_SAT_CYCLE_ENDED
- HA dispatcher signal: SIGNAL_PID_UPDATED

---

### `types.py`
Domain type definitions (pure Python, no HA dependency).

**Classes:**
- `SustainedRuntime` (frozen dataclass) — normalized state for sustained-condition timers (started_at, initialized, elapsed_seconds)
- `Percentiles` (frozen dataclass) — p50/p90 summary for a metric
- `BoilerStatus` (Enum) — 16 states: OFF, IDLE, INSUFFICIENT_DATA, PREHEATING, AT_SETPOINT_BAND, STALLED_IGNITION, MODULATING_UP, MODULATING_DOWN, IGNITION_SURGE, CENTRAL_HEATING, HEATING_HOT_WATER, COOLING, ANTI_CYCLING, PUMP_STARTING, WAITING_FOR_FLAME, OVERSHOOT_COOLING, POST_CYCLE_SETTLING
- `CycleKind` (str Enum) — MIXED, UNKNOWN, CENTRAL_HEATING, DOMESTIC_HOT_WATER
- `CycleClassification` (str Enum) — GOOD, UNCERTAIN, INSUFFICIENT_DATA, OVERSHOOT, UNDERHEAT, SHORT_CYCLING
- `HeaterState` (str Enum) — ON / OFF
- `PWMStatus` (str Enum) — ON / OFF / IDLE
- `CycleControlMode` (str Enum) — PWM / CONTINUOUS
- `HeatingCurveRecommendation` (str Enum) — INCREASE / DECREASE / HOLD / INSUFFICIENT_SAMPLES
- `RelativeModulationState` (str Enum) — OFF / COLD / PWM_OFF / HOT_WATER
- `HeatingSystem` (StrEnum) — UNKNOWN / HEAT_PUMP / RADIATORS / UNDERFLOOR; property base_offset (20 for underfloor, 27.2 otherwise)
- `HeatingMode` (StrEnum) — ECO / COMFORT

---

### `helpers.py`
Pure utility functions (mostly no HA dependency except `dt` and `State`).

**Key functions:**
- `timestamp()` — current UTC timestamp as float
- `event_timestamp(time)` — timestamp from HA event time or now
- `seconds_since(start)`, `elapsed_seconds()`, `non_negative_elapsed_seconds()`, `is_within_elapsed_window()`
- `sustained_runtime(current, started_at)` — returns SustainedRuntime normalized struct
- `state_age_seconds(state)`, `is_state_stale(state, max_age)`
- `convert_time_str_to_seconds(time_str)` — HH:MM:SS string to seconds
- `calculate_derivative_per_hour(error, time_sec)`
- `calculate_default_maximum_setpoint(heating_system)` — 50 for underfloor, 55 otherwise
- `snake_case(value)`
- `float_value()`, `to_float()`, `int_value()`, `to_int()` — safe converters
- `clamp(value, low, high)`, `clamp_to_range(value, range_limit)`
- `filter_none()`, `ensure_list()`, `average()`, `min_max()`, `percentile_interpolated()`

---

### `entry_data.py`
Configuration and entry data classes.

**Classes:**
- `SatMode` (StrEnum) — FAKE, MQTT_EMS, MQTT_OPENTHERM, MQTT_OTTHING, SERIAL, ESPHOME, SIMULATOR, SWITCH
- `SensorsConfig` (frozen dataclass) — inside/outside/humidity sensor entity IDs, staleness timeouts
- `PidConfig` (frozen dataclass) — P/I/D gains, automatic_gains flag, automatic_gains_value, heating_curve_coefficient
- `PwmConfig` (frozen dataclass) — cycles_per_hour, duty_cycle_seconds, maximum_relative_modulation, force_pulse_width_modulation
- `LimitsConfig` (frozen dataclass) — min/max setpoint, consumption bounds, climate_valve_offset, target_temperature_step
- `PressureHealthConfig` (frozen dataclass) — min/max pressure, max drop rate, max age
- `PresetConfig` (frozen dataclass) — thermal_comfort, heating_mode, preset temperatures, sync flags
- `SolarGainConfig` (frozen dataclass) — enabled, freeze_integral, minimum_elevation, minimum_rise_per_hour, setpoint_offset_celsius
- `SimulationConfig` (frozen dataclass) — enabled, simulated_heating/cooling/warming_up rates
- `SatConfig` (frozen dataclass) — main config wrapper; provides typed property accessors for all sub-configs above; reads from HA entry data/options
- `SatEntryData` (dataclass) — runtime container: config, coordinator, heating_control, sentry client, climate entity reference, climate_ready event

---

## 2. Control Algorithms

### `pid.py` — class `PID`
PID controller for boiler setpoint tuning.

**Responsibilities:** Compute P/I/D terms to produce a boiler setpoint offset relative to the heating curve.

**Key properties:**
- `kp` — automatic (curve * coeff / divisor) or manual (config proportional)
- `ki` — automatic (kp / 8400) or manual (config integral)
- `kd` — automatic (0.07 * 8400 * kp) or manual (config derivative)
- `proportional`, `integral`, `derivative`, `raw_derivative`, `last_error`
- `output` — heating_curve + P + I + D
- `available` — True when last_error and heating_curve.value are both set

**Key methods:**
- `reset()` — clears integral and raw_derivative
- `update(state, freeze_integral)` — updates derivative and integral; dispatches SIGNAL_PID_UPDATED
- `_update_integral(state)` — resets integral outside deadband; accumulates Ki * error * 60s inside deadband; clamps to [-curve_value, curve_value]
- `_update_derivative(state)` — temperature-slope derivative (not error-slope); negative sign (rising temp damps); adaptive low-pass alpha = dt/(60+dt); caps at +/-5.0
- `async_added_to_hass()` — restores P/I/D state from HA Store
- `_async_save_state()` — persists state to HA Store

**PID update interval:** 60 seconds (PID_UPDATE_INTERVAL)

---

### `heating_curve.py` — class `HeatingCurve`
Weather-compensated flow temperature target.

**Key methods:**
- `calculate(target_temp, outside_temp)` (static) — formula: 4*(target-20) + 0.03*(outside-20)^2 - 0.4*(outside-20)
- `update(target_temp, outside_temp)` — computes and stores: base_offset + (coeff/4) * curve_value
- `reset()` — clears cached value

**Base offsets:** 20 (underfloor), 27.2 (radiators/heat pump)

---

### `solar_gain.py` — class `SolarGainController`
Passive solar gain detection and setpoint suppression.

**Responsibilities:** Detect when indoor temperature rises due to solar irradiation (not heating) and suppress boiler setpoint to avoid overheating.

**Input signals:** indoor temp sample + timestamp, valves_open, sun_elevation, flame_active, is_heating_mode, relative_modulation

**Detection criteria:**
- Sun elevation >= configured minimum (default 12 degrees)
- Indoor rise rate >= configured minimum (default 0.6 C/hr)
- Valves are open
- Boiler output is low (flame off OR relative_modulation <= 20%)

**Output:** `SolarGainSnapshot` (active bool, rise_per_hour, sun_elevation)

**Hold duration:** SOLAR_GAIN_HOLD_SECONDS (600s = 10 min) after last detection event

**Integral freeze:** when active and freeze_integral=True, PID integral accumulation is skipped

---

### `pwm.py` — class `PWM`
Pulse Width Modulation controller.

**Responsibilities:** Control boiler on/off cycling to achieve a target effective setpoint when boiler minimum modulation is too high (overshoot prevention).

**State:** enabled bool, status (ON/OFF/IDLE), current_cycle count, duty_cycle tuple (on_sec, off_sec), effective_on_temperature

**Key methods:**
- `enable() / disable() / reset()`
- `restore(state)` — restore from HA entity state attributes
- `update(device_state, requested_setpoint, timestamp)` — main control loop; manages ON to OFF and OFF to ON transitions; tracks flame ignition timing; enforces min ON time (HEATER_STARTUP_TIMEFRAME = 180s)
- `on_cycle_end(cycle)` — react to cycle classification: OVERSHOOT enables PWM, UNDERHEAT disables PWM
- `_calculate_duty_cycle(requested_setpoint, device_state)` — calculates (on_sec, off_sec) from (requested - base_offset) / (boiler_temp - base_offset); four regions: below-min, low, mid, high, max

**Duty cycle thresholds:** based on cycles_per_hour; lower threshold = 3min/cycle_period

---

### `heating_control.py` — class `SatHeatingControl`
Central coordinator for all heating output decisions.

**Responsibilities:** Integrates PWM, setpoint computation, relative modulation control, and cycle tracking.

**Key properties:**
- `control_setpoint`, `relative_modulation_value`, `pwm_state`, `cycles`, `last_cycle`, `device_status`, `control_mode`, `relative_modulation_state`

**Key methods:**
- `update(demand)` — main control update; decides ON/OFF; calls PWM.update(); computes setpoint (PWM or continuous); sets control modulation; pushes to coordinator
- `_compute_pwm_control_setpoint()` — ON phase: suppresses modulation (setpoint = flow - offset); OFF phase: uses return_temp + flame_off_offset
- `_compute_continuous_control_setpoint()` — clamps setpoint against boiler temperature to prevent rapid ramp-up/down
- `_compute_relative_modulation_value()` — sets 0% when PWM is enabled, max% otherwise; Geminox quirk: minimum 10%
- `_maybe_enable_pwm_on_sustained_overshoot()` — enables PWM after 60s continuous overshoot
- `_maybe_disable_pwm_runtime()` — disables PWM after 180s underheat or 300s saturation

---

## 3. Cycle Tracking Subsystem

### `cycles/tracker.py` — class `CycleTracker`
Detects flame ON/OFF transitions and builds cycle snapshots.

**Key method:**
- `update(sample)` — on flame ON: fire EVENT_SAT_CYCLE_STARTED; on flame OFF: build cycle, fire EVENT_SAT_CYCLE_ENDED; accumulate samples during ON phase

**Builds cycle metrics:** flow temperature percentiles (p50/p90), return temperature, modulation level, setpoint errors, flow/return delta, DHW fraction, shape metrics (time-in-band, overshoot timing)

---

### `cycles/classifier.py` — class `CycleClassifier`
Classifies completed cycles based on metrics.

**Classifications:**
- INSUFFICIENT_DATA: zero-duration cycle
- UNCERTAIN: unknown kind, DHW mixing, or PWM-IDLE
- SHORT_CYCLING: PWM duration < 80% of on_time
- UNDERHEAT: flow never reached setpoint - 2 C
- OVERSHOOT: flow exceeded setpoint + overshoot margin
- GOOD: none of the above

---

### `cycles/history.py` — class `CycleHistory`
Rolling statistical history of completed cycles.

**Windows:** recent (~4h default) and daily (24h)

**Tracked metrics (per window):**
- duty_ratio, long_cycle_fraction, median_on_duration, overshoot_fraction, underheat_fraction
- p50/p90 percentiles for: flow_return_delta, flow_control_setpoint_error, flow_requested_setpoint_error
- off_with_demand_duration (time off with heating demand present, before next cycle)

---

## 4. Device Tracking Subsystem

### `device/__init__.py` — class `DeviceTracker`
Tracks boiler state transitions and device health.

**Tracks:** flame ON/OFF timestamps, hot water ON/OFF timestamps, modulation reliability

**Key properties:** status (BoilerStatus), flame_on_since, flame_off_since, hot_water_on_since, hot_water_off_since, modulation_reliable

**Persistence:** stores modulation_reliable flag to LittleFS via HA Store

---

### `device/status.py` — class `DeviceStatusEvaluator`
Evaluates device status from a snapshot of boiler state.

**16 BoilerStatus states** resolved by rule-based evaluation:
- OFF, IDLE, OVERSHOOT_COOLING, ANTI_CYCLING (180s min-off), STALLED_IGNITION, COOLING, PUMP_STARTING, WAITING_FOR_FLAME, POST_CYCLE_SETTLING, HEATING_HOT_WATER, IGNITION_SURGE (rapid temp rise in first 30s), PREHEATING (>6 C below setpoint), AT_SETPOINT_BAND (+/-1.5 C), MODULATING_UP, MODULATING_DOWN, CENTRAL_HEATING

---

### `device/modulation.py` — class `ModulationReliabilityTracker`
Determines if the boiler relative modulation signal is trustworthy.

**Logic:** Collects last 50 flame-on modulation samples; reliable if 40%+ of last 10 samples exceed threshold.

---

## 5. Temperature Subsystem

### `temperature/state.py` — class `TemperatureState`
Frozen dataclass: entity_id, current, setpoint, last_updated, last_changed, last_reported. Property `error = setpoint - current`.

### `temperature/history.py` — class `TemperatureHistory`
Rolling history of temperature errors for heating curve recommendation.

---

## 6. Area / Multi-room Support

### `area.py` — classes `Area`, `Areas`

**`Area`:** Represents a single secondary climate-controlled room.
- Has its own `HeatingCurve` and `PID` instances
- Reads current/target temperature from HA climate entity (or overridden sensor via `sensor_temperature_id` attribute)
- `weight` property: demand weight = max(delta - 0.2, 0) clamped to [0, 2]
- `requires_heat`: valve_position >= 10% OR pid.output > COLD_SETPOINT
- `valve_position`: reads `current_valve_position` attribute from HA climate entity

**`Areas`:** Container for multiple Area instances; aggregation helpers:
- `Areas._PIDs.output`: 75th-percentile PID output of areas requiring heat + 5 C headroom; takes highest within headroom
- `Areas._PIDs.overshoot_cap`: cooling cap from rooms overshooting target (min cap from all overshooting rooms)
- `Areas._HeatingCurves.update(outside_temp)`: updates all area heating curves

---

## 7. Coordinator Layer

### `coordinator/__init__.py` — classes `SatDataUpdateCoordinator`, `SatDataUpdateCoordinatorFactory`

**Factory** resolves coordinator implementation by mode:
- `SatMode.FAKE` to SatFakeCoordinator
- `SatMode.SIMULATOR` to SatSimulatorCoordinator
- `SatMode.SWITCH` to SatSwitchCoordinator
- `SatMode.ESPHOME` to SatEspHomeCoordinator
- `SatMode.MQTT_EMS` to SatEmsMqttCoordinator
- `SatMode.MQTT_OPENTHERM` to SatOpenThermMqttCoordinator
- `SatMode.MQTT_OTTHING` to SatOtthingMqttCoordinator
- `SatMode.SERIAL` to SatSerialCoordinator

**Base coordinator abstract interface:**
- Abstract: `id`, `type`, `setpoint`, `active`, `member_id`
- With defaults: `flame_active`, `hot_water_active`, `boiler_temperature`, `return_temperature`, `boiler_pressure`, `relative_modulation_value`, `boiler_capacity`, `boiler_power`, `minimum/maximum setpoint/modulation`
- Capability flags: `supports_setpoint_management`, `supports_hot_water_setpoint_management`, `supports_relative_modulation`, `supports_relative_modulation_management`, `supports_maximum_setpoint_management`
- Control methods: `async_set_heater_state()`, `async_set_control_setpoint()`, `async_set_control_hot_water_setpoint()`, `async_set_control_max_setpoint()`, `async_set_control_max_relative_modulation()`, `async_set_control_thermostat_setpoint()`
- Manufacturer quirk: `supports_relative_modulation` returns False for Ideal, Intergas, Geminox, Nefit

**MQTT base coordinator (`coordinator/mqtt/__init__.py`):**
- Subscribes to HA MQTT topics; persists last data to HA Store; publishes commands via HA MQTT publish
- Subclasses implement: `get_tracked_entities()`, `_get_topic_for_subscription()`, `_get_topic_for_publishing()`, `boot()`

---

## 8. Manufacturer Support

### `manufacturer.py` — classes `Manufacturer`, `ManufacturerFactory`
17 named manufacturers + Other. Each has a MemberID (OT slave MemberID byte) and a friendly name.
Factory resolves by name or by MemberID (may return multiple matches for shared IDs like 4 and 27).

### `manufacturers/` directory
Individual manufacturer classes with quirks:
- `Geminox`: quirk flags: minimum modulation 10%, no relative modulation reported
- `Ideal`, `Intergas`, `Nefit`: no relative modulation reported
- Others: standard behavior

---

## 9. HA Entity Platforms

### `climate.py` — class `SatClimate`
Main HA climate entity (ClimateEntity + RestoreEntity).

**Key responsibilities:**
- Exposes target/current temperature, HVAC mode (HEAT/OFF), preset modes (none/home/away/sleep/comfort/activity)
- Schedules PID control loop (60s interval) and heating control loop (5s debounced)
- Handles outside sensor (sensor entity OR weather entity with temperature attribute)
- Window sensor integration: delays setpoint drop by configurable time; sets PRESET_ACTIVITY
- Thermostat sync: optionally tracks a separate thermostat entity and follows its setpoint
- Multi-room: reads room temperatures from secondary climate entities
- Solar gain: integrates SolarGainController on each PID tick
- Summer Simmer Index: optionally replaces current_temperature with heat-perceived temperature
- Presets: 5 configurable temperature presets; optional cascade to secondary climates on preset change
- State attributes: rooms dict, current humidity, setpoint, PID state, summer simmer, valve status, solar gain info, PWM state, relative modulation state

### `sensor.py` — HA SensorEntity subclasses
- `SatPidSensor`: PID output (primary or per-room); attributes: error, P, I, D
- `SatRequestedSetpoint`: requested boiler setpoint (heating curve + PID)
- `SatHeatingCurveSensor`: current heating curve value
- `SatHeatingCurveRecommendationSensor`: INCREASE/DECREASE/HOLD recommendation based on daily temperature error statistics (needs 6+ samples)
- `SatErrorValueSensor`: current temperature error (setpoint - current)
- `SatCurrentPowerSensor`: estimated current boiler power (kW), if supported
- `SatCurrentConsumptionSensor`: estimated gas consumption (m3/h) from modulation level + configured bounds
- `SatManufacturerSensor`: detected boiler manufacturer name
- `SatCycleSensor`: last cycle classification + detailed attributes
- `SatDeviceSensor`: current BoilerStatus

### `binary_sensor.py` — HA BinarySensorEntity subclasses
- `SatControlSetpointSyncSensor`: PROBLEM if control setpoint != coordinator setpoint for >60s
- `SatRelativeModulationSyncSensor`: PROBLEM if modulation value != coordinator maximum for >60s
- `SatCentralHeatingSyncSensor`: PROBLEM if HVAC action != device active state for >60s
- `SatPressureHealthSensor`: PROBLEM if boiler pressure out of configured band OR drop rate exceeded OR stale reading; uses EMA smoothing + linear regression for drop rate; 120s confirmation delay
- `SatDeviceHealthSensor`: PROBLEM if BoilerStatus == INSUFFICIENT_DATA
- `SatCycleHealthSensor`: PROBLEM if last cycle is OVERSHOOT, UNDERHEAT, or SHORT_CYCLING
- `SatWindowSensor`: grouped window-open binary sensor (any)

### `number.py`
HA NumberEntity subclasses exposing configurable setpoints as adjustable entities (PID gains, etc.).

### `services.py`
Registers HA services:
- `sat.reset_integral` -- resets PID integral for named entity IDs

### `config_flow.py` — class `SatFlowHandler`
HA config flow (VERSION 11) supporting modes: mosquitto/esphome/serial/switch (advanced: simulator).

---

## 10. Ancillary Features

### `summer_simmer.py` — class `SummerSimmer`
Calculates Summer Simmer Index (heat + humidity comfort metric).
- `index(temp_c, humidity)` — returns perceived temperature in Celsius
- `perception(temp_c, humidity)` — returns comfort label ('Cool' to 'Circulatory Collapse Imminent')

### `state.py`
Overshoot protection value management -- reading/writing calibrated OPV from/to persistent HA store.

---

## 11. Configuration Parameters (OPTIONS_DEFAULTS)

| Parameter | Default | Type |
|---|---|---|
| proportional | 45 | str (converted to float) |
| integral | 0 | str |
| derivative | 6000 | str |
| cycles_per_hour | 4 | int |
| automatic_gains | True | bool |
| automatic_gains_value | 2.0 | float |
| overshoot_protection | False | bool |
| simulation | False | bool |
| thermal_comfort | False | bool |
| solar_gain_compensation | False | bool |
| solar_gain_min_elevation | 12.0 | float |
| solar_gain_min_rise_per_hour | 0.6 | float |
| solar_gain_setpoint_offset_celsius | 2.0 | float |
| solar_gain_freeze_integral | True | bool |
| push_setpoint_to_thermostat | False | bool |
| sync_climates_with_mode | True | bool |
| sync_climates_with_preset | False | bool |
| minimum_setpoint | 10 | int |
| maximum_relative_modulation | 100 | int |
| force_pulse_width_modulation | False | bool |
| flame_off_setpoint_offset_celsius | 18.0 | float |
| modulation_suppression_delay_seconds | 20 | int |
| modulation_suppression_offset_celsius | 1.0 | float |
| flow_setpoint_offset_celsius | 2.0 | float |
| minimum_boiler_pressure | 0.8 | float |
| maximum_boiler_pressure | 2.5 | float |
| maximum_pressure_drop_rate | 0.3 | float |
| pressure_max_value_age | 00:30:00 | str |
| minimum_consumption | 0 | int |
| maximum_consumption | 0 | int |
| duty_cycle | 00:13:00 | str |
| climate_valve_offset | 0 | int |
| target_temperature_step | 0.5 | float |
| sensor_max_value_age | 06:00:00 | str |
| window_minimum_open_time | 00:00:15 | str |
| activity_temperature | 10 | int |
| away_temperature | 10 | int |
| home_temperature | 18 | int |
| sleep_temperature | 15 | int |
| comfort_temperature | 20 | int |
| heating_curve_coefficient | 2.0 | float |
| heating_mode | comfort | HeatingMode |
| heating_system | radiators | HeatingSystem |
