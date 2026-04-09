# SAT Python to C++ Function Mapping

Source: Python `custom_components/sat/` vs C++ `src/OTGW-firmware/SAT*.ino`
Date: 2026-04-09
Audit phase: Phase A, Task-89

Legend:
- FULL: C++ implementation matches Python logic faithfully
- PARTIAL: C++ covers the main path but differs in detail (simplified metrics, different thresholds, missing edge cases)
- MISSING: No C++ equivalent exists

---

## PID Controller (`pid.py` -> `SATpid.ino`)

| Python | C++ | Match |
|---|---|---|
| `PID.__init__()` | `satPidReset()` (static vars init) | FULL |
| `PID.kp` (auto gain) | `_pidCalculateGains()` kp = (coeff * curve) / divisor | FULL |
| `PID.ki` (auto gain) | `_pidCalculateGains()` ki = kp / 8400 | FULL |
| `PID.kd` (auto gain) | `_pidCalculateGains()` kd = 0.07 * 8400 * kp | FULL |
| `PID.kp` (manual) | `state.sat.fKp` from `settings.sat.fPropGain` | PARTIAL (manual mode present but less tested) |
| `PID._update_integral()` | `_pidUpdateIntegral()` | FULL (deadband reset, Ki*error*60s, clamp to [0, curveValue]) |
| `PID._update_derivative()` | `_pidUpdateDerivative()` | FULL (temperature-based, negative sign, adaptive alpha, cap +-5) |
| `PID.output` | `satPidUpdate()` return value = curve + P + I + D | FULL |
| `PID.reset()` | `satPidReset()` | FULL |
| `PID.update(state, freeze_integral)` | `satPidUpdate(roomTemp, targetTemp, curveValue, boilerTemp)` | PARTIAL (freeze_integral from solar gain state, not parameter) |
| `PID.async_added_to_hass()` (restore state) | No equivalent | MISSING (firmware restarts with zeroed PID state) |
| `PID._async_save_state()` (persist) | No equivalent | MISSING |
| `PID` dispatcher signal `SIGNAL_PID_UPDATED` | MQTT publish of PID values | PARTIAL (MQTT publish, not in-process signal) |

---

## Heating Curve (`heating_curve.py` -> `SATcontrol.ino`)

| Python | C++ | Match |
|---|---|---|
| `HeatingCurve.calculate(target, outside)` | `satCalcHeatingCurve(targetTemp, outsideTemp)` formula body | FULL |
| `HeatingCurve.update(target, outside)` | `satCalcHeatingCurve()` stores in `state.sat.fHeatingCurveValue` | FULL |
| `HeatingCurve.value` | `state.sat.fHeatingCurveValue` | FULL |
| `HeatingCurve.reset()` | implicit (not explicitly reset in C++) | PARTIAL |
| base_offset logic (20 / 27.2) | `satGetBaseOffset()` | FULL |

---

## PWM Controller (`pwm.py` -> `SATcontrol.ino`)

| Python | C++ | Match |
|---|---|---|
| `PWM.__init__()` | PWM state vars in `SATcontrol.ino` | FULL |
| `PWM.enable() / disable() / reset()` | `satPwmEnable() / satPwmDisable() / satPwmReset()` | FULL |
| `PWM.update(device_state, requested_setpoint, timestamp)` | `satPwmUpdate()` | PARTIAL (C++ uses millis() directly; same state machine but some timing details differ) |
| `PWM._calculate_duty_cycle(...)` | `satPwmCalcDutyCycle()` | FULL (four-region formula, base_offset, boiler temp denominator) |
| `PWM.on_cycle_end(cycle)` | Handled in `satCycleCheckAutoSwitch()` + `satCycleOnFlameChange()` | PARTIAL (same logic, slightly different integration point) |
| `PWM.restore(state)` (from HA entity attributes) | No equivalent | MISSING (firmware restarts from defaults) |
| `PWMStatus.ON/OFF/IDLE` | `SATMode` / `SATCyclePhase` enums | PARTIAL (different enum organization) |
| Effective boiler temperature (EWMA tracking) | `_satPwmEffectiveOnTemp` (EWMA in C++) | FULL |
| Min ON time (HEATER_STARTUP_TIMEFRAME = 180s) | `SAT_PWM_MIN_ON_SEC = 30s` | PARTIAL (C++ uses 30s, Python uses 180s) |
| Cycles-per-hour cap | `satGetMaxCyclesPerHour()` | FULL |

