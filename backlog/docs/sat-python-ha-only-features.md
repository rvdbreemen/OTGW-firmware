# SAT Python: HA-Specific vs Portable Features

Source: audit of `other-projects/SAT-releases-thermo-nova/custom_components/sat/`
Date: 2026-04-09
Audit phase: Phase A, Task-88

---

## 1. Features That Are Purely HA-Specific (Cannot Be Ported)

These features are deeply bound to the Home Assistant runtime and have no meaningful firmware equivalent.

### 1.1 HA Config Flow (`config_flow.py`)
The entire SatFlowHandler class is a HA UI wizard for setting up the integration. Firmware uses a web UI settings page instead.

### 1.2 HA Entity Platforms (climate, sensor, binary_sensor, number)
- `SatClimate` (ClimateEntity, RestoreEntity) -- HA entity lifecycle, state machine, HVAC mode/preset model
- All `SatXxxSensor` (SensorEntity) subclasses -- HA entity registration, state publishing
- All `SatXxxBinarySensor` (BinarySensorEntity) subclasses -- HA entity registration
- `number.py` (NumberEntity) -- HA UI sliders for PID gains

The firmware exposes equivalent data via REST API and MQTT. The entity model itself is HA-only.

### 1.3 HA Services (`services.py`)
`sat.reset_integral` service registered on the HA service bus. Firmware exposes equivalent via REST API endpoint.

### 1.4 HA Events and Dispatcher Signals
- `EVENT_SAT_CYCLE_STARTED`, `EVENT_SAT_CYCLE_ENDED` fired on the HA event bus
- `SIGNAL_PID_UPDATED` dispatched via HA async_dispatcher_send

Firmware uses direct function calls / MQTT publication instead.

### 1.5 HA Config Entry Lifecycle (`__init__.py`)
`async_setup_entry`, `async_unload_entry`, `async_reload_entry`, `async_migrate_entry` -- these are HA integration lifecycle hooks with no equivalent concept in firmware (firmware uses LittleFS-backed settings with version migration logic in settingStuff.ino).

### 1.6 Entity State Restoration (`RestoreEntity`)
`async_get_last_state()` -- reads last known entity state from HA database. Firmware uses LittleFS to persist relevant state fields.

### 1.7 HA Store Persistence (`homeassistant.helpers.storage.Store`)
Python uses HA Store for persistent data (PID state, device modulation reliability, MQTT coordinator last data). Firmware uses LittleFS / settings struct.

### 1.8 Sentry Error Monitoring
`initialize_sentry()` in `__init__.py`. There is no equivalent in embedded firmware. The firmware uses telnet debug output and MQTT status topics.

### 1.9 HA Coordinator Pattern (`DataUpdateCoordinator`)
`SatDataUpdateCoordinator` extends HA's `DataUpdateCoordinator` with debounced listener notification. Firmware uses a single cooperative control loop with direct state access.

### 1.10 MQTT via HA MQTT Component
Coordinator MQTT subclasses use `homeassistant.components.mqtt.async_subscribe` and `mqtt.async_publish`. These route through the HA MQTT integration. The firmware uses its own MQTT client (MQTTstuff.ino) directly.

### 1.11 ESPHome Coordinator (`coordinator/esphome/`)
Communicates with an ESPHome device via the HA ESPHome integration API. This mode doesn't apply to the OTGW firmware at all.

### 1.12 Weather Entity Support
Python reads outside temperature from a HA `weather.*` entity via `state.attributes.get("temperature")`. Firmware uses its own weather fetch (Open-Meteo API, SATweather.ino).

### 1.13 Sun Entity Integration
Python reads `sun.sun` entity elevation for solar gain detection. Firmware reads latitude/longitude from settings and either fetches elevation from the weather API or uses a local sun-elevation formula.

### 1.14 Window Sensor as HA Binary Sensor Group
`SatWindowSensor` extends `BinarySensorGroup` -- aggregates multiple HA binary sensor entities. Firmware implements window sensor via MQTT topic or external sensor input.

### 1.15 Thermostat Follow-mode (`push_setpoint_to_thermostat`)
Calls `CLIMATE_DOMAIN / SERVICE_SET_TEMPERATURE` on external HA thermostats to push the SAT target. Firmware has no equivalent multi-thermostat cascade.

### 1.16 Secondary Climate Entity Management
`sync_climates_with_mode`: cascades HVAC mode changes to linked HA climate entities. Firmware cannot query or control other thermostats via HA service calls.

---

## 2. Features That CAN Be Ported (Algorithm Logic)

These features contain domain logic that is independent of the HA runtime and can be expressed in C++.

