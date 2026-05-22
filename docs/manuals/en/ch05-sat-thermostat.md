## Chapter 5: SAT - Smart Autotune Thermostat

### Overview

SAT (Smart Autotune Thermostat) transforms the OpenTherm Gateway from a transparent protocol bridge into an active, self-tuning heating controller. Instead of forwarding your wall thermostat's on/off requests to the boiler, SAT takes over: it reads the current room temperature, calculates the exact water temperature the boiler needs to produce, and injects that setpoint directly into the OpenTherm stream.

The result is weather-compensated heating that keeps room temperatures stable within a fraction of a degree, reduces gas consumption, and dramatically cuts the number of boiler ignition cycles. SAT is entirely self-contained: it runs on the ESP microcontroller with no dependency on Home Assistant, cloud services, or internet access.

SAT was inspired by (and ported from) the excellent Home Assistant SAT custom component by Alex Wijnholds (Alexwijn), with design feedback and validation from George Dellas (sergeantd). The firmware embeds the controller directly in the OTGW so the heating loop keeps running even when Home Assistant, MQTT, or WiFi are unavailable.

---

### 5.1 What SAT Does

With SAT, every control loop cycle (default 30 seconds):

1. Reads room temperature (from the OT bus, an external sensor pushed via MQTT, a BLE sensor on ESP32, or a weighted multi-area average).
2. Reads outdoor temperature (from the OT bus, an external MQTT push, or the built-in Open-Meteo weather API).
3. Calculates the heating curve value: the base flow temperature needed given current outdoor conditions.
4. Applies a PID v3 correction on top of the heating curve (with automatic or manual gain selection).
5. Applies optional adjustments: solar gain compensation, thermal comfort (humidity) correction, multi-zone override.
6. Clamps the result within safety limits.
7. Sends a `CS=` command to the PIC gateway to inject the setpoint into the OpenTherm stream.
8. Publishes all state to MQTT and the web UI dashboard.

---

### 5.2 Prerequisites and Compatibility

- An OpenTherm-compatible boiler. SAT includes a manufacturer table with quirk flags for Atag, Baxi, Ferroli, Geminox, Ideal, Immergas, Intergas, Itho, Nefit, Radiant, Remeha, Sime, Vaillant, Viessmann, Worcester, and others. Auto-detection is the default.
- A room temperature source (in priority order): OT bus (MsgID 24), external MQTT push, REST API push, multi-area weighted average, or BLE sensor (ESP32 only).
- An outdoor temperature source: OT bus (MsgID 27), external MQTT push, or the built-in Open-Meteo weather API fetch.
- You do not need to remove your wall thermostat. SAT intercepts the setpoint; your thermostat's away mode and schedule still work.
- The OTGW must be in gateway mode (GW=1), not bypass or monitor mode.

---

### 5.3 Enabling SAT

#### Via the Web UI

1. Open the web interface (`http://otgw.local/`).
2. Go to the SAT tab or Settings > SAT.
3. Set "SAT Enabled" to on.
4. Choose your heating system type (auto-detect, radiators, heat pump, or underfloor).
5. Set the target room temperature and heating curve coefficient.
6. Save settings.

#### Via MQTT

```bash
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/enabled" -m "true"
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/target" -m "21.0"
```

Every configurable SAT parameter has a matching MQTT command topic of the form `%mqtt_sub_topic%/sat/<name>`. The full set is declared in `MQTTstuff.ino` (`handleMQTTcallback`) and covers: `enabled`, `target`, `control_mode`, `heating_system`, `manufacturer`, `max_modulation`, `interval`, `preset`, `heating_curve`, `deadband`, `overshoot_margin`, `flow_offset`, `flame_off_offset`, `mod_sup_delay`, `mod_sup_offset`, `cycles_per_hour`, `valve_offset`, `boiler_capacity`, `target_temp_step`, `sensor_max_age`, `error_monitoring`, `auto_gains_value`, `heating_mode`, `force_pwm`, `pwm_auto_switch`, `push_setpoint`, `ovp_enabled`, `ovp_value`, `ovp_start`, `ovp_stop`, `reset_integral`, `flush`, `flush_threshold_h`, `simulation`, `solar_gain`, `solar_freeze_integral`, `solar_min_elevation`, `sun_elevation`, `summer_simmer`, `summer_threshold`, `summer_min_hours`, `comfort_adjust`, `comfort_humidity`, `comfort_max_offset`, `thermal_comfort`, `humidity`, `humidity_timeout_s`, `window`, `window_detection`, `multi_area`, `multi_area_count`, `area/<0..3>`, `auto_tune`, `auto_tune_rate`, `zone_count`, `zone_timeout_s`, `zone/<n>/room_temp`, `zone/<n>/setpoint`, `valves_open`, `preset_sync`, `preset_sync_topic`, `preset_<comfort|eco|away|sleep|activity|home>`, `min_pressure`, `max_pressure`, `max_pressure_drop`, `dhw_enabled`, `dhw_setpoint`, `indoor_temp`, `outdoor_temp`, `ble_enable`, `ble_mac`, `ble_interval`.

