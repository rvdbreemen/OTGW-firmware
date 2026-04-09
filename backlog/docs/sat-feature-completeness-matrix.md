# SAT Feature Completeness Matrix

Source: Python `custom_components/sat/` vs C++ `src/OTGW-firmware/SAT*.ino`
Date: 2026-04-09
Audit phase: Phase A, Task-91

Status values:
- FULL: Equivalent or better in C++
- PARTIAL: Ported but with simplifications or gaps
- MISSING: No C++ equivalent
- N/A: HA-only, intentionally not ported

---

## Core Control Loop

| Feature | Status | C++ location | Notes |
|---|---|---|---|
| PID: proportional term | FULL | SATpid.ino | |
| PID: integral term (deadband accumulation) | FULL | SATpid.ino | |
| PID: derivative term (temperature slope) | FULL | SATpid.ino | |
| PID: auto-gain from heating curve | FULL | SATpid.ino | kp/ki/kd formulas identical |
| PID: manual gain mode | PARTIAL | SATpid.ino | Selectable but less tested |
| PID: integral freeze (solar gain) | FULL | SATpid.ino | Via state.sat.bSolarGainActive |
| PID: state persistence across restarts | MISSING | -- | Python uses HA Store; firmware restarts zeroed |
| Heating curve formula | FULL | SATcontrol.ino | Formula 100% identical |
| Heating curve coefficient | FULL | SATcontrol.ino | settings.sat.fHeatingCurveCoeff |
| Heating system type (radiators/underfloor/heat pump) | FULL | SATcontrol.ino | satGetEffectiveHeatingSystem() |
| Base offset per system (20 / 27.2) | FULL | SATcontrol.ino | satGetBaseOffset() |
| Setpoint min/max clamp | FULL | SATcontrol.ino | |
| Continuous mode: setpoint clamping vs boiler temp | PARTIAL | SATcontrol.ino | Python has more edge cases covered |
| PWM mode: duty cycle calculation (4-region) | FULL | SATcontrol.ino | All four regions ported |
| PWM mode: on/off state machine | FULL | SATcontrol.ino | ON->OFF->ON transitions |
| PWM mode: effective boiler temperature (EWMA) | FULL | SATcontrol.ino | |
| PWM mode: min ON time enforcement | PARTIAL | SATcontrol.ino | C++ uses 30s; Python uses 180s |
| PWM mode: setpoint suppression (flame on phase) | PARTIAL | SATcontrol.ino | Flame-off hold setpoint logic present |
| PWM mode: setpoint from return temp (flame off) | PARTIAL | SATcontrol.ino | |
| Sustained overshoot -> enable PWM (60s) | FULL | SATcycles.ino | |
| Sustained underheat -> disable PWM (180s) | FULL | SATcycles.ino | |
| Sustained saturation -> disable PWM (300s) | FULL | SATcycles.ino | |
| DHW overshoot guard (300s after DHW) | FULL | SATcontrol.ino | |
| Relative modulation control (MM command) | FULL | SATcontrol.ino | |
| Geminox min modulation 10% quirk | FULL | SATcontrol.ino | SAT_QUIRK_MIN_MOD_10 |
| Max modulation always 100% for heat pumps | FULL | SATcontrol.ino | satAlwaysMaxModulation() |
| CS=0 on heater OFF | FULL | SATcontrol.ino | |
| OPV calibration state machine | FULL | SATcontrol.ino | satOvpCalibrate() |
| OPV dataset requirement (40 samples) | MISSING | -- | C++ calibrates from 20-min measurement only |

---

## Cycle Tracking