---

## Heating Control (`heating_control.py` -> `SATcontrol.ino`)

| Python | C++ | Match |
|---|---|---|
| `SatHeatingControl.update(demand)` | `satControlLoop()` main loop body | PARTIAL (same overall flow; C++ is more monolithic) |
| `_compute_continuous_control_setpoint()` | Continuous mode setpoint in `satControlLoop()` | PARTIAL (C++ applies simpler clamping) |
| `_compute_pwm_control_setpoint()` | PWM mode setpoint computation in `satControlLoop()` | PARTIAL (same idea: suppress modulation + flame-off hold) |
| `_compute_relative_modulation_value()` | `addCommandToQueue("MM=...")` in `satControlLoop()` | PARTIAL (same logic; Geminox min-10% quirk ported) |
| `_maybe_enable_pwm_on_sustained_overshoot()` | `satCycleCheckAutoSwitch()` overshoot branch | FULL (60s sustain threshold) |
| `_maybe_disable_pwm_runtime()` underheat | `satCycleCheckAutoSwitch()` underheat branch | FULL (180s sustain) |
| `_maybe_disable_pwm_on_sustained_saturation()` | `satCycleCheckAutoSwitch()` saturation branch | FULL (300s sustain) |
| DHW overshoot guard (300s after DHW off) | `satControlLoop()` DHW guard check | FULL |
| `HeatingDemand` dataclass | Computed inline in `satControlLoop()` | PARTIAL |
| `ControlLoopSample` dataclass | No equivalent struct | MISSING (C++ uses global state directly) |

---

## Cycle Tracking (`cycles/` -> `SATcycles.ino`)

| Python | C++ | Match |
|---|---|---|
| `CycleTracker.update(sample)` | `satCycleOnFlameChange()` + `satCycleSample()` | PARTIAL (same transitions; C++ lacks per-sample metrics rich set) |
| `CycleTracker._build_cycle_state()` | `_cycleRecord()` | PARTIAL (C++ records simpler set: class, duration, max flow, overshoot_sec) |
| `CycleTracker._build_metrics()` (p50/p90 percentiles) | No equivalent | MISSING (C++ uses EMA instead of percentiles) |
| `CycleTracker._build_cycle_shape_metrics()` | Partial: overshoot_sec tracked | PARTIAL |
| `CycleTracker._determine_cycle_kind()` | `_cycleDetectKind()` | FULL (same DHW fraction thresholds: >0.8 = DHW, <0.2 = CH) |
| `CycleClassifier.classify()` | `_cycleClassify()` | PARTIAL (simplified: uses max_flow vs setpoint rather than tail percentile metrics) |
| `CycleClassifier._classify_pwm()` | Short-cycling detection in `_cycleClassify()` | PARTIAL (C++ checks duration < SHORT_DURATION_SEC only) |
| `CycleClassifier._classify_continuous()` | Overshoot/underheat checks | PARTIAL |
| `CycleHistory.record_cycle()` | `_cycleRecord()` + EMA updates | PARTIAL (EMA replaces rolling window) |
| `CycleHistory.window_statistics` (duty_ratio, fractions) | EMA vars: `_ema_dutyRatio`, `_ema_overshootFrac`, `_ema_underheatFrac` | PARTIAL (EMA, not windowed statistics) |
| `CycleHistory.flow_return_delta` p50/p90 | No equivalent | MISSING |
| `CycleHistory.flow_control_setpoint_error` p50/p90 | No equivalent | MISSING |
| `CycleHistory.off_with_demand_duration` | No equivalent | MISSING |

