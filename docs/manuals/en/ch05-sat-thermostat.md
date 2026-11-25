## Chapter 5: SAT - Smart Autotune Thermostat

### Overview

SAT (Smart Autotune Thermostat) transforms the OpenTherm Gateway from a transparent protocol bridge into an active, self-tuning heating controller. Instead of forwarding your wall thermostat's on/off requests to the boiler, SAT takes over: it reads the current room temperature, calculates the exact water temperature the boiler needs to produce, and injects that setpoint directly into the OpenTherm stream.

The result is weather-compensated heating that keeps room temperatures stable within a fraction of a degree, reduces gas consumption, and dramatically cuts the number of boiler ignition cycles. SAT is entirely self-contained: it runs on the ESP microcontroller with no dependency on Home Assistant, cloud services, or internet access.

SAT was inspired by (and ported from) the excellent Home Assistant SAT custom component by Alex Wijnholds (Alexwijn), with design feedback and validation from George Dellas (sergeantd). The firmware embeds the controller directly in the OTGW so the heating loop keeps running even when Home Assistant, MQTT, or WiFi are unavailable.


> **SAT is opt-in and off by default.** SAT takes over your boiler's control setpoint, so it stays off (`satenabled` is false) until you switch it on, and the setup wizard asks you to confirm first.
>
> **Reporting SAT field results.** Field data is what makes SAT better. If you run SAT, please send a capture: (1) check the version at `http://<ip>/api/v2/device/info` (`fwversion`); (2) check that `http://<ip>/api/v2/sat/status` shows `"simulation": false`; (3) start the capture tool from the release zip (`capture-mqtt-debug.bat`, `.sh` or `-macos.sh`) with `--long-run`, for at least a night and a morning; (4) note your boiler model, radiators or floor heating, and any setting you changed with the time; (5) stop with `q` and attach the `-upload.zip` to a GitHub issue, or post it in #english-support or #nederlandse-ondersteuning on Discord (beta testers: #beta-testing).

---

### 5.1 What SAT Does

With SAT, every control loop cycle (default 30 seconds):

1. Reads room temperature (from the OT bus, an external sensor pushed via MQTT, a BLE sensor on ESP32, or a weighted multi-area average).
2. Reads outdoor temperature (from the OT bus, an external MQTT push, or the built-in Open-Meteo weather API).
3. Calculates the heating curve value: the base flow temperature needed given current outdoor conditions.
4. Applies a PID v3 correction on top of the heating curve (gains computed from the heating curve).
5. Applies optional adjustments: multi-zone demand (zones can raise the setpoint or cap it), solar gain compensation.
6. Clamps the result within safety limits.
7. Sends a `CS=` command to the PIC gateway to inject the setpoint into the OpenTherm stream.
8. Publishes all state to MQTT and the web UI dashboard.

---

### 5.2 Prerequisites and Compatibility

- An OpenTherm-compatible boiler. SAT includes a manufacturer table with quirk flags for Atag, Baxi, Ferroli, Geminox, Ideal, Immergas, Intergas, Itho, Nefit, Radiant, Remeha, Sime, Vaillant, Viessmann, Worcester, and others. Auto-detection is the default.
- A room temperature source. SAT uses the first one that has a usable value, in this order: a BLE sensor (stale after 5 minutes without an update), the multi-area weighted average (when multi-area is on and at least one area is valid), an external push over MQTT (`sat/indoor_temp`) or the REST API (only with `satexternaltemp` on; stale after `satsensormaxage`, default 21600 s = 6 hours), and last the thermostat's room temperature on the OT bus (MsgID 24; stale after 5 minutes without a refresh).
- An outdoor temperature source: OT bus (MsgID 27), external MQTT push, or the built-in Open-Meteo weather API fetch.
- You do not need to remove your wall thermostat. SAT intercepts the setpoint; your thermostat's away mode and schedule still work.
- The OTGW must be in gateway mode (GW=1), not bypass or monitor mode.

---

### 5.3 Enabling SAT

#### Via the Web UI

The v2 web UI presents SAT as a single page with three depth levels, so you see only as much as you need: a simple thermostat view (dial, presets, current status), a control view (control mode, boiler status, coefficient, deadband, PID output), and a technical view (health, raw PID terms, PWM/cycle detail, simulation, raw JSON). All three share the same UI shell described in chapter 3.

1. Open the web interface (`http://otgw2.local/`).
2. Go to the SAT page.
3. Set "SAT Enabled" to on.
4. Choose your heating system (auto, radiators, or underfloor) and your heating source (auto, gas boiler, heat pump, or hybrid). System and source are separate settings (see below).
5. Set the target room temperature and heating curve coefficient.
6. Save settings.

#### Via MQTT

```bash
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/enabled" -m "true"
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/target" -m "21.0"
```

Every configurable SAT parameter has a matching MQTT command topic of the form `%mqtt_sub_topic%/sat/<name>`. The full set is declared in `MQTTstuff.ino` (`handleMQTTcallback`) and covers: `enabled`, `target`, `control_mode`, `heating_system`, `manufacturer`, `max_modulation`, `interval`, `preset`, `heating_curve`, `deadband`, `overshoot_margin`, `flow_offset`, `flame_off_offset`, `mod_sup_delay`, `mod_sup_offset`, `cycles_per_hour`, `valve_offset`, `boiler_capacity`, `target_temp_step`, `sensor_max_age`, `error_monitoring`, `heating_mode`, `overshoot_protection`, `push_setpoint`, `follow_thermostat`, `fireplace_mode`, `external_temp`, `reset_integral`, `flush`, `flush_threshold_h`, `simulation`, `solar_gain`, `solar_freeze_integral`, `solar_min_elevation`, `sun_elevation`, `summer_simmer`, `summer_threshold`, `summer_min_hours`, `thermal_comfort`, `humidity`, `humidity_timeout_s`, `window`, `window_detection`, `multi_area`, `multi_area_count`, `area/<0..3>`, `zone_count`, `zone_timeout_s`, `zone/<n>/room_temp`, `zone/<n>/setpoint`, `valves_open`, `preset_sync`, `preset_sync_topic`, `preset_<comfort|eco|away|sleep|activity|home>`, `min_pressure`, `max_pressure`, `max_pressure_drop`, `dhw_enabled`, `dhw_setpoint`, `indoor_temp`, `outdoor_temp`, `ble_enable`, `ble_mac`, `ble_interval`.

Sending `ON` / `OFF` / `true` / `false` / `1` / `0` works for boolean topics. Numeric topics accept the usual decimal representation.

#### Via REST API

```bash
curl -X POST -d '{"name":"satenabled","value":"true"}' \
  http://otgw2.local/api/v2/settings
```

---

### 5.4 Configuration Parameters

#### Core settings