| Feature | Status | C++ location | Notes |
|---|---|---|---|
| Flame ON/OFF transition detection | FULL | SATcycles.ino | satCycleOnFlameChange() |
| Cycle duration measurement | FULL | SATcycles.ino | |
| Cycle kind detection (CH/DHW/MIXED) | FULL | SATcycles.ino | Same thresholds |
| Max/min flow temperature per cycle | FULL | SATcycles.ino | |
| Overshoot seconds per cycle | FULL | SATcycles.ino | |
| Cycle classification (GOOD/OVERSHOOT/UNDERHEAT/SHORT/UNCERTAIN) | PARTIAL | SATcycles.ino | C++ uses simpler max-flow criterion; Python uses tail percentile metrics |
| Short cycling detection | PARTIAL | SATcycles.ino | C++ uses duration threshold only |
| Cycle history (rolling window) | PARTIAL | SATcycles.ino | C++ uses EMA with fixed history size [16]; Python uses full deque with pruning |
| Duty ratio tracking | PARTIAL | SATcycles.ino | C++ uses EMA; Python uses windowed sum |
| Overshoot fraction tracking | PARTIAL | SATcycles.ino | C++ uses EMA |
| Underheat fraction tracking | PARTIAL | SATcycles.ino | C++ uses EMA |
| Flow/return delta percentiles (p50/p90) | MISSING | -- | Only tracked in Python history |
| Flow/setpoint error percentiles | MISSING | -- | Only tracked in Python history |
| Off-with-demand duration | MISSING | -- | |
| 4h/24h rolling window statistics | MISSING | -- | C++ has no windowed aggregation |
| Cycle phase detection (startup/steady/cooldown) | FULL | SATcycles.ino | satCycleGetPhaseName() |
| Cycle events (EVENT_SAT_CYCLE_STARTED/ENDED) | PARTIAL | SATcontrol.ino | Published via MQTT; no HA event bus |

---

## Device Status

| Feature | Status | C++ location | Notes |
|---|---|---|---|
| BoilerStatus OFF | FULL | SATcontrol.ino | |
| BoilerStatus IDLE | FULL | SATcontrol.ino | |
| BoilerStatus PREHEATING | FULL | SATcontrol.ino | |
| BoilerStatus AT_SETPOINT_BAND | FULL | SATcontrol.ino | |
| BoilerStatus MODULATING_UP | FULL | SATcontrol.ino | |
| BoilerStatus MODULATING_DOWN | FULL | SATcontrol.ino | |
| BoilerStatus IGNITION_SURGE | FULL | SATcontrol.ino | 0.5 C/s in 30s window |
| BoilerStatus CENTRAL_HEATING | FULL | SATcontrol.ino | |
| BoilerStatus HEATING_HOT_WATER | FULL | SATcontrol.ino | |
| BoilerStatus COOLING | FULL | SATcontrol.ino | |
| BoilerStatus ANTI_CYCLING | FULL | SATcontrol.ino | 180s threshold |
| BoilerStatus PUMP_STARTING | FULL | SATcontrol.ino | |
| BoilerStatus WAITING_FOR_FLAME | FULL | SATcontrol.ino | |
| BoilerStatus OVERSHOOT_COOLING | FULL | SATcontrol.ino | |
| BoilerStatus POST_CYCLE_SETTLING | FULL | SATcontrol.ino | |
| BoilerStatus STALLED_IGNITION | PARTIAL | SATcontrol.ino | C++ uses fixed 600s; Python adapts to last cycle duration |
| Modulation reliability tracking | PARTIAL | SATcontrol.ino | C++ simpler heuristic |
| Flame transition timestamps | FULL | SATcontrol.ino | _bs_flameOnMs / _bs_flameOffMs |
| DHW transition timestamps | PARTIAL | SATcontrol.ino | |

---

## Manufacturer Support

| Feature | Status | C++ location | Notes |
|---|---|---|---|
| Auto-detection from OT MemberID | FULL | SATcontrol.ino | satDetectManufacturer() |
| Manual manufacturer selection | FULL | SATcontrol.ino | settings.sat.iManufacturer |
| 17 manufacturers + Other | FULL | SATcontrol.ino | PROGMEM table |
| Ambiguous MemberID handling (shared IDs 4, 27) | PARTIAL | SATcontrol.ino | C++ returns first match; Python returns list |
| No relative modulation (Ideal/Intergas/Geminox/Nefit) | FULL | SATcontrol.ino | SAT_QUIRK_NO_REL_MOD |
| Geminox minimum modulation 10% | FULL | SATcontrol.ino | SAT_QUIRK_MIN_MOD_10 |
| Ideal/Intergas MI-500 boot quirk | FULL | SATcontrol.ino | SAT_QUIRK_MI_500_BOOT |
| Immergas TP quirk | FULL | SATcontrol.ino | SAT_QUIRK_IMMERGAS_TP |

---

## Solar Gain