Sending `ON` / `OFF` / `true` / `false` / `1` / `0` works for boolean topics. Numeric topics accept the usual decimal representation.

#### Via REST API

```bash
curl -X POST -d '{"name":"satenabled","value":"true"}' \
  http://otgw.local/api/v2/settings
```

---

### 5.4 Configuration Parameters

#### Core settings

| Setting | Default | Range | Description |
|---|---|---|---|
| `satenabled` | false | bool | Master on/off switch |
| `satheatsystem` | 0 | 0-3 | 0 = auto-detect, 1 = radiators, 2 = heat pump, 3 = underfloor |
| `sattargettemp` | 20.0 | 5-30 C | Desired room temperature |
| `satcoefficient` | 1.5 | 0.1-5.0 | Heating curve steepness |
| `satdeadband` | 0.1 | 0.05-2.0 C | PID deadband width |
| `satinterval` | 30 | 10-300 s | Control loop interval |
| `satexternaltemp` | false | bool | Use external indoor sensor via MQTT |
| `satpwmautoswitch` | true | bool | Automatically switch between continuous and PWM modes |
| `satforcepwm` | false | bool | Force PWM mode regardless of boiler modulation |
| `satmaxrelmod` | 100 | 0-100 % | Maximum relative modulation sent to boiler (MM= command) |
| `satmanufacturer` | 0 | 0-18 | Boiler manufacturer (0 = auto-detect) |

#### Preset temperatures

| Setting | Default | Range | Description |
|---|---|---|---|
| `satpresetcomfort` | 21.0 | 15-28 C | Comfort preset |
| `satpreseteco` | 18.0 | 10-22 C | Eco preset |
| `satpresetaway` | 15.0 | 5-18 C | Away preset |
| `satpresetsleep` | 16.0 | 5-22 C | Sleep preset |
| `satpresethome` | 18.0 | 5-28 C | Home preset |
| `satpresetactivity` | 10.0 | 5-20 C | Activity preset (used by window detection) |
| `satpresetsync` | false | bool | Sync preset changes to secondary entities via MQTT |

#### PID tuning

| Setting | Default | Range | Description |
|---|---|---|---|
| `satautogains` | true | bool | Automatic PID gain calculation from heating curve |
| `satkpmanual` | 5.0 | 0-100 | Manual proportional gain (when autogains = false) |
| `satkimanual` | 0.0005 | 0-1 | Manual integral gain |
| `satkdmanual` | 0.0 | 0-10 | Manual derivative gain |
| `satautotune` | false | bool | Enable automatic PID gains tuning |
| `satautotunerate` | 0.02 | 0-1 | Adjustment rate per tuning cycle (2%) |

The integral term uses a **symmetric clamp** `[-curveValue, +curveValue]` with a `+/-20 C` hard safety cap (alpha.28 / alpha.30). Negative accumulation is required in mild weather so the controller can nudge the flow setpoint below the heating curve when the room is slightly above target. The PID integral and raw derivative state are persisted on every save and **warm-started** on the next boot (alpha.32), so a power cycle does not reset to a cold start. `sat/flush` or an explicit `satdisable` clears the persisted state.

#### Advanced control

| Setting | Default | Range | Description |
|---|---|---|---|
| `satmaxsetpoint` | 65.0 | 30-80 C | Global safety ceiling for flow setpoint |
| `satflameoffoffset` | 0.0 | 0-5 C | Setpoint offset when flame off (anti-cycling hysteresis) |
| `satovershootmargin` | 2.0 | 0-10 C | Overshoot margin for cycle classification |
| `satmodsupdelay` | 20.0 | 0-60 s | Modulation suppression delay |
| `satmodsupoffset` | 1.0 | 0-5 C | Modulation suppression offset below setpoint |
| `satflowoffset` | 2.0 | 0-10 C | Continuous mode: max setpoint drop from boiler temp |
| `satcyclesperhour` | 3 | 2-6 | Target cycles per hour |
| `satpushsetpoint` | false | bool | Push SAT target to thermostat display (TC= command) |

#### Choosing the heating curve coefficient

| Insulation level | Building type | Suggested coefficient |
|---|---|---|
| Excellent | Modern passive house, triple glazing | 0.5 - 0.8 |
| Good | Post-2000 build, cavity wall insulation | 0.8 - 1.2 |
| Average | 1970s-2000s, partial insulation | 1.2 - 1.8 |
| Poor | Pre-1970, single glazing, no cavity insulation | 1.8 - 2.5 |

Start in the middle of the appropriate range. If the room consistently fails to reach the target, increase the coefficient by 0.2-0.3. If the room overshoots, decrease it. SAT also publishes a heating curve recommendation (see section 5.12) to help you dial in the correct value.

---

### 5.5 How the Heating Curve Works

```
setpoint = base_offset + (coefficient / 4) * curve_value

curve_value = 4 * (T_target - 20)
            + 0.03 * (T_outside - 20)^2
            - 0.4  * (T_outside - 20)
```

Where `base_offset` is 27.2 C for radiators and 20.0 C for underfloor.