---

## Device Tracking (`device/` -> `SATcontrol.ino`)

| Python | C++ | Match |
|---|---|---|
| `DeviceTracker.update()` (state transitions) | `satUpdateBoilerStatus()` + `_bs_*` vars | PARTIAL |
| `DeviceTracker._record_flame_transitions()` | Flame ON/OFF tracking in `satUpdateBoilerStatus()` and `satCycleOnFlameChange()` | FULL |
| `DeviceTracker._record_hot_water_transitions()` | Hot water tracking in `satControlLoop()` | PARTIAL |
| `DeviceStatusEvaluator.evaluate()` | `satUpdateBoilerStatus()` state machine | PARTIAL (all 16 states have equivalents but some timing constants differ) |
| `DeviceStatusEvaluator.is_in_anti_cycling()` | Anti-cycle check (BS_ANTI_CYCLE_MIN_OFF_MS = 180s) | FULL |
| `DeviceStatusEvaluator.is_ignition_stalled()` | Stalled ignition check (BS_STALLED_IGNITION_MIN_MS = 600s) | PARTIAL (Python adapts threshold to last cycle duration; C++ uses fixed 600s) |
| `DeviceStatusEvaluator.is_overshoot_cooling()` | Overshoot cooling state in `satUpdateBoilerStatus()` | FULL |
| `DeviceStatusEvaluator.is_pump_start_phase()` | PUMP_STARTING state | FULL |
| `DeviceStatusEvaluator.is_ramping_up()` | IGNITION_SURGE state (0.5 C/s in 30s window) | FULL |
| `ModulationReliabilityTracker` | Modulation reliability in `satControlLoop()` | PARTIAL (simpler heuristic in C++) |

---

## Manufacturer Support (`manufacturer.py` -> `SATcontrol.ino`)

| Python | C++ | Match |
|---|---|---|
| `ManufacturerFactory.resolve_by_name(name)` | `satGetEffectiveManufacturer()` from settings | FULL |
| `ManufacturerFactory.resolve_by_member_id(id)` | `satDetectManufacturer(memberID)` | FULL |
| MANUFACTURERS dict (17 entries) | `satManufacturerTable[]` PROGMEM (17 entries) | FULL |
| Manufacturer quirks (individual files) | `SAT_QUIRK_*` bitmask per table entry | FULL |
| `supports_relative_modulation` False for Ideal/Intergas/Geminox/Nefit | `SAT_QUIRK_NO_REL_MOD` | FULL |
| Geminox min modulation 10% | `SAT_QUIRK_MIN_MOD_10` | FULL |
| `ManufacturerFactory.resolve_by_member_id()` returns list (ambiguous IDs) | C++ returns first match only | PARTIAL |

---

## Solar Gain (`solar_gain.py` -> `SATcontrol.ino`)

| Python | C++ | Match |
|---|---|---|
| `SolarGainController.update(signals)` | `satSolarGainUpdate()` | PARTIAL |
| `_indoor_rise_per_hour(sample)` | Indoor temp rise computation inline | PARTIAL (C++ computes over longer intervals) |
| `_is_solar_gain_event()` | Solar gain detection inline | PARTIAL |
| `_boiler_output_is_low()` | Flame + modulation check | FULL |
| Sun elevation from HA `sun.sun` entity | Sun elevation from weather API (Open-Meteo) or computed from lat/lon | PARTIAL |
| 10 min hold after solar gain detection | `SOLAR_GAIN_HOLD_SECONDS = 600s` | FULL |
| Integral freeze while solar gain active | `state.sat.bSolarGainActive` gate in `_pidUpdateIntegral()` | FULL |
| Setpoint offset during solar gain | Setpoint reduction by `settings.sat.fSolarGainOffset` | FULL |

---

## Summer Simmer Index (`summer_simmer.py` -> MISSING)

