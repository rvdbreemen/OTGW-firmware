# SAT Integration — Future Plan

Generated from the SAT audit (April 2026). This document captures all remaining open work from the audit, grouped by category, plus speculative future features. All items have corresponding backlog tasks.

---

## Status at time of writing

**Fixed in this sprint:**
- TASK-192: Control mode auto-switch thresholds corrected
- TASK-196: fEnergyTotal persisted to LittleFS
- TASK-211: MM=max sent during PWM OFF phase (was MM=0)
- TASK-213: `sat/current_modulation` now published to MQTT
- TASK-221: weatherFetch() HTTP timeout reduced to 5s (WDT safety)
- TASK-222: PID integral persisted across reboots

---

## Open audit-fix tasks

### Critical / High priority

| Task | Description |
|------|-------------|
| TASK-214 | No 65°C global setpoint cap (`MAXIMUM_SETPOINT` in Python) — boiler can be overdriven in edge cases |
| TASK-226 | Pressure health monitoring missing — Python SAT tracks low/high CH pressure from OT MsgID 18 |

### Medium priority — correctness gaps

| Task | Description |
|------|-------------|
| TASK-194 | Setpoint clamp applied when flame is off — should pass through unchanged |
| TASK-198 | DHW setpoint (TW=) never sent to boiler; 300s post-DHW overshoot guard missing |
| TASK-200 | Solar gain modulation threshold: 30% instead of Python's 20% |
| TASK-203 | No per-hour PWM cycle counter — `cycles_per_hour` limit never enforced |
| TASK-204 | Thermal comfort mode: Summer Simmer Index computed but never fed into PID loop |
| TASK-207 | Sensor staleness timeout: 5 min vs Python's 6 hours — too aggressive |
| TASK-208 | PWM ignition wait: no 180s timeout — waits indefinitely for flame |
| TASK-218 | `set_temperature` does not auto-map to matching preset |
| TASK-219 | `PRESET_NONE` does not restore pre-custom temperature |
| TASK-223 | Stalled ignition adaptive threshold not implemented |
| TASK-224 | OPV calibration: minimum 40-sample requirement not enforced |
| TASK-225 | Cycle classifier uses `max_flow` instead of p90 flow temperature |
| TASK-229 | PWM minimum ON time: 30s in C++ vs 180s in Python — too aggressive cycling |
| TASK-230 | Radiator max setpoint: 62°C in C++ vs Python default 55°C — needs review |

### Minor / low priority — compliance and polish

| Task | Description |
|------|-------------|
| TASK-193 | PID deadband default: 0.25°C vs Python's 0.1°C |
| TASK-195 | SATweather.ino uses HTTPClient without explicit `#include` (implicit, works but fragile) |
| TASK-199 | `sat/ble_*` MQTT topics not documented as ESP32-only in topic reference or HA discovery |
| TASK-201 | NTP hostname-reset workaround not guarded with `#if defined(ESP8266)` |
| TASK-202 | BLE scan callback allocates 3 String objects per advertisement — heap fragmentation |
| TASK-205 | `satHandleControlMode()` uses `strcmp(value, "1")` etc. — PROGMEM violation |
| TASK-206 | Preset not restored when SAT re-enables after being disabled |
| TASK-227 | Rolling 4-hour windowed cycle statistics not implemented |
| TASK-228 | Heating curve recommendation (INCREASE/DECREASE/HOLD) not implemented |

---

## Documentation tasks

| Task | Description |
|------|-------------|
| TASK-209 | MQTT.md SAT section: only 8 of 80+ topics documented |
| TASK-210 | openapi.yaml: zero SAT endpoints (15 are implemented in firmware) |
| TASK-212 | MQTT.md HA discovery: 8 of 20+ SAT entities listed |
| TASK-215 | SAT settings page: ~10 settings likely missing from WebUI |
| TASK-216 | README.md has no SAT section or links |
| TASK-217 | ADR-061 filename collision — WiFi reconnect ADR needs renumbering to ADR-075 |

---

## Future feature ideas (no task yet / deliberately deferred)

These were identified as MISSING from the Python SAT feature set but have no viable implementation path with the current architecture. Tasks TASK-231 to TASK-233 have been created for tracking.

### TASK-231 — MQTT humidity input for Summer Simmer Index

Python SAT uses humidity to compute the Summer Simmer Index and feeds it to the PID as the effective room temperature when thermal comfort mode is active. The OT protocol has no humidity message.

**Proposed solution:** Add MQTT subscription `set/<nodeId>/sat/humidity`. Any external sensor (Zigbee, ESPHome, BLE on OTGW32) can push humidity in. When `sat_thermal_comfort = true` and humidity is fresh (< timeout), use SSI as the PID input instead of raw room temperature.

**Effort:** Medium. MQTT input path pattern already exists. SSI formula is simple. Main work is settings + timeout handling.

### TASK-232 — Boiler power and gas consumption estimation

Python SAT estimates gas consumption as `modulation% × rated_power × efficiency × time`. OT MsgID 17 provides modulation but not rated power.

**Proposed solution:** Add user-configurable `sat_boiler_rated_kw` (default 0 = disabled) and `sat_boiler_efficiency` (default 0.92) settings. Accumulate estimated kWh into `/sat_energy_estimate.json` (save every 0.1 kWh). Publish as `sat/energy_estimated_kwh` retained MQTT topic for HA energy dashboard.

If OT MsgID 74 (CH water flow rate) is available from the boiler, thermal power can be computed more accurately from flow rate × ΔT — but this MsgID is rarely implemented by boilers.

**Effort:** Low–medium. Math is trivial. Main effort is LittleFS persistence and HA integration.

### TASK-233 — Multi-zone PID heating control via MQTT room sensors

Python SAT supports multiple heating zones with independent thermostats and PID loops. OT is single-master/single-slave with one room temp input (MsgID 24) and one CH setpoint output (MsgID 16).

**Proposed solution:** Implement N virtual PIDs via MQTT zone inputs. Each zone pushes `room_temp` and `setpoint` to `set/<nodeId>/sat/zone/<n>/…`. Firmware runs N PID instances and selects the most-demanding CH setpoint (highest demand wins). The boiler still receives one CH setpoint — this gives zone-aware control without multi-thermostat OT support.

**Effort:** High. Requires dynamic PID state allocation, zone selector logic, MQTT per-zone diagnostics, and new settings. Should be a separate sprint.

---

## Suggested sprint order

1. **Sprint A (safety/correctness):** TASK-214, TASK-226, TASK-207, TASK-208, TASK-229
2. **Sprint B (feature gaps):** TASK-194, TASK-198, TASK-200, TASK-203, TASK-204, TASK-218, TASK-219
3. **Sprint C (completeness):** TASK-223, TASK-224, TASK-225, TASK-230, TASK-227, TASK-228
4. **Sprint D (docs + polish):** TASK-193, TASK-202, TASK-205, TASK-206, TASK-209, TASK-210, TASK-212, TASK-215, TASK-216, TASK-217
5. **Sprint E (future features):** TASK-231 (humidity/SSI), TASK-232 (gas estimation), TASK-233 (multi-zone)
