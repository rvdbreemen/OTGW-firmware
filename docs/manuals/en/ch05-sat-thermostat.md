## Chapter 5: SAT - Smart Autotune Thermostat

### Overview

SAT (Smart Autotune Thermostat) transforms the OpenTherm Gateway from a transparent protocol bridge into an active, self-tuning heating controller. Instead of forwarding your wall thermostat's on/off requests to the boiler, SAT takes over: it reads the current room temperature, calculates the exact water temperature the boiler needs to produce, and injects that setpoint directly into the OpenTherm stream.

The result is weather-compensated heating that keeps room temperatures stable within a fraction of a degree, reduces gas consumption, and dramatically cuts the number of boiler ignition cycles. SAT is entirely self-contained: it runs on the ESP microcontroller with no dependency on Home Assistant, cloud services, or internet access.

---

### 5.1 What SAT Does

With SAT, every control loop cycle (default 30 seconds):

1. Reads room temperature (from the OT bus, or an external sensor pushed via MQTT).
2. Reads outdoor temperature (from the OT bus, an external MQTT push, or a weather API call).
3. Calculates the heating curve value: the base flow temperature needed given current outdoor conditions.
4. Applies a PID correction on top of the heating curve.
5. Clamps the result within safety limits.
6. Sends a `CS=` command to the PIC gateway to inject the setpoint into the OpenTherm stream.
7. Publishes all state to MQTT and the web UI dashboard.

---

### 5.2 Prerequisites and Compatibility

- An OpenTherm-compatible boiler.
- A room temperature source: OT bus (MsgID 24), external MQTT push, or BLE sensor (ESP32 only).
- An outdoor temperature source: OT bus (MsgID 27), external MQTT push, or weather API fetch.
- You do not need to remove your wall thermostat. SAT intercepts the setpoint; your thermostat's away mode and schedule still work.

---

### 5.3 Enabling SAT

#### Via the Web UI

1. Open Settings and find the SAT section.
2. Set "SAT Enabled" to on.
3. Choose your heating system type: radiator or underfloor.
4. Set the target room temperature and heating curve coefficient.
5. Save settings.

#### Via MQTT

```bash
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/enabled" -m "true"
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/target" -m "21.0"
```

---

### 5.4 Configuration Parameters

| Setting | Default | Range | Description |
|---|---|---|---|
| `satenabled` | false | bool | Master on/off switch |
| `satsystem` | 0 | 0-1 | 0 = radiator, 1 = underfloor |
| `sattargettemp` | 20.0 | 5-30 °C | Desired room temperature |
| `satcoefficient` | 1.5 | 0.1-5.0 | Heating curve steepness |
| `satdeadband` | 0.25 | 0.05-2.0 °C | PID deadband width |
| `satinterval` | 30 | 10-300 s | Control loop interval |
| `satexternaltemp` | false | bool | Use external indoor sensor via MQTT |
| `satpwmautoswitch` | true | bool | Automatically switch between continuous and PWM modes |

#### Choosing the heating curve coefficient

| Insulation level | Building type | Suggested coefficient |
|---|---|---|
| Excellent | Modern passive house, triple glazing | 0.5 - 0.8 |
| Good | Post-2000 build, cavity wall insulation | 0.8 - 1.2 |
| Average | 1970s-2000s, partial insulation | 1.2 - 1.8 |
| Poor | Pre-1970, single glazing, no cavity insulation | 1.8 - 2.5 |

Start in the middle of the appropriate range. If the room consistently fails to reach the target, increase the coefficient by 0.2-0.3. If the room overshoots, decrease it.

---

### 5.5 How the Heating Curve Works

```
setpoint = base_offset + (coefficient / 4) * curve_value

curve_value = 4 * (T_target - 20)
            + 0.03 * (T_outside - 20)^2
            - 0.4  * (T_outside - 20)
```

Where `base_offset` is 27.2°C for radiators and 20.0°C for underfloor.

Worked examples for a radiator system, coefficient 1.5:

| Outside temp | Target | Flow setpoint |
|---|---|---|
| +15°C | 20°C | ~28°C |
| +5°C | 20°C | ~32°C |
| -5°C | 20°C | ~38°C |
| -10°C | 21°C | ~43°C |

---

### 5.6 Control Modes: Continuous and PWM

**Continuous mode (default)**: SAT sends the PID output directly as a flow temperature setpoint. The boiler modulates to reach and hold it. Preferred for modern condensing boilers.

**PWM mode**: For boilers that cannot modulate below 30-40%, SAT cycles the flame on and off at a calculated duty cycle. Within each control interval, the boiler runs at full setpoint for `duty * interval` seconds, then idles.

