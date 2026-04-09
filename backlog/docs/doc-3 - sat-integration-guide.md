---
id: doc-3
title: sat-integration-guide
type: other
created_date: '2026-04-09 05:23'
---
# SAT Integration Guide

SAT (Smart Autotune Thermostat) is an embedded heating controller built into OTGW-firmware v1.4.0 and later. It turns your OpenTherm Gateway into a standalone smart thermostat — no external controller, cloud service, or Home Assistant automation required.

---

## What SAT Does

SAT replaces or augments your thermostat's flow temperature control by:

- Computing a **weather-compensated heating curve** (stooklijn) from the outdoor temperature.
- Running a **PID v3 controller** on top of the heating curve to correct for indoor temperature error.
- Switching between **continuous modulation** (boiler runs at a modulated level) and **PWM flame cycling** (boiler runs at full or minimum modulation, duty-cycled by SAT) based on the boiler's actual behavior.
- Automatically tuning the heating curve recommendation based on observed temperature errors.
- Monitoring flame health, pressure, cycle quality, and energy usage.

SAT does not require your thermostat to disappear — it co-exists with it, intercepting and overriding the CS= (control setpoint) command on the OpenTherm bus.

---

## Hardware Requirements

SAT works with any OTGW-firmware-supported hardware:

| Hardware | Support |
|----------|---------|
| ESP8266 + NodoShop PIC | Full support |
| OTGW32 (ESP32 + OT-direct) | Full support |
| ESP32 with BLE | Full support + BLE temperature sensor |

SAT requires the OTGW to be in **gateway mode** (GW=1), not bypass or monitor mode. The PIC or OT-direct interface must be able to inject CS= and MM= commands onto the OpenTherm bus.

---

## What SAT Needs to Work

SAT needs at least:

1. **Outdoor temperature** — either from the OT bus (if your thermostat publishes it), from an MQTT push (`sat/outdoor_temp`), or from the built-in Open-Meteo weather API.
2. **Room temperature** — from the OT bus (Tr, MsgID 24), from an MQTT push (`sat/indoor_temp`), from a BLE sensor (ESP32), or from a multi-area average.
3. **Boiler communication** — SAT reads flow temperature (MsgID 25), flame status (MsgID 0), and modulation level (MsgID 17) from the OT bus.

---

## Step 1: Enable MQTT

SAT is controlled via MQTT. Configure MQTT in the OTGW WebUI:

1. Go to **Settings** in the WebUI.
2. Enter your MQTT broker address, port (default 1883), and optionally username/password.
3. Set the **Top Topic** (default: `OTGW`) and **Unique ID** (default: `otgw-{MAC}`).
4. Save settings. The OTGW will connect to your broker.

Verify connectivity by checking `OTGW/value/otgw-AABBCCDDEEFF/otgw-firmware/version` — it should publish the firmware version.

---

## Step 2: Enable SAT

### Via MQTT

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/enabled" -m "true"
```

### Via WebUI

1. Open the **SAT** tab in the WebUI.
2. Click **Enable SAT**.

SAT starts in `continuous` mode with default settings. The status badge changes from "Disabled" to "Active".

---

## Step 3: Configure Key Settings

These are the settings you should configure before leaving SAT on:

### Heating System Type

Tell SAT what kind of heating system you have:

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/heating_system" -m "1"
# 0=auto-detect, 1=radiators, 2=heat pump, 3=underfloor
```

### Heating Curve Coefficient

The heating curve (stooklijn) slope. Start with the default (1.5) and adjust based on the curve recommendation SAT publishes (`sat/curve_recommendation`):

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/heating_curve" -m "1.5"
```

### Target Temperature

The desired room temperature:

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/target" -m "20.5"
```

### Outdoor Temperature Source

If your thermostat does not send outdoor temp via OT bus, enable the weather API:

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/heating_system" -m "0"
# Then enable weather in WebUI (SAT tab, weather section, click "Detect Location")
```

Or push outdoor temp from Home Assistant:

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/outdoor_temp" -m "8.5"
```

### Boiler Manufacturer (Optional but Recommended)