Worked examples for a radiator system, coefficient 1.5:

| Outside temp | Target | Flow setpoint |
|---|---|---|
| +15 C | 20 C | ~28 C |
| +5 C | 20 C | ~32 C |
| -5 C | 20 C | ~38 C |
| -10 C | 21 C | ~43 C |

---

### 5.6 Control Modes: Continuous and PWM

**Continuous mode (default)**: SAT sends the PID output directly as a flow temperature setpoint. The boiler modulates to reach and hold it. Preferred for modern condensing boilers with good modulation range.

Continuous mode includes clamp logic: if the PID setpoint drops rapidly but the boiler water is still hot, the minimum setpoint is clamped to `boiler_temp - flow_offset` (default 2 C). This prevents the boiler from stopping and immediately restarting (short cycling).

**PWM mode**: For boilers that cannot modulate below 30-40%, SAT cycles the flame on and off at a calculated duty cycle. Minimum flame-on time is 180 seconds to prevent excessively short burner cycles.

**Force PWM**: Setting `satforcepwm` to true forces PWM mode regardless of boiler behavior.

**Auto-switch**: When `satpwmautoswitch` is enabled (default), SAT monitors boiler behavior using cycle classification (see section 5.11) and switches modes automatically:

- Continuous to PWM: when flow temperature stays more than the overshoot margin above setpoint for 60 seconds (sustained overshoot, indicating the boiler cannot modulate low enough).
- PWM to continuous: when flow temperature stays more than 2 C below setpoint for 180 seconds (sustained underheat) or after 300 seconds of saturation (boiler at max but room still cold).

| Situation | Recommended mode |
|---|---|
| Modern condensing boiler with good modulation | Continuous |
| Boiler with high minimum modulation (>30%) | PWM |
| Underfloor heating (slow response) | Continuous |
| Oversized boiler with radiators | PWM |
| Not sure | Start with Continuous, auto-switch enabled |

---

### 5.7 Safety System

SAT implements six independent safety layers. Any single layer tripping sends `CS=0`, releasing the setpoint override and returning control to the wall thermostat immediately.

**Fundamental safety principles:**
- Fail-safe: every error results in `CS=0`. The boiler falls back to the wall thermostat.
- No automatic recovery: SAT stays disabled until the user explicitly re-enables it.
- Independent layers: a bug in one layer does not affect the others.

**Layer 1: Boot safety.** At every startup, restart, or power failure, SAT sends `CS=0` before the first control cycle. This prevents the boiler from running indefinitely at the last setpoint while the ESP is still booting.

**Layer 2: Disable safety.** When SAT is turned off (via settings, MQTT, or REST API), `CS=0` is sent immediately, the PID is reset, and all state variables are cleared.

**Layer 3: Stale sensor fallback.** External temperatures expire (5 min indoor, 10 min outdoor, 5 min BLE). SAT falls back to OT bus values without alarming. If the OT bus value is also unavailable, Layer 5 activates.

**Layer 4: Hard temperature ceiling.** After every calculation, the setpoint is clamped: underfloor max 50 C, radiators max 80 C, global safety ceiling configurable (default 65 C). Minimum is 10 C.

**Layer 5: Consecutive sensor failure counter.** 10 consecutive invalid room temperature readings trips the safety.

**Layer 6: PIC communication check.** 5 consecutive failed `CS=` commands trips the safety.

**Recovery after safety shutdown:**
1. The `safety_tripped` field is visible in the web interface (red badge), MQTT (`sat/safety_tripped`), and REST API.
2. SAT stays inactive until the user explicitly re-enables it.
3. Recovery via web interface: toggle the SAT switch off and back on.
4. Recovery via MQTT: publish `true` to `OTGW/set/{UniqueId}/sat/enabled`.

---

### 5.8 Simulation Mode

Simulation mode lets you run the SAT control loop without sending real commands to the PIC. In simulation mode:
- SAT calculates all setpoints and PID values normally.
- SAT does NOT send commands to the PIC.
- All MQTT topics and REST API values are available with simulated data.
- The simulated room temperature responds to the calculated setpoint using configurable heating and cooling rates (`fSimHeatRate`, `fSimCoolRate`).

Useful for testing Home Assistant automations, verifying that the heating curve produces logical values, and demonstrations without an active heating system.

```bash
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/simulation" -m "ON"
```

---

### 5.9 Weather Integration (Open-Meteo)

SAT can fetch outdoor weather data and a 24-hour hourly temperature forecast from the Open-Meteo API. This is a free API that requires no API key.

On ESP32 the firmware requests the full Open-Meteo current-conditions set: temperature, apparent temperature, humidity, wind speed / direction / gusts, cloud cover, sea-level pressure, precipitation / rain / snowfall, sun elevation, WMO weather code, and the `is_day` flag. The hourly arrays include temperature, dew point, cloud cover, and precipitation probability. ESP8266 stays on a minimal 5-field request to fit memory.

Weather data provides:
- Outdoor temperature for the heating curve (fallback or primary, depending on configuration).
- Humidity data for thermal comfort adjustment.
- Sun elevation data for solar gain gating.
- Forecast data for proactive setpoint planning.