| Feature | Status | C++ location | Notes |
|---|---|---|---|
| Indoor temperature rise rate calculation | PARTIAL | SATcontrol.ino | Same formula; different sampling interval |
| Sun elevation check | PARTIAL | SATcontrol.ino | C++ uses weather API; Python reads sun.sun entity |
| Solar gain event detection | PARTIAL | SATcontrol.ino | |
| 10-minute hold after detection | FULL | SATcontrol.ino | SOLAR_GAIN_HOLD_SECONDS = 600s |
| Integral freeze on solar gain | FULL | SATpid.ino | |
| Setpoint offset reduction | FULL | SATcontrol.ino | settings.sat.fSolarGainOffset |
| Minimum elevation threshold (configurable) | FULL | SATcontrol.ino | settings.sat.fSolarGainMinElevation |
| Minimum rise rate threshold (configurable) | FULL | SATcontrol.ino | settings.sat.fSolarGainMinRise |

---

## Ancillary Features

| Feature | Status | C++ location | Notes |
|---|---|---|---|
| Weather fetch (outdoor temp + humidity + wind) | FULL | SATweather.ino | Open-Meteo API |
| 24h hourly temperature forecast | FULL | SATweather.ino | |
| BLE temperature sensor (ATC/pvvx) | FULL (ESP32) | SATble.ino | ESP32 only; up to 4 sensors |
| BLE temperature sensor (BTHome v2) | FULL (ESP32) | SATble.ino | |
| Multi-area / secondary room PIDs | MISSING | -- | Python has full multi-area support |
| Area overshoot cap | MISSING | -- | |
| Area valve position weighting | MISSING | -- | |
| Summer Simmer Index | MISSING | -- | |
| Thermal comfort mode (SSI replaces room temp) | MISSING | -- | |
| Boiler power estimation (kW) | MISSING | -- | |
| Gas consumption estimation (m3/h) | MISSING | -- | |
| Pressure health monitoring | MISSING | -- | |
| Pressure drop rate (linear regression) | MISSING | -- | |
| Heating curve recommendation (INCREASE/DECREASE/HOLD) | MISSING | -- | |
| Temperature error statistics (daily window) | MISSING | -- | |
| Preset temperatures (home/away/sleep/comfort/activity) | N/A | -- | HA ClimateEntity feature |
| Window sensor integration | N/A | -- | Handled in HA; MQTT notification only in firmware |
| Thermostat follow mode (push setpoint) | N/A | -- | HA service call feature |
| Secondary climate cascade | N/A | -- | HA service call feature |
| Config flow / UI wizard | N/A | -- | HA integration setup |
| HA entity registration | N/A | -- | HA platform feature |
| Sentry error monitoring | N/A | -- | HA/cloud feature |

---

## Summary Statistics

| Category | Total features | FULL | PARTIAL | MISSING | N/A |
|---|---|---|---|---|---|
| Core control loop | 23 | 16 | 5 | 2 | 0 |
| Cycle tracking | 17 | 7 | 6 | 4 | 0 |
| Device status | 17 | 15 | 2 | 0 | 0 |
| Manufacturer support | 8 | 7 | 1 | 0 | 0 |
| Solar gain | 7 | 5 | 2 | 0 | 0 |
| Ancillary features | 19 | 4 | 0 | 8 | 7 |
| **Total** | **91** | **54 (59%)** | **16 (18%)** | **14 (15%)** | **7 (8%)** |

---

## Critical Gaps (MISSING features with heating-quality impact)

1. **Multi-area PID aggregation** -- single-zone only; Python supports up to N rooms with demand weighting
2. **Cycle metrics (flow/return delta p50/p90, setpoint error p50/p90)** -- C++ classifier is simpler and less accurate
3. **Rolling windowed cycle statistics (4h/24h)** -- C++ uses EMA which responds differently
4. **Heating curve recommendation** -- no self-diagnostic capability in firmware
5. **Pressure health monitoring** -- boiler pressure not monitored; leak/air detection not available
6. **PID state persistence across restarts** -- PID starts from zero after reboot; Python restores from storage
7. **OPV dataset requirement (40 samples)** -- C++ has no minimum sample count before accepting calibration result
8. **Stalled ignition adaptive threshold** -- C++ uses fixed 600s; Python adapts to last cycle duration