| Feature | Python location | Portability notes |
|---|---|---|
| PID controller (P/I/D computation) | `pid.py` | Fully ported: `SATpid.ino` |
| Heating curve formula | `heating_curve.py` | Fully ported: `SATcontrol.ino` |
| PWM duty cycle calculation | `pwm.py` | Fully ported: `SATcontrol.ino` |
| PWM on/off state machine | `pwm.py` | Fully ported: `SATcontrol.ino` |
| Sustained overshoot/underheat detection | `heating_control.py` | Fully ported: `SATcycles.ino` |
| Cycle kind detection (CH/DHW/MIXED) | `cycles/tracker.py` | Fully ported: `SATcycles.ino` |
| Cycle classification (GOOD/OVERSHOOT/UNDERHEAT/SHORT) | `cycles/classifier.py` | Partially ported: `SATcycles.ino` (simplified metric set) |
| Cycle history (rolling windows, EMA) | `cycles/history.py` | Partially ported: `SATcycles.ino` (uses EMA instead of deque) |
| Device status evaluation (16 states) | `device/status.py` | Partially ported: `SATcontrol.ino` (satUpdateBoilerStatus) |
| Modulation reliability tracking | `device/modulation.py` | Partially ported: `SATcontrol.ino` |
| Manufacturer detection by MemberID | `manufacturer.py` | Fully ported: `SATcontrol.ino` (PROGMEM table) |
| Manufacturer quirks (Geminox min-mod, no-rel-mod) | manufacturers/ | Fully ported: `SATcontrol.ino` (SAT_QUIRK_* bitmask) |
| Auto-gain calculation (kp/ki/kd from curve) | `pid.py` | Fully ported: `SATpid.ino` |
| Heating system type (radiators/underfloor/heat pump) | `types.py` | Fully ported: `SATcontrol.ino` |
| Solar gain detection (sun elevation + rise rate) | `solar_gain.py` | Partially ported: `SATcontrol.ino` (sun elevation via weather API; rise rate computed inline) |
| Solar gain integral freeze | `pid.py` | Fully ported: `SATpid.ino` |
| Summer Simmer Index formula | `summer_simmer.py` | Not yet ported |
| Thermal comfort mode (SSI replaces temp) | `climate.py` | Not yet ported |
| Valve-open detection | `climate.py::valves_open` | Partially ported: firmware tracks via OT status flags |
| Multi-area PID aggregation | `area.py::Areas._PIDs` | Not yet ported (firmware has single-area PID) |
| Area overshoot cap | `area.py::Areas._PIDs.overshoot_cap` | Not yet ported |
| Pressure health monitoring | `binary_sensor.py::SatPressureHealthSensor` | Not yet ported |
| Heating curve recommendation (INCREASE/DECREASE/HOLD) | `sensor.py::SatHeatingCurveRecommendationSensor` | Not yet ported |
| Temperature history statistics | `temperature/history.py` | Not yet ported |
| OPV calibration state machine | `state.py` | Partially ported: `SATcontrol.ino` (satOvpCalibrate) |
| Weather fetch (Open-Meteo) | (Python reads HA weather entity instead) | Ported: `SATweather.ino` (firmware fetches directly) |
| BLE temperature sensor | (Python reads HA sensor entities) | Ported (ESP32 only): `SATble.ino` |
| Continuous mode setpoint clamping | `heating_control.py::_compute_continuous_control_setpoint` | Partially ported: `SATcontrol.ino` |
| PWM mode setpoint suppression | `heating_control.py::_compute_pwm_control_setpoint` | Partially ported: `SATcontrol.ino` |
| Boiler power estimation | `coordinator/__init__.py::boiler_power` | Not yet ported |
| Gas consumption estimation | `sensor.py::SatCurrentConsumptionSensor` | Not yet ported |
| Overshoot protection setpoint (per heating system) | `const.py::OVERSHOOT_PROTECTION_SETPOINT` | Ported: `SATcontrol.ino` (satGetMaxSetpoint) |

---

## 3. HA-Only Feature Summary (Non-portable)

The following categories are HA-only and should NOT be targeted for firmware port:

1. **HA integration lifecycle** (config flow, entry setup/teardown, migration)
2. **HA entity model** (ClimateEntity, SensorEntity, BinarySensorEntity, NumberEntity)
3. **HA service bus** (service calls to external climate entities)
4. **HA event bus** (cycle events, state change events)
5. **HA Store persistence** (DataStore for cross-restart state)
6. **HA MQTT component** (routing through HA MQTT integration)
7. **HA entity state tracking** (tracking other entities by entity_id in HA state machine)
8. **ESPHome coordinator** (entirely ESPHome-specific)
9. **Sentry error monitoring** (cloud crash reporting)
10. **DHCP / MQTT auto-discovery** (config_flow DHCP/MQTT service info handlers)