| Setting | Default | Range | Description |
|---|---|---|---|
| `satweatherenable` | false | bool | Enable weather data fetching |
| `satweatherlat` | 0.0 | -90 to 90 | Latitude (from browser geolocation or manual entry) |
| `satweatherlon` | 0.0 | -180 to 180 | Longitude |
| `satweatherinterval` | 900 | 300-3600 s | Poll interval (default 15 min, minimum 5 min) |

The web UI can auto-detect your location using browser geolocation. Weather data is fetched via plain HTTP (no HTTPS). The first fetch is gated 5 minutes after boot so DHCP, NTP, and MQTT have time to settle before the first HTTPS-free Open-Meteo call. The streaming JSON parser is allocation-free (no malloc inside the SAT loop). For debugging, the telnet console accepts `w` to force an immediate fetch + dump (TASK-513).

---

### 5.10 Solar Gain Compensation

Solar gain compensation detects when sunlight through windows is heating the room and reduces the flow setpoint to prevent overheating.

**How it works:**
1. SAT tracks the indoor temperature rise rate using an EMA (exponential moving average).
2. When the rise rate exceeds the configured minimum and boiler modulation is low (<20%), a solar gain condition is detected.
3. If sun elevation data is available (from the weather API), SAT also checks that the sun is above the minimum elevation before triggering.
4. The condition must be sustained for 10 minutes before activation (to avoid false triggers).
5. Once active, the flow setpoint is reduced by `fSolarSetpointOffset` (default 2 C).
6. When the rise rate drops below the threshold for 10 minutes, solar gain is cleared.
7. Optionally, the PID integral term is frozen during solar gain to prevent integral windup.

| Setting | Default | Range | Description |
|---|---|---|---|
| `satsolargain` | false | bool | Enable solar gain compensation |
| `satsolarminrise` | 0.5 | 0.1-5.0 C/hr | Minimum rise rate to trigger |
| `satsolaroffset` | 2.0 | 0.5-5.0 C | Setpoint reduction during solar gain |
| `satsolarminelevation` | 12.0 | 0-90 degrees | Minimum sun elevation for activation |
| `satsolarfreezeintegral` | true | bool | Freeze PID integral during solar gain |

---

### 5.11 Cycle Monitoring

SAT tracks flame on/off transitions and classifies each completed heating cycle. This drives automatic PWM/continuous mode switching and provides diagnostic data.

**Per-cycle metrics collected:**
- Duration (seconds)
- Maximum flow temperature
- Overshoot time (seconds above setpoint + margin)
- Flow-setpoint error
- Flow-return temperature delta (average)

**Cycle classification:**
- NORMAL: healthy cycle with appropriate duration and no sustained overshoot.
- SHORT_CYCLING: cycle shorter than 60 seconds (indicates boiler cannot modulate low enough).
- OVERSHOOT: flow temperature exceeded setpoint plus overshoot margin for more than 60 seconds.
- UNDERHEAT: flow temperature stayed more than 2 C below setpoint for more than 180 seconds.

Since alpha.31, classification uses **tail-window percentiles** (last 180 s on ESP32, 64 s on ESP8266) rather than the full-cycle P90/P10. A cycle that started with overshoot but corrected itself is no longer flagged OVERSHOOT on the strength of its early peak; the full-cycle P90 is still logged alongside for diagnostics.

**Rolling statistics:**
- Cycles per hour counter (rolling 60-minute window, max 6).
- 4-hour rolling window with per-cycle records.
- EMA-smoothed fractions: duty ratio, overshoot fraction, underheat fraction, long-cycle fraction.

**Heating Curve Recommendation (HCR):**
SAT collects intra-day error samples (the difference between the room temperature and the target) and computes a daily median. Using a rolling window of up to 30 daily medians persisted to LittleFS (`/sat/sat_hcr.json`), SAT publishes a recommendation:

| Condition | Recommendation |
|---|---|
| Daily median error > +0.5 C for 3+ consecutive days | DECREASE (room consistently too warm) |
| Daily median error < -0.5 C for 3+ consecutive days | INCREASE (room consistently too cold) |
| Otherwise | HOLD (curve is well tuned) |

The recommendation is published to MQTT as `sat/curve_recommendation` and shown in the web UI. It survives reboots.

---

### 5.12 Pressure Monitoring

SAT monitors boiler CH water pressure (OT MsgID 18) and generates alarms when pressure leaves the configured band.

**Features:**
- EMA-smoothed pressure reading to filter noise.
- Linear regression drop rate detection (bar/hour).
- 120-second confirmation window before alarm activation (avoids false triggers from brief transients).

| Setting | Default | Range | Description |
|---|---|---|---|
| `satminpressure` | 0.8 | 0-5 bar | Low pressure alarm threshold |
| `satmaxpressure` | 2.5 | 0-5 bar | High pressure alarm threshold |
| `satmaxpressuredrop` | 0.3 | 0-2 bar/hr | Maximum pressure drop rate before alarm |