| Setting | Default | Range | Description |
|---|---|---|---|
| `satenabled` | false | bool | Master on/off switch |
| `satsystem` | 0 | 0-2 | Heating SYSTEM (heat distribution): 0 = auto (defaults to radiators), 1 = radiators, 2 = underfloor |
| `satsource` | 0 | 0-3 | Heating SOURCE (energy device): 0 = auto, 1 = gas boiler, 2 = heat pump, 3 = hybrid |
| `sattargettemp` | 20.0 | 5-30 C | Desired room temperature |
| `satcoefficient` | 1.5 | 0.1-5.0 | Heating curve steepness |
| `satinterval` | 30 | 10-300 s | Control loop interval: how often SAT computes and sends the setpoint. The PID state does not follow it: it advances on its own fixed 60 s timer (see PID tuning) |
| `satexternaltemp` | false | bool | Use external indoor sensor via MQTT |
| `satovershootprotection` | true | bool | PWM start-up and modulation suppression (see 5.6). Applies to every heating source |
| `satmaxrelmod` | 100 | 0-100 % | Maximum relative modulation sent to boiler (MM= command) |
| `satmanufacturer` | 0 | 0-18 | Boiler manufacturer (0 = auto-detect) |

#### Heating source versus heating system

SAT keeps two orthogonal choices apart:

- The heating **system** (`satsystem`) is how heat is distributed: radiators or underfloor. It drives the base offset of the heating curve, the hard temperature ceiling (underfloor is capped lower), and the per-system cold cutoff.
- The heating **source** (`satsource`) is the energy device: gas boiler, heat pump, or hybrid. It drives timing (minimum on-time, cycles) and, for heat pumps, forcing maximum relative modulation.

Source selection is manual and authoritative: whatever you set is what SAT controls against. SAT can also derive a source hint from the OpenTherm bus (a cooling-capable boiler is likely a heat pump), but that auto-detect is only a hint for display and never overrides your manual choice.

A per-system **cold cutoff** (COLD_SETPOINT: 28.2 C for radiators, 21.0 C for underfloor) means that when genuinely low demand drives the requested flow setpoint below that threshold, SAT commands the boiler fully off rather than firing for a trickle of heat. This avoids needless short cycles on mild days.

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

The integral term uses a **symmetric clamp** `[-curveValue, +curveValue]` and no other absolute cap (the earlier `+/-20 C` cap was removed, Sergeant D entry 75). Negative accumulation is required in mild weather so the controller can nudge the flow setpoint below the heating curve when the room is slightly above target. The PID integral and raw derivative state are persisted on every save and **warm-started** on the next boot (alpha.32), so a power cycle does not reset to a cold start. `sat/flush` or an explicit `satdisable` clears the persisted state.

**PID timing (TASK-1368).** The integral and derivative advance once every 60 s on their own timer, as in thermo-nova (Sergeant D entries 42 and 111). This is independent of `satinterval`: each control tick recomputes the output from the current heating curve, but only the 60 s step moves the PID state. Before TASK-1368 every 30 s tick stepped the PID, so the integral grew twice as fast. The derivative is 0 on the first output after a reset or restore. Zone PIDs step on the same timer. After a boot the restored state is held unchanged until the first valid room temperature arrives; that reading takes a step at once (entry 115). The first tick after enabling SAT sends a computed setpoint, not the SAT minimum `CS=10`.

**PID gains.** SAT recomputes the gains from the heating curve value: Kp = coefficient x curve value / 3 for radiators (/ 4 for underfloor heating), Ki = Kp / 8400, Kd = 0.05 x 8400 x Kp = 420 x Kp, each rounded to 3 decimals. The Kd factor is 0.05 from beta.386 on; it was 0.063 before (Sergeant D entry 425, which supersedes the factor of entry 279; entry 427 puts it in 2.0.0). Thermo-nova uses 0.07 (`custom_components/sat/pid.py:93`), so the lower factor is a deliberate choice of Sergeant D.

**Integral outside the deadband (TASK-1890).** The integral only accumulates while the room is within 0.1 C of the target. When the error leaves that band, the integral is halved once and then held while the error stays outside; back inside, it builds up again from the held value (entries 333 and 414; ADR-259, Proposed). This applies to the main PID and to every zone PID. A change of the target still resets the integral to 0. Thermo-nova sets the integral to 0 as soon as the error leaves the band (`custom_components/sat/pid.py:185-189`); the halving is a deliberate deviation.

#### Advanced control

| Setting | Default | Range | Description |
|---|---|---|---|
| `satmaxsetpoint` | 65.0 | 30-80 C | Global safety ceiling for flow setpoint |
| `satflameoffoffset` | 18.0 | 0-30 C | PWM ON phase only: while waiting for the flame, the setpoint is return temperature + this offset. Not applied in continuous mode or in the PWM OFF phase |
| `satovershootmargin` | 2.0 | 0.5-5 C | Overshoot margin: continuous-to-PWM auto-switch (flow above the requested PID setpoint + margin for 60 s) and cycle classification. Units that stored another value keep it |
| `satmodsupdelay` | 20.0 | 0-60 s | Modulation suppression delay |
| `satmodsupoffset` | 1.0 | 0-5 C | Modulation suppression offset below setpoint |
| `satflowoffset` | 2.0 | 0-10 C | Continuous mode: max setpoint drop from boiler temp |
| `satcyclesperhour` | 3 | 3-4 | PWM cycle length for gas boilers and Auto, stored as cycles per hour: 3 = 20 min, 4 = 15 min. The Web UI shows the field "PWM cycle length" in minutes for every appliance type; heat pumps and hybrids store it in `sathpcycle` (seconds: 1800 = 30 min, 2400 = 40 min, 3600 = 60 min for a heat pump only) |
| `satpushsetpoint` | false | bool | Push SAT target to thermostat display (TC= command). SAT pushes the room setpoint to the thermostat when the target changes (as thermo-nova); a thermostat that rejects the remote override keeps its own setpoint, and SAT does not keep re-pushing |
| `satfollowthermostat` | on when `satpushsetpoint` is on, else off | bool | A setpoint change on the thermostat becomes the SAT target (see 5.21). A unit with no stored value gets the `satpushsetpoint` value once; a stored value always wins |
| `satfireplace` | off | bool | Fireplace mode: SAT heats on the heating curve only, as with no room temperature (see 5.21). MQTT `sat/fireplace_mode` |
| `satcurveselftune` | true | bool | Heating curve self-tune: SAT may change `satcoefficient` by 0.05 per step (see 5.11). MQTT `sat/curve_self_tune` |

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

**Every heating start runs in PWM first.** When heating starts (after a pause, after SAT is switched on, after a restart), SAT begins in PWM with the first phase ON, also when SAT is set to manual continuous, and decides between PWM and continuous only after the flame has burned for 60 s; during those 60 s no switch to continuous happens (Sergeant D entries 385, 389, 391, 394; ADR-252, Proposed). A start where the flame was already burning when SAT took over is left as it was.