**Auto-switch**: When `satpwmautoswitch` is enabled (default), SAT monitors boiler behavior and switches modes automatically.

---

### 5.7 Safety System

SAT implements six independent safety layers. Any single layer tripping sends `CS=0`, releasing the setpoint override and returning control to the wall thermostat immediately.

1. **Boot safety**: `CS=0` is sent before the first control cycle.
2. **Disable safety**: When SAT is turned off, `CS=0` is sent immediately.
3. **Stale sensor fallback**: External temps expire (5 min indoor, 10 min outdoor); SAT falls back to OT bus values.
4. **Hard temperature ceiling**: Underfloor capped at 50°C, radiators at 80°C. Minimum is 10°C.
5. **Consecutive sensor failure counter**: 10 invalid room temperature readings trips the safety.
6. **PIC communication check**: 5 consecutive failed `CS=` commands trips the safety.

---

### 5.8 Simulation Mode

Simulation mode lets you run the SAT control loop without sending real commands to the PIC. Useful for testing configuration without affecting the actual heating system.

```bash
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/simulation" -m "ON"
```

---

### 5.9 Home Assistant Integration

When MQTT is enabled, SAT entities are automatically discovered by Home Assistant.

**Climate entity**: The `sat_climate` entity shows current room temperature, target temperature, and mode. Available modes are `off`, `heat` (continuous), and `pwm`. Setting the target temperature persists the value to ESP flash storage.

**Key sensor entities**:

| Entity | Topic suffix | Description |
|---|---|---|
| `sensor.otgw_sat_setpoint` | `sat/setpoint` | Final flow temperature sent to boiler (°C) |
| `sensor.otgw_sat_heating_curve` | `sat/heating_curve` | Heating curve base value (°C) |
| `sensor.otgw_sat_pid_output` | `sat/pid_output` | PID corrected output (°C) |
| `sensor.otgw_sat_error` | `sat/error` | PID error: target minus room temperature (°C) |
| `sensor.otgw_sat_mode` | `sat/mode` | Control mode: `off`, `continuous`, or `pwm` |
| `sensor.otgw_sat_room_temp` | `sat/room_temp` | Room temperature used by PID (°C) |
| `sensor.otgw_sat_outside_temp` | `sat/outside_temp` | Outdoor temperature used by heating curve (°C) |
| `sensor.otgw_sat_boiler_status` | `sat/boiler_status` | Current boiler status (text label) |
| `sensor.otgw_sat_pwm_duty` | `sat/pwm_duty` | PWM duty cycle (0-1) |
| `sensor.otgw_sat_power` | `sat/power` | Estimated boiler power output (W) |
| `sensor.otgw_sat_energy_total` | `sat/energy_total` | Accumulated energy estimate (kWh) |
| `binary_sensor.otgw_sat_safety_tripped` | `sat/safety_tripped` | Whether a safety layer has tripped |
| `binary_sensor.otgw_sat_modulation_reliable` | `sat/modulation_reliable` | Whether boiler modulation feedback is reliable |
| `binary_sensor.otgw_sat_setpoint_mismatch` | `sat/setpoint_mismatch` | Setpoint mismatch between SAT and boiler |
| `binary_sensor.otgw_sat_thermal_model_valid` | `sat/thermal_model_valid` | Whether the thermal model has enough data |
| `binary_sensor.otgw_sat_solar_gain` | `sat/solar_gain` | Solar gain compensation active |
| `binary_sensor.otgw_sat_auto_tune_active` | `sat/auto_tune_active` | Auto-tune in progress |

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

### 5.10 Known Limitations

- Not all thermostats report room temperature on OT MsgID 24. If yours does not, use an external sensor pushed via MQTT.
- Outdoor temperature (MsgID 27) is rarely exchanged on the bus. An external push or weather API call is typically needed.
- BLE temperature sensor scanning requires an ESP32 build.
- SAT controls the flow temperature setpoint, but the wall thermostat still controls whether heating is enabled or disabled.

---

### 5.11 Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Room never reaches target temperature | Coefficient too low | Increase by 0.2-0.3 |
| Room consistently overshoots target | Coefficient too high | Decrease by 0.2-0.3 |
| Frequent short boiler cycles | Boiler minimum modulation too high | Enable PWM auto-switch |
| Safety tripped on startup | PIC not ready at SAT init | Normal behavior. Re-enable SAT. |
| "External temp stale" in logs | MQTT push stopped | Check HA automation and broker connectivity |