Set this if your boiler is known to have quirks:

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/manufacturer" -m "remeha"
# Supported: atag, baxi, brotge, dedietrich, ferroli, geminox, ideal, immergas,
#            intergas, itho, nefit, radiant, remeha, sime, vaillant, viessmann,
#            worcester, other, auto
```

---

## Step 4: Home Assistant Auto-Discovery

SAT publishes HA MQTT auto-discovery configs automatically when SAT is enabled. No manual configuration is needed.

### Entities Created

| Entity Type | Entity Name | Description |
|-------------|-------------|-------------|
| `climate` | `{hostname}_SAT_Climate` | Climate entity — controls target temp, shows mode |
| `sensor` | `{hostname}_SAT_Setpoint` | Flow temperature setpoint (°C) |
| `sensor` | `{hostname}_SAT_Heating_Curve` | Heating curve value (°C) |
| `sensor` | `{hostname}_SAT_PID_Output` | PID output (°C) |
| `sensor` | `{hostname}_SAT_Error` | PID error (°C) |
| `sensor` | `{hostname}_SAT_Control_Mode` | off / continuous / pwm |
| `sensor` | `{hostname}_SAT_Boiler_Status` | Boiler state (0-14) |
| `sensor` | `{hostname}_SAT_PWM_Duty` | PWM duty cycle (%) |
| `sensor` | `{hostname}_SAT_Room_Temp` | Room temperature (°C) |
| `sensor` | `{hostname}_SAT_Outside_Temp` | Outside temperature (°C) |
| `sensor` | `{hostname}_SAT_Humidity` | Indoor humidity (%) |
| `sensor` | `{hostname}_SAT_Comfort_Offset` | Humidity comfort offset (°C) |
| `sensor` | `{hostname}_SAT_Estimated_Room_Temp` | Estimated room temp during fallback (°C) |
| `sensor` | `{hostname}_SAT_Pressure` | Boiler system pressure (bar) |
| `sensor` | `{hostname}_SAT_Pressure_Drop_Rate` | Pressure drop rate (bar/h) |
| `sensor` | `{hostname}_SAT_Power` | Current boiler power (kW) |
| `sensor` | `{hostname}_SAT_BLE_Temperature` | BLE sensor temperature (ESP32 only) |
| `sensor` | `{hostname}_SAT_BLE_Humidity` | BLE sensor humidity (ESP32 only) |
| `sensor` | `{hostname}_SAT_BLE_RSSI` | BLE sensor signal (ESP32 only) |

The climate entity uses:
- `sat/target` for setpoint state and command
- `sat/mode` for HVAC mode state (`off` = off, anything else = heat)
- `Tr` (OT bus room temperature) for current temperature
- `sat/climate_attributes` for extra attributes

### Triggering Discovery Manually

If entities don't appear in HA, force a discovery republish:

```bash
curl -X POST http://otgw.local/api/v2/otgw/discovery
```

Or via MQTT command topic:

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/command" -m "F"
```

---

## Step 5: Push Outdoor Temperature from HA (Recommended)

Add an automation in HA to push the outdoor temperature:

```yaml
automation:
  - alias: "Push outdoor temp to OTGW SAT"
    trigger:
      - platform: state
        entity_id: sensor.outdoor_temperature
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw-AABBCCDDEEFF/sat/outdoor_temp"
          payload_template: "{{ states('sensor.outdoor_temperature') }}"
```

---

## Step 6: Push Room Temperature from HA (Optional)

If you have a more accurate room temperature source than what the thermostat sends via OT:

```yaml
automation:
  - alias: "Push room temp to OTGW SAT"
    trigger:
      - platform: state
        entity_id: sensor.living_room_temperature
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw-AABBCCDDEEFF/sat/indoor_temp"
          payload_template: "{{ states('sensor.living_room_temperature') }}"
```

The pushed temperature expires after `iSensorMaxAgeS` seconds (default: 6 hours). SAT falls back to the OT bus Tr value when the pushed value expires.

---

## Step 7: Configure Presets (Optional)