**Continuous mode**: SAT sends the PID output directly as a flow temperature setpoint. The boiler modulates to reach and hold it. Preferred for modern condensing boilers with good modulation range.

Continuous mode includes clamp logic: if the PID setpoint drops rapidly but the boiler water is still hot, the minimum setpoint is clamped to `boiler_temp - flow_offset` (default 2 C). This prevents the boiler from stopping and immediately restarting (short cycling).

**PWM mode**: For boilers that cannot modulate below 30-40%, SAT cycles the flame on and off at a calculated duty cycle. Minimum flame-on time is 180 seconds to prevent excessively short burner cycles.

**Overshoot protection** (`satovershootprotection`, default on, every heating source): in PWM mode SAT drives the control setpoint (CS) itself. OFF phase: CS = 10 C and CH off, so the boiler is not relit. ON phase, waiting for the flame: CS = return temperature + `satflameoffoffset`, held for `satmodsupdelay` seconds after ignition. After that: CS = flow temperature - `satmodsupoffset`, refreshed about every 3 seconds (not on the 30 s control tick) so the burner stays at its lowest level. If the flame was already on when the ON phase began, CS keeps its previous value until `satmodsupdelay` has passed since the real flame-on time. With the setting off, the OFF phase still sends CS = 10 C and CH off, so the boiler stops in every OFF phase (a deliberate choice; thermo-nova only does this with the option on). The ON phase then gets no hold and no suppression: CS keeps the value of the first ON tick (the requested setpoint) for the rest of that ON phase, and MM=0 applies.

**Modulation (MM=) in PWM mode**: MM=0 in both phases while PWM is enabled. Heat pumps and hybrid systems never get MM=0: they keep the configured maximum modulation (`satmaxrelmod`), in PWM as well. Geminox boilers get MM=10 instead of 0. The flame-off offset is no longer added outside the PWM ON hold, so continuous mode sends the PID setpoint while the flame is off.

**Auto-switch**: SAT always monitors boiler behavior using cycle classification (see section 5.11) and switches modes automatically:

- Continuous to PWM: when the flow temperature minus the requested (PID) setpoint stays at or above the switch margin for 60 seconds (Sergeant D entry 407; ADR-254, Proposed). The margin is the setting `satpwmswitchmargin` (0.5 to 5.0 C), default 2.0 C; a stored old default of 1.5 C moves to 2.0 C once, a value you set yourself stays (Robert, Q-407a). From beta.391 this rule switches to PWM at most once per burn (one flame-on): once SAT has gone from PWM back to continuous while the flame burns, it stays in continuous until the flame goes out (Sergeant D entries 429 and 430). Only the fail-safe can still switch to PWM in that burn: the flow stays 3.0 C or more above the request for 30 s (entries 359 and 429; 3.5 C before beta.391). Only the first PWM ON of a burn counts as a start toward the hourly cycle limit; a later PWM ON in the same burn does not (entry 428, which changes entry 89; TASK-2913). From beta.377 neither rule, the switch margin nor the fail-safe, runs while a tolerance-band pause holds CH off: leftover heat after hot water then no longer switches SAT to PWM with the burner off (Sergeant D entries 410 and 417; thermo-nova skips its switch when the heater is not on, `heating_control.py:276-282`).
- PWM to continuous: in a PWM ON phase, when the flame is on and the requested setpoint stays above the flow temperature for 60 seconds (entries 339 and 342; ADR-236, Proposed), or after 300 seconds of saturation (ADR-236: unchanged). Not during the first 60 s of a start (entry 394).
- The boiler's configuration message (MsgID 3, including "master low-off not allowed") does not block PWM: PWM is allowed from the start on every boiler (entries 392, 393; ADR-251).

**Tolerance band** (Sergeant D entries 376, 377, 384, 401, 402, 403; ADR-253, which partly supersedes ADR-250): when the room reaches the target + 0.1 C, SAT sends CH off while PWM keeps its ON/OFF rhythm; a burn in continuous mode still runs at least 180 s first. Heating restarts as soon as the room is below the target + 0.1 C, but only after CH has been off for at least 10 minutes, counted from the moment the band turned CH off (entries 402, 403). Our readings, to be confirmed by Sergeant D: a band decision while CH was already off (for example a warm room right after a restart) starts no pause (entry 406), and a lost room temperature ends the pause so the heating curve takes over (entry 405, with entries 38 and 46). PWM ON phases that start during a band pause do not count as starts toward the hourly limit (entry 398; entry 400 for a phase that sees CH allowed again). With zones, eco lets the main room decide and comfort keeps heating while any room is more than 0.1 C below its own target (entry 378).

| Situation | Recommended mode |
|---|---|
| Modern condensing boiler with good modulation | Continuous |
| Boiler with high minimum modulation (>30%) | PWM |
| Underfloor heating (slow response) | Continuous |
| Oversized boiler with radiators | PWM |
| Not sure | Start with Continuous, SAT switches by itself |

---

### 5.7 Safety System

SAT implements six independent safety layers. Any single layer tripping sends `CS=0`, releasing the setpoint override and returning control to the wall thermostat immediately.

**Fundamental safety principles:**
- Fail-safe: every error results in `CS=0`. The boiler falls back to the wall thermostat.
- No automatic recovery after a safety trip: SAT stays disabled until the user explicitly re-enables it. A lost OpenTherm link is not a safety trip; see "OpenTherm lost" below.
- Independent layers: a bug in one layer does not affect the others.

**Layer 1: Boot.** After a start-up, restart or power failure SAT sends no control command (CS, MM, CH) for the first 60 s; the gateway's own start-up reads of MsgID 3, 15 and 48 still run (Sergeant D entries 388 and 395). The earlier boot `CS=0` is gone (entry 129); see "Restart" below for what follows.

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

**OpenTherm lost (TASK-1365).** When SAT has seen no boiler reply for 30 s, the OpenTherm link counts as lost. Any boiler reply counts, a Data-Invalid reply included (maintainer, Q33 A). On entering the loss SAT sends one hand-back, the same as a switch-off: `CS=0`, `CH=1`, `MM=T` (Sergeant D entries 39 and 47). After that it sends nothing to the boiler while the loss lasts. SAT stays enabled and no setting is written. On the first new boiler reply SAT resumes by itself and takes MsgID 1 back; the PID restarts from 0 (entry 79). The state is visible as `sat/ot_link_lost` over MQTT, as `ot_link_lost` in `GET /api/v2/sat/status` and, from beta.382, as the hint "SAT paused: no OpenTherm connection, the boiler is back under normal control" on the SAT page.