| Python | C++ | Match |
|---|---|---|
| `SummerSimmer.index(temp, humidity)` | No equivalent | MISSING |
| `SummerSimmer.perception(temp, humidity)` | No equivalent | MISSING |
| Thermal comfort mode (SSI replaces temp) | No equivalent | MISSING |

---

## Multi-Area Support (`area.py` -> MISSING)

| Python | C++ | Match |
|---|---|---|
| `Area` class (per-room PID + heating curve) | No equivalent | MISSING |
| `Areas._PIDs.output` (75th-percentile aggregation) | No equivalent | MISSING |
| `Areas._PIDs.overshoot_cap` (cooling cap) | No equivalent | MISSING |
| `Area.valve_position` (reads HA climate attr) | Not applicable | MISSING |
| `Area.weight` (demand weight [0,2]) | No equivalent | MISSING |

---

## Pressure Health (`binary_sensor.py::SatPressureHealthSensor` -> MISSING)

| Python | C++ | Match |
|---|---|---|
| EMA-smoothed pressure tracking | No equivalent | MISSING |
| Linear regression drop rate calculation | No equivalent | MISSING |
| Min/max pressure band check | No equivalent | MISSING |
| 120s problem confirmation delay | No equivalent | MISSING |

---

## Heating Curve Recommendation (`sensor.py::SatHeatingCurveRecommendationSensor` -> MISSING)

| Python | C++ | Match |
|---|---|---|
| Temperature error history (rolling daily window) | No equivalent | MISSING |
| INCREASE/DECREASE/HOLD recommendation | No equivalent | MISSING |
| Daily median error check | No equivalent | MISSING |

---

## OPV Calibration (`state.py` -> `SATcontrol.ino`)

| Python | C++ | Match |
|---|---|---|
| `SERVICE_START_OVERSHOOT_PROTECTION_CALCULATION` | `satOvpStartCalibration()` triggered via REST/MQTT | PARTIAL |
| `SERVICE_SET_OVERSHOOT_PROTECTION_VALUE` | REST API endpoint sets `settings.sat.fOvpValue` | PARTIAL |
| Calibration: set high setpoint + MM=0 | `satOvpCalibrate()` SAT_CALIB_STARTING phase | FULL |
| Calibration: wait for flame + warm-up | SAT_CALIB_WARMING phase | FULL |
| Calibration: measure max temp for 20 min | SAT_CALIB_MEASURING phase | FULL |
| Calibration: store result to persistent store | `settings.sat.fOvpValue` saved to LittleFS | FULL |
| OVERSHOOT_PROTECTION_REQUIRED_DATASET (40 samples) | No dataset requirement in C++ | MISSING |

---

## Weather Integration (`coordinator/` weather entity -> `SATweather.ino`)

| Python | C++ | Match |
|---|---|---|
| Read outside temp from HA weather entity | `weatherFetch()` from Open-Meteo API | PARTIAL (C++ fetches directly; same data semantics) |
| 24-hour hourly temperature forecast | `_weather_forecastTemp[24]` array | FULL |
| Stale sensor handling (max_age) | `SAT_STALE_OUTDOOR_MS` (10 min) | FULL |
| Multiple outside sensor fallback | C++ uses single weather source | PARTIAL |

---

## BLE Temperature Sensor (HA sensor entity -> `SATble.ino`)

| Python | C++ | Match |
|---|---|---|
| Read indoor temp from HA sensor entity | `satBLEGetTemperature()` via BLE scan (ESP32 only) | PARTIAL (Python reads any HA sensor; C++ reads BLE directly) |
| ATC/pvvx format parsing | `parseBLEAtcFormat()` | FULL |
| BTHome v2 format parsing | `parseBLEBTHomeFormat()` | FULL |
| Multiple sensor slots | `_bleSensors[4]` array | FULL |
| Stale sensor timeout | `BLE_STALE_MS = 300s` | FULL |