SAT supports 6 presets. You can configure the temperature for each:

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset_comfort" -m "21.0"
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset_eco" -m "18.0"
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset_away" -m "15.0"
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset_sleep" -m "16.0"
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset_home" -m "18.5"
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset_activity" -m "10.0"
```

Activate a preset:

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset" -m "away"
```

---

## Step 8: OPV Calibration (Recommended for Better Mode Switching)

After SAT has been running for a few days, run the OPV (Overshoot Protection Value) calibration. This teaches SAT the maximum flow temperature your boiler produces at minimum modulation, enabling proactive mode switching instead of reactive.

See [sat-opv-calibration.md](sat-opv-calibration.md) for the full procedure.

---

## Control Modes

SAT has two heating control modes plus an auto-switch feature:

| Mode | Description |
|------|-------------|
| `continuous` | Boiler runs continuously; SAT sends a CS= flow temperature setpoint adjusted by the PID controller |
| `pwm` | Boiler is duty-cycled by SAT: flame ON for a fraction of the cycle time, OFF for the rest; setpoint is sent at max or a high value during ON phase |
| auto-switch | With `sat/pwm_auto_switch = true`, SAT automatically switches between the two modes based on observed overshoot and underheat patterns |

---

## Monitoring

Key topics to watch:

| Topic | What to Look For |
|-------|-----------------|
| `sat/mode` | Should be `continuous` or `pwm`, not `off` when heating |
| `sat/boiler_status` | `at_setpoint` or `modulating_down` are normal |
| `sat/cycle_class` | Repeated `overshoot` or `short` suggests coefficient or OPV needs adjustment |
| `sat/curve_recommendation` | Follow this to tune the heating curve coefficient |
| `sat/safety_tripped` | `true` means SAT has shut down for safety; check telnet debug |
| `sat/pressure_health` | `OFF` means pressure alarm is active |
| `sat/flame_health` | `OFF` means flame anomaly detected |

---

## Known Limitations vs Python SAT

OTGW-firmware's SAT is a C++ port of the Python SAT custom component (thermo-nova branch). Some features differ:

- **No HA climate entity presets in WebUI**: The climate entity supports presets via MQTT but the HA UI preset dropdown requires additional HA configuration beyond what auto-discovery provides.
- **No multi-zone schedule**: SAT has no built-in scheduler. Use HA automations to activate presets on a schedule.
- **BLE sensor limited to one primary**: The firmware tracks a primary BLE sensor (best RSSI), not a weighted average of multiple sensors.
- **Auto-tune is simplified**: The Python SAT Relay auto-tune method is not implemented; OTGW-firmware uses an EMA-based cycle analysis approach.
- **No adaptive heating schedule**: Learning when you typically need heat is not implemented.

---

## Simulation Mode

For testing without a real boiler, enable simulation mode:

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/simulation" -m "true"
```

In simulation mode, SAT simulates a virtual room and boiler based on configurable heat/cool rates. The SAT dashboard shows a `SIMULATION` badge. All MQTT topics publish simulated values.

Disable simulation:

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/simulation" -m "false"
```

---

## REST API Quick Reference

| Endpoint | Method | Description |
|----------|--------|-------------|
| `GET /api/v2/sat/status` | GET | Full SAT runtime state |
| `GET /api/v2/sat/status?detail=full` | GET | Extended health diagnostics |
| `POST /api/v2/sat/target` | POST | Set target temperature |
| `POST /api/v2/sat/externaltemp` | POST | Push indoor temperature |
| `POST /api/v2/sat/externaloutdoor` | POST | Push outdoor temperature |
| `POST /api/v2/sat/enable` | POST | Enable/disable SAT |
| `POST /api/v2/sat/mode` | POST | Set control mode |
| `POST /api/v2/sat/preset` | POST | Activate preset |
| `POST /api/v2/sat/humidity` | POST | Push humidity |
| `POST /api/v2/sat/area/<0-3>` | POST | Push area temperature |
| `GET /api/v2/sat/weather` | GET | Weather integration status |
| `POST /api/v2/sat/settings/<name>` | POST | Update any SAT setting |