**No ignition and heating failure (TASK-1882).** Two messages help you spot a boiler problem; neither changes what SAT sends. *No ignition*: SAT asked for heat (a PWM ON phase, or a setpoint above the cold cutoff) for 3 minutes, the boiler flow stayed below the setpoint and no flame came. *Heating failure*: SAT stood at its maximum (setpoint or PWM duty) for 15 minutes and the room rose less than 0.5 °C. A slow, heavy house can show the second one on a cold morning without anything being wrong; the telnet line `SATEVENT heating_failure=...` gives the room at the start and end of each 15-minute window. Both show on the SAT page (warning), in REST `sat/status` (`no_ignition`, `heating_failure`, with counters and the last window's room start and end), over MQTT and as Home Assistant problem sensors.

**Outside temperature missing (TASK-1390).** SAT needs an outside temperature that is younger than 1 hour (Sergeant D entries 56, 57 and 126). It takes the first valid one of: the external sensor (MQTT/REST) for its first 10 minutes, the boiler's own sensor (MsgID 27, from a live boiler reply; from beta.381 also the PIC's own answer to the thermostat with the value set by `OT=`), the weather service, the external sensor up to 1 hour, and the value saved at the last restart. When none is valid SAT sends the same one-time hand-back as for a lost OpenTherm link (`CS=0`, `CH=1`, `MM=T`) and then sends nothing; it resumes by itself as soon as a valid value arrives. The state is visible as `sat/outside_temp_missing` over MQTT, as `outside_temp_missing` in `GET /api/v2/sat/status` and as a hint on the SAT page (from beta.382: "SAT paused: no outside temperature for 1 hour, your thermostat controls the boiler"). While the boiler's value is fresh the weather service is not fetched; from beta.380 the fetches start again 10 minutes before that value turns 1 hour old, so the two sources do not expire at the same moment (TASK-2039).

**Restart (TASK-1482).** SAT saves the last valid outside temperature with its PID state. After a restart within the hour, with a PID state that is at most 30 minutes old, SAT stays silent for the first 60 s after the boot (entry 388) and then takes over at its first control pass once the boiler has replied (entries 126 and 129); until the first room temperature arrives it controls on the heating curve only (entry 388); a PIC keeps SAT's last setpoint for about a minute in between. Otherwise SAT hands the boiler back once and stays quiet until it has a boiler reply and a valid outside temperature (entry 121). Whether PWM was on is kept as well: SAT restarts in PWM with a fresh cycle.

The hand-back resets the PID. It also saves the zeros to flash, except when the loss comes before the first room temperature reading since boot, or before the clock is set by NTP. In those cases the saved PID state stays on flash, so a boiler that is slow to answer right after a boot does not wipe the warm-start state (d06005d7d, entry 115).

---

### 5.8 Simulation Mode

Simulation mode runs the full SAT control loop against a **synthetic boiler** instead of a
real one, so the entire algorithm can be exercised and observed without any heating
hardware. In simulation mode:

- SAT calculates all setpoints and PID values normally.
- SAT does **not** transmit any OpenTherm commands on the bus (bus-tx isolation). The
  regulator's output is recorded as a *command trace* (the last would-be command is
  surfaced) instead of being sent to the PIC/boiler.
- All MQTT topics and REST API values are available, driven by simulated data.

Unlike the early experimental version (which modelled only room temperature), the
simulator now models the **boiler side** too, so the cycle classifier, flame-health state
machine, 4-hour statistics and the daily heating-curve recommendation
all run under simulation:

- **Flame state machine**: the synthetic flame ignites and extinguishes based on flow
  versus setpoint, with realistic minimum on/off timing.
- **Modulation and flow/return temperatures**: modulation tracks demand; flow climbs
  while firing and the return follows with a load-dependent ΔT.
- **Room temperature**: responds to the synthetic flame using configurable heating and
  cooling rates (`fSimHeatRate`, `fSimCoolRate`). The defaults (0.5 and 0.1 °C/min) make the
  simulated house about 6 times faster than a real one, so a test run stays short. Thermal
  learning cannot run at that speed: every sample comes out at 6 × the cooling rate (0.6), above
  the 0.3 limit, and the log says once per run "Sim: thermal learning skips samples". To
  test thermal learning, set `satsimcoolrate` to 0.016 and `satsimheatrate` to 0.08 (a realistic
  house, coefficient about 0.1), and put both back afterwards.
- **Diurnal outdoor temperature**: a day/night curve drives the weather-dependent
  heating curve (or the real weather feed when valid).
- **Scenario injection**: short test scenarios (e.g. sensor noise, a DHW draw) can be
  injected to exercise specific code paths.

**Boiler-absence safety:** simulation requires that no real boiler answers the bus. If a
real boiler is detected while simulation is on, simulation is **automatically disabled**
so SAT never regulates against synthetic state while a live boiler is listening.

Switching simulation on is also refused while a real boiler answers (REST answers `409` with `simulation unavailable: boiler hardware detected`). In Home Assistant the simulation switch shows as unavailable while a real boiler answers and simulation is off, so it cannot be flipped by mistake; it is available again when no boiler answers, and always while simulation is on, so you can switch it off (MQTT `sat/simulation_available`, ADR-255; the web UI switch follows the same rule).

Useful for testing Home Assistant automations, verifying that the heating curve produces
logical values, validating the cycle/flame logic, and demonstrations without an active
heating system. The full contract is documented in
[ADR-117](../../adr/ADR-117-sat-simulation-contract.md).

```bash
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/simulation" -m "ON"
```

---

### 5.9 Weather Integration (Open-Meteo)

SAT can fetch outdoor weather data and a 24-hour hourly temperature forecast from the Open-Meteo API. This is a free API that requires no API key.

On ESP32 the firmware requests the full Open-Meteo current-conditions set: temperature, apparent temperature, humidity, wind speed / direction / gusts, cloud cover, sea-level pressure, precipitation / rain / snowfall, sun elevation, WMO weather code, and the `is_day` flag. The hourly arrays include temperature, dew point, cloud cover, and precipitation probability.

Weather data provides:
- Outdoor temperature for the heating curve (fallback or primary, depending on configuration).
- Sun elevation data for solar gain gating.
- Forecast data for proactive setpoint planning.

| Setting | Default | Range | Description |
|---|---|---|---|
| `satweatherenable` | false | bool | Enable weather data fetching |
| `satweatherlat` | 0.0 | -90 to 90 | Latitude (manual entry, or browser geolocation over HTTPS) |
| `satweatherlon` | 0.0 | -180 to 180 | Longitude |
| `satweatherinterval` | 900 | 300-3600 s | Poll interval (default 15 min, minimum 5 min) |