**MQTT topics:**
- `sat/ch_pressure`: current raw OT MsgID 18 reading (bar).
- `sat/ch_pressure_status`: `ok`, `low`, or `high`.

---

### 5.13 Summer Suppression (Summer Simmer)

When enabled, SAT automatically disables heating when the outdoor temperature stays above a threshold for a configurable number of consecutive hours. This prevents the boiler from running unnecessarily during warm weather.

**How it works:**
1. SAT checks the outdoor temperature every 5 minutes.
2. When the outdoor temperature stays at or above the threshold, hours are accumulated.
3. When accumulated hours reach the minimum, summer mode activates and heating is suppressed.
4. Summer mode deactivates when the outdoor temperature drops below `threshold - 2 C` and the accumulated hours decay to zero.

| Setting | Default | Range | Description |
|---|---|---|---|
| `satsummersimmer` | false | bool | Enable summer auto-disable |
| `satsummerthreshold` | 18.0 | 5-35 C | Outdoor temperature threshold |
| `satsummerminhours` | 6 | 1-48 h | Consecutive hours above threshold to trigger |

**Summer Simmer Index:** SAT also calculates a thermal comfort index based on the Summer Simmer formula (using indoor temperature and humidity). This index is optionally used as PID input instead of raw room temperature when `satthermalcomfort` is enabled, providing humidity-aware temperature control.

---

### 5.14 Multi-Area Temperature

SAT supports up to 4 temperature input areas with configurable weights. The effective room temperature used by the PID controller is the weighted average of all valid, non-stale area temperatures.

Areas can be fed from three independent sources:

1. **MQTT push** to `sat/area/<0..3>`.
2. **DS18B20 sensor mapping** (TASK-587). A Dallas sensor address (16 hex chars) can be bound to an area in the Settings UI; `pollSensors()` then routes that sensor's reading to `satSetAreaTemp()` automatically. REST endpoints: `GET /api/v2/sat/sensor-areas`, `PATCH /api/v2/sat/sensor-areas`. Persisted as `settings.sat.sSensorArea[0..3]`.
3. **REST API** `POST /api/v2/sat/area/<n>`.

Example MQTT push:
```bash
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/area/0" -m "21.5"
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/area/1" -m "20.8"
```

| Setting | Default | Range | Description |
|---|---|---|---|
| `satmultiarea` | false | bool | Enable multi-area weighted temperature |
| `satmultiareacount` | 0 | 0-4 | Number of configured areas |
| `satareaweight0..3` | 1.0 | 0-10 | Weight per area |

Each area value expires after 5 minutes without an update. Stale areas are excluded from the weighted average. If all areas become stale, SAT falls back to the primary room temperature source.

---

### 5.15 Multi-Zone PID Control

For homes with multiple independently controlled heating zones (e.g., separate radiator circuits or zone valves), SAT supports up to 4 PID zones. Each zone runs its own PID calculation. The zones are aggregated into a single boiler setpoint as follows (alpha.29 / alpha.30):

- **P75 selection**: the per-zone PID outputs are sorted and the 75th-percentile value is picked (ceiling rank). For 4 zones this filters out a single highest-demand outlier instead of letting it dominate.
- **Headroom**: a fixed offset (`satzoneheadroom`, default 5.0 C) is added to the P75 result, giving the boiler enough margin to satisfy the upper-demand zones.
- **Overshoot cap**: if any zone is already above its target, the aggregate setpoint is reduced to prevent over-heating compliant zones.
- Each zone PID uses a symmetric integral clamp `[-curveValue, +curveValue]` so a zone that is slightly above its target can pull its share of the setpoint below the heating curve.

Zones receive their temperature and target via MQTT. Zone 1 always maps to the primary SAT target temperature. Zones 2-4 receive independent targets and temperature inputs.

| Setting | Default | Range | Description |
|---|---|---|---|
| `satzonecount` | 1 | 1-4 | Number of active heating zones |
| `satzonetimeout` | 300 | 60-3600 s | Seconds without update before a zone goes inactive |
| `satzoneheadroom` | 5.0 | 0-15 C | Headroom added to the P75 zone-aggregate setpoint |

When `satzonecount` is 1 (default), SAT operates in single-zone mode and this feature has no overhead.

---

### 5.16 BLE Temperature Sensor (ESP32 Only)

On ESP32 builds (OTGW32 / Thermo-Nova), SAT scans for BLE temperature sensors and uses one as the room temperature input. The BLE stack is **NimBLE-Arduino** (ADR-092), which replaced Bluedroid in 2.0.0 and frees roughly 400 KB of flash. Supported advertising formats:

- **ATC/pvvx custom firmware** (Xiaomi LYWSD03MMC with custom firmware): service data UUID 0x181A. Reports temperature, humidity, and battery level.
- **BTHome v2**: service data UUID 0xFCD2. Standard BTHome protocol for temperature and humidity sensors. Encrypted advertisements are rejected.

The radio runs a **continuous scan** from boot (TASK-494). Every format-pass advertisement is folded into a persistent 8-slot roster, regardless of whether it is currently selected. The `satbleinterval` setting controls the publish / state-update cadence; it does **not** throttle the radio scan itself.