Browsers only allow geolocation over HTTPS or on localhost. When you open the web UI over HTTPS (behind a reverse proxy), the SAT page shows a **Detect location** button. On plain `http://<device-ip>` the button is hidden and the SAT page shows this hint instead: "Enter your Latitude and Longitude under SAT settings › Weather, or let Home Assistant publish sat/sun_elevation." The words "SAT settings › Weather" are a link: it opens the SAT page in the Control view at the Weather group with the Latitude field focused (while SAT is off the fields are locked and the group heading gets the focus). **Detect location** only appears in a secure context, for example behind an HTTPS Home Assistant ingress. A published `sat/sun_elevation` only replaces the sun elevation for solar gain (section 5.10); fetching the weather itself still needs Latitude and Longitude. Weather data is fetched via plain HTTP (no HTTPS). The first fetch is gated 5 minutes after boot so DHCP, NTP, and MQTT have time to settle before the first HTTPS-free Open-Meteo call. The streaming JSON parser is allocation-free (no malloc inside the SAT loop). For debugging, the telnet console accepts `w` to force an immediate fetch + dump (TASK-513).

---

### 5.10 Solar Gain Compensation

Solar gain compensation detects when sunlight through windows is heating the room and reduces the flow setpoint to prevent overheating.

**How it works:**
1. SAT tracks the indoor temperature rise rate using an EMA (exponential moving average).
2. When the rise rate exceeds the configured minimum and boiler modulation is low (<20%), a solar gain condition is detected.
3. If a sun elevation is known, SAT also checks that the sun is above the minimum elevation before triggering. Its source: `sat/sun_elevation` pushed over MQTT (for example from the Home Assistant sun entity), which wins while it is younger than 1 hour; otherwise the firmware computes it from the weather latitude and longitude and the NTP time. The SAT weather card in the web UI names the source in use and asks for no location while a pushed value is fresh (TASK-2015).
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

Since alpha.31, classification uses **tail-window percentiles** (the last 180 s) rather than the full-cycle P90/P10. A cycle that started with overshoot but corrected itself is no longer flagged OVERSHOOT on the strength of its early peak; the full-cycle P90 is still logged alongside for diagnostics.

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

**Heating curve self-tune (TASK-1525, TASK-1889).** With `satcurveselftune` on (the default), SAT can change the heating curve coefficient itself. It watches how much the PID has to add on top of the heating curve (the PID output minus the curve value, capped at the setpoint ceiling SAT sends). It decides after every counted hour, on a rolling window of the last 12 counted hours (Sergeant D entries 423 and 424; ADR-260, Proposed). An hour counts when at least 90 % of its expected control ticks are clean. Ticks with hot water, a sun elevation of 12 degrees or more or none known, a lost room temperature, or an active no-ignition or heating-failure message are left out, and so are PWM ticks on a heat pump or hybrid; outside PWM on a gas boiler, ticks with the flame off while CH is on, or with modulation at or below the boiler minimum, are left out too. The tune is split in a warm-up phase (room more than 0.1 C from the target) and a floating phase (within 0.1 C). The coefficient moves by 0.05 only when both phases have a mean beyond +1.0 C (raise) or beyond -1.0 C (lower) in the same direction; a phase without data does not veto, and a floating phase with less than 1 hour of data in the window does not vote, so the warm-up phase decides alone. After a step the window is emptied, so two steps are at least 12 counted hours apart. A manual change of the coefficient also empties the window. The tune changes only `satcoefficient`, never the PID terms. In thermo-nova the coefficient is a fixed configuration option (`custom_components/sat/const.py:97`, `entry_data.py:213`); the self-tune is an addition of the firmware.

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

For homes with multiple independently controlled heating zones (e.g., separate radiator circuits or zone valves), SAT supports up to 4 PID zones. Each zone runs its own PID calculation. SAT combines the zones with the PID of the main room into a single boiler setpoint, using the same method as the thermo-nova reference implementation:

- **Which zones count**: a zone counts when it has recent, valid temperature and target data (not off, not expired, within range) and its own PID output is above the cold setpoint (28.2 C for radiators, 21.0 C for underfloor heating). A zone without data does not count.
- **Zone demand**: from the counted zones SAT takes the 75th-percentile PID output, never above the highest zone output. With up to 4 zones this is the highest output, so the zones ask for what the most demanding zone needs. The setting `satzoneheadroom` (default 5.0 C) is the most the zones may ask above that 75th-percentile output. With 4 zones or fewer it has no visible effect.
- **Main room**: the boiler setpoint is the higher of the main room's own PID output and the zone demand. The zones can only raise the demand of the main room, never lower it.
- **Overshoot cap**: when a counted zone is more than 0.3 C above its target, the setpoint is capped at the cold setpoint plus 10 C, minus 4 C for every degree the zone is too warm. The cap never goes below the cold setpoint, and the lowest cap of all zones applies. The cap can pull the setpoint below the demand of the main room. Example for radiators: a zone 1 C too warm limits the setpoint to 34.2 C; a zone 4 C too warm limits it to 28.2 C, which is the cold setpoint (at or below it SAT switches central heating off).
- **ECO heating mode**: in ECO the zones are ignored and only the PID of the main room counts.
- Each zone PID uses a symmetric integral clamp `[-curveValue, +curveValue]` so a zone that is slightly above its target can pull its share of the setpoint below the heating curve. Outside its 0.1 C deadband a zone PID halves its integral once and then holds it, like the main PID (see PID tuning; entry 414).

Zones receive their temperature and target via MQTT. Each zone, zone 1 included, is an extra room next to the main room, with its own target and temperature input.

| Setting | Default | Range | Description |
|---|---|---|---|
| `satzonecount` | 1 | 1-4 | Number of active heating zones |
| `satzonetimeout` | 300 | 60-3600 s | Seconds without update before a zone goes inactive |
| `satzoneheadroom` | 5.0 | 0-15 C | Most the zones may ask above their 75th-percentile output (no effect with 4 zones or fewer) |

When `satzonecount` is 1 (default), SAT only uses a zone if room and target data is pushed for it. A zone without data does not count, so a setup without zone data behaves as before. The per-zone MQTT diagnostics (`sat/zone/<n>/...`) are published only when more than one zone is configured.

---

### 5.16 BLE Temperature Sensor (ESP32 Only)

On the LOLIN S3 Mini and S3 Mini Pro, SAT scans for BLE temperature sensors and can use one as the room temperature input. BLE sensors are surfaced under their own **Sensors** category in the web UI (not buried under SAT), alongside the 1-Wire and GPIO sensors. Scanning is a passive, continuous background activity that is **on by default** (subject to the PSRAM gate below), so sensors appear on their own over time. The BLE stack is **NimBLE-Arduino** (ADR-092), which replaced Bluedroid in 2.0.0 and frees roughly 400 KB of flash. Supported advertising formats:

- **ATC/pvvx custom firmware** (Xiaomi LYWSD03MMC with custom firmware): service data UUID 0x181A. Reports temperature, humidity, and battery level.
- **BTHome v2**: service data UUID 0xFCD2. Standard BTHome protocol for temperature and humidity sensors. Encrypted advertisements are rejected.
- **Xiaomi MiBeacon** (stock Mijia sensors such as MJ_HT_V1 and the unencrypted LYWSD03MMC): service data UUID 0xFE95. Plaintext frames are decoded directly. **Encrypted MiBeacon v4/v5** frames are also supported: provide a per-sensor bindkey and SAT decrypts them in place with AES-128-CCM. Bindkeys are entered per sensor in the roster (a trusted-LAN feature). They are write-only: the UI shows only whether a key is set, never the key itself.

The radio runs a **continuous scan** from boot (TASK-494). Every format-pass advertisement is folded into a persistent 8-slot roster, regardless of whether it is currently selected. The `satbleinterval` setting controls the publish / state-update cadence; it does **not** throttle the radio scan itself.

#### PSRAM-aware default

One combined firmware image ships for all ESP32 boards; `psramFound()` decides at runtime whether BLE actually scans:

- **Boards with PSRAM** (the LOLIN S3 Mini and S3 Mini Pro): BLE runs by default. The extra RAM absorbs the NimBLE stack without destabilising the rest of the firmware.
- **Boards without PSRAM**: BLE cannot be switched on (TASK-2025, ADR-257). Without PSRAM the scanner uses up the memory the gateway needs: on a test unit the heap ran out within about 2 minutes and the gateway stopped answering. The **Enable BLE sensors** switch stays visible but is disabled and greyed out, with the reason as its tooltip, and the BLE card says so. A REST or MQTT request to switch it on is refused (REST answers `409` with `BLE needs PSRAM: not available on this board`), and a stored `on`, for example from settings saved on an older firmware, is switched off at boot and saved as off.

In short, BLE is active when it is enabled AND the board has PSRAM. Firmware before 2.0.0-beta.375 let you accept a risk warning to run BLE without PSRAM; if such a gateway hangs or becomes unreachable, section 7.12a shows how to recover it.

#### Self-discovering roster (TASK-508)

Open the **Sensors** category in the web UI to see the discovered roster, an 8-slot list. Each entry shows the last temperature, RSSI, and age. Give entries friendly names ("Living room", "Bedroom") and rename them at any time; the active sensor is selected from the roster. Auto-select promotes the single fresh sensor when only one is in range. Labels propagate to Home Assistant via the retained discovery configs (per-MAC entities are created automatically). "Forget" wipes a slot and clears its HA discovery topics with zero-byte retained payloads.

The Sensors card offers three controls:

- **Rescan**: trigger an active-scan name burst so freshly powered sensors announce themselves immediately instead of waiting for the next passive advertisement.
- **Clear roster**: wipe all eight slots (a two-press confirm guards against accidents).
- **Name-prefix filter**: type a case-insensitive prefix to narrow the roster by the sensor's advertised BLE name. By default this only filters the display. Enable the **restrict roster** toggle to promote it to an ingestion gate: sensors whose known name does not match the prefix are then kept out of the roster entirely. Sensors with an empty or unknown name are always admitted.

**Trust.** BLE advertisements are unauthenticated broadcasts: any sensor in radio range that sends a supported format lands in the roster, a neighbour's too. SAT uses only the selected sensor. While none is selected and exactly one roster sensor sent a reading in the last two publish intervals, that one is selected automatically. In a building with many BLE devices, select your sensor by hand, or set a name-prefix filter with **restrict roster** on. The firmware never connects to a sensor and never writes to it.

The roster is exposed at:

- `GET /api/v2/sat/ble/discovery` - JSON dump of all roster slots
- `POST /api/v2/sat/ble/select` `{"mac":"AA:BB:.."}` - promote MAC to active sensor
- `POST /api/v2/sat/ble/label`  `{"mac":"AA:BB:..","label":"Living room"}` - rename a slot
- `POST /api/v2/sat/ble/bindkey` `{"mac":"AA:BB:..","key":"<32 hex>"}` - set (or clear, with an empty key) a slot's encrypted-MiBeacon bindkey. Write-only.
- `POST /api/v2/sat/ble/rescan` - trigger an active-scan name burst
- `POST /api/v2/sat/ble/forget` `{"mac":"AA:BB:.."}` - drop slot and clean up HA

| Setting | Default | Range | Description |
|---|---|---|---|
| `satbleenable` | true | bool | Enable BLE sensor scanning (passive, continuous). No-op on boards without a BLE radio. On a board without PSRAM it is always false: switching it on is refused (REST `409`) and a stored true is cleared at boot (TASK-2025) |
| `satbleriskack` | false | bool | Kept for settings files from older firmware; it no longer opens BLE on a board without PSRAM (TASK-2025, ADR-257) |
| `satblemac` | "" | MAC address | Active sensor MAC (set via roster select; empty = auto-select first fresh entry) |
| `satbleinterval` | 30 | 10-300 s | Publish / state-update cadence (NOT scan rate; scan is continuous) |
| `satblenameprefix` | "" | up to 23 chars | Name-prefix filter (case-insensitive; empty = off) |
| `satblenamefilteringest` | false | bool | false = filter the display only; true = also gate roster ingestion (restrict roster) |

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

### 5.17 Thermal Comfort (Summer Simmer Index)

With `satthermalcomfort` on, SAT replaces the room temperature the PID sees with the Summer Simmer Index, which combines room temperature and humidity. It is the only humidity use in the setpoint logic.

The separate humidity-based target adjustment (`satcomfortadjust`, `satcomforthumidity`, `satcomfortmaxoffset`) was removed in TASK-1282 (Sergeant D entry 114). An old `settings.ini` that still holds those keys loads without error; the keys are ignored.

Thermal comfort uses indoor humidity only: a value pushed over MQTT (`sat/humidity`) or REST, or the humidity of the BLE room sensor. The most recent reading wins. Outdoor weather humidity is not used. A reading counts as stale after `sathumiditytimeout` seconds (default 1800). Without a fresh indoor humidity SAT uses the raw room temperature, and the Summer Simmer Index topics (`sat/summer_simmer_index`, `sat/summer_simmer_perception`) are not published (TASK-1441, Sergeant D entry 117).

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
| `satdhwenabled` | false | bool | Standalone DHW control. SAT sends the DHW setpoint to the boiler only while this is on. The SAT page has a toggle for it |
| `satdhwsetpoint` | 0.0 | 30-60 C | DHW setpoint. A value below 30 means inactive (shown as "off"); the slider on the SAT page is disabled while standalone DHW control is off |

---

### 5.20 Power and Energy Tracking

SAT estimates boiler power output and tracks cumulative energy consumption.