#### Self-discovering roster (TASK-508)

Open the SAT settings panel in the web UI to see the discovered roster: each entry shows the last temperature, RSSI, and age. Give entries friendly labels ("Living room", "Bedroom"); the active sensor is selected from the roster. Auto-select promotes the single fresh sensor when only one is in range. Labels propagate to Home Assistant via the retained discovery configs (per-MAC entities are created automatically). "Forget" wipes a slot and clears its HA discovery topics with zero-byte retained payloads.

The roster is exposed at:

- `GET /api/v2/sat/ble/discovery` - JSON dump of all roster slots
- `POST /api/v2/sat/ble/select` `{"mac":"AA:BB:.."}` - promote MAC to active sensor
- `POST /api/v2/sat/ble/label`  `{"mac":"AA:BB:..","label":"Living room"}` - rename a slot
- `POST /api/v2/sat/ble/forget` `{"mac":"AA:BB:.."}` - drop slot and clean up HA

| Setting | Default | Range | Description |
|---|---|---|---|
| `satbleenable` | false | bool | Enable BLE temperature sensor scanning |
| `satblemac` | "" | MAC address | Active sensor MAC (set via roster select; empty = auto-select first fresh entry) |
| `satbleinterval` | 30 | 10-300 s | Publish / state-update cadence (NOT scan rate; scan is continuous) |

#### MQTT topic shape

Per-sensor state is published under the `sat/ble/<mac>/...` subtree (compact MAC, no colons):

| Topic suffix | Payload |
|---|---|
| `sat/ble/<mac>/temp` | Temperature (C) |
| `sat/ble/<mac>/rh` | Relative humidity (%) |
| `sat/ble/<mac>/bat` | Battery level (%) |
| `sat/ble/<mac>/rssi` | Signal strength (dBm) |

The legacy `sat/ble_temp` / `sat/ble_humidity` / `sat/ble_sensor_rssi` / `sat/ble_battery` / `sat/ble_sensor_count` / `sat/ble_temp_valid` topics carry the currently-selected sensor's values for backward compatibility.

BLE temperature values expire after 5 minutes without a new advertisement. SAT falls back to MQTT or OT bus values when the selected sensor goes stale.

---

### 5.17 Thermal Comfort (Humidity Adjustment)

SAT can adjust the effective target temperature based on indoor humidity. At high humidity, air feels warmer, so the target can be slightly reduced. At low humidity, the target is slightly increased.

| Setting | Default | Range | Description |
|---|---|---|---|
| `satcomfortadjust` | false | bool | Enable humidity-based target adjustment |
| `satcomforthumidity` | 50.0 | 20-80 % | Reference humidity (no adjustment at this level) |
| `satcomfortmaxoffset` | 1.0 | 0-3 C | Maximum target temperature adjustment |

Humidity data comes from: BLE sensor humidity, MQTT push, or weather API fallback. Values expire after 30 minutes (`iHumidityTimeoutS`).

---

### 5.18 Window Detection

SAT can detect open windows via MQTT input and reduce heating to the Activity preset temperature while the window is open.

```bash
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/window" -m "open"
```

| Setting | Default | Range | Description |
|---|---|---|---|
| `satwindowdetection` | false | bool | Enable window detection |
| `satwindowminopentime` | 60 | 10-600 s | Minimum seconds window must stay open before action |

---

### 5.19 DHW Control

SAT can control domestic hot water setpoint in standalone or fallback mode.

| Setting | Default | Range | Description |
|---|---|---|---|
| `satdhwenabled` | false | bool | Enable DHW control |
| `satdhwsetpoint` | 0.0 | 30-60 C | DHW setpoint (0 = inactive) |

---

### 5.20 Power and Energy Tracking

SAT estimates boiler power output and tracks cumulative energy consumption.

| Setting | Default | Range | Description |
|---|---|---|---|
| `satboilercapacity` | 24.0 | 1-100 kW | Boiler thermal capacity for power calculation |
| `satboilerratedkw` | 0.0 | 0-100 kW | Rated boiler input power (0 = disabled) |
| `satboilerefficiency` | 0.92 | 0-1.0 | Boiler efficiency |

Published to MQTT as `sat/power` (W) and `sat/energy_total` (kWh).

---

### 5.21 Home Assistant Integration

When MQTT is enabled, SAT entities are automatically discovered by Home Assistant.

**Climate entity**: The firmware publishes a thermostat climate entity via HA auto-discovery (`climate.<hostname>_thermostat`). It shows current room temperature, target temperature, and mode. HA-visible modes are `off` and `heat`; the `off` / `heat` mapping is driven by the `thermostat_connected` state of the PIC, so switching heat off in HA releases SAT control. Target temperature bounds follow the HA discovery config: min 12 C, max 28 C, step 0.5 C, precision 0.1 C. Setting the target in HA sends a `TT=` command, which SAT persists to ESP flash. Presets (comfort, eco, away, sleep, home, activity) are pushed through `sat/preset` when configured.