| Setting | Default | Range | Description |
|---|---|---|---|
| `satboilercapacity` | 24.0 | 1-100 kW | Boiler thermal capacity (heat output). `sat/power` = modulation % x capacity |
| `satdhwcapacity` | 0.0 | 0-100 kW | Capacity used instead of `satboilercapacity` while the boiler reports DHW active. Set it when your boiler reports modulation relative to a separate DHW limit. 0 = use the boiler capacity |
| `satboilerratedkw` | 0.0 | 0-200 kW | Rated boiler gas input. Enables the estimated gas input (> 0) and is its upper limit; it is not the multiplier. 0 = estimate off |
| `satboilerefficiency` | 0.92 | 0.5-1.0 | Boiler efficiency. Estimated gas input (`energy_estimated_kwh`) = `sat/power` / efficiency |
| `satminconsumption` | 0.0 | 0-8 m³/h | Gas consumption at minimum modulation. With `satmaxconsumption`, it sets `sat/consumption` and `sat/consumption_dhw`. 0 = off |
| `satmaxconsumption` | 0.0 | 0-8 m³/h | Gas consumption at maximum modulation. Consumption = minimum + modulation/100 x (maximum - minimum) while the flame is on, else 0. It is published as `sat/consumption` (CH: the boiler reports CH mode and not hot water) or as `sat/consumption_dhw` (hot water active); the other one reads 0. Published only when both settings are above 0 |

Published to MQTT as `sat/power` (kW) and `sat/energy_total` (kWh), and as `sat/consumption` (CH) and `sat/consumption_dhw` (hot water), both m³/h, when both consumption settings are above 0.

---

### 5.21 Home Assistant Integration

When MQTT is enabled, SAT entities are automatically discovered by Home Assistant.

**Climate entity**: The firmware publishes a thermostat climate entity via HA auto-discovery (`climate.<hostname>_thermostat`). It shows current room temperature, target temperature, and mode. HA-visible modes are `off` and `heat`; the `off` / `heat` mapping is driven by the `thermostat_connected` state of the PIC, so switching heat off in HA releases SAT control. Target temperature bounds follow the HA discovery config: min 12 C, max 28 C, step 0.5 C, precision 0.1 C. Setting the target in HA sends a `TT=` command, which SAT persists to ESP flash. Presets (comfort, eco, away, sleep, home, activity) are pushed through `sat/preset` when configured.

The climate entity also publishes `sat/climate_attributes` (a JSON blob) as the HA `json_attributes_topic` (TASK-594). HA surfaces the SAT operational state as climate-card attributes (mode, PID output, error, curve recommendation, etc.) without requiring template sensors.

A second climate entity (`climate.<hostname>_dhw_control`) exposes DHW setpoint control when the boiler supports it, with modes `off` / `auto` and range 40-60 C.

**Follow the thermostat setpoint (TASK-1259)**: with `satfollowthermostat` on and SAT enabled, a room setpoint change on the wall thermostat becomes the SAT target, so the thermostat works as a control surface next to the SAT UI and Home Assistant. Rules:

- Only a change is adopted (edge-triggered), never a periodic sync. A change of 0.1 C or more counts; there is no minimum time between two changes. The first setpoint after boot, and the first one after SAT or the option is switched on, is only recorded.
- Debounce: the new value must appear in two consecutive thermostat MsgID 16 frames (within 0.05 C of each other). A different value in between restarts the check, so a single-frame flicker, a PIC clear-before-set (Remeha iSense) or a reset glitch adopts nothing.
- The source is the thermostat's own MsgID 16 Write-Data (the T frame), valid range 5-30 C. It is the thermostat's value even while SAT pushes a TC= override, and it is the same on a PIC gateway and on OT-Direct. The PS=1 summary carries no thermostat frame, so nothing is adopted in PS=1 mode.
- A thermostat change also clears an active preset (the preset becomes none, and the preset sync topic is updated). The earlier temperature from before the preset is not restored, so the value the user just set stays.
- With `satpushsetpoint` on, a change is adopted once SAT has pushed a TC= since boot. Before the first push the value is only recorded. With `satpushsetpoint` off no push is needed: every confirmed change is adopted. (Until TASK-1298 the previous thermostat value also had to match the last push; thermostats such as the Honeywell Round never show an accepted override as their own setpoint, so that rule blocked every dial change on them.)
- Echo guard: with `satpushsetpoint` on, a new value within 0.25 C of the last TC= SAT pushed, or of the last value SAT adopted from the thermostat, is SAT's own echo and is not adopted. The 0.25 C covers thermostats that round TC= to their 0.5 C step (TC=20.3 shown as 20.5). A thermostat that mirrors the override and still reports the older push for a moment after a newer one is no change at all, because that older value is the one already recorded. With `satpushsetpoint` off every confirmed change is adopted.
- SAT pushes on a target change only (as thermo-nova), never on a timer. A target taken over from the thermostat is not pushed back. A thermostat that refuses the remote override keeps its own setpoint, and SAT does not re-push.
- Known limitation: when window detection closes the window, it restores the preset from before the window, which can undo a thermostat change adopted in the meantime. Replayed thermostat frames (the `/simulate` bench replay) are accepted like live ones.
- CH enable is unchanged: while SAT is enabled it drives the boiler through its own CS and CH, and the thermostat's CH enable does not decide. When SAT is off, or has tripped a safety, the thermostat controls the boiler again.
- A unit that has no stored `satfollowthermostat` takes the `satpushsetpoint` value the first time it starts; the value is then saved and a later change of `satpushsetpoint` does not move it.

**Fireplace mode (TASK-1374)**: for a home where a stove or air conditioner heats the room that holds the room sensor. With `satfireplace` on (HA switch "Fireplace mode", MQTT `sat/fireplace_mode`, SAT page setting), SAT sends only the heating curve value to the control loop, with P, I and D at 0, exactly as when the room temperature is missing; the rest of the house keeps getting heat although the sensor room reached its setpoint. The room temperature stays visible and is still recorded, but SAT does not regulate on it, learn from it, or start a PV boost from it. Switching the mode off lets the normal PID control resume.

**TT= and TC= remote-override semantics (TASK-466, ADR-223)**: When `satpushsetpoint` is enabled, SAT pushes its target to the thermostat display with `TC=`, like the PIC gateway: the value is answer state only. On OT-Direct in gateway mode the thermostat reads it from OpenTherm MsgID 9 (remote override room setpoint) and MsgID 100 (`RemoteOverrideFunction`); nothing is written to the boiler for it and the thermostat's own room setpoint (MsgID 16) passes unchanged. The two commands carry different priorities in MsgID 100:

- `TT=<v>` -  manual and program change priority (MsgID 100 low byte `0x03`, the PIC's `b'11'`).
- `TC=<v>` -  manual change priority only (MsgID 100 low byte `0x01`, the PIC's `b'01'`).

With `OH=1` (the default, as on the PIC) the low byte is copied into the high byte, so the thermostat reads `0x0303` or `0x0101`; `OH=0` leaves the high byte 0 (TASK-1750, TASK-1961).

Both end at `TT=0` / `TC=0`, at a replacement `TT=` / `TC=`, or by the release rule of the PIC, which applies to TT and TC alike: counting starts after the thermostat has read MsgID 9 once. A thermostat MsgID 16 within 0.25 C of the override counts as acceptance. Before acceptance the 4th mismatching MsgID 16 frame releases the override; after acceptance the first mismatch releases it. A thermostat that never reads MsgID 9 never releases the override. In OT-Direct master mode (no thermostat) `TT=` / `TC=` write MsgID 16 and MsgID 100 to the boiler; `TT=0` then puts a `BS=` or MQTT `room_setpoint` value back on MsgID 16 instead of clearing it, as the PIC's `TT=0` leaves `BS=` alone (TASK-1751). The current override mode is reflected in the `PR=O` report as `O=T<v>` (TT), `O=C<v>` (TC), or `O=N` (none). Multi-channel set commands (MQTT, REST, web UI, serial) are coalesced by MsgID inside the OTDirect command queue (TASK-494): repeated WRITE_DATA for the same MsgID within one OT cycle collapses to a latest-value-wins single frame on the bus.

**SAT settings entities (Task-81 / Task-284)**: Since 2.0.0, all controllable SAT parameters are exposed as HA entities so the full SAT feature set can be driven from a dashboard without sending raw MQTT messages.

| HA component | Count | Examples |
|---|---|---|
| `number` | ~25 | heating curve coefficient, deadband, overshoot margin, control interval, max modulation, flame-off offset, flow offset, modulation-suppression delay/offset, boiler capacity, summer threshold, preset temperatures (comfort/eco/away/sleep/activity/home), min/max pressure, max pressure drop, target temp step |
| `switch` | 13 | `solar_gain`, `summer_simmer`, `multi_area`, `simulation`, `window_detection`, `overshoot_protection`, `push_setpoint`, `follow_thermostat`, `fireplace_mode`, `external_temp`, `preset_sync`, `dhw_enabled`, `dhw_enable` |
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
| `sensor.otgw_sat_power` | `sat/power` | Estimated boiler power output (kW) |
| `sensor.otgw_sat_energy_total` | `sat/energy_total` | Accumulated energy estimate (kWh) |
| `sensor.otgw_sat_ch_pressure` | `sat/ch_pressure` | CH water pressure (bar) |
| `sensor.otgw_sat_ch_pressure_status` | `sat/ch_pressure_status` | Pressure status: ok, low, or high |
| `sensor.otgw_sat_curve_recommendation` | `sat/curve_recommendation` | Heating curve recommendation: INCREASE, DECREASE, or HOLD |
| `binary_sensor.otgw_sat_safety_tripped` | `sat/safety_tripped` | Whether a safety layer has tripped |
| `binary_sensor.otgw_sat_modulation_reliable` | `sat/modulation_reliable` | Whether boiler modulation feedback is reliable |
| `binary_sensor.otgw_sat_setpoint_mismatch` | `sat/setpoint_mismatch` | Setpoint mismatch between SAT and boiler |
| `binary_sensor.otgw_sat_thermal_model_valid` | `sat/thermal_model_valid` | Whether the thermal model has enough data |
| `binary_sensor.otgw_sat_solar_gain` | `sat/solar_gain` | Solar gain compensation active |
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
- While SAT is enabled it sets both the flow temperature setpoint and the CH enable bit the boiler sees: SAT's `CH=0` (cold cutoff, PWM OFF phase, all valves closed, summer simmer) overrides the thermostat's heat demand. On a PIC board the PIC applies the `CH=` override to the thermostat's status message; in OT-Direct gateway mode the gateway rewrites the CH enable bit of the thermostat's relayed status message (MsgID 0). When SAT is off or has tripped a safety, the thermostat controls CH enable again. On OT-Direct a capture with a live thermostat that shows the boiler following SAT's `CH=0` is still open. Sources: ADR-208, ADR-199, ADR-179.
- Multi-zone support runs independent PID loops per zone, but SAT controls a single boiler. Individual boiler circuits per zone are not supported.
- The PIC co-processor holds the last `CS=` setpoint if the ESP fails. Layer 1 (boot safety) corrects this at the next startup.
- Weather data is fetched via plain HTTP only. No HTTPS is used.

---

### 5.23 Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Room never reaches target temperature | Coefficient too low | Increase by 0.2-0.3. Check `sat/curve_recommendation`. |
| Room consistently overshoots target | Coefficient too high | Decrease by 0.2-0.3. Check `sat/curve_recommendation`. |
| Frequent short boiler cycles | Boiler minimum modulation too high | Reduce `satcyclesperhour`. |
| Safety tripped on startup | PIC not ready at SAT init | Normal behavior. Re-enable SAT. |
| "External temp stale" in logs | MQTT push stopped | Check HA automation and broker connectivity. |
| SAT sends no setpoints | PIC not reachable | Check serial connection. Check `otgw-pic/picavailable` via MQTT. |
| Temperature oscillates strongly | Deadband too narrow or poor boiler modulation | Widen deadband, or switch to PWM mode. |
| Setpoint always at minimum (10 C) | SAT active but safety tripped | Check `sat/safety_tripped`. Re-enable SAT via web UI or MQTT. |
| SAT tab not visible in web interface | SAT not yet enabled | Enable SAT in settings. |
| Home Assistant climate entity missing | Auto-discovery disabled or MQTT not connected | Check MQTT connection and HA discovery setting. |
| Pressure alarm keeps triggering | Pressure band too narrow or boiler filling issue | Adjust `satminpressure`/`satmaxpressure`. Check boiler pressure gauge. |
| Solar gain never activates | Rise rate threshold too high or sun elevation data missing | Lower `satsolarminrise`. Push `sat/sun_elevation` from Home Assistant, or set the weather latitude and longitude so the firmware computes it (NTP time needed). |
| Summer mode does not activate | Threshold too high or hours too long | Lower `satsummerthreshold` or `satsummerminhours`. |

---

### 5.24 Debugging

**Telnet debug trace.** SAT has its own conditional debug channel. Connect to the OTGW on TCP port 23 and press `5` to toggle SAT control / cycle / HCR tracing (default on in 2.0.0), `6` for OTDirect tracing (default on), and `7` for the SAT BLE scan debug stream (per-advertisement summary). The telnet `w` shortcut forces an immediate Open-Meteo fetch + dump. The complete set of toggles is shown in the telnet welcome banner.

**Flushing short-lived state.** `sat/flush` clears transient runtime state (cycle window, HCR samples, recent pressure history) without erasing persistent settings. Useful after large changes to the heating system or sensors.

**Persisted files on LittleFS.** SAT stores long-running state under `/sat/`: `sat_hcr.json` (heating curve recommendation samples), plus cycle/window records. These survive reboots.