The climate entity also publishes `sat/climate_attributes` (a JSON blob) as the HA `json_attributes_topic` (TASK-594). HA surfaces the SAT operational state as climate-card attributes (mode, PID output, error, curve recommendation, etc.) without requiring template sensors.

A second climate entity (`climate.<hostname>_dhw_control`) exposes DHW setpoint control when the boiler supports it, with modes `off` / `auto` and range 40-60 C.

**TT= and TC= remote-override semantics (TASK-466)**: When `satpushsetpoint` is enabled, SAT pushes its target to the thermostat display using PIC-parity remote-override semantics on OpenTherm MsgID 16 + MsgID 100 (`RemoteOverrideFunction`). The two commands carry different priorities:

- `TT=<v>` -  program-priority override; auto-clears when the thermostat sends three consecutive MsgID 16 frames within 0.25 C of the override, then the next frame differs by more than 0.5 C (typically a scheduled program change).
- `TC=<v>` -  manual-priority override; persists indefinitely until cleared by `TC=0` or replacement.

Inbound thermostat MsgID 16 frames are observed by the OTDirect state machine; the auto-clear honoredCount is driven from that stream. The current override mode is reflected in the `PR=O` report as `O=T<v>` (TT), `O=C<v>` (TC), or `O=N` (none). Multi-channel set commands (MQTT, REST, web UI, serial) are coalesced by MsgID inside the OTDirect command queue (TASK-494): repeated WRITE_DATA for the same MsgID within one OT cycle collapses to a latest-value-wins single frame on the bus.

**SAT settings entities (Task-81 / Task-284)**: Since 2.0.0, all controllable SAT parameters are exposed as HA entities so the full SAT feature set can be driven from a dashboard without sending raw MQTT messages.

| HA component | Count | Examples |
|---|---|---|
| `number` | ~25 | heating curve coefficient, deadband, overshoot margin, control interval, max modulation, flame-off offset, flow offset, modulation-suppression delay/offset, boiler capacity, comfort humidity, summer threshold, auto-tune rate, preset temperatures (comfort/eco/away/sleep/activity/home), min/max pressure, max pressure drop, target temp step |
| `switch` | 13 | `solar_gain`, `summer_simmer`, `comfort_adjust`, `multi_area`, `auto_tune`, `simulation`, `window_detection`, `force_pwm`, `push_setpoint`, `ovp_enabled`, `preset_sync`, `dhw_enabled`, `pwm_auto_switch` |
| `select` | 1 | `sat_heating_system` with options `0` (auto), `1` (radiators), `2` (heat pump), `3` (underfloor) |

Each entity writes to its matching `%mqtt_sub_topic%/sat/<parameter>` command topic and reads from the corresponding state topic. All changes go through `updateSetting()` so they are persisted to flash and clamped to safe bounds.

**Key sensor entities**:

| Entity | Topic suffix | Description |
|---|---|---|
| `sensor.otgw_sat_setpoint` | `sat/setpoint` | Final flow temperature sent to boiler (C) |
| `sensor.otgw_sat_heating_curve` | `sat/heating_curve` | Heating curve base value (C) |
| `sensor.otgw_sat_pid_output` | `sat/pid_output` | PID corrected output (C) |
| `sensor.otgw_sat_error` | `sat/error` | PID error: target minus room temperature (C) |
| `sensor.otgw_sat_mode` | `sat/mode` | Control mode: `off`, `continuous`, or `pwm` |
| `sensor.otgw_sat_room_temp` | `sat/room_temp` | Room temperature used by PID (C) |
| `sensor.otgw_sat_outside_temp` | `sat/outside_temp` | Outdoor temperature used by heating curve (C) |
| `sensor.otgw_sat_boiler_status` | `sat/boiler_status` | Current boiler status (text label) |
| `sensor.otgw_sat_pwm_duty` | `sat/pwm_duty` | PWM duty cycle (0-1) |
| `sensor.otgw_sat_power` | `sat/power` | Estimated boiler power output (W) |
| `sensor.otgw_sat_energy_total` | `sat/energy_total` | Accumulated energy estimate (kWh) |
| `sensor.otgw_sat_ch_pressure` | `sat/ch_pressure` | CH water pressure (bar) |
| `sensor.otgw_sat_ch_pressure_status` | `sat/ch_pressure_status` | Pressure status: ok, low, or high |
| `sensor.otgw_sat_curve_recommendation` | `sat/curve_recommendation` | Heating curve recommendation: INCREASE, DECREASE, or HOLD |
| `binary_sensor.otgw_sat_safety_tripped` | `sat/safety_tripped` | Whether a safety layer has tripped |
| `binary_sensor.otgw_sat_modulation_reliable` | `sat/modulation_reliable` | Whether boiler modulation feedback is reliable |
| `binary_sensor.otgw_sat_setpoint_mismatch` | `sat/setpoint_mismatch` | Setpoint mismatch between SAT and boiler |
| `binary_sensor.otgw_sat_thermal_model_valid` | `sat/thermal_model_valid` | Whether the thermal model has enough data |
| `binary_sensor.otgw_sat_solar_gain` | `sat/solar_gain` | Solar gain compensation active |
| `binary_sensor.otgw_sat_auto_tune_active` | `sat/auto_tune_active` | Auto-tune in progress |
| `binary_sensor.otgw_sat_summer_active` | `sat/summer_active` | Summer suppression active |

**Using an external sensor from Home Assistant**:

```yaml
automation:
  - alias: "Push room temperature to SAT"
    trigger:
      - platform: state
        entity_id: sensor.living_room_temperature
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw-AABBCCDDEEFF/sat/indoor_temp"
          payload: "{{ states('sensor.living_room_temperature') }}"
```

**Using an external outdoor temperature source**:

```yaml
automation:
  - alias: "Push outdoor temperature to SAT"
    trigger:
      - platform: state
        entity_id: sensor.outdoor_temperature
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw-AABBCCDDEEFF/sat/outdoor_temp"
          payload: "{{ states('sensor.outdoor_temperature') }}"
```

External temperature values expire automatically (5 minutes for indoor, 10 minutes for outdoor). If the push stops, SAT falls back to OpenTherm bus values without alarming.

---

### 5.22 Known Limitations

- Not all thermostats report room temperature on OT MsgID 24. If yours does not, use an external sensor pushed via MQTT, REST API, BLE (ESP32), or a DS18B20 sensor mapped to an area.
- Outdoor temperature (MsgID 27) is rarely exchanged on the bus. An external push, weather API fetch, or REST API push is typically needed.
- BLE temperature sensor scanning requires an ESP32 build. The roster holds up to 8 sensors; only one is selected as the active SAT input at a time.
- SAT controls the flow temperature setpoint, but the wall thermostat still controls whether heating is enabled or disabled.
- Multi-zone support runs independent PID loops per zone, but SAT controls a single boiler. Individual boiler circuits per zone are not supported.
- The PIC co-processor holds the last `CS=` setpoint if the ESP fails. Layer 1 (boot safety) corrects this at the next startup.
- Weather data is fetched via plain HTTP only. No HTTPS is used.

---

### 5.23 Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Room never reaches target temperature | Coefficient too low | Increase by 0.2-0.3. Check `sat/curve_recommendation`. |
| Room consistently overshoots target | Coefficient too high | Decrease by 0.2-0.3. Check `sat/curve_recommendation`. |
| Frequent short boiler cycles | Boiler minimum modulation too high | Enable PWM auto-switch, or force PWM mode. Reduce `satcyclesperhour`. |
| Safety tripped on startup | PIC not ready at SAT init | Normal behavior. Re-enable SAT. |
| "External temp stale" in logs | MQTT push stopped | Check HA automation and broker connectivity. |
| SAT sends no setpoints | PIC not reachable | Check serial connection. Check `otgw-pic/picavailable` via MQTT. |
| Temperature oscillates strongly | Deadband too narrow or poor boiler modulation | Widen deadband, or switch to PWM mode. |
| Setpoint always at minimum (10 C) | SAT active but safety tripped | Check `sat/safety_tripped`. Re-enable SAT via web UI or MQTT. |
| SAT tab not visible in web interface | SAT not yet enabled | Enable SAT in settings. |
| Home Assistant climate entity missing | Auto-discovery disabled or MQTT not connected | Check MQTT connection and HA discovery setting. |
| Pressure alarm keeps triggering | Pressure band too narrow or boiler filling issue | Adjust `satminpressure`/`satmaxpressure`. Check boiler pressure gauge. |
| Solar gain never activates | Rise rate threshold too high or sun elevation data missing | Lower `satsolarminrise`. Enable weather API for sun elevation. |
| Summer mode does not activate | Threshold too high or hours too long | Lower `satsummerthreshold` or `satsummerminhours`. |

---

### 5.24 Debugging and Calibration

**Telnet debug trace.** SAT has its own conditional debug channel. Connect to the OTGW on TCP port 23 and press `5` to toggle SAT control / cycle / HCR tracing (default on in 2.0.0), `6` for OTDirect tracing (default on), and `7` for the SAT BLE scan debug stream (per-advertisement summary). The telnet `w` shortcut forces an immediate Open-Meteo fetch + dump. The complete set of toggles is shown in the telnet welcome banner.

**OPV (Overshoot Protection Value) calibration.** SAT can measure the minimum stable boiler setpoint (the point at which the flame stays lit without short-cycling) and use it as a lower clamp. Trigger a calibration run with:

```bash
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/ovp_start" -m ""
```

The calibration collects at least 40 samples before accepting a result. Stop it early (and discard) with `sat/ovp_stop`. The measured value is stored in `SATovpvalue` and is only applied when `SATovpenabled` is true.

**Flushing short-lived state.** `sat/flush` clears transient runtime state (cycle window, HCR samples, recent pressure history) without erasing persistent settings. Useful after large changes to the heating system or sensors.

**Persisted files on LittleFS.** SAT stores long-running state under `/sat/`: `sat_hcr.json` (heating curve recommendation samples), plus cycle/window records. These survive reboots.
