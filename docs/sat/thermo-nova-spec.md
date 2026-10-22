# SAT specification (thermo-nova, ground truth)

This document describes SAT (Smart Autotune Thermostat) as implemented in Alexwijn/SAT, branch `releases/thermo-nova`, commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb` (2026-03-18). There is no separate specification document, so this code is the ground truth. The document is meant for Sergeant D to validate and correct.

How to read the citations:

- `file:line` and `file:line-line` are relative to `custom_components/sat` in that commit. To open one on GitHub use `https://github.com/Alexwijn/SAT/blob/7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb/custom_components/sat/<file>#L<n>`, or `#L<n>-L<m>` for a range. Example: `pwm.py:120-134` is `.../custom_components/sat/pwm.py#L120-L134`.
- A short form such as `` `:54-56` `` refers to the last file named before it on the same line.
- Citations that start with `tests/` are relative to the repository root, not to `custom_components/sat`.
- In section 8, citations that start with `pyotgw/`, `OTGW-Commands.md` or `OT-spec` point outside SAT. They are labelled there as context and are not SAT behaviour.
- Pin check, as the sections report it: section 1 says the match of the local copy with this commit could not be verified. Sections 2 and 5 report a file-by-file comparison with the commit and matching git blob hashes for all 78 files. This conflict is left as stated.
- Each section ends with "Not found in the code / unclear". Gaps are listed there instead of being filled with assumed behaviour.
- Some facts appear in more than one section (for example the requested setpoint formula). Each section keeps its own citations.

## Contents

- [Overview: one control pass in time order](#overview-one-control-pass-in-time-order)
- [1. Start-up sequence and control loop timing](#1-start-up-sequence-and-control-loop-timing)
- [2. PID controller](#2-pid-controller)
- [3. Heating curve and setpoint selection](#3-heating-curve-and-setpoint-selection)
- [4. PWM, modulation and the suppression mechanism](#4-pwm-modulation-and-the-suppression-mechanism)
- [5. Boiler state, flame and cycle tracking](#5-boiler-state-flame-and-cycle-tracking)
- [6. Areas, presets, window detection and thermostat sync](#6-areas-presets-window-detection-and-thermostat-sync)
- [7. Solar gain, summer simmer, temperature history and health sensors](#7-solar-gain-summer-simmer-temperature-history-and-health-sensors)
- [8. Boiler I/O, DHW, limits and safety, configuration defaults](#8-boiler-io-dhw-limits-and-safety-configuration-defaults)

## Overview: one control pass in time order

This page is a summary. Every statement is taken from the sections below, with the citation used there. Conditions, exceptions and details are in the sections.

**Start-up (once)**

1. A search of all `.py` files found no dedicated start-up, warm-up or settle phase in the control logic. The hits are listed in section 1, Purpose: for example the PWM ignition window (`const.py:18`) and the sustained-condition timers, whose first observation never fires (`helpers.py:51-56`). Section 1, "Not found" item 20, states the limits of that search. Normal control runs from two fixed-period timers (`climate.py:706-710`, `climate.py:712-716`).
2. SAT restores its previous state instead of starting cold: the PID integral, error and derivative (`pid.py:147-155`), the last control setpoint, the relative modulation value and the PWM-enabled flag (`heating_control.py:181-190`, `pwm.py:111-115`), and the HVAC mode, target temperature and presets (`climate.py:169-212`). The PWM status is not restored; it stays `IDLE` (`pwm.py:89-100`).
3. A fresh install (no saved state) starts with HVAC mode OFF (`climate.py:209-210`). The control loop then sends nothing to the boiler until the user selects HEAT (`climate.py:544-545`). On an OTGW over MQTT, `boot()` still publishes `PM=3`, `PM=15` and `PM=48`, plus `MI=500` for Ideal, Intergas and Nefit (`coordinator/mqtt/opentherm.py:166-172`).

**Timers and triggers**

- Every 5 s a timer requests a control pass. The pass runs 1 s later, and a request while one is pending is ignored (`climate.py:706-710`, `climate.py:526-531`). A target temperature or HVAC mode change also requests a pass (`climate.py:601`, `climate.py:664`).
- Every 60 s the main PID is updated (`climate.py:712-716`, `pid.py:20`). Each room has its own 60 s PID timer (`area.py:188-190`).
- The heating curve has no timer. It is recomputed at start-up, on an inside or outside sensor change, on a target change and on a room target change (`climate.py:134`, `climate.py:767-773`, `climate.py:663`, `climate.py:796`).
- Boiler data updates do not trigger a control pass. They only feed the trackers (`heating_control.py:166`, `heating_control.py:242-260`).

**PID tick (every 60 s)**

1. If the inside sensor is older than `sensor_max_value_age` (default 6 h), the PID is reset and the tick ends (`climate.py:468-478`, `const.py:170`). Without a temperature state the tick ends (`climate.py:480-482`).
2. Solar gain is evaluated (`climate.py:484-494`). It is off by default (`const.py:132`).
3. `pid.update()` is skipped while the heating curve has no value (`pid.py:161-163`). The integral is set to 0 whenever the error is more than 0.1 °C, and only accumulates inside that band (`pid.py:187-191`, `const.py:16`). The PID state is saved after every update (`pid.py:179-181`).
4. PID output = heating curve value + P + I + D, rounded to 0.1 (`pid.py:126-132`).

**Control pass (about every 5 s), in this order**

1. **Gate.** If the outside temperature is unknown or the HVAC mode is OFF, the pass returns and nothing is sent to the boiler (`climate.py:544-545`). So switching SAT to OFF sends no `CS=10` and no `CH=0` on this path (`climate.py:544-545`).
2. **Requested setpoint** (`climate.py:369-394`). It is 10 °C while the curve has no value. Otherwise it is the PID output; in COMFORT mode it is raised to the room demand, capped by the room overshoot cap and lowered by the solar offset. It is then clamped to [10, maximum setpoint]; the default maximum is 50 °C for underfloor and 55 °C otherwise (`helpers.py:97-99`).
3. **Demand.** A `HeatingDemand` is built (`climate.py:547-555`). The heater is ON only if the mode is HEAT, the valves are open and the requested setpoint is above 22 °C (`heating_control.py:64-72`).
4. **Demand ON** (`heating_control.py:202-228`):
   1. PWM is disabled when the requested setpoint is at or above the maximum setpoint (`heating_control.py:203-204`).
   2. The sustained-overshoot check may enable PWM (`heating_control.py:206-209`).
   3. The PWM state machine runs (`heating_control.py:211-215`).
   4. Sustained underheat and saturation may disable PWM (`heating_control.py:217-220`).
   5. MM is computed (`heating_control.py:222`).
   6. The control mode is PWM only when PWM is enabled and its status is not `IDLE` (`heating_control.py:147-152`). CS comes from the PWM override (`heating_control.py:380-448`) or from the continuous rule (`heating_control.py:450-508`).
5. **Demand OFF.** PWM is disabled, CS = 10 °C and MM = the configured maximum (`heating_control.py:229-234`).
6. **Send**, on every pass that reaches heating control, with no change detection: CS, then MM, then CH = ON if CS > 22 °C, then the backend's per-pass hook (`heating_control.py:236-240`). On an OTGW over MQTT this is `CS=`, `PM=25`, `MM=` and `CH=`, each followed by a 0.2 s wait (`coordinator/mqtt/opentherm.py:195-222`, `coordinator/mqtt/__init__.py:134-147`).

**MM and the PWM override (modulation suppression)**

- With demand ON, MM is 0 whenever PWM is enabled and DHW is not active, in status `IDLE` too. Otherwise it is the configured maximum, default 100 (`heating_control.py:136-140`, `heating_control.py:517-520`, `const.py:148`). Exceptions: MM is None when the backend does not support modulation management (`heating_control.py:512-515`), and at least 10 for a Geminox boiler (`heating_control.py:522-523`).
- "Modulation suppression" writes CS, not MM (`heating_control.py:439-448`). The PWM override changes CS only when DHW is off and `overshoot_protection` is true (`heating_control.py:385-386`). That flag is read from the config entry data and is False when absent (`entry_data.py:149-151`). No config-flow step sets it; it is only migrated from older options (`__init__.py:159-162`). When the override returns without changing CS, the previous CS is sent again (`heating_control.py:236`).
- With the override active: PWM status OFF gives CS = 10 °C (`heating_control.py:388-390`). PWM ON with the flame off gives CS = clamp(return + 18, 22, maximum) (`heating_control.py:392-408`, `const.py:151`). For 20 s after flame-on that stored value is held; if none is stored, CS is left unchanged (`heating_control.py:414-433`, `const.py:152`). After that, CS = flow - 1 °C on every pass (`heating_control.py:439-448`, `const.py:153`). The full branch table is in section 4, step 5.

**Between passes (boiler data updates)**

- Each coordinator data update feeds the device tracker (flame and DHW times) and the cycle tracker (`heating_control.py:242-260`).
- When a cycle ends, a continuous cycle classified OVERSHOOT enables PWM, and a PWM cycle classified UNDERHEAT disables it (`pwm.py:219-227`, `heating_control.py:167`).

## 1. Start-up sequence and control loop timing

Source read: the local copy at `other-projects/SAT-releases-thermo-nova/custom_components/sat`. All citations are relative to `custom_components/sat`. The task states that this copy matches upstream `Alexwijn/SAT` branch `releases/thermo-nova` at commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`. I could not verify that match locally, because the copy is a plain snapshot inside another git repository and does not contain that commit object. Line numbers are those of the local copy.

Reading guide. "HA" means Home Assistant, the host program SAT runs inside. Some behaviour comes from HA's own library (timers, event bus, restore of entity state). The SAT code only *calls* that library, so those details are kept out of the Behaviour section and listed under "Not found in the code / unclear".

---

### Purpose

This section describes what SAT does from the moment the integration starts until it controls the boiler in steady state:

- the order of the start-up steps;
- what is restored from before a restart;
- how the first PID and heating curve values are formed;
- when the first setpoint is sent to the boiler;
- which timers run, and what triggers a control pass.

The three most important facts:

1. SAT has **no dedicated start-up, warm-up or settle phase** in its control logic. A grep for `warm|startup|settle|preheat|initial` over all `.py` files turns up only these:
   - log texts, docstrings, code comments and exception messages (for example `__init__.py:44`, `area.py:52`, `coordinator/mqtt/otthing.py:157`, `coordinator/esphome/__init__.py:113`);
   - the "Warm" comfort perception labels of the Summer Simmer index (`summer_simmer.py:53`, `summer_simmer.py:57`);
   - the simulator warm-up setting in the config and the config form (`entry_data.py:102`, `entry_data.py:294`, `config_flow.py:341-342`);
   - the simulator-only warm-up (`coordinator/simulator/__init__.py:27`, `coordinator/simulator/__init__.py:126-127`);
   - the PWM ignition window `HEATER_STARTUP_TIMEFRAME` (`const.py:18`, used in `pwm.py:50`, `pwm.py:155`, `pwm.py:194`, `pwm.py:201`). At `pwm.py:50` it also sets the window in which the PWM "effective on temperature" is smoothed after flame-on (`pwm.py:144-149`);
   - the `initialized` flag of the sustained-condition timers: the first observation only records a start time and never fires (`helpers.py:51-56`, read at `heating_control.py:296`, `heating_control.py:345`, `heating_control.py:367`);
   - a cycle-statistics warm-up used only to classify finished cycles (`cycles/tracker.py:114-115`);
   - a 600 s settle window in the boiler-pressure health binary sensor, which suspends the pressure drop-rate check after the boiler's active state changes (`binary_sensor.py:25`, `binary_sensor.py:335-345`). It is a sensor, not a control input;
   - the boiler status labels, including "preheating" and the pump-start phase (`device/status.py:100-102`, `device/status.py:211-230`). These labels are only shown in sensors and are never used in control decisions (`sensor.py:478-487`, `binary_sensor.py:406`; the only other reader is the property itself, `heating_control.py:104-106`).
2. Normal control runs from **two fixed-period timers**:
   - a PID update every 60 s (`pid.py:20`, `climate.py:712-716`);
   - a boiler control pass every 5 s, delayed by 1 s and debounced (`climate.py:706-710`, `climate.py:526-530`).
3. After a restart, SAT **restores its previous state** instead of starting cold: the PID integral, the error and derivative, the last control setpoint, the PWM-enabled flag, the HVAC mode, the target temperature and presets. With an MQTT gateway it also restores the last-known boiler values (details in Behaviour, phase D).

### Inputs

| Input | Where it comes from | Citation |
|---|---|---|
| Configuration (`SatConfig`): entry data plus options, with `OPTIONS_DEFAULTS` filling missing options. Some properties read only the entry data, so an options default does not reach them (for example `overshoot_protection`, `push_setpoint_to_thermostat`, `heating_system`, `entry_data.py:119-155`) | built once per setup | `__init__.py:29`, `const.py:111-185`, `entry_data.py:105-109` |
| Previous climate entity state (HVAC mode, attributes) | HA restore-state store | `climate.py:77`, `climate.py:171` |
| Stored PID state per climate and per room | HA `Store` key `sat.pid.{entity_id}.{device_id}` | `pid.py:145-155` |
| Stored device state (`modulation_reliable`) | `Store` key `sat.device.{device_id}`, legacy `sat.boiler.{device_id}` | `device/__init__.py:105-123` |
| Stored last boiler values (MQTT gateways only) | `Store` key `sat.mqtt.{device_id}` | `coordinator/mqtt/__init__.py:27`, `coordinator/mqtt/__init__.py:71-74` |
| Inside temperature sensor | HA state of `inside_sensor_entity_id` | `climate.py:270-279`, `entry_data.py:192` |
| Outside temperature (sensor or weather entity, a list is allowed) | HA states | `climate.py:317-343`, `entry_data.py:194` |
| Humidity, thermostat, room climates, window sensors | HA states | `climate.py:281-287`, `climate.py:730-748` |
| Boiler data (flame, flow temperature `Tboiler`, return `Tret`, `TSet`, modulation, DHW, CH) | coordinator data | `coordinator/__init__.py:130-143`, `coordinator/mqtt/opentherm.py:59-132` |
| HA lifecycle: whether HA is already running, `EVENT_HOMEASSISTANT_STARTED`, `EVENT_HOMEASSISTANT_STOP` | HA core | `climate.py:133-150` |

### Behaviour (step by step, in time order)

#### Phase A: integration setup (`__init__.py:async_setup_entry`)

These steps run in strict order: each step is awaited before the next one starts.

1. **Build the configuration.** Options are merged over `OPTIONS_DEFAULTS`, so any option the user never set takes its default value (`__init__.py:29`). This applies only to properties that read the options; properties that read the entry data use their own fallback (`entry_data.py:119-155`). Old entity identifiers are migrated (`__init__.py:30`, `__init__.py:205-229`).
2. **Create the coordinator** (the boiler interface) for the configured mode: fake, simulator, switch, ESPHome, MQTT EMS, MQTT OpenTherm (OTGW), MQTT OTthing or serial (`__init__.py:32`, `coordinator/__init__.py:52-87`).
   - Serial (pyotgw) only: the coordinator immediately fills its data with the pyotgw `DEFAULT_STATUS` and subscribes to gateway updates (`coordinator/serial/__init__.py:33`, `coordinator/serial/__init__.py:39-43`).
   - Every coordinator gets a listener debouncer with `cooldown=0.2` s and `immediate=False` (`coordinator/__init__.py:96-102`).
3. **Create the heating control object** `SatHeatingControl` (`__init__.py:33`). Its constructor:
   - creates an empty cycle history (`heating_control.py:83`) and an empty device tracker (`heating_control.py:84`);
   - creates the PWM controller, whose constructor resets it to disabled with status `IDLE` (`heating_control.py:86`, `pwm.py:63`, `pwm.py:87-100`);
   - sets the control setpoint to `MINIMUM_SETPOINT` = 10.0 °C (`heating_control.py:89`, `const.py:35`);
   - sets the relative modulation value to the configured `maximum_relative_modulation`, default 100 (`heating_control.py:90`, `const.py:148`).
4. **Store the entry data** (config, coordinator, heating control) under the entry id (`__init__.py:35-39`).
5. **Optional error monitoring** (Sentry) is started if enabled (`__init__.py:41-48`).
6. **`coordinator.async_setup()`** (`__init__.py:50`):
   - MQTT coordinators load the stored boiler values from `sat.mqtt.{device_id}`. Empty and `None` values are dropped (`coordinator/mqtt/__init__.py:49-50`, `coordinator/mqtt/__init__.py:71-74`). Nobody listens yet, so this only marks the data clean (`coordinator/__init__.py:387-389`).
   - Serial coordinators connect to the gateway with a 5 s timeout. If that fails, setup is aborted with `ConfigEntryNotReady` (`coordinator/serial/__init__.py:169-178`).
7. **`coordinator.async_added_to_hass(hass)`, first call** (`__init__.py:51`). For MQTT coordinators:
   - wait for the MQTT client (`coordinator/mqtt/__init__.py:53`);
   - subscribe to one topic per tracked key (`coordinator/mqtt/__init__.py:55-60`), for OTGW `{topic}/value/{device_id}/{key}` (`coordinator/mqtt/opentherm.py:229-230`);
   - run `boot()` (`coordinator/mqtt/__init__.py:62`). For OTGW, `boot()` publishes `PM=3`, `PM=15` and `PM=48`, plus `MI=500` when the manufacturer known at that moment is Ideal, Intergas or Nefit (`coordinator/mqtt/opentherm.py:166-172`). At this first call that is only the *configured* manufacturer (`coordinator/__init__.py:107-108`); the fallback from the boiler member id is filled in only by a control pass (`coordinator/__init__.py:345-351`). Each publish is followed by a 0.2 s sleep (`coordinator/mqtt/__init__.py:134`, `coordinator/mqtt/__init__.py:147`).
8. **Forward the platforms** `climate`, `sensor`, `number`, `binary_sensor` (`__init__.py:22`, `__init__.py:52`). Phases B and C are reached through this call. Whether HA completes Phase C before this call returns is HA library behaviour (see "Not found in the code / unclear").
9. **Register the `reset_integral` service** with an `entity_id` schema (`__init__.py:53`, `services.py:13-46`). Then register a listener that reloads the whole entry when options change (`__init__.py:55`, `__init__.py:92-100`).

#### Phase B: climate entity creation (`climate.py:async_setup_entry` and constructor)

1. The `SatClimate` object is constructed (`climate.py:65-70`). Its constructor:
   - sets the setpoint, target temperature and HVAC mode to `None` (`climate.py:89-92`);
   - creates the room areas, one per configured secondary climate, each with its own heating curve and PID (`climate.py:112`, `area.py:218-220`, `area.py:49-50`);
   - creates the heating curve with value `None` (`climate.py:113`, `heating_curve.py:14`);
   - creates the PID, whose constructor resets it: integral 0, derivative 0, last error `None` (`climate.py:114`, `pid.py:38-49`, `pid.py:134-137`);
   - creates the solar-gain controller and an empty temperature history (`climate.py:115-118`).
2. The entity is added to HA (`climate.py:72`), stored in the entry data, and `climate_ready` is set (`climate.py:73-74`).
3. The `sensor` and `binary_sensor` platforms wait at most 10 s for `climate_ready` before they create their entities. On timeout they return without creating any (`sensor.py:30-37`, `binary_sensor.py:37-44`).

#### Phase C: climate added to HA (`climate.py:async_added_to_hass`)

Order as written in `climate.py:123-150`:

1. `super().async_added_to_hass()` (`climate.py:125`): HA base classes; `SatEntity` is a `CoordinatorEntity` (`entity.py:21`).
2. **Restore or set defaults** (`climate.py:128`, details in Phase D).
3. **Heating control added** (`climate.py:130`, `heating_control.py:154-167`):
   - the device tracker loads `modulation_reliable` from its store (`heating_control.py:156`, `device/__init__.py:102-125`);
   - SAT subscribes to coordinator updates with `_handle_coordinator_update` (`heating_control.py:166`);
   - SAT subscribes to `EVENT_SAT_CYCLE_ENDED`, which feeds `pwm.on_cycle_end` (`heating_control.py:167`).
4. **Main PID restored** from `sat.pid.{entity_id}.{device_id}` (`climate.py:131`, `pid.py:141-157`).
5. **Start the control machinery.** Two branches:
   - **HA already running** (for example after an options change or a manual reload). These four steps run directly, in this order (`climate.py:133-138`):
     1. `_update_heating_curves()`;
     2. `_register_event_listeners()`;
     3. `control_pid()`;
     4. `schedule_heating_control_loop()`.
   - **HA still starting** (normal HA boot). The same four functions are registered as four separate one-shot listeners on `EVENT_HOMEASSISTANT_STARTED` (`climate.py:139-144`). Nothing in this list runs until HA reports "started".
6. Register an entity-level `reset_integral` service (`climate.py:146`, `climate.py:214-222`). It has no schema and resets only this climate's PID and its rooms. The same service name is also registered at `__init__.py:53`, with a different handler (`services.py:31-46`). Which of the two registrations runs last is not fixed by SAT code (see "Not found in the code / unclear", items 18 and 19).
7. **`coordinator.async_added_to_hass(hass)`, second call** (`climate.py:147`). For MQTT this subscribes to the topics again and runs `boot()` again, so `PM=3`, `PM=15` and `PM=48` (and possibly `MI=500`) are sent a second time (`coordinator/mqtt/__init__.py:52-64`). The code has no guard against this double call.
8. **Rooms added** (`climate.py:148`, `area.py:248-251`, `area.py:177-190`). For each room:
   - restore the room PID from the store under the room's entity id;
   - run the room's `control_pid()` now if HA is running, otherwise on `EVENT_HOMEASSISTANT_STARTED`;
   - **always** start a 60 s room PID timer, in both branches.
   - A room's `control_pid()` only skips when its temperature state is missing and otherwise calls `pid.update()` (`area.py:202-208`). Unlike the main climate, it has no stale-sensor reset and no solar-gain integral freeze. A stale room sensor makes the room's temperature state `None`, so the room PID is skipped instead (`area.py:67-86`, `area.py:111-114`).
9. Register `async_will_remove_from_hass` on `EVENT_HOMEASSISTANT_STOP` (`climate.py:150`).

#### Phase D: what is restored, and how (`climate.py:169-212`, `heating_control.py:181-190`, `pwm.py:111-115`)

**If a previous climate state exists:**

1. **Heating control restore** (`climate.py:174`):
   - PWM `enabled` is restored, but only when the stored `pulse_width_modulation_enabled` attribute is truthy (`pwm.py:113-115`). PWM status, duty cycle and timers are *not* restored: they stay as the reset left them, status `IDLE` (`pwm.py:89-100`).
   - The control setpoint is restored from the `setpoint` attribute (`heating_control.py:186-187`). That attribute is the heating control's own control setpoint (`climate.py:243`).
   - The relative modulation value is restored from `relative_modulation_value` (`heating_control.py:189-190`).
2. **Target temperature** (`climate.py:176-182`). It is always `None` at this point (`climate.py:90`), so the restore always runs.
   - If the stored `temperature` attribute is missing, the target becomes `self.min_temp` and a warning is logged.
   - Otherwise the stored value is used.
   - The line `self.pid.control_setpoint = self.min_temp` (`climate.py:178`, `climate.py:205`) writes an attribute that the PID class never reads: `pid.py` contains no `control_setpoint`.
3. **HVAC mode** is the previous entity state string (`climate.py:184-185`).
4. Preset mode, pre-activity temperature and pre-custom temperature are restored when present (`climate.py:187-194`).
5. **Room targets**: the stored `rooms` attribute is used, or else rebuilt from the current room climates (`climate.py:196-199`, `climate.py:836-853`).

**If no previous state exists (first install):**

- room targets are read from the room climates (`climate.py:201-202`);
- the target becomes `min_temp` (`climate.py:204-207`);
- the HVAC mode becomes **OFF** (`climate.py:209-210`).

**In both cases** the entity state is written once (`climate.py:212`).

**PID restore** (`pid.py:147-155`). If the store is empty, the PID keeps its constructor values. Otherwise it restores:

- `last_error`, `last_temperature`, `last_derivative_updated`;
- `integral` (default: current value);
- `raw_derivative`.

The same happens for each room PID (`area.py:181`).

**Device tracker restore**: only `modulation_reliable` (`device/__init__.py:118-123`). The flame on/off times and the DHW times start as `None` (`device/__init__.py:32-36`).

**MQTT coordinator restore**: all stored data keys, already loaded in Phase A step 6. On the first control pass, SAT can therefore see flame, `Tboiler`, `TSet` and similar values from before the restart, until new MQTT messages replace them (`coordinator/mqtt/__init__.py:71-74`).

#### Phase E: the first control passes

**E1. First heating curve** (`climate.py:688-697`):

- Computed only when both the target temperature and the outside temperature are available. Otherwise the curve stays `None`.
- The room curves are updated with the same outside temperature (`area.py:259-262`).
- The heating curve is **not** recomputed on the periodic ticks. It is recomputed only on:
  - start-up (`climate.py:134`, `climate.py:140`);
  - an inside or outside sensor state change (`climate.py:767-773`);
  - a target temperature change (`climate.py:663`);
  - a room target change (`climate.py:796`).

**E2. First `control_pid()`** (`climate.py:466-502`):

1. Stale-sensor check, active when `sensor_max_value_age` > 0 (default `06:00:00` = 21600 s; `const.py:170`, `entry_data.py:199-201`). If the inside sensor was last updated longer ago than that, the PID is reset (integral and raw derivative set to 0, `pid.py:134-137`) and the pass ends (`climate.py:470-478`). Staleness uses `last_updated`; a missing sensor state is never stale (`helpers.py:59-69`).
2. If the temperature state cannot be built (sensor missing, stale, unknown or unavailable, or no target), the pass is skipped (`climate.py:480-482`, `climate.py:294-315`, `climate.py:669-686`). With `thermal_comfort` on, the "current temperature" is the Summer Simmer index of temperature and humidity, not the raw sensor value (`climate.py:276-277`).
3. Solar gain is evaluated (`climate.py:484-494`). On the very first sample there is no previous sample, so the rise rate is `None`. The hold time starts as `None`, so solar gain cannot become active on that pass (`solar_gain.py:45`, `solar_gain.py:63`, `solar_gain.py:66-69`, `solar_gain.py:80-82`).
4. The median error is recorded when the mode is HEAT and the valves are open (`climate.py:496-497`).
5. `pid.update()` runs (`climate.py:499-502`). It is skipped entirely when the heating curve is still `None` (`pid.py:161-163`). Otherwise, in this order:
   1. **Derivative** (`pid.py:199-237`). All times here are the inside sensor's `last_changed` timestamp, not the clock (`pid.py:202`, `pid.py:215`). If there is no last temperature or no last-update time (fresh install), only that time is stored (`pid.py:201-203`). The time is also only stored, with no new slope, when |error| ≤ 0.1 °C or the temperature did not change (`pid.py:205-213`). If `delta_time` ≤ 60 s nothing changes (`pid.py:217-218`). After a restart the restored `last_derivative_updated` is used, so `delta_time` spans the gap between the stored time and the sensor's current `last_changed`. When that gap is long, the low-pass factor `alpha = delta_time / (60 + delta_time)` is close to 1 (`pid.py:228`). Whether the sensor's `last_changed` after an HA restart is a fresh time is HA behaviour, not SAT code.
   2. **Integral** (`pid.py:185-192`). If |error| > `DEADBAND` (0.1 °C), the integral is set to **0**. A restored integral therefore survives the first pass only when the room is within ±0.1 °C of target (`pid.py:187-189`, `const.py:16`). The integral is frozen while solar gain is active and `solar_gain_freeze_integral` is set (`climate.py:501`, `pid.py:167-168`).
   3. Store the new error and temperature (`pid.py:170-171`), save the PID state (`pid.py:179-181`), and send the `SIGNAL_PID_UPDATED` signal (`pid.py:183`).

**E3. First heating loop pass.** `schedule_heating_control_loop()` (`climate.py:138` or `climate.py:144`) schedules `async_control_heating_loop` **1 s later** (`climate.py:530`). When it runs (`climate.py:540-555`):

1. The "pending" handle is cleared (`climate.py:542`).
2. **Nothing is sent** when the outside temperature is `None` or the raw HVAC mode is `OFF` (`climate.py:544-545`). A fresh install starts in OFF (`climate.py:209-210`), so it sends no boiler command until the user selects HEAT.
3. Otherwise a `HeatingDemand` is built (`climate.py:547-555`) from:
   - the HVAC mode;
   - `valves_open` (always true when no radiators are configured, `climate.py:420-422`);
   - the timestamp;
   - `requested_setpoint`;
   - the outside temperature.
4. `requested_setpoint` (`climate.py:369-394`):
   - with no heating curve: `MINIMUM_SETPOINT` 10 °C;
   - otherwise: the PID output (curve + P + I + D, `pid.py:126-132`), combined with the rooms, the overshoot cap and solar gain, then clamped (see Formulas).
   - If the first `pid.update` was skipped, the output still uses the **restored** `last_error` (P), `integral` (I) and `raw_derivative` (D) (`pid.py:96-114`).
5. `SatHeatingControl.update()` (`heating_control.py:197-240`):
   - If the heater should be ON (HEAT, valves open, requested > `COLD_SETPOINT` 22 °C, `heating_control.py:64-72`):
     1. PWM is disabled when requested ≥ configured maximum setpoint (`heating_control.py:203-204`).
     2. The sustained-overshoot check runs (`heating_control.py:206-209`).
     3. `pwm.update()` runs (`heating_control.py:211-215`).
     4. The PWM disable guards run (`heating_control.py:217-220`).
     5. The relative modulation is computed (`heating_control.py:222`).
     6. The control setpoint is computed for PWM or continuous mode (`heating_control.py:224-228`).
   - Otherwise PWM is disabled, the setpoint becomes 10 °C and the modulation becomes the configured maximum (`heating_control.py:229-234`).
6. **Commands are sent, every pass, with no change detection** (`heating_control.py:236-240`):
   1. control setpoint;
   2. maximum relative modulation;
   3. heater state (ON if control setpoint > 22 °C);
   4. the coordinator's per-pass hook. The base hook fills in the manufacturer from the boiler member id if none is configured (`coordinator/__init__.py:345-351`).

**What decides the first setpoint value:**

- **Continuous mode** (`heating_control.py:450-508`). `previous_setpoint` is the *restored* control setpoint, or 10 °C on a fresh install (`heating_control.py:454`). Cases:
  - no boiler temperature: the setpoint follows the request;
  - boiler temperature ≤ request: follows the request;
  - flame off: follows the request;
  - otherwise the clamp in the Formulas section applies, using `previous_setpoint`.
  - With an MQTT gateway, the boiler temperature and flame state on this first pass can be the values restored from storage (Phase D).
- **PWM mode** (PWM enabled and status not IDLE, `heating_control.py:147-152`). After a restart with PWM restored as enabled, the status is IDLE. Because `pwm.update()` runs *before* the mode is chosen, the first pass can move it:
  - nowhere, if the boiler flow temperature is unknown: PWM sets status IDLE and returns (`pwm.py:122-128`). The pass then uses continuous mode, but the modulation value is still 0 (unless DHW is active), because the modulation state tests only `enabled`, not the status (`heating_control.py:139-140`, `heating_control.py:517-518`);
  - straight to ON if the computed on-time is ≥ 180 s, with no off-time wait from IDLE (`pwm.py:154-177`), unless the hourly cycle check at `pwm.py:156-158` returns first (see "Not found in the code / unclear", item 14);
  - otherwise to OFF (`pwm.py:179-180`).
  - Then `_compute_pwm_control_setpoint` decides (`heating_control.py:380-448`). With PWM status OFF, overshoot protection on, DHW not active and a request above 22 °C, the setpoint becomes 10 °C (`heating_control.py:385-390`). It **returns without changing the setpoint**, so the restored value is sent, in each of these cases:
    - DHW is active, the request is ≤ 22 °C, or overshoot protection is off (`heating_control.py:385-386`). The `overshoot_protection` flag is read from the config entry *data*, not from the options, so the `OPTIONS_DEFAULTS` value at `const.py:122` does not apply to it; when the key is absent it is False (`entry_data.py:149-151`). When it is off, PWM mode never changes the control setpoint at all, and the continuous computation is also skipped (`heating_control.py:224-228`);
    - the flame is off and there is no return temperature (`heating_control.py:393-395`);
    - the flame is on but the device tracker has no flame-on time (`heating_control.py:410-412`);
    - the suppression delay is still running and no flame-off hold value exists (`heating_control.py:428-433`);
    - the suppression delay has passed but the flow temperature is unknown (`heating_control.py:435-437`).
  - The device tracker gets its first state only on the first coordinator notification that follows a *changed* value (see Phase G). Until that notification, `flame_on_since` is `None` (`device/__init__.py:32`, `device/__init__.py:196-202`), and the "no flame-on time" case above applies.

**E4. First outputs to an OTGW over MQTT** (`coordinator/mqtt/opentherm.py:195-222`):

- `CS=<setpoint>`, then `PM=25`;
- `MM=<value>` (Immergas also gets `TP=11:12=<min(value,80)>` first);
- `CH=1` or `CH=0`.

Each publish uses QoS 1 on `{topic}/set/{device_id}/command` and then waits 0.2 s (`coordinator/mqtt/opentherm.py:232-233`, `coordinator/mqtt/__init__.py:134-149`). In simulation mode nothing is published (`coordinator/mqtt/__init__.py:140-141`).

The modulation value (`heating_control.py:510-523`) is:

- `None` when the coordinator does not support relative modulation management (`heating_control.py:512-515`). The base coordinator returns True and the OTGW MQTT coordinator does not override it (`coordinator/__init__.py:316-322`, `coordinator/mqtt/opentherm.py:43-57`);
- the configured maximum when DHW is active. The `HOT_WATER` state is checked first (`heating_control.py:136-137`);
- otherwise 0 when PWM is enabled (state `OFF`, `heating_control.py:139-140`);
- otherwise the configured maximum. That includes the "COLD" state, when the boiler's reported `TSet` is still unknown or ≤ 10 °C (`heating_control.py:142-145`);
- for a Geminox boiler, never below 10 (`heating_control.py:522-523`).

#### Phase F: steady state, timers and triggers

**Timers started by `_register_event_listeners()`** (guarded so they register only once, `climate.py:699-704`):

| Timer or trigger | Period or delay | Calls | Citation |
|---|---|---|---|
| Control tick | every 5 s | `schedule_heating_control_loop` | `climate.py:706-710` |
| Debounce delay | 1 s after a non-forced schedule | `async_control_heating_loop` | `climate.py:526-531` |
| PID tick, main climate | every 60 s | `control_pid` | `climate.py:712-716`, `pid.py:20` |
| PID tick, each room | every 60 s | `Area.control_pid` | `area.py:188-190` |
| Inside or outside sensor change | event | `_update_heating_curves` + write state | `climate.py:718-728`, `climate.py:767-773` |
| Thermostat change (if configured) | event | `async_set_target_temperature(cascade=False)` | `climate.py:730-735`, `climate.py:750-765` |
| Room climate change | event | room PID reset + curve update | `climate.py:737-741`, `climate.py:775-798` |
| Window sensor open (if configured) | after `window_minimum_open_time` (default 15 s) | preset `activity` | `climate.py:743-748`, `climate.py:800-823`, `const.py:172` |
| Window sensor closed | event | restore the pre-activity temperature | `climate.py:825-834` |

**What triggers a boiler control pass:** only calls to `schedule_heating_control_loop`:

- start-up (`climate.py:138`, `climate.py:144`);
- the 5 s tick (`climate.py:708`);
- an HVAC mode change (`climate.py:601`);
- a target temperature change (`climate.py:664`).

**What does not trigger a pass:**

- Sensor changes. They only update the curve and the displayed state (`climate.py:767-773`).
- Coordinator (boiler data) updates. They only feed the trackers (`heating_control.py:166`, `heating_control.py:242-260`).
- The `force=True` branch (`climate.py:517-524`). The grep found no caller that passes it.

**Debouncing** (`climate.py:526-530`). If a pass is already pending, a new request is ignored, so all triggers within the pending second collapse into one pass. The pending marker is cleared when the pass *starts* (`climate.py:542`), not when it ends.

**Target temperature change** (`climate.py:641-667`), in order:

1. Ignored if the value is unchanged.
2. Store the new target.
3. Optionally cascade the target to the radiator climates, and to the thermostat (`TC=` for OTGW, `coordinator/mqtt/opentherm.py:206-207`).
4. Reset the PID and all room PIDs: integral and derivative to 0 (`climate.py:461-464`).
5. Recompute the heating curves.
6. Schedule a pass.

**HVAC mode change** (`climate.py:570-602`):

1. Accept only HEAT or OFF, and store the new mode (`climate.py:573-580`).
2. Reset the PIDs (`climate.py:583`).
3. Optionally cascade the mode to the climates (`climate.py:586-598`). The list holds the radiator climates, plus the room climates when `sync_climates_with_mode` is on (default True, `const.py:139`, read from the options at `entry_data.py:272`; `climate.py:587-588`).
4. Schedule a pass and write the state (`climate.py:601-602`).

**Early exit in step 3.** If any climate in the cascade list has no state, or does not offer the requested mode, the method returns at once (`climate.py:592-595`). The new mode is then already stored and the PIDs are already reset, but no pass is scheduled and the state is not written. The next 5 s tick still runs a pass with the new mode (`climate.py:706-710`).

Switching to OFF (when the method gets to step 4) schedules a pass that returns at `climate.py:544-545` and sends nothing.

#### Phase G: boiler data updates (not a control pass)

1. A coordinator receives data. For MQTT, each message is stored with `async_set_updated_data` (`coordinator/mqtt/__init__.py:115-132`).
2. Listeners are notified through the 0.2 s debouncer (`coordinator/__init__.py:96-102`):
   - **MQTT and serial** use `async_set_updated_data`, which notifies only if a value actually changed (`coordinator/__init__.py:34-39`, `coordinator/__init__.py:381-398`);
   - **ESPHome** notifies on every state update, using the default `force=True` (`coordinator/esphome/__init__.py:352`, `coordinator/esphome/__init__.py:363`, `coordinator/__init__.py:382-384`);
   - **the simulator** notifies at the end of its per-pass hook, so after every control pass (`coordinator/simulator/__init__.py:91`, `coordinator/simulator/__init__.py:117`, called from `heating_control.py:240`).
3. The listener `_handle_coordinator_update` (`heating_control.py:242-260`) then:
   - updates the device tracker (`device/__init__.py:134-153`). On its **first** update there is no previous state. If the flame is already on, `flame_on_since` is set to this update time (`device/__init__.py:196-202`).
   - updates the cycle tracker (`cycles/tracker.py:55-86`). Because its stored "previous flame" value starts as `None`, a flame that is already on at the first update counts as an OFF→ON transition. A new cycle starts and `EVENT_SAT_CYCLE_STARTED` is fired (`cycles/tracker.py:57-69`).

These timestamps then drive the PWM suppression delay (`heating_control.py:410-417`), which applies only in PWM mode with overshoot protection on (`heating_control.py:224-225`, `heating_control.py:385-386`). If the flame was already burning at start-up, the delay counts from the first post-start update, not from the real ignition.

#### Phase H: shutdown, reload and resume

- **On `EVENT_HOMEASSISTANT_STOP`** (`climate.py:150`, `climate.py:152-167`):
  1. cancel the pending pass;
  2. cancel the window timer;
  3. remove the heating control listeners and save `modulation_reliable` (`heating_control.py:169-179`);
  4. call the coordinator's removal. MQTT saves all its data to `sat.mqtt.{device_id}` (`coordinator/mqtt/__init__.py:66-69`). Serial sends control setpoint 0 and maximum modulation `"-"`, then disconnects (`coordinator/serial/__init__.py:180-190`, `coordinator/serial/__init__.py:228-238`);
  5. stop the room timers.
- **PID state is saved only from `pid.update()`** (`pid.py:179-181`). `pid.reset()` does not save (`pid.py:134-139`).
- **An options change reloads the whole entry**: unload, then Phase A again (`__init__.py:55`, `__init__.py:92-100`). HA is already running at that point, so Phase C takes the "HA already running" branch.
- **Kept only in memory** (lost on every restart or reload), with the line where each is initialised:
  - cycle history (`heating_control.py:83`, `cycles/history.py:40-46`);
  - cycle tracker (`heating_control.py:87`, `cycles/tracker.py:40-42`);
  - device tracker flame and DHW times (`device/__init__.py:32-36`);
  - PWM status, duty cycle, cycle count and timers (`pwm.py:87-100`);
  - sustained-condition timers (`heating_control.py:99-101`);
  - flame-off hold setpoint (`heating_control.py:95`);
  - temperature error history (`climate.py:118`, `temperature/history.py:43-44`);
  - solar-gain hold and last sample (`solar_gain.py:45-46`);
  - heating curve value, recomputed from inputs (`heating_curve.py:14`).

#### Timeline

Times are relative. "+1 s", "+5 s" and "+60 s" come from the code. The duration of awaited I/O steps (store loads, MQTT client wait, publishes) is not in the code.

```
NORMAL HA BOOT (HA still starting when SAT loads)
  A1  build config (options over defaults)                 __init__.py:29
  A2  create coordinator                                   __init__.py:32
  A3  create heating control: setpoint 10, PWM disabled/IDLE   heating_control.py:86-90
  A6  coordinator.async_setup: MQTT loads stored boiler data   coordinator/mqtt/__init__.py:71-74
  A7  coordinator.async_added_to_hass #1: subscribe + PM=3/15/48 (+MI=500)  coordinator/mqtt/opentherm.py:166-172
  A8  platforms forwarded:
      B   SatClimate built (curve None, PID reset)         climate.py:65-118
      C2  restore climate state, setpoint, modulation, PWM-enabled   climate.py:169-212
      C3  device tracker loads modulation_reliable, coordinator listener on   heating_control.py:154-167
      C4  PID loads integral/error/derivative              pid.py:141-157
      C5  four one-shot listeners wait for HA STARTED      climate.py:139-144
      C7  coordinator.async_added_to_hass #2: subscribe + boot again   climate.py:147
      C8  rooms: restore room PIDs, start 60 s room timers   area.py:177-190
  A9  services, reload listener                            __init__.py:53-55
  ... HA reports STARTED ...
  S+0     update heating curves                            climate.py:140
  S+0     register timers (5 s tick, 60 s PID) and listeners   climate.py:141
  S+0     first control_pid (PID update if curve and temperature exist)   climate.py:143
  S+0     schedule first pass (pending)                    climate.py:144
  S+0     rooms: first room control_pid                    area.py:186
  S+1 s   FIRST CONTROL PASS -> CS=, PM=25, MM=, CH= (unless OFF or no outside temp)   climate.py:530, heating_control.py:236-240
  every 5 s:  tick -> schedule -> +1 s pass -> CS/PM/MM/CH again   climate.py:706-710
  every 60 s: control_pid -> PID update -> new requested setpoint   climate.py:712-716

RELOAD (HA already running)
  A1..A7 as above, then in C5 directly:
  R+0     update curves, register timers, control_pid, schedule pass   climate.py:133-138
  R+0..   C6-C8 continue (second boot, room PID restore)   climate.py:146-148
  R+1 s   first control pass (exact ordering versus C7/C8 not fixed by the code)
```

### Formulas

Variable names are as in the code.

**Heating curve** (`heating_curve.py:28-39`, `types.py:116-118`):

```
calculate(target, outside) = 4*(target - 20) + 0.03*(outside - 20)^2 - 0.4*(outside - 20)
value = round(base_offset + (coefficient / 4) * calculate(target, outside), 1)
base_offset = 20 for underfloor, 27.2 for every other heating system
coefficient = heating_curve_coefficient (default 2.0)
```

**Error** (`temperature/state.py:15-18`): `error = round(setpoint - current, 3)`, where `setpoint` is the room target temperature.

**PID gains with `automatic_gains` on** (the default, `const.py:120`; `pid.py:61-93`):

```
kp = round(heating_curve_coefficient * heating_curve.value / g, 6)   g = 4 underfloor, else 3
ki = round(kp / 8400, 6)
kd = round(0.07 * 8400 * kp, 6)
```

With `automatic_gains` off, `kp`, `ki` and `kd` are the configured `proportional`, `integral` and `derivative` (defaults 45, 0 and 6000, `const.py:113-115`).

**PID terms** (`pid.py:95-132`, `pid.py:185-237`; the symmetric clamp is `clamp_to_range`, `helpers.py:148-150`):

```
P = round(kp * last_error, 3)
I: if |error| > DEADBAND: integral = 0
   else integral = clamp(integral + ki * error * PID_UPDATE_INTERVAL, -curve, +curve)
   (PID_UPDATE_INTERVAL = 60 is used as dt regardless of the real interval)
D: derivative = -(current - last_temperature) / delta_time
   (only when |error| > DEADBAND, the temperature changed and delta_time > 60 s)
   if |derivative| >= 5.0: raw_derivative = clamp(derivative, -5, 5)
   else: alpha = delta_time / (60 + delta_time)
         raw_derivative = clamp(alpha*derivative + (1-alpha)*raw_derivative, -5, 5)
   D = round(kd * raw_derivative, 3)
output = round(heating_curve.value + P + I + D, 1)
```

**Requested setpoint** (`climate.py:369-394`):

```
if heating_curve.value is None: requested = 10
setpoint = pid.output
ECO:     requested = clamp(round(setpoint,1), 10, coordinator.maximum_setpoint)
COMFORT: setpoint = max(setpoint, rooms_output)          (rooms_output is 10 if no room calls for heat)
         setpoint = min(setpoint, overshoot_cap)          (see room overshoot cap below)
         setpoint -= solar_gain_setpoint_offset_celsius   (if solar gain active and enabled)
         requested = clamp(setpoint, 10, coordinator.maximum_setpoint)
```

Room aggregation and cap: `area.py:272-333`. A room counts only if its PID is available and it calls for heat (`area.py:279-280`, `area.py:309-310`). The room overshoot cap uses only those rooms that are more than 0.3 °C above their target (`area.py:315-316`):

```
cap per room  = 22 + 10 - 4 * degrees_over        (COLD_SETPOINT + COOLING_HEADROOM - COOLING_SLOPE * degrees_over)
overshoot_cap = round(max(22, min(all room caps)), 1)
```

(`area.py:31-34`, `area.py:319-333`). The maximum is `coordinator.maximum_setpoint`, which returns the configured `limits.maximum_setpoint` (`coordinator/__init__.py:274-283`). The configuration already fills that limit from the options, then the entry data, then 50 (underfloor) / 55 (others), so it is never `None` there (`entry_data.py:227-239`, `helpers.py:97-99`).

**Heater state for the demand** (`heating_control.py:64-72`): ON if `hvac_mode == HEAT` and `valves_open` and `requested_setpoint > 22`, otherwise OFF.

**Continuous control setpoint** (`heating_control.py:450-489`):

```
if boiler_temperature is None or boiler_temperature <= requested or flame off:
    control = requested
else:
    minimum_allowed = boiler_temperature - flow_setpoint_offset_celsius   (default 2.0)
    if requested > previous: control = max(requested, minimum_allowed)
    else:                    control = max(requested, min(minimum_allowed, previous))
```

**Heater command** (`heating_control.py:238`): `CH = ON if control_setpoint > 22 else OFF`.

**Sustained-condition timer** (`helpers.py:51-56`): the first observation only records the start (`initialized=True`). The condition can fire only on a later observation, once `elapsed_seconds` reaches the threshold.

### Constants and defaults

| Name | Value | Unit | Citation |
|---|---|---|---|
| Control tick interval | 5 | s | `climate.py:706-710` |
| Debounce delay before a pass | 1 | s | `climate.py:530` |
| `PID_UPDATE_INTERVAL` (PID tick and integral dt) | 60 | s | `pid.py:20`, `pid.py:191` |
| Room PID tick | 60 | s | `area.py:188-190` |
| Coordinator listener debounce cooldown | 0.2 | s | `coordinator/__init__.py:96-102` |
| MQTT wait after each publish | 0.2 | s | `coordinator/mqtt/__init__.py:134`, `coordinator/mqtt/__init__.py:147` |
| Serial connect timeout | 5 | s | `coordinator/serial/__init__.py:171` |
| Platform wait for `climate_ready` | 10 | s | `sensor.py:32`, `binary_sensor.py:39` |
| `MINIMUM_SETPOINT` (initial control setpoint, fallback request) | 10.0 | °C | `const.py:35`, `heating_control.py:89` |
| `COLD_SETPOINT` (heater ON threshold) | 22 | °C | `const.py:34` |
| `DEADBAND` (integral and derivative gate) | 0.1 | °C | `const.py:16` |
| `DERIVATIVE_RAW_CAP` | 5.0 | °C/s (raw slope) | `pid.py:19` |
| `HEATER_STARTUP_TIMEFRAME` (PWM ignition wait and minimum flame-on hold) | 180 | s | `const.py:18` |
| `sensor_max_value_age` default | 06:00:00 (21600) | s | `const.py:170` |
| `window_minimum_open_time` default | 00:00:15 (15) | s | `const.py:172` |
| `maximum_relative_modulation` default (initial modulation value) | 100 | % | `const.py:148`, `heating_control.py:90` |
| `MINIMUM_RELATIVE_MODULATION` (value when PWM enabled) | 0 | % | `const.py:37`, `heating_control.py:517-518` |
| `heating_curve_coefficient` default | 2.0 | - | `const.py:182` |
| `automatic_gains` default | True | - | `const.py:120` |
| `proportional` / `integral` / `derivative` defaults | 45 / 0 / 6000 | - | `const.py:113-115` |
| `heating_mode` default | comfort | - | `const.py:183` |
| `flow_setpoint_offset_celsius` default | 2.0 | °C | `const.py:154` |
| `modulation_suppression_delay_seconds` default | 20 | s | `const.py:152`, `entry_data.py:177-179` |
| `flame_off_setpoint_offset_celsius` default | 18.0 | °C | `const.py:151` |
| `modulation_suppression_offset_celsius` default | 1.0 | °C | `const.py:153` |
| `overshoot_protection` (read from entry data, not options; False when absent) | False | - | `entry_data.py:149-151` (the options default at `const.py:122` is not used by this property) |
| Heating curve `base_offset` | 20 (underfloor), 27.2 (other) | °C | `types.py:116-118` |
| Default maximum setpoint | 50 (underfloor), 55 (other) | °C | `helpers.py:97-99`, `entry_data.py:227-239` |
| `OVERSHOOT_SUSTAIN_SECONDS` / `UNDERHEAT_SUSTAIN_SECONDS` / `SATURATION_SUSTAIN_SECONDS` | 60 / 180 / 300 | s | `const.py:21-23` |
| `DHW_OVERSHOOT_GUARD_SECONDS` | 300 | s | `const.py:19` |
| `SOLAR_GAIN_HOLD_SECONDS` | 600 | s | `const.py:24` |
| Simulator warm-up (`simulated_warming_up`, simulator mode only) | 00:00:15 (15) | s | `const.py:171`, `coordinator/simulator/__init__.py:126-127` |

### State kept between passes

| State | Initial value at start | Persisted across restart? | Citation |
|---|---|---|---|
| Control setpoint `_control_setpoint` | 10 °C, then restored `setpoint` attribute | yes (climate attribute) | `heating_control.py:89`, `heating_control.py:186-187`, `climate.py:243` |
| Relative modulation value | configured maximum, then restored | yes (climate attribute) | `heating_control.py:90`, `heating_control.py:189-190`, `climate.py:257` |
| PWM `enabled` | False, then restored if truthy | yes (climate attribute) | `pwm.py:89`, `pwm.py:113-114`, `climate.py:260` |
| PWM status, duty cycle, cycle count, flame-on time, effective temperature | IDLE / None / 0 | no | `pwm.py:87-100` |
| PWM `_last_update` | wall clock at PWM construction or reset | no | `pwm.py:91` |
| HVAC mode, target temperature, preset, pre-custom and pre-activity temperatures, room targets | restored or defaults | yes (climate state) | `climate.py:169-212` |
| PID integral, last error, raw derivative, last temperature, last derivative time | 0 / None, then restored | yes (`sat.pid.*`, saved every update) | `pid.py:38-43`, `pid.py:141-157`, `pid.py:239-251` |
| Heating curve value | None, recomputed | no | `heating_curve.py:14`, `climate.py:688-697` |
| Device tracker `modulation_reliable` | None, then restored | yes (`sat.device.*`) | `device/__init__.py:102-131` |
| Device tracker flame and DHW on/off times | None | no | `device/__init__.py:32-36` |
| MQTT coordinator data (all tracked keys) | restored from store | yes (`sat.mqtt.*`, saved on removal) | `coordinator/mqtt/__init__.py:66-78` |
| Cycle history and cycle tracker | empty | no | `heating_control.py:83`, `heating_control.py:87` |
| Sustained overshoot, underheat and saturation start times | None | no | `heating_control.py:99-101` |
| Flame-off hold setpoint | None | no | `heating_control.py:95` |
| Last requested setpoint and outside temperature (for cycle samples) | None | no | `heating_control.py:92-93` |
| Temperature error history | empty | no | `climate.py:118` |
| Solar-gain hold and last sample | None | no | `solar_gain.py:45-46` |
| Pending-pass handle (debounce) | None | no | `climate.py:105` |

### Outputs (to the boiler, to entities)

**To the boiler.** OTGW over MQTT is shown; serial uses the matching pyotgw calls.

| When | Command | Citation |
|---|---|---|
| Boot, on **each** of the two `async_added_to_hass` calls | `PM=3`, `PM=15`, `PM=48`, and `MI=500` if the configured manufacturer is Ideal, Intergas or Nefit | `coordinator/mqtt/opentherm.py:166-172`, `__init__.py:51`, `climate.py:147` |
| Every control pass with HVAC not OFF and an outside temperature present (about every 5 s + 1 s) | `CS=<control setpoint>`, `PM=25` | `heating_control.py:236`, `coordinator/mqtt/opentherm.py:195-199` |
| Same pass | `MM=<value>` (Immergas also `TP=11:12=<min(value,80)>`) | `heating_control.py:237`, `coordinator/mqtt/opentherm.py:216-222` |
| Same pass | `CH=1` if control setpoint > 22 °C, else `CH=0` | `heating_control.py:238`, `coordinator/mqtt/opentherm.py:211-214` |
| Target change with `push_setpoint_to_thermostat` set | `TC=<target>` | `climate.py:656-657`, `coordinator/mqtt/opentherm.py:206-207` |
| User sets the boiler maximum setpoint (number entity) | `SH=<value>` | `number.py:125-127`, `coordinator/mqtt/opentherm.py:224-225` |
| Serial only, on removal | control setpoint 0, maximum modulation `"-"` | `coordinator/serial/__init__.py:228-233` |

In the serial coordinator each command is skipped when simulation is enabled (`coordinator/serial/__init__.py:192-226`). For MQTT, simulation suppresses all publishes (`coordinator/mqtt/__init__.py:140-141`).

**To entities and HA:**

- The climate entity state is written at restore (`climate.py:212`), on sensor change (`climate.py:773`), on room change (`climate.py:798`), on HVAC change (`climate.py:602`) and on target change (`climate.py:667`).
- Its attributes include `setpoint`, `requested_setpoint`, `heating_curve`, `relative_modulation_value` and the PWM fields (`climate.py:234-263`).
- `SIGNAL_PID_UPDATED` is sent after each PID update (`pid.py:183`). The requested-setpoint sensor refreshes on it (`sensor.py:71-77`).
- `EVENT_SAT_CYCLE_STARTED` and `EVENT_SAT_CYCLE_ENDED` are fired by the cycle tracker (`cycles/tracker.py:69`, `cycles/tracker.py:81`).

### Edge cases

1. **Fresh install.** The HVAC mode is OFF (`climate.py:209-210`), so the loop returns before sending anything (`climate.py:544-545`). Until the user selects HEAT, the control loop sends nothing; the only boiler commands are the boot commands (`coordinator/mqtt/opentherm.py:166-172`) and user-triggered ones such as `SH=` from the number entity (`number.py:125-127`).
2. **Outside temperature unavailable.** It is stale, unknown or the entity is missing (`climate.py:323-343`). No control pass sends anything (`climate.py:544-545`), and the heating curve stays at its last value, or `None` at start-up (`climate.py:693-694`).
3. **Inside temperature unavailable at start-up.** `control_pid` skips (`climate.py:480-482`). If the curve exists, the first pass uses a requested setpoint built from the restored P, I and D terms (`pid.py:96-132`).
4. **Inside sensor older than `sensor_max_value_age`.** Every 60 s PID tick resets the integral and derivative and returns (`climate.py:470-478`). `last_error` is kept, because `reset` does not clear it (`pid.py:134-137`), so P keeps using the old error.
5. **Restored HVAC state that is not `heat` or `off`** (for example `unavailable`, if it was ever stored). The raw check at `climate.py:544` does not match `OFF`, but `hvac_mode` maps it to OFF (`climate.py:351-356`). The pass then runs with heater OFF and sends `CS=10`, `MM=<max>` and `CH=0` every pass (`heating_control.py:229-238`). Whether HA ever stores such a value is not in SAT code.
6. **Restored integral.** It is zeroed on the first PID update when |error| > 0.1 °C (`pid.py:187-189`). Any target change or HVAC change also zeroes it (`climate.py:583`, `climate.py:660`).
7. **First derivative after a long downtime.** `delta_time` is the sensor's `last_changed` minus the restored `last_derivative_updated` (`pid.py:215`). If that gap is long and a new slope is computed (|error| > 0.1 °C, temperature changed, `pid.py:205-218`), `alpha` ≈ 1 and the new slope almost fully replaces the restored one (`pid.py:228-229`). A slope of 5 or more bypasses the filter and is capped directly (`pid.py:222-225`).
8. **Flame already burning when SAT starts.** The device tracker and the cycle tracker both treat the first update as a flame start (`device/__init__.py:198-202`, `cycles/tracker.py:57-69`). In PWM mode with overshoot protection on, the PWM suppression delay (default 20 s, `const.py:152`) counts from that moment (`heating_control.py:385-386`, `heating_control.py:414-417`).
9. **PWM restored as enabled.** The status is IDLE, so the first `pwm.update` can enter ON at once if the on-time is ≥ 180 s (`pwm.py:155`). If the flame is already on at that moment, the 180 s minimum flame-on hold applies from that pass (`pwm.py:164`, `pwm.py:201-203`). If the flame is off, PWM first waits up to 180 s for ignition (`pwm.py:163`, `pwm.py:186-199`).
10. **Overlapping passes.** The pending marker is cleared at the start of a pass (`climate.py:542`), so a trigger during a running pass schedules a new pass 1 s later (`climate.py:527-530`). A pass with four MQTT publishes sleeps at least 4 × 0.2 s = 0.8 s (`coordinator/mqtt/__init__.py:147`), which alone is shorter than that 1 s. Two passes overlap only if a pass takes longer than 1 s; the code does not bound how long the awaited publishes take, and it has no lock.
11. **Removal callbacks pile up.** Each non-forced schedule adds another removal callback (`climate.py:531`). The code never prunes them.
12. **Double coordinator start.** `async_added_to_hass` runs at `__init__.py:51` and again at `climate.py:147`. For MQTT this means two subscription rounds and two `boot()` runs (`coordinator/mqtt/__init__.py:55-62`). The coordinator's removal is also called from two places (`__init__.py:68`, `climate.py:162`).
13. **Sensor platforms time out.** If `climate_ready` is not set within 10 s, the sensor and binary_sensor platforms add no entities, silently (`sensor.py:30-37`, `binary_sensor.py:37-44`).
14. **Restart soon after a reset.** A target or HVAC change zeroes the integral in memory only, because `pid.reset()` does not save (`pid.py:134-139`, `climate.py:461-464`). If SAT restarts before the next `pid.update()` saves (`pid.py:179-181`), the integral from *before* the reset comes back from the store (`pid.py:154`).
15. **Serial gateway unreachable at setup.** `ConfigEntryNotReady` is raised (`coordinator/serial/__init__.py:172-173`), and the climate entity is never created in that attempt (the exception leaves `async_setup_entry` at `__init__.py:50`, before the platforms are forwarded at `__init__.py:52`).
16. **HVAC mode change with a cascade climate that lacks the mode.** The method returns inside the cascade loop (`climate.py:592-595`) after the mode is stored and the PIDs are reset (`climate.py:573-583`). No pass is scheduled and the state is not written at that moment; the next 5 s tick picks up the new mode (`climate.py:706-710`).

### Additional behaviour (completeness pass)

This subsection covers start-up and shutdown code that the sections above did not cite.

**Cancelling a scheduled pass.** A pass that is scheduled but has not run yet is cancelled by `_cancel_scheduled_heating_control_loop`. If a pass is pending, its timer is cancelled and the handle cleared; otherwise it does nothing (`climate.py:533-538`). It runs at HA stop or entity removal (`climate.py:152-155`). It is also registered as a removal callback each time a pass is scheduled (`climate.py:530-531`). A pass clears the handle itself when it starts (`climate.py:540-542`).

**Base coordinator hooks.**

- `async_setup` and `async_added_to_hass` do nothing in the base class (`coordinator/__init__.py:333-339`). The switch, simulator and fake coordinators do not override them; a search of those three files for these names found nothing. Of these two hooks, the MQTT coordinators override both (`coordinator/mqtt/__init__.py:49-64`), ESPHome overrides only `async_added_to_hass` (`coordinator/esphome/__init__.py:208`), and serial overrides only `async_setup` (`coordinator/serial/__init__.py:177-178`).
- The base removal cancels a pending listener notification, which is the 0.2 s debouncer (`coordinator/__init__.py:341-343`, `coordinator/__init__.py:96-102`). MQTT, serial and ESPHome call it after their own clean-up (`coordinator/mqtt/__init__.py:66-69`, `coordinator/serial/__init__.py:180-190`, `coordinator/esphome/__init__.py:250-255`).

**Unload of the integration entry, in order** (`__init__.py:60-89`):

1. If there is no entry data, the unload reports success (`__init__.py:62-64`).
2. Coordinator removal runs first, then the platforms are unloaded (`__init__.py:66-69`). A `ValueError` from that block counts as "already unloaded" (`__init__.py:70-72`).
3. If the platform unload fails, the function returns False and keeps the entry data (`__init__.py:74-75`).
4. Otherwise Sentry is flushed and closed, and the entry data is removed (`__init__.py:77-87`).

A reload (after an options change) stops with a warning if the unload fails, so Phase A does not run again (`__init__.py:92-97`).

### Not found in the code / unclear

1. **Upstream pin.** I could not verify that the local copy matches commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`. The local tree is a snapshot inside another repository, and that commit object is not present.
2. **When the first periodic tick fires.** The first 5 s and 60 s ticks of `async_track_time_interval` (`climate.py:706-716`) are scheduled by HA library code, not SAT code. The SAT code adds one explicit `control_pid` and one scheduled pass at start-up (`climate.py:137-138`, `climate.py:143-144`).
3. **Order of the four `EVENT_HOMEASSISTANT_STARTED` callbacks** (`climate.py:140-144`). They are registered in this order: curves, listeners, PID, schedule. Whether HA calls them in registration order is HA behaviour, not shown here. If the PID callback ran before the curve callback, the first `pid.update` would skip because the curve is `None` (`pid.py:161-163`).
4. **Ordering race when HA is already running.** The first pass is scheduled at `climate.py:138` for +1 s. The second `boot()` and the room PID restores (`climate.py:147-148`) are awaited after that. Whether they finish within that 1 s depends on I/O timing, which the code does not fix.
5. **The `min_temp` value** used as the fallback target (`climate.py:178-179`, `climate.py:205-206`). SAT does not define `min_temp`, so it comes from the HA climate base class.
6. **`self.pid.control_setpoint = self.min_temp`** (`climate.py:178`, `climate.py:205`). The PID class never reads this attribute (`pid.py`). Its intended purpose is unclear.
7. **What HA does with duplicate MQTT subscriptions** from the double `async_added_to_hass` (point 12 in Edge cases). This is MQTT component behaviour, not SAT.
8. **Meaning of `PM=3`, `PM=15`, `PM=48`, `PM=25` and `MI=500`.** These are OTGW gateway commands. The SAT code sends them without explaining them (`coordinator/mqtt/opentherm.py:166-172`, `coordinator/mqtt/opentherm.py:197`).
9. **What the boiler or gateway does with the last `CS=` and `CH=` after SAT goes OFF.** No command is sent when OFF (`climate.py:544-545`), and holding or timing out the last values is gateway behaviour outside this code.
10. **Restore-state mechanics.** When HA saves the climate state and which states it keeps (`climate.py:171`) are HA `RestoreEntity` behaviour.
11. **The exact debouncer semantics** of `Debouncer(cooldown=0.2, immediate=False)` (`coordinator/__init__.py:96-102`) are HA library behaviour.
12. **`ConfigEntryNotReady` retry timing** for serial (`coordinator/serial/__init__.py:173`) and ESPHome (`coordinator/esphome/__init__.py:113`) is HA behaviour.
13. **`async_will_remove_from_hass` may run twice at HA stop**: it is registered on `EVENT_HOMEASSISTANT_STOP` (`climate.py:150`) and is also the normal entity removal hook. Whether HA calls both is not shown in SAT code.
14. **The PWM cycle limit compares the wrong-looking quantity.** `pwm.py:156` reads `if self._current_cycle >= self._config.duty_cycle_seconds`. It compares a cycle count with `duty_cycle_seconds` (default 13:00 = 780, `const.py:167`, `entry_data.py:220`), not with `cycles_per_hour`. It is quoted here as written; the intent is unclear.
15. **`force_pulse_width_modulation`** is read into the config (`entry_data.py:223`), but no control code reads it. The grep finds only the constant and its default (`const.py:80`, `const.py:149`), the options form (`config_flow.py:728`) and the config field (`entry_data.py:58`, `entry_data.py:223`).
16. **`automatic_gains_value`** is read into the config (`entry_data.py:212`), but the automatic `kp` uses the hard-coded divisors 3 and 4 instead (`pid.py:70-71`).
17. **`_async_entity_changed`** compares the whole new state object with the strings `unavailable` and `unknown` (`climate.py:769`), so that filter never matches. The curve update it triggers reads the outside temperature through `current_outside_temperature`, which does filter those values (`climate.py:334-335`).
18. **Completion order of platform setup.** It is not fixed by SAT code whether the climate entity's `async_added_to_hass` (Phase C) finishes before `async_forward_entry_setups` returns (`__init__.py:52`). The 10 s `climate_ready` wait in the sensor platforms (`sensor.py:30-34`) suggests the authors did not rely on a fixed order. The nesting in the timeline is therefore only indicative.
19. **Which `reset_integral` handler is active.** The entity-level registration (`climate.py:222`) and the integration-level registration (`services.py:41-46`) use the same service name. Which one HA keeps is HA behaviour.
20. **No warm-up or settle period** exists for the room temperature, the PID or the heating curve after start. This conclusion rests on the grep in Purpose and on reading the files listed in this section. A path I did not read could still add one.

## 2. PID controller

Source: Alexwijn/SAT, branch `releases/thermo-nova`, pinned commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`. All paths are relative to `custom_components/sat`. The local copy was compared file by file (line endings ignored) against the pinned commit on GitHub for every file cited below, and all of them are identical. Every citation in this section was re-opened and checked in a citation audit.

### Purpose

The PID controller turns the room temperature error into a correction on top of the heating curve. Its output is a flow temperature request in degrees, not a percentage: `output = heating curve value + P + I + D` (pid.py:126-132). The climate entity uses that output as the base of the "requested setpoint" that goes to the heating control (climate.py:369-394, climate.py:547-555).

There is one PID for the main climate (climate.py:113-114) and one extra PID per configured secondary room ("area") (area.py:48-50, area.py:219-220). Each PID has its own heating curve object (climate.py:113, area.py:49). An area curve uses the area's own target temperature and the real outside temperature (area.py:259-262, climate.py:696).

### Inputs

| Input | Where it comes from | Citation |
|---|---|---|
| Room setpoint (`state.setpoint`) | Climate target temperature | climate.py:303-310 |
| Room temperature (`state.current`) | Inside sensor; replaced by the Summer Simmer index when `thermal_comfort` is on | climate.py:270-279, climate.py:304-311 |
| Sensor change time (`state.last_changed`) | `last_changed` of the inside sensor state in Home Assistant | climate.py:300, climate.py:314 |
| Heating curve value | `HeatingCurve.value`, recomputed from target and outside temperature | heating_curve.py:18-20, heating_curve.py:36-39, climate.py:688-697 |
| Heating system type | Config (`radiators`, `underfloor`, `heat_pump`, `unknown`); `unknown` when the setup data has no heating system | pid.py:52-54, types.py:110-118, entry_data.py:120-124 |
| Gains config | `PidConfig`: proportional, integral, derivative, automatic_gains, automatic_gains_value, heating_curve_coefficient, all read from the options | entry_data.py:43-50, entry_data.py:204-214 |
| Freeze flag | Solar gain snapshot active AND `solar_gain.freeze_integral` | climate.py:499-502 |

The options the PID reads are `OPTIONS_DEFAULTS` overlaid with the entry's stored options (__init__.py:29).

For an area PID, setpoint and room temperature come from the area's climate entity (or its `sensor_temperature_id` override sensor) (area.py:67-129).

### Behaviour (step by step, in time order)

**A. Creation**

1. `PID.from_config` builds the controller with the heating curve, heating system and `PidConfig` (pid.py:51-54).
2. The constructor sets `_integral = 0.0`, `_last_error = None`, `_raw_derivative = 0.0`, `_last_temperature = None`, `_last_derivative_updated = None`, then calls `reset()` (pid.py:33-49).

**B. Start-up of the main climate entity** (climate.py:123-150)

1. The previous climate state is restored (climate.py:128, climate.py:169-212). If no target temperature was saved, the code sets `self.pid.control_setpoint = self.min_temp` and the target to `min_temp` (climate.py:176-180, climate.py:204-207). See "Not found / unclear" about `control_setpoint`.
2. `pid.async_added_to_hass` is called (climate.py:131). It opens a Home Assistant store named `sat.pid.<entity_id>.<device_id>` (pid.py:141-145). If data exists, it restores `_last_error`, `_last_temperature`, `_last_derivative_updated`, `_integral` and `_raw_derivative` (pid.py:147-155). So the integral survives a restart.
3. If Home Assistant is already running, the code updates the heating curves, registers the listeners, runs `control_pid()` once and schedules the heating loop (climate.py:133-138). Otherwise the same four actions run on the `EVENT_HOMEASSISTANT_STARTED` event (climate.py:139-144).
4. The listener registration starts the periodic jobs: the heating loop every 5 s (climate.py:706-710) and `control_pid` every `PID_UPDATE_INTERVAL` = 60 s (climate.py:712-716, pid.py:20).
5. The area PIDs are added after that (climate.py:148). They follow the same pattern: restore from the store, run `control_pid()` once (now or at the start event), then every 60 s on their own timer (area.py:177-190).

**C. Every PID tick (every 60 s, plus the single start-up call)** (climate.py:466-502)

1. Stale sensor check. If `sensor_max_value_age_seconds > 0` and the inside sensor state is older than that, the PID is reset and the tick ends, without an update (climate.py:468-478, helpers.py:59-69). The age is measured from the state's `last_updated` (helpers.py:61).
2. If no `TemperatureState` can be built (sensor missing, target or current temperature missing), the tick ends (climate.py:480-482, climate.py:294-306).
3. The solar gain controller is updated (climate.py:484-494). Its "active" flag is forced to false unless solar gain compensation is enabled and HVAC mode is HEAT (solar_gain.py:52-54), and also when the sun elevation is unknown (solar_gain.py:56-58). Otherwise it is a 600 s hold window started by a detected solar gain event (solar_gain.py:60-64, const.py:24). "Enabled" is the option `solar_gain_compensation` (entry_data.py:281), default False (const.py:132). So with default settings the snapshot is never active.
4. If HVAC mode is HEAT and the valves are open, the median error is recorded in the temperature history (climate.py:496-497). This feeds statistics, not the PID.
5. `pid.update(state, freeze_integral=...)` runs (climate.py:499-502). Inside `update` (pid.py:159-183):
   1. If the heating curve has no value, the update is skipped entirely (pid.py:161-163).
   2. The derivative is updated first (pid.py:165), see section D.
   3. The integral is updated unless `freeze_integral` is true (pid.py:167-168), see section E.
   4. `_last_error = state.error` and `_last_temperature = state.current` are stored (pid.py:170-171).
   5. The state is saved to the store and `SIGNAL_PID_UPDATED` is sent (pid.py:179-183, pid.py:239-251).

`control_pid` has no HVAC mode check, so the PID keeps updating while the climate is OFF (climate.py:466-502). Only the heating loop stops in OFF (climate.py:544-545).

The area tick is simpler: if a temperature state exists, `pid.update(temperature_state)` runs, always without `freeze_integral` (area.py:202-208). There is no stale-sensor reset for areas; a stale area sensor makes `sensor_state()` return `None`, so the tick is skipped (area.py:76-84, area.py:113-114, area.py:204-206).

**D. Derivative update** (pid.py:199-237), in the order the code checks:

1. First time (no `_last_temperature` or no `_last_derivative_updated`): store `state.last_changed` as the timestamp and stop (pid.py:201-203).
2. Error inside the deadband (`abs(error) <= DEADBAND`): store the timestamp and stop. `_raw_derivative` is NOT zeroed; it keeps its previous value (pid.py:205-207).
3. `temperature_delta = state.current - _last_temperature`. If it is exactly 0.0: store the timestamp and stop (pid.py:209-213).
4. `delta_time = state.last_changed - _last_derivative_updated` (seconds). If `delta_time <= 60`: stop, WITHOUT storing a new timestamp (pid.py:215-218).
5. `derivative = -temperature_delta / delta_time` (pid.py:220). The source is the measured room temperature, not the error, and the sign is negated: a rising room gives a negative derivative.
6. If `abs(derivative) >= 5.0`: `_raw_derivative` is set to the value clamped to [-5.0, +5.0], with no filtering, the timestamp is stored, and the function stops (pid.py:222-225, pid.py:19).
7. Otherwise a first-order low-pass filter is applied, then the result is clamped to ±5.0 (pid.py:227-230). The timestamp is stored (pid.py:232).

**E. Integral update** (pid.py:185-197)

1. If `abs(error) > DEADBAND` (0.1): the integral is set to 0.0 and the function stops (pid.py:187-189, const.py:16).
2. Otherwise (error within ±0.1): `_integral += ki * error * 60` (pid.py:191). The 60 is the constant `PID_UPDATE_INTERVAL`, not a measured time step (pid.py:20).
3. The integral is clamped to `[-heating_curve.value, +heating_curve.value]` (pid.py:192, helpers.py:140-150).

So the integral only accumulates while the room is within 0.1 degree of the setpoint, and it is wiped as soon as the error leaves that band. The derivative is the reverse: it is only recalculated while the error is outside the band.

**F. Between PID ticks**

`proportional`, `derivative` and `output` are properties: they are computed again each time they are read (pid.py:95-132). The stored error, integral and raw derivative only change on a tick. But the heating curve value is recomputed on every inside or outside sensor change event, on a target change and on an area climate change (climate.py:718-728, climate.py:767-773, climate.py:663, climate.py:796, climate.py:688-697). With automatic gains, Kp, Ki and Kd depend on the heating curve value (pid.py:67-93). So the output can change between PID ticks through the curve value and the gains. The heating loop reads `requested_setpoint`, and so `pid.output`, on its own 5 s timer, debounced by a 1 s delayed call (climate.py:706-710, climate.py:515-531, climate.py:540-555).

**G. Combination into the requested setpoint** (climate.py:369-394)

1. If the main heating curve has no value: return `MINIMUM_SETPOINT` (10.0) (climate.py:372-373, const.py:35).
2. `setpoint = pid.output` (climate.py:375).
3. ECO heating mode: return `clamp(round(setpoint, 1), 10.0, coordinator.maximum_setpoint)` (climate.py:377-379). This return comes before the area output, the overshoot cap and the solar gain offset, so ECO uses none of them (climate.py:381-392).
4. Otherwise (COMFORT, the default (const.py:183)):
   1. `setpoint = max(setpoint, areas.pids.output)` (climate.py:382-386).
   2. If an overshoot cap exists: `setpoint = min(setpoint, overshoot_cap)` (climate.py:383, climate.py:388-389).
   3. If solar gain is active and solar gain compensation is enabled: subtract `solar_gain.setpoint_offset_celsius` (default 2.0) (climate.py:391-392, entry_data.py:281, entry_data.py:285, const.py:135).
   4. Return `clamp(setpoint, 10.0, coordinator.maximum_setpoint)` (climate.py:394).

`coordinator.maximum_setpoint` is the configured maximum setpoint, or by default 50 for underfloor and 55 for other systems (coordinator/__init__.py:274-283, helpers.py:97-99, entry_data.py:226-239). The switch coordinator returns its minimum setpoint instead (coordinator/switch/__init__.py:42-43).

The ECO / COMFORT choice is only offered in the options when secondary rooms are configured (config_flow.py:604-610).

### Formulas

Variable names as in the code.

**Error** (temperature/state.py:15-18):
`error = round(setpoint - current, 3)`. Positive means the room is too cold.

**Heating curve** (heating_curve.py:27-39, types.py:116-118):
`calculate = 4 * (target_temperature - 20) + 0.03 * (outside_temperature - 20)^2 - 0.4 * (outside_temperature - 20)`
`value = round(base_offset + (coefficient / 4) * calculate, 1)`
`base_offset = 20` for UNDERFLOOR, `27.2` for every other heating system.
`coefficient` is `heating_curve_coefficient` (heating_curve.py:22-25).

**Automatic gains** (when `automatic_gains` is true) (pid.py:61-93):
- `automatic_gain_value = 4` for UNDERFLOOR, otherwise `3` (pid.py:70).
- `kp = round((heating_curve_coefficient * heating_curve.value) / automatic_gain_value, 6)`; `kp = 0.0` while the heating curve has no value (pid.py:67-71).
- `ki = round(kp / 8400, 6)` (pid.py:79-82).
- `kd = round(0.07 * 8400 * kp, 6)`, that is `588 * kp` (pid.py:90-93).

**Manual gains** (when `automatic_gains` is false):
`kp = float_value(proportional)`, `ki = float(integral)`, `kd = float(derivative)` (pid.py:64-65, pid.py:76-77, pid.py:87-88). `float_value` returns `None` for a non-finite or non-numeric value (helpers.py:110-119); a `None` kp gives P = 0 (pid.py:98-99).

**Proportional term** (pid.py:95-101):
`P = round(kp * _last_error, 3)`; 0.0 when kp or `_last_error` is `None`.

**Integral term** (pid.py:103-106, pid.py:185-192):
- if `abs(error) > DEADBAND`: `_integral = 0.0`
- else: `_integral = clamp(_integral + ki * error * PID_UPDATE_INTERVAL, -heating_curve.value, +heating_curve.value)`
- `I = round(_integral, 3)`

**Derivative term** (pid.py:108-119, pid.py:209-230):
- `temperature_delta = current - _last_temperature`
- `delta_time = last_changed - _last_derivative_updated` (s)
- `derivative = -temperature_delta / delta_time` (degrees per second)
- if `abs(derivative) >= DERIVATIVE_RAW_CAP`: `_raw_derivative = clamp(derivative, -5.0, 5.0)`
- else: `alpha = delta_time / (PID_UPDATE_INTERVAL + delta_time)`; `_raw_derivative = clamp(alpha * derivative + (1 - alpha) * _raw_derivative, -5.0, 5.0)`
- `D = round(kd * _raw_derivative, 3)`

**Output** (pid.py:126-132):
`output = round(heating_curve.value + P + I + D, 1)`; 0.0 when the heating curve has no value. The PID itself applies no upper or lower limit to the output; the limits are applied in `requested_setpoint` (climate.py:379, climate.py:394).

**Area aggregation** (area.py:264-301), over areas whose PID is available and that require heat:
- sort the outputs; `index = max(0, min(n - 1, int(n * 0.75)))`; `baseline = outputs[index]` (area.py:294-296)
- `areas.pids.output = round(min(outputs[-1], baseline + 5.0), 1)` (area.py:298-301)
- with no qualifying area: `MINIMUM_SETPOINT` (10.0) (area.py:291-292)
- An area requires heat when its valve position is >= 10 (if the climate reports the attribute `current_valve_position`), else when its PID is available and its output > `COLD_SETPOINT` (22) (area.py:29, area.py:137-143, area.py:165-175, const.py:34).
- Computed from the index formula (not stated in the code): with 1 to 4 qualifying areas, `int(n * 0.75)` equals `n - 1`, so the baseline is the highest output and the +5.0 headroom never limits. It can only limit from 5 areas upward (area.py:295).

**Overshoot cap** (area.py:303-333), per available area that requires heat and whose `error < -0.3`:
- `degrees_over = -error`; `cap = 22 + 10.0 - 4.0 * degrees_over`
- the minimum over all such areas, then raised to at least 22, rounded to 0.1. `None` when no area overshoots.

**Illustrative arithmetic (not from the code, computed from the formulas above).** Radiators, coefficient 2.0, target 20, outside 0: `calculate = 20.0`, `value = 37.2`, `kp = 24.8`, `ki = 0.002952`, `kd = 14582.4`. With error +0.5: P = 12.4, I = 0 (outside the band). If the room rose 0.1 degree over 600 s from `_raw_derivative = 0`: alpha = 600/660, `_raw_derivative = -0.0001515`, D = -2.209. Output = round(37.2 + 12.4 + 0 - 2.209, 1) = 47.4. Inside the band with error +0.05, each tick adds `0.002952 * 0.05 * 60 = 0.0089` degree to the integral.

### Constants and defaults

| Name | Value | Unit | Citation |
|---|---|---|---|
| `PID_UPDATE_INTERVAL` | 60 | s | pid.py:20 |
| `DERIVATIVE_RAW_CAP` | 5.0 | degrees per s (raw derivative) | pid.py:19 |
| `DEADBAND` | 0.1 | degrees (same unit as the room sensor) | const.py:16 |
| Automatic gain divisor, underfloor | 4 | none | pid.py:70 |
| Automatic gain divisor, other systems | 3 | none | pid.py:70 |
| Ki divisor | 8400 | not stated in the code | pid.py:82 |
| Kd factor | 0.07 * 8400 = 588 | not stated in the code | pid.py:93 |
| `CONF_AUTOMATIC_GAINS` default | True | bool | const.py:120 |
| `CONF_AUTOMATIC_GAINS_VALUE` default | 2.0 | none (not read by the PID, see unclear) | const.py:121 |
| `CONF_HEATING_CURVE_COEFFICIENT` default | 2.0 | none | const.py:182 |
| Heating curve coefficient range in options | 0.1 to 12, step 0.1 | none | config_flow.py:616-618 |
| `CONF_PROPORTIONAL` default (manual) | "45" | string, parsed to float | const.py:113 |
| `CONF_INTEGRAL` default (manual) | "0" | string, parsed to float | const.py:114 |
| `CONF_DERIVATIVE` default (manual) | "6000" | string, parsed to float | const.py:115 |
| `CONF_HEATING_SYSTEM` default in options | RADIATORS | enum | const.py:184 |
| Heating curve `base_offset`, underfloor | 20 | degrees | types.py:116-118 |
| Heating curve `base_offset`, other systems | 27.2 | degrees | types.py:116-118 |
| `CONF_SENSOR_MAX_VALUE_AGE` default | "06:00:00" (21600) | s | const.py:170, entry_data.py:199-201, helpers.py:72-79 |
| `CONF_SOLAR_GAIN_FREEZE_INTEGRAL` default | True (only has effect when solar gain compensation is enabled, which is off by default) | bool | const.py:136, const.py:132, solar_gain.py:52-54 |
| `CONF_SOLAR_GAIN_COMPENSATION` default | False | bool | const.py:132 |
| `CONF_SOLAR_GAIN_SETPOINT_OFFSET_CELSIUS` default | 2.0 | degrees | const.py:135 |
| `SOLAR_GAIN_HOLD_SECONDS` | 600.0 | s | const.py:24 |
| `CONF_HEATING_MODE` default | COMFORT | enum | const.py:183 |
| `MINIMUM_SETPOINT` | 10.0 | degrees | const.py:35 |
| `COLD_SETPOINT` | 22 | degrees | const.py:34 |
| Default maximum setpoint | 50 underfloor, 55 others | degrees | helpers.py:97-99 |
| Area percentile | 0.75 | fraction | area.py:267 |
| Area headroom | 5.0 | degrees | area.py:267 |
| Area `OVERSHOOT_MARGIN` | 0.3 | degrees | area.py:33 |
| Area `COOLING_SLOPE` | 4.0 | degrees per degree | area.py:32 |
| Area `COOLING_HEADROOM` | 10.0 | degrees | area.py:34 |
| Area valve threshold for "requires heat" | 10.0 | not stated in the code (compared with attribute `current_valve_position`) | area.py:29, area.py:137-143, area.py:168-170 |
| Heating loop interval | 5 | s | climate.py:706-710 |
| Heating loop debounce delay | 1 | s | climate.py:530 |
| PID store version | 1 | none | pid.py:22 |

### State kept between passes

| State | Meaning | Persisted | Cleared by `reset()` | Citation |
|---|---|---|---|---|
| `_integral` | Accumulated integral (already multiplied by ki) | yes | yes (to 0.0) | pid.py:38, pid.py:136, pid.py:244 |
| `_last_error` | Error of the last tick, used by P and `available` | yes | no | pid.py:39, pid.py:170, pid.py:245 |
| `_raw_derivative` | Filtered temperature slope (degrees per s, negated) | yes | yes (to 0.0) | pid.py:41, pid.py:137, pid.py:246 |
| `_last_temperature` | Room temperature at the last tick | yes | no | pid.py:42, pid.py:171, pid.py:247 |
| `_last_derivative_updated` | Timestamp used as start of `delta_time` | yes | no | pid.py:43, pid.py:232, pid.py:248 |

The store is written after every successful update (pid.py:179-181), and read once when the entity is added (pid.py:147-155). `reset()` clears only the integral and the raw derivative (pid.py:134-137).

`available` is true when `_last_error` is set and the heating curve has a value (pid.py:56-59). Because `_last_error` is restored from the store (pid.py:150), a PID can be available before its first tick after a restart.

**When `reset()` is called:**

| Trigger | PIDs reset | Citation |
|---|---|---|
| Construction | own PID | pid.py:49 |
| Service `sat.reset_integral` | main PID and all area PIDs | climate.py:217-222, services.py:31-46 |
| HVAC mode set to HEAT or OFF, also when the mode was already the same (no equality check) | main PID and all area PIDs | climate.py:570-583, climate.py:461-464 |
| Target temperature changed (no reset if the value is equal) | main PID and all area PIDs | climate.py:641-660 |
| Preset change (it sets a new target through the same function) | main PID and all area PIDs, if the target changes | climate.py:604-624, climate.py:643-644 |
| Window open for the minimum time (preset ACTIVITY), and window closed again (old target restored) | main PID and all area PIDs, if the target changes | climate.py:808-819, climate.py:830-832 |
| Connected thermostat state or target changed (`cascade=False`) | main PID and all area PIDs, if the target changes | climate.py:750-765 |
| Inside sensor older than `sensor_max_value_age` at a tick | main PID only; that tick skips the update | climate.py:468-478 |
| An area climate state event whose target differs from the stored room target (see below) | that area's PID | climate.py:775-795, area.py:335-338 |

The area climate handler runs on every state change event of an area climate entity, so also on a change of its current temperature (climate.py:737-741). It returns early when the new target equals the value stored in `_rooms` (climate.py:788-789). The stored value is only updated when the room is new in `_rooms` or the preset is HOME (climate.py:791-792). The reset at climate.py:795 is not conditional on that update. So outside the HOME preset, once an area target differs from the stored value, every later state event of that climate resets that area's PID again, for as long as the target still differs from the stored value (climate.py:785-795). This stops when the target is set back to the stored value, or when a state event arrives while the preset is HOME, because the stored value is then updated (climate.py:791-792). For a room that is missing from `_rooms`, the default of `.get(..., target_temperature)` equals the new target, so the handler returns at climate.py:788-789: that room is never reset and never added there. See "unclear" item 13.

**When the integral is frozen:** on a main PID tick while the solar gain snapshot is active and `solar_gain.freeze_integral` is true (climate.py:499-502). Then `_update_integral` is not called at all (pid.py:167-168). The snapshot can only be active when solar gain compensation is enabled (solar_gain.py:52-54, entry_data.py:281), which is off by default (const.py:132). So with default settings the integral is never frozen. Area PIDs are never frozen (area.py:208).

### Outputs (to the boiler, to entities)

**To the boiler:** the PID sends nothing directly. Its output enters `requested_setpoint` (climate.py:375), which the heating loop places in a `HeatingDemand` (climate.py:547-555). `SatHeatingControl.update` turns that into the control setpoint, the maximum relative modulation and the heater on/off state that it sends to the coordinator (heating_control.py:197-240). The demand counts as heater ON only when HVAC mode is HEAT, the valves are open and `requested_setpoint > COLD_SETPOINT` (22) (heating_control.py:64-72). When the demand is OFF, the control setpoint becomes `MINIMUM_SETPOINT` (10.0) (heating_control.py:229-234). The heater state sent to the coordinator is ON when the resulting control setpoint is above 22 (heating_control.py:238). The heating loop does nothing when the outside temperature is unknown or HVAC mode is OFF (climate.py:544-545). How the control setpoint is derived from the requested setpoint (continuous and PWM paths) belongs to the heating control and PWM sections (heating_control.py:380-508).

**To entities:**

| Entity | Value | Citation |
|---|---|---|
| Sensor "PID" (main) and "PID (<area>)" per area | state = `pid.output` (degrees C); attributes `error` (= `last_error`), `proportional`, `integral`, `derivative`; available = `pid.available` | sensor.py:45-57, sensor.py:101-181 |
| Sensor "Requested Setpoint" | `climate.requested_setpoint`, refreshed on `SIGNAL_PID_UPDATED` | sensor.py:71-94 |
| Sensor heating curve | `climate.heating_curve.value` | sensor.py:290-291 |
| Climate attributes | `heating_curve`, `requested_setpoint` | climate.py:249-250 |
| Signal `SIGNAL_PID_UPDATED` | sent after every PID update with the entity id | pid.py:183, const.py:213 |

### Edge cases

1. **No heating curve value** (for example no outside temperature yet): `update` is skipped (pid.py:161-163), `output` is 0.0 (pid.py:129-130), automatic kp is 0.0 (pid.py:67-68), and `requested_setpoint` returns 10.0 (climate.py:372-373). The heating curve is only updated when both target and outside temperature exist (climate.py:690-694).
2. **Error exactly 0.1:** counts as inside the band for both terms. The integral uses `> DEADBAND` to reset (pid.py:187), the derivative uses `<= DEADBAND` to hold (pid.py:205).
3. **Error leaves the band:** the integral drops to 0 on the next tick (pid.py:187-189). There is no gradual decay.
4. **Frozen integral with a large error:** the zeroing in step E.1 is inside `_update_integral`, which is not called while frozen (pid.py:167-168, pid.py:185-189). The integral keeps its value even if the error is far outside the band.
5. **Integral clamp moves with the curve:** the limit is `±heating_curve.value` at the moment of the update (pid.py:192). A lower curve value later does not re-clamp the stored integral until the next in-band update.
6. **Derivative inside the band:** `_raw_derivative` is held, so D keeps contributing its last value while the room is within ±0.1 (pid.py:205-207, pid.py:108-114).
7. **Sensor unchanged:** a zero temperature delta stores the new timestamp and leaves `_raw_derivative` as it was (pid.py:211-213).
8. **Large slope:** at `abs(derivative) >= 5.0` degrees per second the filter is bypassed and the value is clamped to ±5.0 (pid.py:222-225).
9. **Stale inside sensor:** the PID is reset every tick while stale, and not updated (climate.py:470-478). A missing sensor state (`None`) is not treated as stale (helpers.py:66-67); the tick then ends because no temperature state can be built (climate.py:300-301, climate.py:480-482).
10. **Integral time step is fixed:** the integral always uses 60 s, also for the extra start-up call (pid.py:191, climate.py:137, climate.py:143).
11. **Manual gains with non-numeric text:** `ki` and `kd` use `float()` directly (pid.py:77, pid.py:88), and `PidConfig` already applies `float()` to all three options (entry_data.py:207-209). The code shows no guard for a value that fails to parse there.
12. **Thermal comfort on:** the error is computed against the Summer Simmer index instead of the raw sensor temperature (climate.py:276-277, climate.py:304-311).
13. **HVAC mode OFF:** the PID keeps ticking and its state keeps changing (climate.py:466-502, climate.py:712-716), but no demand is sent (climate.py:544-545).
14. **Overshoot cap at its floor:** the cap is never below 22 (area.py:331), which equals `COLD_SETPOINT` (const.py:34). If the cap pulls the requested setpoint down to 22, the demand is heater OFF, because ON needs `requested_setpoint > 22` (heating_control.py:72). The control setpoint then becomes 10.0 (heating_control.py:229-234).

### Not found in the code / unclear

1. **`automatic_gains_value` is never read.** It is stored in `PidConfig` (entry_data.py:49, entry_data.py:212), default 2.0 (const.py:121). A search for `automatic_gains_value` and `CONF_AUTOMATIC_GAINS_VALUE` in the Python files finds only its definition and these two places. Automatic Kp uses `heating_curve_coefficient` instead (pid.py:71).
2. **`pid_controller_version`** is written by the config migration (__init__.py:164-165). No Python code reads it in the files searched.
3. **`self.pid.control_setpoint = self.min_temp`** (climate.py:178, climate.py:205). The `PID` class defines no `control_setpoint` (pid.py:30-251). A search for `control_setpoint` together with `pid` finds only these two assignments, so nothing in the PID reads it.
4. **Derivative timing mismatch.** When `delta_time <= 60` the function returns without moving `_last_derivative_updated` (pid.py:217-218), but `_last_temperature` is still overwritten at the end of every update (pid.py:171). So the temperature step of that tick is not carried into a later derivative calculation, while the time window keeps growing. The code does not show whether this is intended.
5. **Derivative time base.** `delta_time` is taken from the sensor's `last_changed` (pid.py:202, pid.py:215), while `temperature_delta` is the change since the previous PID tick (pid.py:171, pid.py:209). The stale check uses `last_updated` instead (helpers.py:61). The code gives no explanation of how these windows are meant to relate.
6. **Integral only inside the deadband.** The code accumulates the integral only for `abs(error) <= 0.1` and zeroes it otherwise (pid.py:187-191). This is what the code does. The design reason is not stated in the code.
7. **The service `reset_integral` is registered twice**, once by the climate entity (climate.py:222) and once in services.py with an `entity_id` schema (services.py:41-46), called from the entry setup after the platforms (__init__.py:52-53). Both reset the main PID and the area PIDs. Which registration is active at run time is not visible from these files.
8. **`Area.async_outside_entity_changed`** passes the area's indoor temperature as the outside temperature to the heating curve (area.py:198-200). The search found no caller. Area curves are updated through `Areas.heating_curves.update` with the real outside temperature instead (area.py:259-262, climate.py:696).
9. **Unit of `DEADBAND`.** The code applies no unit conversion in pid.py or temperature/state.py. The value is compared directly with `setpoint - current` (pid.py:187, temperature/state.py:18). It is degrees in the room sensor's unit; the code does not state Celsius explicitly.
10. **Units of Ki and Kd.** The code does not name units. From the formulas, `ki * error * 60` is added to a degree value (pid.py:191) and `kd * _raw_derivative` uses a slope in degrees per second (pid.py:114, pid.py:220). The origin of the constants 8400 and 0.07 is not explained in the code.
11. **No output limiting or anti-windup in the PID beyond the integral clamp.** The only limit inside pid.py is the integral clamp to ±curve value (pid.py:192) and the derivative clamp to ±5.0 (pid.py:223, pid.py:230). Clamping of the final setpoint happens only in `requested_setpoint` (climate.py:379, climate.py:394). No anti-windup tied to boiler saturation was found in pid.py.
12. **Where the gain settings are stored versus where the PID reads them.** The setup wizard writes `automatic_gains`, `proportional`, `integral` and `derivative` into `self.data` (config_flow.py:462, config_flow.py:479, config_flow.py:482), and the entry is created with `data=self.data` (config_flow.py:546-549). `PidConfig` reads all of them from the options (entry_data.py:207-212), which are `OPTIONS_DEFAULTS` overlaid with the stored entry options (__init__.py:29). The options flow has no automatic-gains field; it only shows P, I and D when `options[CONF_AUTOMATIC_GAINS]` is false (config_flow.py:620-623, config_flow.py:808-812). In the code read (including the migrations, __init__.py:103-198), nothing copies these keys from data to options. Whether a wizard choice of "no automatic gains" reaches the PID therefore cannot be confirmed from the code.
13. **Area PID reset on climate events.** As described under "When `reset()` is called", outside the HOME preset an area PID can be reset on every state event of its climate after a target change, while that target still differs from the stored room value, and a room missing from `_rooms` is never reset there (climate.py:785-795). The code does not state whether this is intended.
14. **Unused area helpers.** `COMFORT_BAND = 0.1` (area.py:31) and the `Area.weight` property (area.py:148-163) are defined, but a search of the Python files found no use of either.

## 3. Heating curve and setpoint selection

Source: Alexwijn/SAT, branch `releases/thermo-nova`, commit 7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb. All paths are relative to `custom_components/sat`. Every statement below cites the lines it was read from. Anything the code does not show is listed in the last subsection, not filled in.

Audit note: every citation in this section was re-opened against the local copy and checked against the statement it supports. Corrections and additions from that audit are already merged into the text.

### Purpose

SAT works out a boiler flow temperature (the "control setpoint") in three layers:

1. **Heating curve.** A weather-compensation value computed from the room target temperature and the outside temperature (heating_curve.py:28-39).
2. **Requested setpoint.** The curve value plus the PID correction, optionally combined with secondary rooms and solar gain, then clamped (climate.py:369-394, pid.py:126-132).
3. **Control setpoint.** What is actually sent to the boiler. Heating control derives it from the requested setpoint, the operating mode (continuous or PWM) and the live boiler state (heating_control.py:197-240).

This section covers layers 1 and 2 and the continuous-mode part of layer 3. The PWM and modulation-suppression override of layer 3 is only summarised here (see "Cross-reference: PWM override") because it belongs to its own section.

### Inputs

| Input | Where it comes from | Citation |
|---|---|---|
| Room target temperature | `SatClimate._target_temperature`, set by the user, a preset, a linked thermostat, or restored at start-up | climate.py:265-268, climate.py:557-568, climate.py:641-647, climate.py:604-624, climate.py:750-765, climate.py:173-182 |
| Room (inside) temperature | State of the configured `inside_sensor_entity_id`, as a float. If thermal comfort is on, it is replaced by the Summer Simmer index computed with humidity | climate.py:270-279, climate.py:669-686 |
| Outside temperature | First usable entity from the configured `outside_sensor_entity_id` list (one or more `sensor` or `weather` entities) | climate.py:317-343, config_flow.py:386-391 |
| Heating system | Config entry data `heating_system`: radiators, heat pump or underfloor. `UNKNOWN` when not set | entry_data.py:119-124, config_flow.py:401-433, types.py:110-114 |
| Heating curve coefficient | Option `heating_curve_coefficient`, read once when the curve object is built | entry_data.py:213, heating_curve.py:13-16, heating_curve.py:22-25, const.py:182, config_flow.py:616-618 |
| PID terms (P, I, D) | PID controller fed with the room temperature error | pid.py:95-132, temperature/state.py:15-18 |
| Secondary rooms | Per-room PID and heating curve (`Areas`) for climates configured as `secondary_climates`. Each room curve uses the same heating system and coefficient as the main curve | area.py:37-50, area.py:218-220, heating_curve.py:22-25, const.py:56, entry_data.py:161-163 |
| Heating mode | Option `heating_mode`: `comfort` (default) or `eco`. The options form shows the choice only when secondary rooms are configured | entry_data.py:257-259, const.py:183, types.py:121-123, config_flow.py:604-610 |
| Solar gain state | `SolarGainController` snapshot (active flag) and the configured offset | climate.py:485-494, climate.py:391-392, entry_data.py:278-286 |
| Maximum setpoint | Option `maximum_setpoint`, else data `maximum_setpoint`, else a default per heating system. The requested setpoint is clamped with `coordinator.maximum_setpoint`; the PWM-disable check and the PWM flame-off hold use `config.limits.maximum_setpoint` directly | entry_data.py:226-239, helpers.py:97-99, coordinator/__init__.py:274-283, climate.py:379, climate.py:394, heating_control.py:203, heating_control.py:398 |
| Boiler flow temperature | `coordinator.boiler_temperature` (device dependent, `None` in the base class) | coordinator/__init__.py:183-186, heating_control.py:456 |
| Flame state | `coordinator.flame_active`. The base class returns `self.active` (the device "actively heating" flag); device backends override it | coordinator/__init__.py:168-171, coordinator/mqtt/opentherm.py:63-65, heating_control.py:476 |
| Valve state of linked radiators | `valves_open` property | climate.py:414-455 |
| HVAC mode | `heat` or `off` | climate.py:100, climate.py:350-356, climate.py:570-581 |

Options are the stored options merged over `OPTIONS_DEFAULTS`, so every option above has a default even when the user never opened the options form (__init__.py:29, const.py:111).

**Outside temperature selection rule.** The configured entities are sorted so that entities whose id contains the text `sensor` come first (climate.py:320-321). For each entity in that order: skip it if it has no state (climate.py:324-326), if it is older than `sensor_max_value_age` (climate.py:328-332), or if it is `unknown` or `unavailable` (climate.py:334-335). An entity whose id contains `sensor` returns its state as a float (climate.py:337-338). An entity whose id contains `weather` returns its `temperature` attribute (climate.py:340-341). If none qualifies, the outside temperature is `None` (climate.py:343).

**Staleness rule.** A state is stale when it is older than `max_age_seconds`. A `max_age_seconds` of 0 or less disables the check (helpers.py:64-69). The default maximum age is `06:00:00` (const.py:170), converted to seconds (entry_data.py:199-201, helpers.py:72-79).

### Behaviour (step by step, in time order)

#### At start-up

1. The climate entity creates one `HeatingCurve` from config (heating system and coefficient) and one `PID` that holds a reference to that curve (climate.py:113-114, heating_curve.py:22-25, pid.py:51-54). The secondary rooms are created just before, each with its own curve and PID (climate.py:112, area.py:49-50). The curve value starts as `None` (heating_curve.py:14).
2. Previous state is restored. Heating control restores the last control setpoint from the `setpoint` attribute, and the last relative modulation value and PWM state (climate.py:173-174, heating_control.py:181-190). The target temperature is restored from the saved `temperature` attribute. If there is none, it falls back to `self.min_temp` (climate.py:176-182, climate.py:204-207). HVAC mode is restored; with no saved state it becomes `off` (climate.py:184-185, climate.py:209-210).
3. PID state (integral, last error, raw derivative, last temperature, last derivative time) is loaded from storage (climate.py:131, pid.py:141-157). The heating curve value itself is not stored. It is recomputed in the next step.
4. If Home Assistant is already running, the code calls in sequence: compute heating curves, register event listeners, run one PID update, schedule one control loop (climate.py:133-138). If Home Assistant is still starting, the same four actions are registered as separate one-shot listeners on the "Home Assistant started" event, in that order (climate.py:139-144).
5. Only after that are the secondary rooms attached to Home Assistant. Each room loads its stored PID state, runs one PID update (now, or at the "started" event) and starts its own 60 s PID timer (climate.py:148, area.py:177-190).

#### When the heating curve is recalculated

The curve is recalculated on events, not on the periodic control tick or the PID tick (climate.py:706-716 call only `schedule_heating_control_loop` and `control_pid`):

- at start-up (climate.py:134, climate.py:140);
- when the inside or outside sensor entity changes state (climate.py:718-728, climate.py:767-773);
- when the target temperature changes to a different value (climate.py:641-664);
- when a secondary room's climate changes its target (climate.py:775-796).

Each recalculation (climate.py:688-697):

1. Reads the target temperature and the outside temperature. If either is `None`, it stops and the old curve value stays (climate.py:690-694).
2. Updates the curve of every secondary room that has a target, using that room's own target and the shared outside temperature (climate.py:696, area.py:259-262).
3. Updates the main curve with the main target and the outside temperature (climate.py:697, heating_curve.py:36-39).

#### Periodic timers

- **Control loop:** every 5 s a timer calls `schedule_heating_control_loop` (climate.py:706-710). That schedules one run after 1 s, unless one is already pending (climate.py:526-531). A forced call runs it at once (climate.py:517-524). A target or HVAC mode change also schedules a run (climate.py:601, climate.py:664).
- **PID update:** every `PID_UPDATE_INTERVAL` = 60 s (climate.py:712-716, pid.py:20). Each secondary room has its own 60 s PID timer (area.py:188-190).

#### PID update pass (every 60 s)

1. If the inside sensor is stale, the main PID is reset and the pass ends (climate.py:468-478). Reset clears the integral and raw derivative (pid.py:134-137).
2. If there is no temperature state (no inside sensor, no target, or no current temperature), the pass ends (climate.py:480-482, climate.py:294-315).
3. The solar gain snapshot is updated (climate.py:484-494).
4. In HEAT mode with valves open, the median of the main and room errors is recorded in a rolling history (climate.py:496-497). This feeds the "Heating Curve Recommendation" sensor (see Outputs).
5. The PID is updated with the room error `setpoint - current` (climate.py:499-502, temperature/state.py:15-18). The integral can be frozen while solar gain is active and `solar_gain_freeze_integral` is on (climate.py:501). If the curve value is `None`, the PID skips the update (pid.py:161-163).

PID internals belong to the PID section. In short: output = curve + P + I + D, rounded to 0.1 (pid.py:126-132). With automatic gains (default on, const.py:120), `kp = heating_curve_coefficient * curve_value / 4` for underfloor and `/ 3` otherwise, so the curve coefficient also scales the PID gain (pid.py:67-71). The integral is reset to 0 whenever |error| > `DEADBAND` = 0.1 (pid.py:187-189, const.py:16), and is clamped to plus or minus the curve value (pid.py:191-192).

A room PID pass skips when the room has no temperature state, and it does not have the stale-reset step of the main PID (area.py:202-208). The room sensor state returns `None` when stale (area.py:76-84).

#### Control loop pass (about every 5 s)

1. **Guard.** If the outside temperature is `None` or the HVAC mode is `off`, the pass returns at once. Heating control is not called and nothing is sent to the boiler (climate.py:540-545).
2. **Requested setpoint** (climate.py:369-394):
   1. If the curve value is `None`, the requested setpoint is `MINIMUM_SETPOINT` = 10 °C (climate.py:372-373, const.py:35).
   2. Start from `pid.output` (climate.py:375).
   3. **ECO mode:** round to 0.1, clamp to [10, maximum setpoint], return. Secondary rooms and solar gain are ignored (climate.py:377-379).
   4. **COMFORT mode:** take the higher of the main output and the secondary-room output (climate.py:381-386). If any room overshoots, take the lower of that and the overshoot cap (climate.py:383, climate.py:388-389). If solar gain is active and enabled, subtract the solar offset (climate.py:391-392). Clamp to [10, maximum setpoint] (climate.py:394).
3. **Heating demand.** A `HeatingDemand` is built with HVAC mode, valve state, timestamp, requested setpoint and outside temperature (climate.py:547-555). Demand is "heater ON" only if the mode is `heat`, the valves are open and the requested setpoint is above `COLD_SETPOINT` = 22 °C (heating_control.py:64-72, const.py:34).
4. **Heating control** (heating_control.py:197-240):
   - **Heater ON:** if the requested setpoint is at or above `config.limits.maximum_setpoint`, PWM is disabled (heating_control.py:203-204). The PWM enable and disable checks and the PWM update run (heating_control.py:206-220). The relative modulation value is computed (heating_control.py:222). Then, in PWM mode, the PWM override runs (heating_control.py:224-225). In continuous mode, the continuous rule below runs (heating_control.py:227-228).
   - **Heater OFF** (HEAT mode, but valves closed or requested setpoint ≤ 22 °C): PWM is disabled, PWM guards are reset, the control setpoint becomes `MINIMUM_SETPOINT` = 10 °C and the relative modulation becomes the configured maximum (heating_control.py:229-234).
5. **Send to the boiler**, every pass that reaches heating control (heating_control.py:236-240):
   1. the control setpoint;
   2. the maximum relative modulation;
   3. the CH enable state: ON if control setpoint > 22 °C, otherwise OFF;
   4. the device-specific loop hook.

**Relative modulation value (heater ON).** If the device does not support modulation management, the value is `None` (heating_control.py:512-515). Otherwise it is 0 while PWM is enabled (state `OFF`) and the configured maximum in every other state; for a Geminox boiler it is at least 10 (heating_control.py:517-523, heating_control.py:133-145, const.py:37).

#### Continuous-mode control setpoint rule

This rule is used when PWM is not enabled or PWM is idle (heating_control.py:147-152). In plain terms, the setpoint normally follows the request. One exception: while the flame burns and the boiler is already hotter than the request, the setpoint is not allowed to drop more than `flow_setpoint_offset_celsius` below the actual boiler temperature. On a falling request it is also not allowed to rise above its previous value (heating_control.py:450-508).

Step by step:

1. Remember the previous control setpoint and read the offset and the boiler flow temperature (heating_control.py:454-456).
2. If the boiler temperature is unknown: control setpoint = requested (heating_control.py:458-465).
3. If boiler temperature ≤ requested: control setpoint = requested (heating_control.py:467-474).
4. If the flame is off: control setpoint = requested (heating_control.py:476-483).
5. Otherwise (flame on, boiler hotter than the request): `minimum_allowed_setpoint = boiler_temperature - offset` (heating_control.py:485).
   - Request rising (requested > previous): control = max(requested, minimum_allowed) (heating_control.py:486-487).
   - Request falling or equal: control = max(requested, min(minimum_allowed, previous)) (heating_control.py:488-489).

The log messages name the two outcomes "clamped to offset minimum" and "held to avoid increase despite offset minimum" (heating_control.py:491-503).

"Previous" is the stored `_control_setpoint`: 10 °C on a fresh start or the restored value (heating_control.py:89, heating_control.py:186-187), 10 °C after any heater-OFF pass (heating_control.py:233), or whatever the PWM override last set (heating_control.py:380-448).

#### Cross-reference: PWM override (summary only)

In PWM mode the override function returns without changing the control setpoint in three cases: hot water is active, the requested setpoint is ≤ 22 °C, or `overshoot_protection` is off (heating_control.py:385-386). `overshoot_protection` is read from the config entry data (entry_data.py:149-151). In those cases the value from the previous pass is sent again (heating_control.py:236). With overshoot protection on, the function can set: 10 °C when PWM status is OFF (heating_control.py:388-390), a flame-off hold value of `return_temperature + flame_off_setpoint_offset_celsius` clamped to [22, `limits.maximum_setpoint`] (heating_control.py:392-408), or `flow_temperature - modulation_suppression_offset_celsius` after the suppression delay (heating_control.py:410-448). The PWM duty cycle also uses the heating-system `base_offset` (pwm.py:229-239). Details belong to the PWM section.

### Formulas

Variable names are as in the code.

**Unscaled curve** (heating_curve.py:28-30):

```
calculate(target_temperature, outside_temperature) =
    4 * (target_temperature - 20)
  + 0.03 * (outside_temperature - 20) ** 2
  - 0.4  * (outside_temperature - 20)
```

**Scaled curve value** (heating_curve.py:36-39):

```
value = round(base_offset + (coefficient / 4) * calculate(...), 1)
```

**Base offset per heating system** (types.py:116-118):

```
base_offset = 20    if heating_system == UNDERFLOOR
base_offset = 27.2  otherwise (RADIATORS, HEAT_PUMP and UNKNOWN)
```

**PID output** (pid.py:126-132): `output = round(heating_curve_value + proportional + integral + derivative, 1)`. It is `0.0` while the curve value is `None`.

**Requested setpoint** (climate.py:369-394), with `clamp(v, lo, hi) = max(lo, min(v, hi))` (helpers.py:140-145):

```
ECO:     clamp(round(pid.output, 1), MINIMUM_SETPOINT, coordinator.maximum_setpoint)
COMFORT: s = pid.output
         s = max(s, areas.pids.output)        if not None
         s = min(s, areas.pids.overshoot_cap) if not None
         s = s - solar_gain.setpoint_offset_celsius   if solar gain active and enabled
         clamp(s, MINIMUM_SETPOINT, coordinator.maximum_setpoint)
```

`areas.pids.output` never returns `None`: with no qualifying room (also with no rooms configured) it returns `MINIMUM_SETPOINT` (area.py:272-292). So the `max` step always runs, and with no rooms it cannot change the result, because the final clamp already has 10 as floor (climate.py:385-386, climate.py:394).

**Secondary-room output** (area.py:272-301). Only rooms whose PID is available and that require heat are considered (area.py:278-280). With none, the result is `MINIMUM_SETPOINT` (area.py:291-292). Otherwise:

```
outputs sorted ascending
index    = max(0, min(n - 1, int(n * 0.75)))
baseline = outputs[index]
chosen   = min(max(outputs), baseline + 5.0)
result   = round(chosen, 1)
```

The percentile 0.75 and headroom 5.0 are constructor defaults (area.py:267-270). A room requires heat if its valve position is ≥ 10, or, without a valve position, if its PID is available and its output is > 22 °C (area.py:165-175). A room PID is available once it has a last error and its curve has a value (pid.py:56-59).

**Overshoot cap** (area.py:303-333). For each heat-requiring room whose error < -0.3 (more than 0.3 °C above target):

```
cap_room = COLD_SETPOINT + COOLING_HEADROOM - COOLING_SLOPE * degrees_over
         = 22 + 10 - 4 * degrees_over
cap = max(22, min(cap_room over rooms)), rounded to 0.1
```

The constants are at area.py:31-34. With no overshooting room the cap is `None` (area.py:324-325).

Derived from the formula above (computed, not stated in the code): at 2.5 °C or more above target in any heat-requiring room, `cap_room` ≤ 22, so the cap is 22 (area.py:322, area.py:331). The COMFORT requested setpoint is then at most 22 °C (climate.py:388-389), which is heater OFF (heating_control.py:72).

**Maximum setpoint used by the clamp** (coordinator/__init__.py:274-283): `limits.maximum_setpoint`, or the per-system default if that is `None`. The default is 50 °C for underfloor and 55 °C otherwise (helpers.py:97-99). The switch backend overrides this property and returns its minimum setpoint instead (coordinator/switch/__init__.py:41-43).

**Effective minimum setpoint of the device** (coordinator/__init__.py:269-272): `max(limits.minimum_setpoint, MINIMUM_SETPOINT)`. It is never used as the floor of the requested or control setpoint; that floor is the constant `MINIMUM_SETPOINT` (climate.py:379, climate.py:394). It does reach the requested setpoint indirectly in switch mode, as the ceiling (see Edge cases).

**Worked examples** (derived from heating_curve.py:30 and heating_curve.py:39 with coefficient 2.0; computed, not taken from the code):

| System | Target °C | Outside °C | calculate() | Curve value °C |
|---|---|---|---|---|
| Radiators (27.2) | 20 | 10 | 7.0 | 30.7 |
| Radiators (27.2) | 20 | 0 | 20.0 | 37.2 |
| Radiators (27.2) | 20 | -10 | 39.0 | 46.7 |
| Radiators (27.2) | 21 | 0 | 24.0 | 39.2 |
| Underfloor (20) | 20 | 10 | 7.0 | 23.5 |
| Underfloor (20) | 20 | 0 | 20.0 | 30.0 |
| Underfloor (20) | 20 | -10 | 39.0 | 39.5 |
| Underfloor (20) | 21 | 0 | 24.0 | 32.0 |

At target 20 °C and outside 20 °C, `calculate()` is 0, so the curve value equals the base offset (heating_curve.py:30, heating_curve.py:39). Each 1 °C of target adds `coefficient` °C to the curve value, because 4 × (coefficient / 4) = coefficient (heating_curve.py:30, heating_curve.py:39). The quadratic term means the curve value starts rising again above an outside temperature of about 26.7 °C (20 + 0.4 / 0.06). That follows from the formula at heating_curve.py:30. It is a mathematical observation, not a documented intent.

### Constants and defaults

| Name | Value | Unit | Citation |
|---|---|---|---|
| Curve target gain | 4 | none in code (dimensionless if the result is read as °C) | heating_curve.py:30 |
| Curve quadratic factor | 0.03 | none in code (per °C if the result is read as °C) | heating_curve.py:30 |
| Curve linear factor | -0.4 | none in code (dimensionless if the result is read as °C) | heating_curve.py:30 |
| Curve reference temperature | 20 | °C | heating_curve.py:30 |
| Coefficient divisor | 4 | | heating_curve.py:39 |
| Curve rounding | 0.1 | °C | heating_curve.py:39 |
| `base_offset` underfloor | 20 | °C | types.py:116-118 |
| `base_offset` radiators, heat pump, unknown | 27.2 | °C | types.py:116-118 |
| `heating_curve_coefficient` default | 2.0 | | const.py:182 |
| `heating_curve_coefficient` UI range | 0.1 to 12, step 0.1 | | config_flow.py:616-618 |
| `heating_system` pre-selected in the setup form | radiators | | config_flow.py:411-413, const.py:184 |
| `heating_system` when not in the entry data | unknown | | entry_data.py:119-124 |
| `heating_mode` default | comfort | | const.py:183 |
| `automatic_gains` default | on | | const.py:120 |
| `MINIMUM_SETPOINT` | 10.0 | °C | const.py:35 |
| `COLD_SETPOINT` | 22 | °C | const.py:34 |
| `MAXIMUM_SETPOINT` | 65.0 | °C | const.py:36 (defined; no use found anywhere) |
| Default maximum setpoint, underfloor | 50 | °C | helpers.py:97-99 |
| Default maximum setpoint, other systems | 55 | °C | helpers.py:97-99 |
| `maximum_setpoint` UI range | 10 to 100, step 1 | °C | config_flow.py:612-614 |
| `minimum_setpoint` option default | 10 | °C | const.py:147 |
| `minimum_setpoint` default in the switch setup form | 50 (range 10 to 100) | °C | config_flow.py:302-304 |
| `flow_setpoint_offset_celsius` default | 2.0 | °C | const.py:154 |
| `flow_setpoint_offset_celsius` UI range | 0 to 10, step 0.1 | °C | config_flow.py:777-780 |
| `climate_valve_offset` default | 0 | °C | const.py:168 |
| `sensor_max_value_age` default | 06:00:00 | h:m:s | const.py:170 |
| `DEADBAND` | 0.1 | °C | const.py:16 |
| `PID_UPDATE_INTERVAL` | 60 | s | pid.py:20 |
| Control loop timer | 5 | s | climate.py:706-710 |
| Control loop debounce | 1 | s | climate.py:530 |
| Room output percentile | 0.75 | | area.py:267 |
| Room output headroom | 5.0 | °C | area.py:267 |
| Room requires-heat valve threshold | 10.0 | (valve position) | area.py:169-170 |
| `OVERSHOOT_MARGIN` (rooms) | 0.3 | °C | area.py:33 |
| `COOLING_HEADROOM` | 10.0 | °C | area.py:34 |
| `COOLING_SLOPE` | 4.0 | °C per °C | area.py:32 |
| `solar_gain_setpoint_offset_celsius` default | 2.0 | °C | const.py:135 |
| `solar_gain_compensation` default | off | | const.py:132 |
| `thermal_comfort` default | off | | const.py:131 |
| Initial control setpoint | 10.0 | °C | heating_control.py:89 |
| Curve recommendation: minimum daily samples | 6 | samples | sensor.py:300 |
| Curve recommendation: error threshold | 0.2 (`DEADBAND * 2`) | °C | sensor.py:301 |
| Error history windows (recent, daily) | 4 h, 24 h | | temperature/history.py:10-11 |

### State kept between passes

| State | Kept where | Persisted? | Citation |
|---|---|---|---|
| Heating curve value (`_value`) | `HeatingCurve` | No, recomputed on start-up and on events | heating_curve.py:14, heating_curve.py:36-39, climate.py:134 |
| Room target temperature | `SatClimate._target_temperature` | Yes, via the HA state attribute `temperature` | climate.py:90, climate.py:171, climate.py:176-182 |
| HVAC mode | `SatClimate._hvac_mode` | Yes, via HA state | climate.py:92, climate.py:184-185 |
| Previous control setpoint (used by the continuous rule) | `SatHeatingControl._control_setpoint` | Yes, via the `setpoint` attribute | heating_control.py:89, heating_control.py:186-187, climate.py:243, heating_control.py:454 |
| PID integral, last error, raw derivative, last temperature, last derivative time | `PID` | Yes, HA storage `sat.pid.<entity>.<device>`, saved after every PID update | pid.py:141-157, pid.py:179-181, pid.py:239-251 |
| Last requested setpoint and outside temperature | `SatHeatingControl` | No | heating_control.py:92-93, heating_control.py:199-200 |
| Flame-off hold setpoint (PWM path) | `SatHeatingControl._flame_off_hold_setpoint` | No | heating_control.py:95, heating_control.py:401, heating_control.py:439 |
| Solar gain snapshot | `SatClimate._solar_gain_snapshot` | No | climate.py:95, climate.py:485 |
| Room error history (for the curve recommendation) | `SatClimate._temperature_history`, samples older than 24 h are pruned | No (in-memory deque) | climate.py:118, climate.py:496-497, temperature/history.py:43-44, temperature/history.py:59-62, temperature/history.py:92-94 |
| Per-room curve and PID | `Area.heating_curve`, `Area.pid` | The PID is stored like the main PID | area.py:49-50, area.py:181 |

**What resets state.** Changing the target temperature to a different value resets the main PID and all room PIDs, then recomputes the curves; setting the same value does nothing (climate.py:643-644, climate.py:659-663, climate.py:461-464). Changing the HVAC mode resets the main and room PIDs only; it does not recompute the curves (climate.py:582-583, climate.py:461-464). A room target change resets that room's PID (climate.py:795). A stale inside sensor resets the main PID only (climate.py:477). The `reset_integral` service resets all PIDs; it is registered both by the climate entity and by services.py (climate.py:217-222, services.py:31-39). A PID reset clears only the integral and the raw derivative. The last error stays, so the P term is unchanged until the next update (pid.py:134-137, pid.py:95-101). `HeatingCurve.reset()` exists (heating_curve.py:32-34), but no caller was found (see "Not found").

### Outputs (to the boiler, to entities)

**To the boiler** (every control pass that reaches heating control, heating_control.py:236-240):

| What | Value | OTGW-over-MQTT command | Citation |
|---|---|---|---|
| Control setpoint | `_control_setpoint` | `CS=<value>`, then `PM=25` | heating_control.py:236, coordinator/mqtt/opentherm.py:195-197 |
| Max relative modulation | `_relative_modulation_value` | `MM=<value>` (for Immergas, `TP=11:12=<min(value,80)>` is sent first) | heating_control.py:237, coordinator/mqtt/opentherm.py:216-220 |
| CH enable | ON if control setpoint > 22 °C | `CH=1` or `CH=0` | heating_control.py:238, coordinator/mqtt/opentherm.py:211-212 |

The serial backend sends the same values through its API (`set_control_setpoint`, `set_ch_enable_bit`, `set_max_relative_mod`) (coordinator/serial/__init__.py:192-220). The ESPHome backend writes them to number and switch entities (coordinator/esphome/__init__.py:257-283). In simulation mode no MQTT command is published (coordinator/mqtt/__init__.py:140-141). Every published MQTT command is followed by a 0.2 s wait (coordinator/mqtt/__init__.py:134, coordinator/mqtt/__init__.py:143-147).

**Pushed to a linked thermostat:** if `push_setpoint_to_thermostat` is on, a target temperature change is also sent as a thermostat setpoint (climate.py:649-657). Over OTGW MQTT that is `TC=<value>` (coordinator/mqtt/opentherm.py:206-207). A change that comes from the linked thermostat itself is applied with `cascade=False`, so it is not pushed back (climate.py:765, climate.py:649).

**Maximum boiler setpoint:** not sent by the control loop. A separate number entity, created only when the backend supports maximum setpoint management, offers a range of 30 to 80 °C and calls `async_set_control_max_setpoint` when the user changes it (number.py:17-18, number.py:115-127). Over OTGW MQTT that sends `SH=<value>` (coordinator/mqtt/opentherm.py:224-225). The entity shows `maximum_setpoint_value`, which for MQTT OpenTherm is the boiler-reported maximum when present (number.py:111-113, coordinator/mqtt/opentherm.py:78-83).

**To Home Assistant entities and attributes:**

- Climate attributes `heating_curve`, `requested_setpoint`, `setpoint` (control setpoint), `outside_temperature`, `valves_open` and `solar_gain_active` (climate.py:234-263).
- Sensor "Requested Setpoint", refreshed on every PID update signal (sensor.py:71-98, pid.py:183).
- Sensor "Heating Curve", unavailable while the curve value is `None` (sensor.py:267-296).
- Sensor "Heating Curve Recommendation" (advisory only). It reads the 24 h window of recorded errors. With fewer than 6 samples it reports `INSUFFICIENT_SAMPLES`. If the daily median error is above 0.2 it reports `INCREASE`, below -0.2 `DECREASE`, otherwise `HOLD` (sensor.py:299-333, types.py:94-99). Because error = target - room (temperature/state.py:15-18), a positive median means the rooms stayed too cold. Its attributes include the recent (4 h) and daily statistics (sensor.py:335-356).

### Edge cases

- **No outside temperature.** The control loop returns before heating control. No setpoint, modulation or CH command is sent while the outside temperature is `None` (climate.py:544-545). The curve also keeps its last value, because the recalculation stops early (climate.py:693-694).
- **HVAC mode off.** The control loop returns before heating control (climate.py:544-545). The 10 °C / CH-off branch (heating_control.py:229-238) is not reached on this path.
- **Curve not yet computed.** The requested setpoint is 10 °C (climate.py:372-373) and the PID output is 0.0 (pid.py:129-130). That gives heater OFF, because 10 ≤ 22 (heating_control.py:72).
- **Requested setpoint at or below 22 °C.** The demand is heater OFF, so the control setpoint is 10 °C and CH is disabled (heating_control.py:72, heating_control.py:229-238).
- **No linked radiators.** `valves_open` is always true (climate.py:420-422).
- **Radiators all closed or idle.** `valves_open` is false (climate.py:454-455), so the demand is heater OFF (heating_control.py:69-70). A linked climate without `hvac_action` counts as open when its target ≥ its current temperature + `climate_valve_offset` (climate.py:443-452).
- **Weather entity as outside source.** Entities with `sensor` in the id are tried first (climate.py:321). A weather entity uses its `temperature` attribute (climate.py:340-341).
- **Thermal comfort on.** The room temperature used for the PID error is the Summer Simmer index, not the raw sensor (climate.py:276-277). The heating curve uses the target and outside temperatures only (climate.py:690-697).
- **Requested setpoint at or above the maximum.** PWM is disabled for that pass (heating_control.py:203-204). The requested setpoint is already clamped to `coordinator.maximum_setpoint` (climate.py:394). Both use the same number except in switch mode (coordinator/__init__.py:274-283, coordinator/switch/__init__.py:41-43).
- **Switch mode.** The switch backend returns `minimum_setpoint` as its `maximum_setpoint` (coordinator/switch/__init__.py:41-43). So the requested setpoint is clamped to [10, `max(minimum_setpoint, 10)`] (climate.py:394, coordinator/__init__.py:269-272). The switch setup form asks for that minimum setpoint, default 50 °C (config_flow.py:302-304).
- **ECO mode with secondary rooms.** Rooms do not change the requested setpoint (climate.py:377-379), but they still count in `valves_open` when radiators are linked (climate.py:418).
- **Room overshoot of 2.5 °C or more** (derived, see Formulas). In COMFORT mode the cap of 22 °C makes the demand heater OFF (area.py:322, area.py:331, climate.py:388-389, heating_control.py:72).
- **Continuous mode, boiler hotter than the request, flame on.** The setpoint stays at least `boiler - 2.0` °C (default offset) and does not rise on a falling request (heating_control.py:485-489).
- **Rounding.** The curve and PID output are rounded to 0.1 °C (heating_curve.py:39, pid.py:132). ECO rounds the requested setpoint again (climate.py:379). COMFORT does not round after the solar subtraction (climate.py:391-394). The continuous floor `boiler_temperature - offset` is not rounded (heating_control.py:485). The MQTT command formats the value with plain `{value}` (coordinator/mqtt/opentherm.py:196). The value sent can therefore carry more than one decimal.
- **Heat pump.** It uses the same `base_offset` 27.2 as radiators (types.py:118) and the 55 °C default maximum (helpers.py:99).
- **Heating system unset.** It is `UNKNOWN` (entry_data.py:121-122), which also gets base offset 27.2 (types.py:118). In `limits`, the maximum-setpoint default uses `RADIATORS` as the fallback (entry_data.py:228). The setup flow asks for the heating system only for the ESPHome, MQTT OpenTherm, MQTT EMS, MQTT OT-Thing, serial and simulator modes. The other modes (fake and switch, entry_data.py:22-30) go straight to the areas step without it (config_flow.py:364-374). Entries migrated from config version 1 get `radiators` or `underfloor` written into the data (__init__.py:121-124).
- **SAT loaded while Home Assistant is already running.** The curves are computed at climate.py:134, before the areas are attached to Home Assistant at climate.py:148. An area returns no target temperature while it has no `hass` reference (area.py:105-106), so its curve is skipped (area.py:261) and stays `None` until the next sensor or target event. Until then that room's PID is not available (pid.py:57-59) and is left out of the room output and the overshoot cap (area.py:279, area.py:309).

### Not found in the code / unclear

1. **Configured minimum setpoint is not a floor for the flow setpoint.** The requested setpoint is clamped from below with the constant `MINIMUM_SETPOINT` = 10 (climate.py:379, climate.py:394). The configured `limits.minimum_setpoint` (entry_data.py:238) feeds `coordinator.minimum_setpoint` (coordinator/__init__.py:269-272) and `capabilities` (coordinator/__init__.py:122-128). A grep over all `*.py` files found no use of `minimum_setpoint` or `capabilities` in climate.py or heating_control.py. Uses found: the switch coordinator, where it becomes the setpoint and the maximum setpoint (coordinator/switch/__init__.py:37-43), the simulator (coordinator/simulator/__init__.py:122-123), and the config migration, which sets `minimum_setpoint` to 10 or to a stored overshoot-protection value (__init__.py:112-119). Whether the switch-mode ceiling is intended is not stated in the code.
2. **No time-based rate limit or ramp** on the requested or control setpoint was found in climate.py, heating_control.py, heating_curve.py or pid.py. The only limiter is the continuous-mode floor and hold (heating_control.py:485-489).
3. **No upper clamp on the control setpoint in continuous mode.** `max(requested, boiler - offset)` (heating_control.py:487) is not clamped to the maximum setpoint afterwards.
4. **`MAXIMUM_SETPOINT` = 65 °C** (const.py:36): a whole-word grep found no use outside its definition.
5. **What the boiler receives when HVAC is switched off.** The loop returns early (climate.py:544-545). No code path that sends a low setpoint or `CH=0` on switch-off was found in climate.py or heating_control.py.
6. **`min_temp` value.** SAT reads `self.min_temp` (climate.py:178-179, climate.py:205-206) but does not define it in any file searched. It comes from the Home Assistant `ClimateEntity` base class, whose value was not checked here.
7. **`self.pid.control_setpoint = self.min_temp`** (climate.py:178, climate.py:205) sets an attribute that the `PID` class never reads (pid.py:30-251 has no `control_setpoint`). Its intent is unclear.
8. **`Area.async_outside_entity_changed`** (area.py:198-200) has no caller in the code base (grep). It passes the room's current temperature where the outside temperature is expected.
9. **`_async_entity_changed` guard.** It compares the `new_state` State object (not `new_state.state`) with the strings `unavailable` and `unknown` (climate.py:769), so this filter likely does not catch those states (this depends on Home Assistant `State` equality, which was not checked). The curve update then relies on the `None` checks in `current_outside_temperature` and `_update_heating_curves` (climate.py:334-335, climate.py:693-694).
10. **`HeatingCurve.reset()`** (heating_curve.py:32-34): no caller found by grep. `SatHeatingControl.reset()` (heating_control.py:192-195) also had no caller in the grep results.
11. **PWM mode with overshoot protection off.** The override returns without assigning a setpoint (heating_control.py:385-386), so the previous pass's control setpoint is sent again (heating_control.py:236). Whether that is intended is not stated in the code.
12. **Why a weather entity is ranked after sensors**, and why the base offsets are 27.2 and 20, are not explained in the code. Only the values are present (climate.py:321, types.py:118).
13. **Meaning of `PM=25`** sent after every `CS=` (coordinator/mqtt/opentherm.py:197) is not explained in the SAT code.
14. **Heating curve recommendation is not applied.** The coefficient is only read from the options (entry_data.py:213) and used at heating_curve.py:25 and pid.py:71; a grep found no code that writes it. The recommendation is a sensor value only (sensor.py:318-333).
15. **Changing the coefficient at run time.** The curve stores the coefficient once in its constructor (heating_curve.py:13-16). How an options change reaches a running curve (for example by reloading the entry) was not checked in this section.
16. **Relative modulation `None`.** When modulation management is unsupported the value becomes `None` (heating_control.py:512-515) and is still passed to the backend (heating_control.py:237). The MQTT OpenTherm backend formats it into `MM={value}` without a check (coordinator/mqtt/opentherm.py:216-220). Whether that backend can report "unsupported" was not checked.
17. **Two maxima.** The requested-setpoint clamp uses `coordinator.maximum_setpoint` (climate.py:394), but the PWM-disable check and the flame-off hold use `config.limits.maximum_setpoint` (heating_control.py:203, heating_control.py:398). They differ only in switch mode (coordinator/switch/__init__.py:41-43). The code does not say whether that difference is intended.

## 4. PWM, modulation and the suppression mechanism

Source: Alexwijn/SAT, branch `releases/thermo-nova`, commit 7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb. Paths are relative to `custom_components/sat/`. Paths that start with `tests/` are relative to the repository root and are used only to cross-check what the code does; the code is the authority.

Reading guide. "Tick" means one run of the heating control loop (`SatHeatingControl.update`, heating_control.py:197-240). "CS" is the control setpoint sent to the boiler. "MM" is the maximum relative modulation sent to the boiler. "CH" is the central-heating enable bit (the serial coordinator calls it `set_ch_enable_bit`, coordinator/serial/__init__.py:212). These are the command names the OpenTherm Gateway MQTT coordinator publishes (coordinator/mqtt/opentherm.py:196, coordinator/mqtt/opentherm.py:212, coordinator/mqtt/opentherm.py:220). The OpenTherm message IDs behind these commands are not part of this code (see the final list).

### Purpose

SAT computes a "requested setpoint" (a flow temperature) from the heating curve and PID controllers (climate.py:370-394). The heating-control layer then decides how to make the boiler deliver that heat:

- **Continuous mode**: the requested setpoint is sent to the boiler as CS, with a clamp that stops CS from being driven far below the current flow temperature while the flame burns (heating_control.py:450-508).
- **PWM mode** (pulse width modulation): the burner is switched on and off in timed phases. The fraction of time ON (duty cycle) is derived from the requested setpoint relative to the flow temperature the boiler reaches when it burns (pwm.py:229-301). During PWM, MM is set to 0 (heating_control.py:139-140, heating_control.py:517-518) and CS is overridden by the "flame-off setpoint" and the "modulation suppression" setpoint (heating_control.py:380-448).

The class docstring states the coordinating role: "Coordinate PWM, setpoint, and modulation decisions." (heating_control.py:75-76).

Important framing for the reader: in this code the "modulation suppression" mechanism does **not** write a modulation value. It writes the **control setpoint** (CS): after the flame has been on for a configurable delay, CS is set to the measured flow temperature minus an offset (heating_control.py:439-448). Modulation itself is limited separately, by sending MM = 0 for the whole time PWM is enabled (heating_control.py:517-518). The user-facing description of the option says "Reduce modulation by temporarily lowering the setpoint after flame-on (Pulse Width Modulation only)." (translations/en.json:245). The option description for the offset says "How much to lower the flow setpoint during suppression" (translations/en.json:251).

### Inputs

Per tick, `SatHeatingControl.update` receives a `HeatingDemand` (heating_control.py:54-62, heating_control.py:197) built by the climate entity (climate.py:547-555):

| Input | Meaning | Source |
|---|---|---|
| `requested_setpoint` | Flow temperature SAT wants, from PID and heating curve, clamped to [10, maximum setpoint] | climate.py:370-394 |
| `hvac_mode` | HEAT or OFF | climate.py:351-356, climate.py:549 |
| `valves_open` | Whether any controlled radiator valve is open; True when no radiators are configured | climate.py:414-455 |
| `outside_temperature` | Needed only to be non-None; the loop does not run without it | climate.py:544-545 |
| `timestamp` | Event time of the tick | climate.py:551 |

The heating control also reads the boiler state snapshot `DeviceState` from the coordinator (coordinator/__init__.py:130-143, device/types.py:14-26):

| Field | Meaning | Source |
|---|---|---|
| `flame_active` | Burner flame on | coordinator/__init__.py:134 |
| `hot_water_active` | DHW active (OTGW MQTT: the DHW enable topic) | coordinator/__init__.py:135, coordinator/mqtt/opentherm.py:67-69 |
| `central_heating` | CH active | coordinator/__init__.py:136 |
| `setpoint` | Control setpoint as reported back by the device | coordinator/__init__.py:138 |
| `flow_temperature` | Boiler flow temperature (`boiler_temperature`) | coordinator/__init__.py:139 |
| `return_temperature` | Boiler return temperature | coordinator/__init__.py:140 |
| `relative_modulation_level` | Actual relative modulation | coordinator/__init__.py:141 |
| `max_modulation_level` | Maximum relative modulation setting as read back | coordinator/__init__.py:142 |

Configuration inputs (defaults in the constants table below): `cycles_per_hour`, `duty_cycle` (as `duty_cycle_seconds`), `maximum_relative_modulation`, `force_pulse_width_modulation` (entry_data.py:216-224); `overshoot_protection` (entry_data.py:149-151); `flame_off_setpoint_offset_celsius`, `modulation_suppression_delay_seconds`, `modulation_suppression_offset_celsius`, `flow_setpoint_offset_celsius` (entry_data.py:173-187); `maximum_setpoint` (entry_data.py:226-246); `heating_system`, which sets `base_offset` (entry_data.py:119-124, types.py:110-118). The option values are the stored options laid over `OPTIONS_DEFAULTS` (__init__.py:29). `overshoot_protection` and `heating_system` are read from the entry data instead, so `OPTIONS_DEFAULTS` does not apply to them (entry_data.py:121-124, entry_data.py:151). In the options form, the max-modulation, flame-off and suppression fields are shown only for the MQTT OpenTherm, serial and simulator modes (config_flow.py:726, config_flow.py:751-775).

Timestamps kept by the device tracker and used here: `flame_on_since` and `hot_water_off_since` (device/__init__.py:86-100).

### Behaviour (step by step, in time order)

#### 1. Start-up of the integration (what PWM-related state exists before the first tick)

1. On construction, the heating control creates the PWM controller and sets `_control_setpoint = MINIMUM_SETPOINT` (10 °C) and `_relative_modulation_value = maximum_relative_modulation` (heating_control.py:86-90). The PWM constructor calls `reset()`, which sets `enabled = False` and status `IDLE` (pwm.py:63, pwm.py:87-92).
2. If Home Assistant has a saved state of the climate entity, the heating control restores: PWM enabled from attribute `pulse_width_modulation_enabled` (only when it is truthy), the control setpoint from attribute `setpoint`, and the modulation value from attribute `relative_modulation_value` (climate.py:171-174, heating_control.py:181-190, pwm.py:111-115). PWM status is not restored; it stays `IDLE` (pwm.py:111-115).
3. The PWM-cycle-end listener is registered: every `sat_cycle_ended` event calls `PWM.on_cycle_end` (heating_control.py:154-167).
4. When Home Assistant is running, the climate entity runs one control loop immediately (debounced by 1 s) and registers a 5 s repeating trigger for the loop and a 60 s trigger for the PID (climate.py:133-144, climate.py:699-716, climate.py:515-531, pid.py:20). Otherwise the same calls are made once Home Assistant reports it has started (climate.py:139-144).

There is no other PWM-specific start-up sequence in this code. After a fresh install (no restored state) the HVAC mode is set to OFF (climate.py:209-210), and the loop returns before it calls the heating control (climate.py:544-545). So the heating control sends no CS, MM or CH until the user selects HEAT. (Other commands, such as the OTGW MQTT coordinator's `PM=` requests in `boot()`, are outside this section; coordinator/mqtt/opentherm.py:166-172.) From then on PWM starts disabled and the tick uses the continuous rule until one of the enable triggers in step 3 below fires (heating_control.py:147-152, pwm.py:87-89).

#### 2. Every tick (about every 5 s)

The 5 s timer does not run the loop directly: it schedules a run 1 s later, and does nothing if a run is already scheduled (climate.py:706-710, climate.py:526-530). A change of HVAC mode or target temperature schedules an extra run (climate.py:601, climate.py:664).

The loop is skipped entirely when the outside temperature is unknown or `_hvac_mode` is OFF (climate.py:544-545). Otherwise `SatHeatingControl.update` runs this fixed order (heating_control.py:197-240):

1. Store requested setpoint and outside temperature (heating_control.py:199-200).
2. Decide heater demand: ON only if HVAC is HEAT, valves are open and `requested_setpoint > COLD_SETPOINT` (22 °C) (heating_control.py:64-72).
3. **If demand is ON** (heating_control.py:202-228):
   1. If `requested_setpoint >= maximum_setpoint`, disable PWM (heating_control.py:203-204).
   2. Check "sustained overshoot", which may enable PWM (heating_control.py:206-209, details in step 3).
   3. Run the PWM state machine (heating_control.py:211-215, details in step 4).
   4. Check "sustained underheat" and "sustained saturation", which may disable PWM (heating_control.py:217-220, details in step 3).
   5. Compute the MM value (heating_control.py:222, details in step 6).
   6. Determine the control mode: PWM when PWM is enabled **and** its status is not IDLE, otherwise CONTINUOUS (heating_control.py:147-152). Compute CS with the PWM override (heating_control.py:224-225, details in step 5) or the continuous rule (heating_control.py:227-228, details in step 7).
4. **If demand is OFF**: disable PWM, reset the sustain timers, set CS = 10 °C and MM = `maximum_relative_modulation` (heating_control.py:229-234).
5. Send to the boiler, in this order, on every tick: CS, then MM, then CH = ON if `CS > 22 °C` else OFF (heating_control.py:236-238). Then call the device-specific loop hook (heating_control.py:240).

Separately from the tick, every coordinator data update feeds the device tracker (flame and DHW timestamps) and the cycle tracker (heating_control.py:242-260).

#### 3. When PWM is used: automatic switching between continuous and PWM

PWM is **enabled** by exactly these code paths:

| Trigger | Conditions (all must hold) | Citation |
|---|---|---|
| Sustained overshoot (per tick) | PWM not yet enabled; DHW not active; not within 300 s after DHW turned off; demand ON; requested setpoint > 22 °C (always true once demand is ON, heating_control.py:72); flow temperature known; `flow_temperature >= requested_setpoint + 3.0` continuously for at least 60 s | heating_control.py:262-305, const.py:19, const.py:21, cycles/const.py:10 |
| Cycle ended as overshoot | Completed cycle with control mode CONTINUOUS and classification OVERSHOOT | pwm.py:219-223, heating_control.py:167 |
| Restore after restart | Saved attribute `pulse_width_modulation_enabled` is truthy | pwm.py:111-115 |

PWM is **disabled** by exactly these code paths. `disable()` calls `reset()`, so status returns to IDLE and the cycle counter, duty cycle and effective temperature are cleared (pwm.py:87-109):

| Trigger | Conditions | Citation |
|---|---|---|
| Requested at or above max | `requested_setpoint >= limits.maximum_setpoint` (checked every tick with demand ON) | heating_control.py:203-204 |
| No demand | HVAC not HEAT, valves closed, or requested setpoint <= 22 °C (only reached when the loop runs, see edge cases) | heating_control.py:229-231, heating_control.py:64-72 |
| Sustained underheat (per tick) | PWM enabled; DHW not active and not within 300 s after DHW off; demand ON; requested > 22 °C (always true once demand is ON); PWM status ON; flow known; `flow_temperature <= requested_setpoint + (-3.0)` continuously for at least 180 s | heating_control.py:307-356, const.py:22, cycles/const.py:11 |
| Sustained saturation (per tick) | Same gate as underheat (PWM status ON etc.); PWM off-time is 0 s continuously for at least 300 s | heating_control.py:325-328, heating_control.py:358-378, const.py:23 |
| Cycle ended as underheat | Completed cycle with control mode PWM and classification UNDERHEAT | pwm.py:225-227, heating_control.py:167 |

Not a disable path: a change of HVAC mode or target temperature calls `reset_control_state`, which resets only the PID controllers, not PWM (climate.py:461-464, climate.py:583, climate.py:660). `SatHeatingControl.reset`, which would reset PWM, has no caller (heating_control.py:192-195).

How a "sustained" timer works: on the first qualifying tick the timer starts and that tick never fires; on later ticks the elapsed time since the start is compared with the threshold; any tick where the condition fails resets the timer to "not started" (helpers.py:51-56, heating_control.py:293-297, heating_control.py:342-346, heating_control.py:364-368). The underheat and saturation timers are also reset on every tick where PWM status is not ON (heating_control.py:321-323), so they only count inside one ON phase. All three timers are reset when PWM is enabled by overshoot, or disabled by underheat or saturation (heating_control.py:304-305, heating_control.py:353-354, heating_control.py:375-376, heating_control.py:525-528).

How cycles are classified for the cycle-end triggers. A cycle runs from a flame OFF->ON transition to the next ON->OFF transition (cycles/tracker.py:55-86). Samples are taken on every coordinator data update, not on control ticks (heating_control.py:242-260), and only while the flame is on (cycles/tracker.py:71-72). A cycle with fewer than 3 samples is ignored (cycles/tracker.py:31, cycles/tracker.py:98-100). The PWM state used for the cycle is the one recorded with the flame-off sample (cycles/tracker.py:78, heating_control.py:253). The cycle's control mode is PWM if PWM was enabled at the flame-off sample, else CONTINUOUS (cycles/tracker.py:129). Note that this differs from the tick's control mode, which also requires status not IDLE (heating_control.py:147-152). Classification uses the "tail" of the cycle: samples from `max(start + min(60 s, 25 % of duration), end - 180 s)` onward (cycles/tracker.py:113-119). The classifier (cycles/classifier.py:17-90):

1. Duration <= 0: INSUFFICIENT_DATA (cycles/classifier.py:19-20).
2. Cycle kind unknown, or any DHW in the tail: UNCERTAIN (cycles/classifier.py:22-26).
3. PWM enabled and status IDLE: UNCERTAIN (cycles/classifier.py:28-29).
4. PWM enabled and not IDLE (PWM rule): if the 50th or 90th percentile of `flow - requested_setpoint` is missing, UNCERTAIN (cycles/classifier.py:42-43). If the 90th percentile of `flow - requested_setpoint` in the tail is <= -3.0 °C it is UNDERHEAT, unless the effective requested setpoint is below 22 °C (then UNCERTAIN) (cycles/classifier.py:40-54). If the PWM status is ON and the cycle lasted less than 80 % of the PWM on-time, or the on-time is missing or <= 0, it is SHORT_CYCLING (cycles/classifier.py:56-64). Otherwise GOOD (cycles/classifier.py:66).
5. Otherwise (continuous rule): if a percentile of `flow - control_setpoint` is missing, UNCERTAIN (cycles/classifier.py:71-72). If the 90th percentile of `flow - control_setpoint` in the tail is >= 3.0 °C it is OVERSHOOT; else if the 50th percentile is <= -3.0 °C it is UNDERHEAT (or UNCERTAIN below 22 °C) (cycles/classifier.py:69-90). Otherwise GOOD (cycles/classifier.py:37).

Only OVERSHOOT (continuous) and UNDERHEAT (PWM) change PWM enablement (pwm.py:219-227).

#### 4. The PWM state machine (`PWM.update`, once per tick while demand is ON)

States: IDLE, ON, OFF (types.py:81-85). The state machine does nothing when PWM is disabled (pwm.py:119-120).

Per call, in order (pwm.py:117-217):

1. **Missing flow temperature**: status becomes IDLE, the phase timer restarts, return (pwm.py:122-128).
2. **Initial effective temperature**: if not set yet and the flame is on, `effective_on_temperature = flow_temperature` (pwm.py:130-132).
3. `elapsed = now - last_update` (time in the current phase) (pwm.py:134).
4. **Duty cycle recalculated every tick** from the current requested setpoint (pwm.py:135-136, formulas below).
5. **Hourly cycle counter window**: if no window has started or more than 3600 s passed since it started, the counter is set to 0 and a new window starts now (pwm.py:138-141). This is a fixed one-hour window that restarts, not a sliding window (pwm.py:138-141).
6. **Effective temperature learning**: while status is ON and the flame is on, the flame-on time is recorded if missing; during the first 180 s after flame-on, `effective_on_temperature = 0.3 * flow + 0.7 * effective_on_temperature` (pwm.py:143-149).
7. **OFF or IDLE -> ON**: if `on_time >= 180 s` and (status is IDLE or `elapsed >= off_time`) (pwm.py:154-155):
   - If the counter has reached `duty_cycle_seconds`, log "Reached max cycles per hour" and return without starting (pwm.py:156-158).
   - Otherwise: counter + 1; status ON; phase timer restarts; `waiting_for_flame_on = not flame_active`; flame-on time = now if flame on, else unset; if the flame is already on, `effective_on_temperature = flow` (pwm.py:160-167). Return (pwm.py:177).
   - If the start condition is false and status is IDLE, status becomes OFF (pwm.py:179-180).
8. **ON phase** (pwm.py:185-215):
   - **Waiting for ignition**: if the flame is now on, the phase timer restarts at ignition, flame-on time = now, waiting ends, `effective_on_temperature = flow`, return (pwm.py:186-193). If the flame is still off and less than 180 s have passed, return (pwm.py:194-196). After 180 s without flame, stop waiting, log a warning, and continue with the timer that started at ON entry (pwm.py:197-199).
   - **Minimum flame-on hold**: if the flame-on time is set and less than 180 s ago, return; the ON phase cannot end yet (pwm.py:201-203).
   - **ON -> OFF**: if `elapsed >= on_time` (the on-time from this tick's recalculation), status OFF, flame-on time cleared, phase timer restarts, waiting cleared (pwm.py:205-215).

The ON phase does not react to a flame loss after ignition: no code path in the ON block sets `waiting_for_flame_on` again or clears the flame-on time when the flame goes out (pwm.py:185-215).

#### 5. The CS override in PWM mode: flame-off setpoint and modulation suppression

This runs on a tick whose control mode is PWM (heating_control.py:224-225), after the state machine and MM computation. Function: `_compute_pwm_control_setpoint` (heating_control.py:380-448). Branches, evaluated in this order:

| # | Condition | CS result | Citation |
|---|---|---|---|
| 0 | DHW active, or requested <= 22 °C (cannot be true here, because this function only runs with demand ON, which needs requested > 22 °C), or `overshoot_protection` is False | **CS is not changed** this tick (keeps the previous tick's value) | heating_control.py:385-386, heating_control.py:72, heating_control.py:202 |
| 1 | PWM status OFF | CS = 10 °C (`MINIMUM_SETPOINT`), which makes CH = OFF | heating_control.py:388-390, heating_control.py:238, const.py:35 |
| 2 | PWM ON, flame off, return temperature unknown | CS not changed; warning logged | heating_control.py:392-395 |
| 3 | PWM ON, flame off | **Flame-off setpoint**: CS = clamp(return + `flame_off_setpoint_offset_celsius`, 22, maximum_setpoint). The value is also stored as `_flame_off_hold_setpoint` | heating_control.py:392-408 |
| 4 | PWM ON, flame on, no flame-on timestamp in the device tracker | CS not changed; warning logged | heating_control.py:410-412 |
| 5 | PWM ON, flame on, less than `modulation_suppression_delay_seconds` since flame-on, a stored flame-off setpoint exists | **Hold**: CS = the stored flame-off setpoint | heating_control.py:414-426 |
| 6 | Same as 5 but no stored flame-off setpoint | CS not changed ("pending") | heating_control.py:428-433 |
| 7 | PWM ON, flame on, delay passed, flow unknown | CS not changed; warning logged | heating_control.py:435-437 |
| 8 | PWM ON, flame on, delay passed | **Suppression**: stored flame-off setpoint is cleared; CS = flow_temperature - `modulation_suppression_offset_celsius` | heating_control.py:439-448 |

What triggers suppression: the flame being on for at least the delay while PWM is in its ON phase and `overshoot_protection` is enabled (heating_control.py:385-386, heating_control.py:410-417, heating_control.py:439-448). The flame-on time comes from the device tracker, which records the timestamp of the coordinator update at which the flame went from off to on (device/__init__.py:196-212, device/__init__.py:86-88). The elapsed time is measured against the current wall clock (heating_control.py:414, helpers.py:12-14).

What is suppressed: only CS is written. CS is set 1 °C (default) below the **measured** flow temperature (heating_control.py:440-448, const.py:153). The requested setpoint is not sent as CS during this phase. It is still used for the duty cycle (pwm.py:135, pwm.py:238), for the sustained-underheat check during the ON phase (heating_control.py:337-338), in the gate of branch 0 (heating_control.py:385), and in the cycle classifier's PWM rule that can disable PWM on UNDERHEAT (cycles/tracker.py:273-277, cycles/classifier.py:45). The flame is not commanded off by suppression: CH stays ON as long as `CS > 22 °C` (heating_control.py:238). The suppressed value has no lower or upper clamp (heating_control.py:441-448).

Holds and timing: the 20 s (default) delay counts from flame-on (heating_control.py:414-417, const.py:152). During that delay CS stays at the flame-off setpoint computed while the flame was off (heating_control.py:419-426). After the delay CS is recomputed on every tick as `flow - offset`, so it follows the flow temperature at the tick rate (heating_control.py:441-448, climate.py:706-710).

How suppression ends: there is no timer that ends it. It is replaced when a branch above it applies: PWM status goes OFF (CS = 10 °C) or the flame goes off (flame-off setpoint again) (heating_control.py:388-408). It also stops when PWM is disabled or its status becomes IDLE, because the control mode then becomes CONTINUOUS and the continuous rule computes CS (heating_control.py:147-152, heating_control.py:224-228). When DHW becomes active, suppression does not end: branch 0 leaves CS unchanged, so CS stays frozen at the last suppressed value until DHW ends (heating_control.py:385-386).

The stored flame-off setpoint is cleared only in branch 8 (heating_control.py:439). Neither PWM OFF, PWM disable nor the PWM reset clears it (heating_control.py:388-390, pwm.py:87-109). A value stored in an ON phase that ended before suppression began therefore stays available to a later hold.

Interaction with the flame: a flame-off during the ON phase immediately switches CS to `return + 18 °C` (default) and stores it; at the next ignition a new flame-on time starts a new 20 s hold of that value, followed by suppression (heating_control.py:392-426, device/__init__.py:204-212). The PWM ON timer is not affected by such a flame loss (pwm.py:185-215).

Interaction with the PWM minimum times: the PWM state machine keeps its own flame-on time with a 180 s minimum hold (pwm.py:201-203). The suppression delay uses the device tracker's flame-on time and its own 20 s delay (heating_control.py:410-417). They are separate variables (pwm.py:98, device/__init__.py:32).

#### 6. Maximum relative modulation (MM)

Computed every tick with demand ON (heating_control.py:222, heating_control.py:510-523):

1. If the coordinator does not support modulation management, the value is None (heating_control.py:512-515). The base coordinator returns True (coordinator/__init__.py:315-322); the OTGW serial and OTGW MQTT coordinators do not override it (coordinator/serial/__init__.py:61-75, coordinator/mqtt/opentherm.py:43-57). The OT-Thing MQTT coordinator returns False, so MM is None on that backend (coordinator/mqtt/otthing.py:61-63).
2. The modulation state is HOT_WATER if DHW is active; else OFF if PWM is enabled; else COLD if the device-reported setpoint is unknown or <= 10 °C; else PWM_OFF (heating_control.py:133-145).
3. State OFF gives MM = 0 (`MINIMUM_RELATIVE_MODULATION`); every other state gives MM = `maximum_relative_modulation` (default 100) (heating_control.py:517-520, const.py:37, const.py:148).
4. For a Geminox boiler, MM is at least 10 (heating_control.py:522-523).

So MM = 0 whenever PWM is enabled and DHW is not active, in the ON phase, the OFF phase and the IDLE status alike (heating_control.py:136-140). With demand OFF, MM = `maximum_relative_modulation` (heating_control.py:234). A test confirms MM = 0 for PWM enabled in IDLE (tests/test_heating_control.py:278-285) and MM = maximum during DHW (tests/test_heating_control.py:288-295).

#### 7. Continuous mode CS (for comparison)

Function `_compute_continuous_control_setpoint` (heating_control.py:450-508):

1. Flow temperature unknown: CS = requested (heating_control.py:458-465).
2. Flow <= requested: CS = requested (heating_control.py:467-474).
3. Flame off: CS = requested (heating_control.py:476-483).
4. Flame on and flow above requested: `minimum_allowed = flow - flow_setpoint_offset_celsius` (default 2.0 °C). If requested is above the previous CS (`requested_setpoint > previous_setpoint`), CS = max(requested, minimum_allowed); otherwise CS = max(requested, min(minimum_allowed, previous CS)) (heating_control.py:454, heating_control.py:485-489, const.py:154).

The continuous mode does not touch MM beyond step 6 (heating_control.py:510-523).

#### 8. Timeline example (defaults, radiators, `overshoot_protection` enabled)

Assumptions for the arithmetic only: `cycles_per_hour = 4`, base offset 27.2 °C, requested setpoint 40 °C, effective boiler temperature 50 °C, return 30 °C. Constants: const.py:119, types.py:117-118, const.py:151-153.

Duty cycle: (40 - 27.2) / (50 - 27.2) = 0.561, in the mid range, on = int(900 x 0.561) = 505 s, off = int(900 x 0.439) = 394 s (pwm.py:238, pwm.py:274-283, pwm.py:54).

| Time | Event | PWM state | CS sent | MM | CH |
|---|---|---|---|---|---|
| t0 | PWM becomes enabled (for example sustained overshoot for 60 s) | Same tick: IDLE -> ON, cycle 1, waiting for flame (flame off) | Flame off: 30 + 18 = 48 °C, stored as hold | 0 | ON |
| t0 + 5 s ... | Boiler not yet burning | ON, waiting (up to 180 s) | 48 °C each tick (recomputed from return) | 0 | ON |
| t1 (ignition) | Flame on | ON timer restarts at t1; effective temperature = flow | Hold 48 °C (less than 20 s since flame-on) | 0 | ON |
| t1 + 20 s | Suppression delay passed | ON | flow - 1 °C, every tick | 0 | ON |
| t1 ... t1 + 180 s | Effective temperature learning (0.3 / 0.7 filter); minimum flame-on hold | ON, cannot end | flow - 1 °C | 0 | ON |
| t1 + on_time (about 505 s if inputs stay as assumed) | ON phase elapsed | ON -> OFF, timer restarts | 10 °C | 0 | OFF |
| OFF phase | Waiting until `elapsed >= off_time` (about 394 s), recalculated each tick | OFF | 10 °C | 0 | OFF |
| next start | `on_time >= 180 s` and off-time elapsed | OFF -> ON, cycle 2 | Flame off: return + 18 °C again | 0 | ON |

Citations for the rows: pwm.py:154-177, pwm.py:186-203, pwm.py:205-215, pwm.py:143-149, heating_control.py:388-448, heating_control.py:517-518, heating_control.py:236-238.

### Formulas

Duty cycle percentage (pwm.py:229-239):

```
base_offset         = 20 if heating_system == UNDERFLOOR else 27.2          (types.py:116-118)
boiler_temperature  = effective_on_temperature or flow_temperature
boiler_temperature  = max(boiler_temperature, base_offset + 1)
p = (requested_setpoint - base_offset) / (boiler_temperature - base_offset)
p = min(max(p, 0), 1)
```

Thresholds, fixed at construction (pwm.py:52-61):

```
on_time_lower_threshold   = 180 s
on_time_upper_threshold   = 3600 / max(1, cycles_per_hour)
on_time_maximum_threshold = 2 * on_time_upper_threshold
duty_cycle_lower_threshold = 180 / on_time_upper_threshold
duty_cycle_upper_threshold = 1 - duty_cycle_lower_threshold
min_duty_cycle_percentage  = duty_cycle_lower_threshold / 2
max_duty_cycle_percentage  = 1 - min_duty_cycle_percentage
```

On and off times, first matching row wins, results truncated with `int()` (pwm.py:246-301):

| Range of p | on_time | off_time | Citation |
|---|---|---|---|
| p < min_duty, flame on and no DHW | 180 | maximum - 180 | pwm.py:247-257 |
| p < min_duty, otherwise | 0 | maximum | pwm.py:259-260 |
| p <= lower | 180 | 180 / p - 180 | pwm.py:263-272 |
| p <= upper | upper x p | upper x (1 - p) | pwm.py:274-283 |
| p <= max_duty | 180 / (1 - p) - 180 | 180 | pwm.py:285-294 |
| p > max_duty | maximum | 0 | pwm.py:296-301 |

With the default `cycles_per_hour = 4`: upper = 900 s, maximum = 1800 s, lower threshold 0.20, upper threshold 0.80, min duty 0.10, max duty 0.90 (pwm.py:52-61, const.py:119). The tests confirm these numbers: p = 0.05 gives (0, 1800) without flame and (180, 1620) with flame; 0.15 gives (180, 1020); 0.50 gives (450, 450); 0.85 gives (1020, 180); 0.95 gives (1800, 0) (tests/test_pwm.py:108-133).

Effective boiler temperature filter, first 180 s after flame-on in the ON phase (pwm.py:148-149):

```
effective_on_temperature = 0.3 * flow_temperature + 0.7 * effective_on_temperature
```

Flame-off setpoint (heating_control.py:397-398):

```
flame_off_hold_setpoint = clamp(return_temperature + flame_off_setpoint_offset_celsius, 22, maximum_setpoint)
```

Suppression setpoint (heating_control.py:440-441):

```
suppressed_setpoint = flow_temperature - modulation_suppression_offset_celsius
```

Sustain thresholds (heating_control.py:288-289, heating_control.py:337-338, heating_control.py:359-360):

```
overshoot : flow_temperature >= requested_setpoint + 3.0   for >= 60 s
underheat : flow_temperature <= requested_setpoint - 3.0   for >= 180 s
saturation: off_time_seconds == 0 (not > 0)                for >= 300 s
```

CH command (heating_control.py:238): `CH = ON if control_setpoint > 22 else OFF`.

### Constants and defaults

| Name | Value | Unit | Citation |
|---|---|---|---|
| `HEATER_STARTUP_TIMEFRAME` (min on-time to start an ON phase, ignition wait, min flame-on hold, filter window) | 180 | s | const.py:18, pwm.py:50, pwm.py:155, pwm.py:194, pwm.py:201 |
| `_on_time_lower_threshold` | 180 | s | pwm.py:53 |
| Hourly counter window | 3600 | s | pwm.py:138 |
| Effective temperature filter weight on new sample | 0.3 | - | pwm.py:149 |
| `CONF_CYCLES_PER_HOUR` default | 4 | per hour | const.py:119 |
| Cycles per hour choices when the form's heating system is radiators | 3 or 4 | per hour | config_flow.py:695-702 |
| Cycles per hour choices when the form's heating system is heat pump | 2 or 3 | per hour | config_flow.py:686-693 |
| `CONF_DUTY_CYCLE` default (`duty_cycle_seconds`, used as the cycle-count cap) | "00:13:00" = 780 | s (compared with a count) | const.py:167, entry_data.py:220, pwm.py:156 |
| `CONF_MAXIMUM_RELATIVE_MODULATION` default | 100 | % | const.py:148 |
| `MINIMUM_RELATIVE_MODULATION` (MM during PWM) | 0 | % | const.py:37 |
| `MAXIMUM_RELATIVE_MODULATION` | 100 | % | const.py:38 |
| Geminox MM floor | 10 | % | heating_control.py:522-523 |
| `CONF_FORCE_PULSE_WIDTH_MODULATION` default | False | - | const.py:149 |
| `overshoot_protection` when absent | False (`bool(None)`); the `OPTIONS_DEFAULTS` entry False at const.py:122 is not used, because the value is read from the entry data | - | entry_data.py:151, __init__.py:29, const.py:122 |
| `CONF_FLAME_OFF_SETPOINT_OFFSET_CELSIUS` default | 18.0 (range 0 to 30, step 0.5) | °C | const.py:151, config_flow.py:764-767 |
| `CONF_MODULATION_SUPPRESSION_DELAY_SECONDS` default | 20 (range 0 to 120, step 1) | s | const.py:152, config_flow.py:768-771 |
| `CONF_MODULATION_SUPPRESSION_OFFSET_CELSIUS` default | 1.0 (range 0 to 10, step 0.1) | °C | const.py:153, config_flow.py:772-775 |
| `CONF_FLOW_SETPOINT_OFFSET_CELSIUS` default (continuous clamp) | 2.0 (range 0 to 10, step 0.1) | °C | const.py:154, config_flow.py:777-780 |
| `COLD_SETPOINT` | 22 | °C | const.py:34 |
| `MINIMUM_SETPOINT` (CS in PWM OFF and with no demand) | 10.0 | °C | const.py:35 |
| `OVERSHOOT_SUSTAIN_SECONDS` | 60.0 | s | const.py:21 |
| `UNDERHEAT_SUSTAIN_SECONDS` | 180.0 | s | const.py:22 |
| `SATURATION_SUSTAIN_SECONDS` | 300.0 | s | const.py:23 |
| `DHW_OVERSHOOT_GUARD_SECONDS` | 300.0 | s | const.py:19 |
| `OVERSHOOT_MARGIN_CELSIUS` | 3.0 | °C | cycles/const.py:10 |
| `UNDERSHOOT_MARGIN_CELSIUS` | -3.0 | °C | cycles/const.py:11 |
| `base_offset`, underfloor | 20 | °C | types.py:118 |
| `base_offset`, all other systems | 27.2 | °C | types.py:118 |
| Default maximum setpoint, underfloor / other | 50 / 55 | °C | helpers.py:97-99 |
| Control loop repeat | 5 | s | climate.py:706-710 |
| Control loop debounce | 1 | s | climate.py:530 |
| PID update interval | 60 | s | pid.py:20, climate.py:712-716 |
| Minimum samples per cycle | 3 | samples | cycles/tracker.py:31 |
| Cycle tail length | 180 | s | cycles/tracker.py:116 |
| Cycle warm-up excluded | min(60, 25 % of duration) | s | cycles/tracker.py:114 |
| Short-cycling ratio (PWM) | 0.8 of on-time | - | cycles/classifier.py:63 |
| MQTT delay after each published command | 0.2 | s | coordinator/mqtt/__init__.py:134-147 |
| Sync binary sensor mismatch delay | 60 | s | binary_sensor.py:71-88 |

### State kept between passes

PWM controller (pwm.py:87-100):

| Variable | Meaning | Citation |
|---|---|---|
| `_enabled` | PWM allowed | pwm.py:89, pwm.py:102-109 |
| `_status` | IDLE / ON / OFF | pwm.py:92 |
| `_last_update` | Start of the current phase (phase timer) | pwm.py:91, pwm.py:124, pwm.py:162, pwm.py:188, pwm.py:208 |
| `_current_cycle`, `_first_duty_cycle_start` | Cycle counter and start of its one-hour window | pwm.py:90, pwm.py:96, pwm.py:138-141 |
| `_duty_cycle`, `_last_duty_cycle_percentage` | Last computed (on, off) and p | pwm.py:93, pwm.py:97, pwm.py:136, pwm.py:238-239 |
| `_waiting_for_flame_on` | ON phase waiting for ignition | pwm.py:95, pwm.py:163 |
| `_flame_on_at` | PWM's own flame-on time (180 s hold and filter) | pwm.py:98, pwm.py:145-146 |
| `_effective_on_temperature` | Learned boiler temperature when burning | pwm.py:100, pwm.py:130-131, pwm.py:149 |

Only `_enabled` survives a restart, through the climate attribute (pwm.py:111-115, climate.py:260).

Heating control (heating_control.py:89-101):

| Variable | Meaning | Citation |
|---|---|---|
| `_control_setpoint` | Last CS; reused when a branch leaves CS unchanged; restored on restart | heating_control.py:89, heating_control.py:186-187 |
| `_relative_modulation_value` | Last MM; restored on restart | heating_control.py:90, heating_control.py:189-190 |
| `_flame_off_hold_setpoint` | Flame-off setpoint kept for the suppression hold; cleared only when suppression applies; not restored after a restart | heating_control.py:95, heating_control.py:401, heating_control.py:439, heating_control.py:181-190 |
| `_sustained_overshoot_started_at`, `_sustained_underheat_started_at`, `_sustained_saturation_started_at` | Sustain timers | heating_control.py:99-101 |

Device tracker: `_last_flame_on_at`, `_last_flame_off_at`, `_last_flame_off_was_overshoot`, `_last_hot_water_on_at`, `_last_hot_water_off_at` (device/__init__.py:32-36). `_last_flame_off_at` is cleared whenever there is no demand by the device's own measure (setpoint > flow + 0.7) (device/__init__.py:144-145, device/status.py:127-132, device/const.py:9). The boiler status derived by `DeviceStatusEvaluator` (device/status.py:32-116) is exposed as a sensor and a binary sensor only (sensor.py:480, binary_sensor.py:406); no PWM or suppression decision reads it (heating_control.py:103-106 is its only accessor in the heating control).

Modulation reliability: the device tracker keeps up to 50 relative-modulation samples taken while the flame is on and while the boiler's maximum modulation setting is known and at least 3 % (device/modulation.py:23-34). Once 8 samples exist it marks modulation "reliable" when at least `max(2, int(8 x 0.4))` = 3 of the last 8 samples are >= 3 % (device/modulation.py:36-44, device/const.py:16-17). It is persisted (device/__init__.py:127-132) and used only to choose how the modulation direction is derived for the boiler status (device/__init__.py:162-194). No PWM or MM decision reads it.

### Outputs (to the boiler, to entities)

To the boiler, every tick in this order (heating_control.py:236-238):

| Output | OTGW over MQTT | OTGW serial | Citation |
|---|---|---|---|
| Control setpoint | `CS=<value>` followed by `PM=25` | `set_control_setpoint(value)` | coordinator/mqtt/opentherm.py:195-199, coordinator/serial/__init__.py:192-196 |
| Max relative modulation | `MM=<value>`; for Immergas first `TP=11:12=<min(value, 80)>` | `set_max_relative_mod(value)` | coordinator/mqtt/opentherm.py:216-222, coordinator/serial/__init__.py:216-220 |
| CH enable | `CH=1` or `CH=0` | `set_ch_enable_bit(1 or 0)` | coordinator/mqtt/opentherm.py:211-214, coordinator/serial/__init__.py:210-214 |

Each MQTT command is followed by a 0.2 s pause (coordinator/mqtt/__init__.py:134-147). In simulation mode nothing is published (coordinator/mqtt/__init__.py:140-141, coordinator/serial/__init__.py:193). When the serial coordinator disconnects it sends CS = 0 and MM = "-" (coordinator/serial/__init__.py:228-233).

To entities (climate state attributes, climate.py:234-263): `setpoint` (CS, climate.py:243), `requested_setpoint` (climate.py:250), `relative_modulation_value` and `relative_modulation_state` (climate.py:257-258), `pulse_width_modulation_enabled`, `pulse_width_modulation_duty_cycle` (on, off seconds) and `pulse_width_modulation_state` (climate.py:260-262). Binary sensors "Control Setpoint Synchronization" and "Relative Modulation Synchronization" turn on when SAT's value and the boiler's read-back value differ for 60 s (binary_sensor.py:91-154). They exist only when the coordinator supports setpoint or modulation management respectively (binary_sensor.py:59-63).

### Edge cases

- **`overshoot_protection` off**: in PWM mode CS is never recomputed; it keeps whatever value the last tick left (a continuous-mode value, 10 °C, or a restored value) (heating_control.py:224-228, heating_control.py:385-386). MM is still 0 (heating_control.py:517-518). The continuous rule does not run in PWM mode (heating_control.py:227-228).
- **DHW active during PWM**: CS unchanged, MM = maximum, sustain timers reset (heating_control.py:136-137, heating_control.py:268-270, heating_control.py:313-315, heating_control.py:385-386). PWM itself keeps running its state machine (pwm.py:117-217 has no DHW check except in the low-duty case, pwm.py:248).
- **PWM enabled but IDLE** (no flow temperature, or cycle cap reached while IDLE): the tick uses the continuous CS rule while MM stays 0 (heating_control.py:147-152, heating_control.py:139-140, pwm.py:122-128, pwm.py:156-158).
- **Duty below minimum without flame**: on-time 0, so the ON phase never starts (on-time < 180 s); the state machine stays OFF (an IDLE status becomes OFF) and, with `overshoot_protection` on, CS = 10 °C (pwm.py:259-260, pwm.py:155, pwm.py:179-180, heating_control.py:385-390).
- **Duty above maximum**: on-time = 2 x upper, off-time 0. If that lasts 300 s inside one ON phase, PWM is disabled (pwm.py:296-301, heating_control.py:358-378).
- **On-time shrinks during the ON phase**: because the on-time is recomputed every tick, the phase can end as soon as the minimum flame-on hold (180 s) has passed (pwm.py:135, pwm.py:201-205). The test tests/test_pwm.py:168-210 checks that the hold keeps the phase ON before that point and that it ends afterwards.
- **No ignition within 180 s**: the ON timer continues from ON entry, so the ON phase can end without a flame (pwm.py:197-199, pwm.py:205).
- **Flame already on when PWM starts** (for example enable by sustained overshoot while burning): normally there is no stored flame-off setpoint, so during the suppression delay CS is not changed; after the delay suppression applies (heating_control.py:410-433). A flame-off setpoint left over from an earlier ON phase that ended before suppression would be used for the hold instead, because only branch 8 clears it (heating_control.py:419-426, heating_control.py:439). The device tracker's flame-on time may be much older than the PWM start, so the delay may already have passed (heating_control.py:414-417, device/__init__.py:198-200, device/__init__.py:209-211). The PWM state machine starts its own 180 s flame-on hold at ON entry in this case (pwm.py:164, pwm.py:201-203).
- **Flame-off setpoint clamp**: the lower clamp is exactly 22 °C, and CH requires CS > 22 °C, so a clamped value of 22 °C gives CH = OFF (heating_control.py:398, heating_control.py:238).
- **Suppressed setpoint at or below 22 °C** (flow <= 23 °C at 1 °C offset): CH = OFF (heating_control.py:441, heating_control.py:238).
- **HVAC switched to OFF**: the control loop returns before calling the heating control, so this code sends no CS, MM or CH on that path (climate.py:544-545, climate.py:570-602). The "demand OFF" branch (CS = 10 °C, MM = maximum) is reached with HVAC HEAT and closed valves or a requested setpoint <= 22 °C (heating_control.py:64-72, heating_control.py:229-234). It is also reached when the stored HVAC mode is unknown or unavailable: the early return tests the raw stored mode against OFF, while the demand uses the `hvac_mode` property, which maps those two states to OFF (climate.py:544, climate.py:351-356, climate.py:549). The stored mode is restored from the saved state (climate.py:184-185).
- **HVAC OFF and back to HEAT does not reset PWM**: changing the HVAC mode resets only the PID controllers (climate.py:583, climate.py:461-464). Because the loop does not run while OFF, none of the tick-based disable paths apply in that period; only the cycle-end listener, which keeps running on coordinator updates, can change PWM enablement (climate.py:544-545, heating_control.py:166-167). A PWM that was enabled before can therefore still be enabled at the first tick after HEAT returns (heating_control.py:202-215).
- **Requested at or above maximum setpoint**: PWM is disabled every tick in that state (heating_control.py:203-204). The requested setpoint is clamped to the coordinator's `maximum_setpoint`, which the base coordinator takes from the same configured limit, so a requested setpoint pinned at the clamp keeps PWM disabled (climate.py:394, coordinator/__init__.py:274-283).
- **Sustained overshoot does not check the flame**: the docstring says "while flame is active" but the code tests only the flow temperature (heating_control.py:262-305).
- **Modulation management unsupported** (some coordinators): MM value becomes None and is still passed to the coordinator (heating_control.py:512-515, heating_control.py:237).

### Not found in the code / unclear

1. **`force_pulse_width_modulation`** is read into `PwmConfig` and offered in the options form (entry_data.py:223, config_flow.py:726-728), but no control code reads it (search over all `.py` files found it only in const.py, entry_data.py and config_flow.py). Its described effect, "Force Pulse Width Modulation even when modulation control is available" (translations/en.json:267), is not implemented in this commit.
2. **Cycle cap**: the start of an ON phase is refused when `_current_cycle >= duty_cycle_seconds` (pwm.py:156-158), and `duty_cycle_seconds` is the "duty_cycle" time option, default 780 s (entry_data.py:220, const.py:167). A cycle count is thus compared with a number of seconds; with the default the cap is 780 cycles per hour. The "duty_cycle" option is not offered in any options form (a search for `CONF_DUTY_CYCLE` in config_flow.py finds nothing), so in practice the default applies. Whether `cycles_per_hour` was intended here is not visible in the code. `cycles_per_hour` is used only for the on-time thresholds (pwm.py:54), although its option text says "The maximum amount of duty cycles per hour." (translations/en.json:340). The test fixture also uses `duty_cycle_seconds` as the cap (tests/test_pwm.py:73-79, tests/test_pwm.py:158-165).
3. **How `overshoot_protection` gets set**: it is read from the entry data (entry_data.py:149-151). No step in config_flow.py sets it (a search for "overshoot" in config_flow.py finds nothing). It is only migrated from older options (__init__.py:159-162). The English translation still lists it in the system configuration form (translations/en.json:335, translations/en.json:342), but that form does not include it (config_flow.py:678-711).
4. **Overshoot protection calculation**: constants `OVERSHOOT_PROTECTION_SETPOINT`, `OVERSHOOT_PROTECTION_REQUIRED_DATASET`, `SERVICE_START_OVERSHOOT_PROTECTION_CALCULATION` and `SERVICE_SET_OVERSHOOT_PROTECTION_VALUE` exist (const.py:188-202), but no code uses them; the only registered service is `reset_integral` (services.py:41-46, climate.py:214-222). The only trace of a stored overshoot value is a migration into `minimum_setpoint` (__init__.py:113-119). `minimum_setpoint` is not used by the PWM or suppression logic (pwm.py:229-301, heating_control.py:380-448).
5. **Unused PWM constants**: `PWM_ENABLE_MARGIN_CELSIUS`, `PWM_DISABLE_MARGIN_CELSIUS`, `PWM_ENABLE_LOW_MODULATION_PERCENT`, `PWM_DISABLE_LOW_MODULATION_PERCENT`, `PWM_LOW_MODULATION_PERSISTENCE_TICKS` (const.py:26-30), `BOILER_DEADBAND` (const.py:17) and `MAX_BOILER_TEMPERATURE_AGE` (const.py:40) are defined but not referenced anywhere else. There is no low-modulation based PWM switching in this commit.
6. **`SERVICE_PULSE_WIDTH_MODULATION`** is defined (const.py:200) but no service handler is registered for it (services.py:13-46, climate.py:214-222).
7. **Translation keys without code**: "enable_flame_off_setpoint" and "enable_modulation_suppression" appear in the translation (translations/en.json:218-219, translations/en.json:244-245), but there is no such option in const.py or config_flow.py; flame-off setpoint and suppression are not separately switchable. Both are active whenever PWM mode and `overshoot_protection` apply (heating_control.py:385-448).
8. **"Temporarily"**: the option text says suppression lowers the setpoint "temporarily" (translations/en.json:245). The code has no end time for suppression; it lasts for the rest of the ON phase while the flame burns (heating_control.py:439-448).
9. **What the boiler does with CS = flow - 1 °C and MM = 0** (whether it holds minimum modulation, ends the flame, or keeps rising) is boiler behaviour and is not described in the code.
10. **Sustained-overshoot docstring vs code**: the docstring says "while flame is active" but no flame check exists (heating_control.py:263-291). Whether that is intended is unclear.
11. **CS left unchanged in PWM branches 0, 2, 4, 6, 7** (heating_control.py:385-437): the code shows the previous CS is kept; whether that is the intended fallback is not stated.
12. **HVAC OFF sends nothing**: the loop returns early when HVAC is OFF (climate.py:544-545); whether another component then resets CS/CH on the boiler was not found in the files read for this section.
13. **Which cycles-per-hour choices are shown**: the options form picks the choices from `options.get(CONF_HEATING_SYSTEM)` (config_flow.py:686, config_flow.py:695). Those options are `OPTIONS_DEFAULTS` overlaid with the entry's stored options only (config_flow.py:581, config_flow.py:808-812), where the heating system default is radiators (const.py:184). The heating system itself is kept in the entry data (entry_data.py:119-124, __init__.py:121-124). So unless a heating system value is stored in the options, the form shows the radiator choices (3 or 4) for every system, and the heat-pump choices are not reached. Whether that is intended is not visible in the code.
14. **`SatHeatingControl.reset` has no caller**: it would reset PWM and the sustain timers (heating_control.py:192-195), but no code calls it (a search for `_heating_control.reset` and `reset_control_state` finds only the PID resets in climate.py:461-464). Whether PWM was meant to be reset on HVAC mode or target changes is not visible in the code.
15. **OpenTherm message IDs**: SAT sends OpenTherm Gateway commands (`CS=`, `MM=`, `CH=`) or serial-library calls (coordinator/mqtt/opentherm.py:195-222, coordinator/serial/__init__.py:192-220). Which OpenTherm message IDs the gateway uses for them is not in this code.
16. Files read for this section: pwm.py, heating_control.py, climate.py (lines 120-270, 340-610, 640-800), const.py, entry_data.py (lines 40-250), helpers.py (lines 1-110), types.py (lines 70-123), device/__init__.py, device/status.py (lines 25-40, 110-140), device/const.py, device/types.py, device/modulation.py, cycles/const.py, cycles/classifier.py, cycles/tracker.py, coordinator/__init__.py (lines 100-400), coordinator/serial/__init__.py (lines 55-80, 185-238), coordinator/mqtt/opentherm.py (lines 35-233), coordinator/mqtt/otthing.py (lines 55-70), coordinator/mqtt/__init__.py (lines 125-159), services.py, pid.py (lines 15-25), binary_sensor.py (lines 55-160), config_flow.py (lines 576-588, 675-812), translations/en.json (lines 205-275, 325-350), __init__.py (lines 105-170), and tests/test_pwm.py (lines 60-215), tests/test_heating_control.py (lines 265-300). sensor.py:480 and binary_sensor.py:406 were seen only as search results. cycles/history.py and cycles/types.py were not read; PWM decisions do not call the history (pwm.py:219-227 uses only the `Cycle` object from the tracker event, cycles/tracker.py:78-81).

## 5. Boiler state, flame and cycle tracking

Source: Alexwijn/SAT, branch `releases/thermo-nova`, commit 7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb. All paths are relative to `custom_components/sat`. Every statement below cites the lines it is based on. Citation audit (2026-10-22): the git blob hashes of all 78 files under `custom_components/sat` in the local copy match the GitHub tree of commit 7beaaf531 exactly, so the line numbers below are those of the pinned commit.

### Purpose

SAT watches the boiler through three cooperating parts:

1. **Boiler snapshot (`DeviceState`)**: a frozen record of flame, hot water, central heating, setpoint, flow, return and modulation, built from the coordinator's current data (coordinator/__init__.py:130-143, device/types.py:14-26).
2. **Device tracker (`DeviceTracker`)**: remembers the previous snapshot, flame and hot water on/off timestamps, whether the last flame-off was an overshoot, and whether the modulation reading is trustworthy. From this it derives one `BoilerStatus` label (device/__init__.py:21-42, device/__init__.py:134-153, types.py:23-49).
3. **Cycle tracker and history (`CycleTracker`, `CycleHistory`)**: a "cycle" is one flame-on period, from flame OFF->ON to flame ON->OFF (cycles/tracker.py:60-84). At the end of each cycle SAT computes statistics, classifies the cycle (GOOD, OVERSHOOT, UNDERHEAT, SHORT_CYCLING, UNCERTAIN, INSUFFICIENT_DATA, types.py:60-72), stores it in rolling windows and fires an event (cycles/tracker.py:78-81, cycles/history.py:102-129).

The only places where this area changes what SAT sends to the boiler are: the cycle classification switching PWM on or off (pwm.py:219-227, heating_control.py:167), the flame-on timestamp used by the PWM setpoint override (heating_control.py:410-433), and the hot water off timestamp used as a guard window (heating_control.py:272, heating_control.py:313). The `BoilerStatus` label itself is only read by entities (sensor.py:470-487, binary_sensor.py:391-406); a search for `device_status` found no other reader (heating_control.py:103-106).

### Inputs

**Boiler snapshot fields** (device/types.py:14-26), filled by the coordinator (coordinator/__init__.py:130-143):

| `DeviceState` field | Coordinator property | Base default | Via OTGW MQTT (`mqtt/opentherm.py`) |
|---|---|---|---|
| `flame_active` | `flame_active` | equals `active` (coordinator/__init__.py:168-171) | topic key `flame` == `"ON"` (coordinator/mqtt/opentherm.py:13-15, coordinator/mqtt/opentherm.py:63-65) |
| `hot_water_active` | `hot_water_active` | `False` (coordinator/__init__.py:173-176) | topic key `domestichotwater` == `"ON"` (coordinator/mqtt/opentherm.py:22, coordinator/mqtt/opentherm.py:67-69) |
| `central_heating` | `active` | abstract (coordinator/__init__.py:156-159) | topic key `centralheating` == `"ON"` (coordinator/mqtt/opentherm.py:23, coordinator/mqtt/opentherm.py:59-61) |
| `setpoint` | `setpoint` | abstract (coordinator/__init__.py:150-154) | `TSet` (coordinator/mqtt/opentherm.py:17, coordinator/mqtt/opentherm.py:71-76) |
| `flow_temperature` | `boiler_temperature` | `None` (coordinator/__init__.py:183-186) | `Tboiler` (coordinator/mqtt/opentherm.py:20, coordinator/mqtt/opentherm.py:106-111) |
| `return_temperature` | `return_temperature` | `None` (coordinator/__init__.py:188-191) | `Tret` (coordinator/mqtt/opentherm.py:21, coordinator/mqtt/opentherm.py:113-118) |
| `relative_modulation_level` | `relative_modulation_value` | `None` (coordinator/__init__.py:208-211) | `RelModLevel` (coordinator/mqtt/opentherm.py:19, coordinator/mqtt/opentherm.py:127-132) |
| `max_modulation_level` | `maximum_relative_modulation_value` | `None` (coordinator/__init__.py:264-267) | `MaxRelModLevelSetting` (coordinator/mqtt/opentherm.py:29, coordinator/mqtt/opentherm.py:152-157) |

The OTGW MQTT coordinator subscribes to `<topic>/value/<device_id>/<key>` for each of these keys (coordinator/mqtt/opentherm.py:174-193, coordinator/mqtt/opentherm.py:229-230). For the serial (pyotgw) backend the flame comes from `DATA_SLAVE_FLAME_ON`, hot water from `DATA_SLAVE_DHW_ACTIVE` and central heating from `DATA_MASTER_CH_ENABLED` (coordinator/serial/__init__.py:53-59, coordinator/serial/__init__.py:161-163).

**Control-loop sample** (`ControlLoopSample`, heating_control.py:40-51), built on every coordinator update (heating_control.py:250-260):

| Field | Source |
|---|---|
| `timestamp` | `event_timestamp(time)`, which falls back to "now" when no time is passed (heating_control.py:252, helpers.py:17-19) |
| `pwm` | current `PWMState`: `enabled`, `status`, `duty_cycle` (on, off seconds) (heating_control.py:253, pwm.py:19-39, pwm.py:78-85) |
| `device_state` | `coordinator.state` (heating_control.py:254) |
| `control_setpoint` | SAT's own last computed control setpoint `_control_setpoint` (heating_control.py:255) |
| `requested_setpoint` | last requested setpoint from the climate loop `_last_requested_setpoint` (heating_control.py:256, heating_control.py:199) |
| `outside_temperature` | last outside temperature (heating_control.py:257) |
| `relative_modulation` | SAT's commanded maximum modulation `_relative_modulation_value` (heating_control.py:258) |

`outside_temperature` and `relative_modulation` are stored in the sample but the cycle code does not read them; the metrics use `device_state.relative_modulation_level` instead (cycles/tracker.py:282-283).

### Behaviour (step by step, in time order)

#### 1. When a pass runs (sampling cadence)

- The heating control registers `_handle_coordinator_update` as a coordinator listener (heating_control.py:166). Each pass first updates the device tracker, then the cycle tracker, with the same snapshot (heating_control.py:242-260).
- The coordinator notifies listeners when its data changes: `async_set_updated_data` merges new values and calls `async_notify_listeners(False)`, which returns without notifying when nothing changed (coordinator/__init__.py:394-398, coordinator/__init__.py:381-392). A value counts as changed only if it differs from the stored one (coordinator/__init__.py:28-39).
- Notifications go through a debouncer with `cooldown=0.2` s and `immediate=False`, which calls `async_update_listeners` (coordinator/__init__.py:96-102, coordinator/__init__.py:392). How the debouncer merges several changes inside the cooldown is Home Assistant framework code (see the last list).
- For OTGW MQTT, every received MQTT message is normalised and passed to `async_set_updated_data` (coordinator/mqtt/__init__.py:100-113, coordinator/mqtt/__init__.py:115-132). The serial backend does the same for every pyotgw report (coordinator/serial/__init__.py:39-43). The ESPHome and simulator backends instead call `async_notify_listeners()` with the default `force=True`, which notifies even without a changed value (coordinator/esphome/__init__.py:350-352, coordinator/esphome/__init__.py:363, coordinator/simulator/__init__.py:116-117, coordinator/__init__.py:382-384).
- So tracking is event driven (passes follow data notifications), not tied to the 5 s control loop timer. That timer calls `schedule_heating_control_loop` (climate.py:706-710), which schedules `async_control_heating_loop` 1 s later unless a run is already pending (climate.py:515-531), and that runs `SatHeatingControl.update` (climate.py:540-555).
- The device tracker runs before the cycle tracker in the same pass. On the pass where the flame goes out, the device tracker therefore still receives the previous cycle as `last_cycle` (heating_control.py:244-248, heating_control.py:250-260).

#### 2. Start-up (integration loaded)

In the order the code runs:

1. `async_setup_entry` creates the coordinator and `SatHeatingControl` (__init__.py:32-33). The constructor creates an empty `CycleHistory`, a new `DeviceTracker`, the PWM controller and a `CycleTracker` (heating_control.py:83-87).
2. `coordinator.async_setup()` runs (__init__.py:50). For the MQTT backends this loads the coordinator data saved at the last unload from HA storage `sat.mqtt.<device_id>` and feeds it to `async_set_updated_data` (coordinator/mqtt/__init__.py:27, coordinator/mqtt/__init__.py:49-50, coordinator/mqtt/__init__.py:71-74). No listener is registered yet, so this notifies nobody: the dirty flag is cleared and the call returns (coordinator/__init__.py:387-389). The restored values (for example the last `flame` value) stay in the coordinator data that `coordinator.state` reads (coordinator/__init__.py:130-143, coordinator/mqtt/opentherm.py:63-65).
3. `coordinator.async_added_to_hass` subscribes the MQTT topics and runs `boot()` (__init__.py:51, coordinator/mqtt/__init__.py:52-64). For OTGW MQTT, `boot()` publishes `PM=3`, `PM=15`, `PM=48`, and `MI=500` for Ideal, Intergas and Nefit boilers (coordinator/mqtt/opentherm.py:166-172).
4. The platforms are set up (__init__.py:52). When the climate entity is added it first restores its previous HA state, which restores the PWM `enabled` flag, the control setpoint and the relative modulation value (climate.py:123-128, climate.py:169-174, heating_control.py:181-190, pwm.py:111-115).
5. Then `SatHeatingControl.async_added_to_hass` runs (climate.py:130):
   - `DeviceTracker.async_added_to_hass` opens HA storage `sat.device.<device_id>`. If it is empty, it copies data from the legacy key `sat.boiler.<device_id>` (heating_control.py:156, device/__init__.py:102-113).
   - If stored data has `modulation_reliable`, that value is loaded into the modulation reliability tracker (device/__init__.py:115-123, device/modulation.py:18-19). No other device tracker or cycle state is restored.
   - The coordinator listener and the `sat_cycle_ended` listener are registered (heating_control.py:166-167). Only from here on do data notifications produce tracking passes.
6. The 5 s control loop timer is registered once HA is running (climate.py:133-144, climate.py:699-710).

State at the first pass:

- `CycleHistory` is empty: three empty windows, no last cycle, no off-with-demand duration (cycles/history.py:40-46). `CycleTracker` has no previous flame state, no cycle start and no last flame-off (cycles/tracker.py:31-42).
- Before the first snapshot, the status reads `INSUFFICIENT_DATA` (device/__init__.py:44-49).
- `CycleTracker.reset()` exists (cycles/tracker.py:48-53), but no code in the repository calls it. `SatHeatingControl.reset()` resets PWM and the three sustained-condition timers (heating_control.py:192-195, heating_control.py:525-528). A search for `.reset()` found no caller of it either.

#### 3. Device tracker, every pass (`DeviceTracker.update`, device/__init__.py:134-153)

In this order:

1. Store `last_cycle`: the history's last recorded cycle, or `None` when it ended more than 6 h ago by the wall clock (device/__init__.py:136, heating_control.py:246, heating_control.py:119-121, cycles/history.py:48-57).
2. Shift current to previous state and previous to last update time (device/__init__.py:138-142).
3. If there is no space heating demand (see `has_demand` under Formulas), clear `last_flame_off_at` (device/__init__.py:144-145).
4. Record flame transitions (device/__init__.py:196-212):
   - First snapshot ever: if the flame is on, `last_flame_on_at` = now (device/__init__.py:198-202).
   - ON->OFF: `last_flame_off_at` = now, and `last_flame_off_was_overshoot` = `did_overshoot_at_flame_off(previous)`, evaluated on the last flame-on snapshot (device/__init__.py:204-207).
   - OFF->ON: `last_flame_on_at` = now, overshoot flag cleared (device/__init__.py:209-212).
5. Record hot water transitions the same way into `last_hot_water_on_at` / `last_hot_water_off_at` (device/__init__.py:214-228).
6. Update modulation reliability. If its verdict changed, save it to storage (device/__init__.py:150-151, device/__init__.py:127-132).
7. Evaluate the status (device/__init__.py:153-160).

`last_flame_on_at` is only ever set, never cleared (device/__init__.py:200, device/__init__.py:211).

#### 4. Status classification (`DeviceStatusEvaluator.evaluate`, device/status.py:34-116)

The first matching rule wins:

| # | Condition | Status | Citation |
|---|---|---|---|
| 1 | `central_heating` is false | `OFF` | device/status.py:39-40 |
| 2 | flame off, last flame-off was overshoot, and flow > setpoint | `OVERSHOOT_COOLING` | device/status.py:42-48, device/status.py:201-209 |
| 3 | flame off, demand present, less than 180 s since flame-off | `ANTI_CYCLING` | device/status.py:50-56, device/status.py:171-186 |
| 4 | flame off, demand present, not in 3 or 2, time since off >= threshold | `STALLED_IGNITION` | device/status.py:58-66, device/status.py:135-168 |
| 5 | flame off and the previous snapshot had flame on | `COOLING` (only on that one pass) | device/status.py:68-70 |
| 6 | flame off, no flame ever seen since load, setpoint - flow > 6, flow not rising | `PUMP_STARTING` | device/status.py:72-74, device/status.py:212-230 |
| 7 | flame off and demand present | `WAITING_FOR_FLAME` | device/status.py:76-78 |
| 8 | flame off, no demand, 0 to 60 s since flame-off | `POST_CYCLE_SETTLING` | device/status.py:80-82, device/status.py:189-198 |
| 9 | flame off, otherwise | `IDLE` | device/status.py:84-85 |
| 10 | flame on and hot water active | `HEATING_HOT_WATER` | device/status.py:87-88 |
| 11 | flame on, setpoint or flow unknown | `CENTRAL_HEATING` | device/status.py:90-93 |
| 12 | flame on, within 30 s of flame-on, flow rising >= 0.5 °C/s | `IGNITION_SURGE` | device/status.py:95-96, device/status.py:233-259 |
| 13 | setpoint - flow > 6 | `PREHEATING` | device/status.py:98-102 |
| 14 | abs(setpoint - flow) <= 1.5 | `AT_SETPOINT_BAND` | device/status.py:104-106 |
| 15 | modulation direction > 0 | `MODULATING_UP` | device/status.py:108-110 |
| 16 | modulation direction < 0 | `MODULATING_DOWN` | device/status.py:112-113 |
| 17 | otherwise | `CENTRAL_HEATING` | device/status.py:115-116 |

Notes on the status rules:

- Rule 8 appears unreachable through `DeviceTracker.update`; see "`POST_CYCLE_SETTLING` looks unreachable" under Edge cases.
- Rule 6 can only fire before the first flame-on seen since load, because it requires `last_flame_on_at is None` (device/status.py:214-216) and that timestamp is never cleared (device/__init__.py:200, device/__init__.py:211).
- In every rule that reads a setpoint (rules 2, 3, 4, 6, 7, 8, 13 and 14, directly or through `has_demand`, plus the `None` check of rule 11), "setpoint" is `DeviceState.setpoint`, the boiler setpoint the device reports (for OTGW MQTT `TSet`, coordinator/mqtt/opentherm.py:71-76). It is not SAT's internally computed setpoint (device/status.py:91, device/status.py:98, device/status.py:127-132). Rule 12 does not read the setpoint (device/status.py:233-259).
- `has_demand` is the hysteresis test under Formulas (device/status.py:127-132). Rules 3, 4 and 8 need `last_flame_off_at`, and step 3.3 clears that whenever demand disappears (device/__init__.py:144-145).

#### 5. Cycle tracker, every pass (`CycleTracker.update`, cycles/tracker.py:55-86)

1. **Flame OFF->ON** (current on, previous not on; this includes the very first pass when `previous` is `None`) (cycles/tracker.py:57-60):
   - Compute the off-with-demand duration and store it in the history (cycles/tracker.py:61-64, cycles/history.py:163-165). See Formulas.
   - Set `cycle_start` = sample timestamp, empty the sample buffer, fire HA event `sat_cycle_started` with the sample (cycles/tracker.py:66-69, const.py:209).
2. **Flame on**: append the sample to the buffer (cycles/tracker.py:71-72).
3. **Flame ON->OFF** (cycles/tracker.py:74-84):
   - Remember `last_flame_off_timestamp` = this sample's timestamp (cycles/tracker.py:76).
   - Build the cycle with `end_time` = this sample's timestamp, using the PWM state and device state of this flame-off sample (cycles/tracker.py:78). The flame-off sample itself is not in the buffer, because only flame-on samples are appended (cycles/tracker.py:71-72).
   - If a cycle was built, record it in the history and fire `sat_cycle_ended` with the cycle and the sample (cycles/tracker.py:79-81, const.py:210).
   - Clear the buffer and the cycle start (cycles/tracker.py:83-84).
4. Remember the current flame state for the next pass (cycles/tracker.py:86).

#### 6. Building a finished cycle (`_build_cycle_state`, cycles/tracker.py:88-140)

1. If there is no start time, discard (cycles/tracker.py:90-92).
2. `duration` = max(0, end - start), `sample_count` = number of buffered flame-on samples (cycles/tracker.py:94-96).
3. If `sample_count` < `minimum_samples_per_cycle` (default 3), discard the cycle. It is not recorded and no end event fires (cycles/tracker.py:31, cycles/tracker.py:98-100, cycles/tracker.py:79-81).
4. Count hot water samples and space heating samples, and compute the two fractions (cycles/tracker.py:102-107). Determine the cycle kind (cycles/tracker.py:108, cycles/tracker.py:313-324).
5. Minimum and maximum flow temperature over all samples, ignoring missing values (cycles/tracker.py:110-111, helpers.py:178-184).
6. Define the time windows: warm-up = min(60 s, 25 % of duration); observation start = start + warm-up; tail start = max(observation start, end - 180 s) (cycles/tracker.py:113-116).
7. Compute three metric sets (cycles/tracker.py:118-120):
   - `metrics`: percentiles over **all** samples, with no warm-up exclusion (cycles/tracker.py:118, cycles/tracker.py:237-241).
   - `tail`: percentiles over samples with timestamp >= tail start (cycles/tracker.py:119, cycles/tracker.py:237-241).
   - `shape`: time-weighted metrics from observation start (cycles/tracker.py:120, cycles/tracker.py:163-232).
8. Classify the cycle (cycles/tracker.py:121, cycles/classifier.py:17-37).
9. `control_mode` = `PWM` if `pwm_state.enabled` at the flame-off sample, else `CONTINUOUS` (cycles/tracker.py:129). This test does not look at PWM status. `SatHeatingControl.control_mode` is different: it requires PWM enabled **and** status not `IDLE` (heating_control.py:147-152).

#### 7. Cycle classification (`CycleClassifier.classify`, cycles/classifier.py:17-37)

The first matching rule wins. All inputs are the **tail** metrics and the PWM/device state of the flame-off sample:

| # | Condition | Result | Citation |
|---|---|---|---|
| 1 | duration <= 0 | `INSUFFICIENT_DATA` | cycles/classifier.py:19-20 |
| 2 | kind is `UNKNOWN` | `UNCERTAIN` | cycles/classifier.py:22-23 |
| 3 | any hot water in the tail (`hot_water_active_fraction` > 0) | `UNCERTAIN` | cycles/classifier.py:25-26 |
| 4 | PWM enabled and PWM status `IDLE` | `UNCERTAIN` | cycles/classifier.py:28-29 |
| 5 | PWM enabled and status not `IDLE` | PWM rules (below) | cycles/classifier.py:31-32 |
| 6 | otherwise | continuous rules (below), or `GOOD` if they return nothing | cycles/classifier.py:34-37 |

**PWM rules** (`_classify_pwm`, cycles/classifier.py:40-66):

1. If p50 or p90 of `flow_requested_setpoint_error` is missing: `UNCERTAIN` (cycles/classifier.py:42-43).
2. Underheat if p90 of `flow_requested_setpoint_error` <= -3 °C (cycles/classifier.py:45). If underheat: effective setpoint = tail p50 of requested setpoint, else `device_state.setpoint`. If that is below `COLD_SETPOINT` (22 °C): `UNCERTAIN`, else `UNDERHEAT` (cycles/classifier.py:47-54).
3. If the PWM status is `ON` at flame-off: `SHORT_CYCLING` when the PWM on-time is unknown or <= 0, or when the cycle duration < 0.8 x the PWM on-time (cycles/classifier.py:56-64).
4. Otherwise `GOOD` (cycles/classifier.py:66).

**Continuous rules** (`_classify_continuous`, cycles/classifier.py:69-90):

1. If p50 or p90 of `flow_control_setpoint_error` is missing: `UNCERTAIN` (cycles/classifier.py:71-72).
2. Overshoot if p90 of `flow_control_setpoint_error` >= +3 °C. This is checked first and returns `OVERSHOOT` (cycles/classifier.py:74, cycles/classifier.py:77-78).
3. Underheat if p50 of `flow_control_setpoint_error` <= -3 °C. The same cold-setpoint guard applies (`UNCERTAIN` below 22 °C), else `UNDERHEAT` (cycles/classifier.py:75, cycles/classifier.py:80-88).
4. Otherwise no verdict, which `classify` turns into `GOOD` (cycles/classifier.py:90, cycles/classifier.py:34-37).

PWM cycles are judged against the **requested** setpoint, continuous cycles against SAT's **control** setpoint (cycles/classifier.py:42-45, cycles/classifier.py:71-75, cycles/tracker.py:267-277).

#### 8. Recording into history (`CycleHistory.record_cycle`, cycles/history.py:102-161)

1. Append a duration sample with the duration capped at 1800 s (cycles/history.py:104-111).
2. Append a delta sample with the **full-cycle** p50 values of flow-return delta, flow-control error and flow-requested error (cycles/history.py:113-118).
3. Append the classification (cycles/history.py:120-123).
4. Prune all three windows: drop entries whose end time is more than 24 h before this cycle's end (cycles/history.py:125-127, cycles/history.py:253-266).
5. Store this cycle as `last_cycle` (cycles/history.py:129).
6. Debug log. For cycles of 1800 s or longer, also log the shape metrics (cycles/history.py:131-161).

#### 9. How cycle data feeds control decisions

- **PWM on/off from the cycle verdict.** On `sat_cycle_ended` the heating control calls `PWM.on_cycle_end` (heating_control.py:167). A `CONTINUOUS` cycle classified `OVERSHOOT` enables PWM. A `PWM` cycle classified `UNDERHEAT` disables PWM, which also resets PWM state (pwm.py:219-227, pwm.py:106-109). No other classification changes anything here.
- **Flame-on timestamp for the PWM setpoint override.** This override runs only when `control_mode` is `PWM` (PWM enabled and status not `IDLE`), and only when there is no hot water, the requested setpoint is above 22 °C, overshoot protection is configured and the PWM status is not `OFF` (heating_control.py:147-152, heating_control.py:224-225, heating_control.py:385-390). With the flame on, `DeviceTracker.flame_on_since` gives the flame-on time. The elapsed time is the wall clock now (`timestamp()`) minus that value, while the value itself was stored from the pass timestamp (heating_control.py:410-414, helpers.py:12-19, device/__init__.py:86-88). Until `modulation_suppression_delay_seconds` has passed, the flame-off hold setpoint is kept (or nothing is changed if there is none). After that delay the control setpoint becomes flow - `modulation_suppression_offset_celsius` (heating_control.py:415-448). The PWM controller keeps its own separate `_flame_on_at` for its timing (pwm.py:144-149, pwm.py:164, pwm.py:201-203). The full PWM mechanism belongs to the PWM section of this spec.
- **Hot water off timestamp as a guard.** For 300 s after hot water switched off (`DHW_OVERSHOOT_GUARD_SECONDS`), the sustained-overshoot PWM enable is blocked and the PWM disable guards are reset (heating_control.py:272-274, heating_control.py:313-315, const.py:19, helpers.py:42-48).
- **Modulation reliability** is used only to choose how the modulation direction is computed for the status label (device/__init__.py:162-194).
- **Not consumed by control:** the rolling `CycleStatistics` are exposed as the `cycles` property of the heating control (heating_control.py:113-116), but nothing else in the repository reads that property. A search for `.cycles` found only that definition. The `BoilerStatus` label goes only to entities (sensor.py:470-487, binary_sensor.py:391-406). The off-with-demand duration ends up only inside those unread statistics (cycles/history.py:69-76).

### Formulas

Variable names are as in the code.

**Space heating demand (device tracker)** (device/status.py:127-132):
```
has_demand = setpoint is not None and flow_temperature is not None
             and setpoint > flow_temperature + BOILER_DEMAND_HYSTERESIS   # 0.7
```

**Overshoot at flame-off**, evaluated on the last flame-on snapshot (device/status.py:118-124, device/__init__.py:207):
```
did_overshoot = setpoint >= COLD_SETPOINT (22)
                and flow_temperature >= setpoint + BOILER_OVERSHOOT_DELTA (3.0)
```

**Overshoot cooling** (device/status.py:201-209): `last_flame_off_was_overshoot and not flame_active and flow_temperature > setpoint`.

**Anti-cycling** (device/status.py:171-186): `not flame_active and has_demand and 0 <= (last_update_at - last_flame_off_at) < 180`.

**Stalled ignition threshold** (device/status.py:157-168):
```
threshold = max(BOILER_STALL_IGNITION_MIN_OFF_SECONDS (600),
                last_cycle.duration * BOILER_STALL_IGNITION_OFF_RATIO (3.0))   # when last_cycle exists
stalled   = time_since_off >= threshold
```

**Post-cycle settling** (device/status.py:189-198): `not has_demand and 0 <= time_since_off <= 60`.

**Ignition surge ramp** (device/status.py:247-259):
```
(last_update_at - last_flame_on_at) <= 30
ramp_rate = (flow - previous.flow) / (last_update_at - previous_update_at)
surge     = ramp_rate >= 0.5      # °C per second, only when the flow rose
```

**Modulation direction** (device/__init__.py:162-194):
```
if modulation_reliable and both modulation values present:
    delta_mod = cur_mod - prev_mod
    if delta_mod >  3.0: return +1
    if delta_mod < -3.0: return -1
    # otherwise fall through to the flow gradient
delta_flow = cur_flow - prev_flow
+1 if delta_flow > 0.2;  -1 if delta_flow < -0.1;  else 0
```
"Previous" means the previous coordinator update, not a fixed time step (device/__init__.py:138-142).

**Modulation reliability** (device/modulation.py:21-46):
```
only when flame_active, relative_modulation_level known,
     max_modulation_level known and >= 3.0
keep the last 50 values
if fewer than 8 values: no verdict
window = last 8 values
above_threshold  = count(value >= 3.0 in window)
required_samples = max(2, int(8 * 0.4)) = 3
reliable = above_threshold >= required_samples
```

**Cycle kind** (cycles/tracker.py:102-108, cycles/tracker.py:313-324):
```
fraction_domestic_hot_water = count(hot_water_active) / sample_count
fraction_space_heating      = count(not hot_water_active and central_heating) / sample_count
DOMESTIC_HOT_WATER  if fraction_domestic_hot_water > 0.8 and fraction_space_heating < 0.2
CENTRAL_HEATING     if fraction_space_heating > 0.8 and fraction_domestic_hot_water < 0.2
MIXED               if fraction_domestic_hot_water > 0.1 and fraction_space_heating > 0.1
UNKNOWN             otherwise
```
The kind names are the `CycleKind` values (types.py:52-57).

**Cycle windows** (cycles/tracker.py:113-116):
```
effective_warmup  = min(60.0, duration * 0.25)
observation_start = start_time + effective_warmup
tail_start        = max(observation_start, end_time - 180.0)
```

**Per-sample signals** (cycles/tracker.py:261-292):
```
flow_return_delta             = flow_temperature - return_temperature
flow_control_setpoint_error   = flow_temperature - control_setpoint      # SAT's computed setpoint
flow_requested_setpoint_error = flow_temperature - requested_setpoint
```
A sample is skipped for a signal when one of its inputs is missing (cycles/tracker.py:243-250).

**Percentiles** (linear interpolation, helpers.py:187-203, used at cycles/tracker.py:252-259):
```
sorted values v[0..n-1];  n == 1 -> v[0];  empty -> None
position = (n - 1) * q;   i = int(position);  f = position - i
p = v[i] + (v[min(i+1, n-1)] - v[i]) * f       # q = 0.50 (p50), 0.90 (p90)
```

**Tail hot water fraction** (cycles/tracker.py:294-296): `count(hot_water_active in tail) / count(tail samples)`, or 0.0 when the tail is empty.

**Shape metrics** (cycles/tracker.py:163-232), over samples with timestamp >= `observation_start`. For each consecutive pair (current, next):
```
interval = max(0, next.timestamp - current.timestamp)
sp       = current.control_setpoint, or current.device_state.setpoint if None
err      = current.flow_temperature - sp
if err missing: overshoot streak = 0, no contribution
max_flow_control_setpoint_error = max over err
in_band   (abs(err) <= 1.0): time_in_band_seconds   += interval
overshoot (err >= 3.0):      total_overshoot_seconds += interval
    time_to_first_overshoot_seconds = max(0, current.timestamp - start_time)   (first time)
    streak += interval; when streak >= OVERSHOOT_SUSTAIN_SECONDS (60):
        time_to_sustained_overshoot_seconds = max(0, current.timestamp - start_time)   (first time)
else: streak = 0
```
Fewer than 2 relevant samples gives all zeros or `None` (cycles/tracker.py:168-175). Each interval gets the error of its first sample (cycles/tracker.py:186-197).

**Off-with-demand duration**, measured at flame OFF->ON (cycles/tracker.py:142-161):
```
if no previous flame-off: None
off_duration = max(0, now - last_flame_off_timestamp)
demand_present = not hot_water_active and central_heating
                 and setpoint is not None and flow_temperature is not None
                 and setpoint > flow_temperature          # no hysteresis here
demand_present -> off_duration
otherwise       -> None, and last_flame_off_timestamp is cleared
```
The demand is judged on the flame-on sample. This test has no 0.7 hysteresis, unlike `has_demand` (cycles/tracker.py:149-155, device/status.py:132).

**Window statistics** (cycles/history.py:167-236), with `now` = the latest end time stored in any window (cycles/history.py:238-251):
```
window samples   = entries with end_time >= now - window_seconds
sample_count     = number of recorded cycles in the window
duty_ratio       = clamp(sum(capped durations) / window_seconds, 0, 1)   # 0 when the sum is 0
median_on_duration_seconds = median(capped durations) or None
long_cycle_fraction = count(capped duration >= 600) / count
overshoot_fraction  = count(OVERSHOOT)  / count(classifications)
underheat_fraction  = count(UNDERHEAT)  / count(classifications)
delta percentiles   = p50, p90 over the stored full-cycle p50 values
```
`recent` uses 4 h and `daily` uses 24 h (cycles/history.py:69-100). The result types are in cycles/types.py:62-94. A cycle's `duration` is `max(0, end - start)` (cycles/types.py:56-59).

### Constants and defaults

| Name | Value | Unit | Citation |
|---|---|---|---|
| `minimum_samples_per_cycle` | 3 | samples | cycles/tracker.py:31 |
| Tail length | 180 | s | cycles/tracker.py:116 |
| Warm-up cap / fraction | 60 / 0.25 | s / of duration | cycles/tracker.py:114 |
| `IN_BAND_MARGIN_CELSIUS` | 1.0 | °C | cycles/const.py:3 |
| `MAX_ON_DURATION_SECONDS_FOR_ROLLING_WINDOWS` | 1800 | s | cycles/const.py:4 |
| `TARGET_MIN_ON_TIME_SECONDS` | 600 | s | cycles/const.py:7 |
| `OVERSHOOT_MARGIN_CELSIUS` | +3.0 | °C | cycles/const.py:10 |
| `UNDERSHOOT_MARGIN_CELSIUS` | -3.0 | °C | cycles/const.py:11 |
| `LAST_CYCLE_MAX_AGE_SECONDS` | 21600 (6 h) | s | cycles/const.py:14 |
| `DEFAULT_DUTY_WINDOW_SECONDS` | 900 (15 min) | s | cycles/const.py:17 |
| `DEFAULT_CYCLES_WINDOW_SECONDS` | 3600 (1 h) | s | cycles/const.py:18 |
| `DEFAULT_MEDIAN_WINDOW_SECONDS` ("recent") | 14400 (4 h) | s | cycles/const.py:19 |
| `DAILY_WINDOW_SECONDS` | 86400 (24 h) | s | cycles/const.py:20 |
| PWM short-cycle ratio | 0.8 | x PWM on-time | cycles/classifier.py:63 |
| Kind thresholds | 0.8 / 0.2 / 0.1 | fraction | cycles/tracker.py:315-321 |
| `OVERSHOOT_SUSTAIN_SECONDS` | 60 | s | const.py:21 |
| `COLD_SETPOINT` | 22 | °C | const.py:34 |
| `DHW_OVERSHOOT_GUARD_SECONDS` | 300 | s | const.py:19 |
| `EVENT_SAT_CYCLE_STARTED` / `_ENDED` | `sat_cycle_started` / `sat_cycle_ended` | event name | const.py:209-210 |
| `STORAGE_VERSION` (device store) | 1 | - | device/const.py:3 |
| `BOILER_PREHEAT_DELTA` | 6.0 | °C | device/const.py:6 |
| `BOILER_SETPOINT_BAND` | 1.5 | °C | device/const.py:7 |
| `BOILER_OVERSHOOT_DELTA` | 3.0 | °C | device/const.py:8 |
| `BOILER_DEMAND_HYSTERESIS` | 0.7 | °C | device/const.py:9 |
| `BOILER_ANTI_CYCLING_MIN_OFF_SECONDS` | 180 | s | device/const.py:10 |
| `BOILER_GRADIENT_THRESHOLD_UP` | 0.2 | °C per update | device/const.py:11 |
| `BOILER_GRADIENT_THRESHOLD_DOWN` | -0.1 | °C per update | device/const.py:12 |
| `BOILER_POST_CYCLE_SETTLING_SECONDS` | 60 | s | device/const.py:13 |
| `BOILER_RAMP_UP_WINDOW_SECONDS` | 30 | s | device/const.py:14 |
| `BOILER_RAMP_UP_RATE_CELSIUS_PER_SECOND` | 0.5 | °C/s | device/const.py:15 |
| `BOILER_MODULATION_DELTA_THRESHOLD` | 3.0 | % | device/const.py:16 |
| `BOILER_MODULATION_RELIABILITY_MIN_SAMPLES` | 8 | samples | device/const.py:17 |
| Modulation history length | 50 | samples | device/modulation.py:33-34 |
| Modulation required share | 0.4 (min 2) | of window | device/modulation.py:41 |
| `BOILER_STALL_IGNITION_OFF_RATIO` | 3.0 | x last cycle duration | device/const.py:18 |
| `BOILER_STALL_IGNITION_MIN_OFF_SECONDS` | 600 | s | device/const.py:19 |
| Coordinator notify debounce | 0.2 | s | coordinator/__init__.py:99 |
| Control loop timer | 5 | s | climate.py:708 |

The gradient thresholds are compared with the raw flow difference between two consecutive updates. The code does not divide by time (device/__init__.py:186-191).

### State kept between passes

**`DeviceTracker`** (device/__init__.py:22-42):
- `_current_state`, `_previous_state`, `_current_status`, `_last_update_at`, `_previous_update_at`.
- `_last_cycle`: a copy of the history's last cycle, refreshed every pass (device/__init__.py:136).
- `_last_flame_on_at`, `_last_flame_off_at`, `_last_flame_off_was_overshoot`, `_last_hot_water_on_at`, `_last_hot_water_off_at`.
- `_modulation_tracker`: holds `_reliable` (tri-state: `None`, `True` or `False`) and up to 50 flame-on modulation values (device/modulation.py:10-12, device/modulation.py:33-34).
- **Persisted:** only `modulation_reliable`, in HA storage `sat.device.<device_id>`. It is saved when the verdict changes and on unload (device/__init__.py:127-132, device/__init__.py:150-151, heating_control.py:169-179).

**`CycleTracker`** (cycles/tracker.py:35-42): the buffer of flame-on samples of the running cycle, `_last_flame_active`, `_current_cycle_start` (exposed as `started_since`, cycles/tracker.py:44-46), `_last_flame_off_timestamp`.

**`CycleHistory`** (cycles/history.py:40-46): three deques (durations, deltas, classifications) holding up to 24 h of entries counted back from the newest cycle end, `_last_cycle`, and `_off_with_demand_duration`. None of these is persisted; no storage code exists in cycles/history.py (cycles/history.py:1-266).

**MQTT coordinator data** (not part of this area, but it feeds every snapshot): the MQTT backends save all received values to HA storage `sat.mqtt.<device_id>` on unload and load them back at the next start (coordinator/mqtt/__init__.py:27, coordinator/mqtt/__init__.py:66-78).

### Outputs (to the boiler, to entities)

**To the boiler:** this area writes no OpenTherm commands itself. It influences commands indirectly:
- PWM enable/disable through `on_cycle_end` (pwm.py:219-227). This changes the control setpoint path (heating_control.py:224-228) and the maximum modulation SAT sends: with PWM enabled and no hot water it is `MINIMUM_RELATIVE_MODULATION` (0), at least 10 for Geminox boilers, otherwise the configured maximum, and `None` when the coordinator does not support relative modulation management (heating_control.py:136-140, heating_control.py:510-523, heating_control.py:236-237, const.py:37).
- `flame_on_since` timing in the PWM setpoint override (heating_control.py:410-448).
- The hot water off guard window (heating_control.py:272, heating_control.py:313).
- For OTGW MQTT, the resulting commands are `CS=` + `PM=25`, `MM=` and `CH=` (coordinator/mqtt/opentherm.py:195-199, coordinator/mqtt/opentherm.py:216-222, coordinator/mqtt/opentherm.py:211-214).

**HA events:** `sat_cycle_started` with `{"sample"}` (cycles/tracker.py:69) and `sat_cycle_ended` with `{"cycle", "sample"}` (cycles/tracker.py:81). No listener for `sat_cycle_started` exists in the repository; a search for `EVENT_SAT_CYCLE_STARTED` finds only its definition and the fire call (const.py:209, cycles/tracker.py:69).

**Entities:**
- **Cycle Status** sensor: shows the last cycle's classification name, or `INSUFFICIENT_DATA` when there is none or it is older than 6 h. It explicitly refreshes on `sat_cycle_ended` (sensor.py:421-439, cycles/history.py:48-57). Like all SAT entities it is also a `CoordinatorEntity` (entity.py:21-23), whose further refresh behaviour is Home Assistant framework code. Its attributes are kind, sample_count, duration_seconds, max_flow_temperature, both fractions, tail hot water fraction, tail p90 of both errors, tail p90 flow, tail p50 control and requested setpoint (sensor.py:441-463).
- **Cycle Health** binary sensor (problem class): on when the last cycle is `OVERSHOOT`, `UNDERHEAT` or `SHORT_CYCLING`, that is anything other than GOOD, UNCERTAIN or INSUFFICIENT_DATA (binary_sensor.py:414-442).
- **Device Status** sensor: the `BoilerStatus` name. It is unavailable while the status is `INSUFFICIENT_DATA` (sensor.py:470-487).
- **Device Health** binary sensor (problem class): on when the status is `INSUFFICIENT_DATA` (binary_sensor.py:391-406).
- The entities are registered at sensor.py:47-48 and binary_sensor.py:50-51.

### Edge cases

- **Flame already on at load.** On the first pass `_last_flame_active` is `None`, so a burning flame counts as OFF->ON. The cycle start becomes the first sample time, not the real ignition (cycles/tracker.py:57-69). The device tracker likewise sets `last_flame_on_at` to that first update (device/__init__.py:198-202). The off-with-demand duration is `None` because no previous flame-off is known (cycles/tracker.py:144-145).
- **Stale values at load (MQTT backends).** Until fresh MQTT messages overwrite them, the snapshot reads the values restored from `sat.mqtt.<device_id>` (coordinator/mqtt/__init__.py:71-74, coordinator/__init__.py:130-143). A `flame` value of `ON` saved at the last unload is therefore seen as a burning flame on the first pass, with the consequences of the previous point.
- **Two clock reads per pass.** The device tracker and the cycle tracker each call `event_timestamp(time)` separately (heating_control.py:245, heating_control.py:252). With no `time` argument each call reads the wall clock (helpers.py:12-19), so the two timestamps of one pass can differ slightly.
- **Very short flame bursts.** Fewer than 3 flame-on samples means the cycle is dropped silently: no history entry, no end event, `last_cycle` unchanged (cycles/tracker.py:98-100). Because sampling is per data change, the number of samples depends on how often MQTT values change (coordinator/__init__.py:381-398).
- **Cycle length vs. tail.** For a cycle shorter than 240 s, `end - 180` falls before observation start, so the tail starts at observation start (warm-up = 25 % of duration) (cycles/tracker.py:114-116).
- **Missing values.** Missing flow, return or setpoints are skipped per signal. If p50 or p90 are missing the cycle becomes `UNCERTAIN` (cycles/tracker.py:243-250, cycles/classifier.py:42-43, cycles/classifier.py:71-72). In the shape metrics a missing error resets the overshoot streak (cycles/tracker.py:199-202).
- **PWM state at flame-off decides the branch.** Classification and `control_mode` use the PWM state of the flame-off sample, not the state during the cycle (cycles/tracker.py:78, cycles/tracker.py:121, cycles/tracker.py:129). A cycle that ends while PWM is enabled but `IDLE` becomes `UNCERTAIN` (cycles/classifier.py:28-29). It is still recorded with `control_mode = PWM` (cycles/tracker.py:129).
- **Hot water.** Any hot water sample in the tail makes the cycle `UNCERTAIN` (cycles/classifier.py:25-26). A mixed cycle is not discarded, only labelled `MIXED` (cycles/tracker.py:321-322).
- **Low setpoint.** Underheat at a setpoint below 22 °C is reported as `UNCERTAIN`, not `UNDERHEAT` (cycles/classifier.py:51-52, cycles/classifier.py:85-86). Overshoot at flame-off is never flagged below 22 °C (device/status.py:121-122).
- **History time base.** The windows are measured back from the newest recorded cycle end, not from the wall clock (cycles/history.py:183-201, cycles/history.py:238-251). With no new cycles the window statistics freeze. `last_cycle` does use the wall clock and expires after 6 h (cycles/history.py:48-57, helpers.py:22-25).
- **Long cycles in statistics.** Durations above 1800 s are capped before they enter the duty ratio and median (cycles/history.py:105-111). Window membership is by end time only, so a cycle that started before the window counts fully (cycles/history.py:189).
- **Demand loss clears flame-off.** When `has_demand` becomes false, `last_flame_off_at` is cleared (device/__init__.py:144-145). The flame-off-based statuses (anti-cycling, stalled, settling) then restart from the next flame-off.
- **`POST_CYCLE_SETTLING` looks unreachable through `DeviceTracker.update` (derived from the order of the code).** The only non-test caller of the evaluator is the device tracker (device/__init__.py:155-160); the tests build snapshots by hand (tests/test_boiler_status.py:25, path relative to the repository root) and none of them checks `POST_CYCLE_SETTLING`. Rule 8 needs no demand and a set `last_flame_off_at` (device/status.py:189-198). On every pass without demand, `last_flame_off_at` is cleared before the flame transitions are recorded (device/__init__.py:144-147). So without demand it can only be set during the pass in which the flame goes out (device/__init__.py:204-206). On that pass the previous snapshot had the flame on, so rule 2 (`OVERSHOOT_COOLING`) or rule 5 (`COOLING`) returns first (device/status.py:42-48, device/status.py:68-70). No other code sets `last_flame_off_at` (device/__init__.py:33, device/__init__.py:145, device/__init__.py:206).
- **Modulation reliability while PWM is enabled.** With PWM enabled SAT commands maximum modulation `MINIMUM_RELATIVE_MODULATION` (0) (heating_control.py:139-140, heating_control.py:517-518, const.py:37). For OTGW MQTT `max_modulation_level` is read from `MaxRelModLevelSetting` (coordinator/mqtt/opentherm.py:152-157). If the device reports SAT's command back there, the reading is below 3.0 and the reliability tracker skips the update (device/modulation.py:28-29). Whether the reported value equals SAT's command depends on the gateway and is not shown in this code.

### Additional behaviour (completeness pass)

This subsection covers device-tracker code that the sections above did not cite.

**What the status evaluator gets to see.** Step 7 of the device tracker (`device/__init__.py:153`) builds one snapshot and passes it to `DeviceStatusEvaluator.evaluate` (`device/__init__.py:155-160`). The snapshot holds (`device/__init__.py:63-84`):

- the current `DeviceState` and the stored `last_cycle`;
- the time of this update and of the previous update, and the previous `DeviceState`;
- `last_flame_on_at`, `last_flame_off_at` and `last_flame_off_was_overshoot`;
- the hot water on and off times;
- the modulation direction, computed at that moment (`device/__init__.py:83`).

If no current state exists yet, there is no snapshot and the status becomes `OFF` (`device/__init__.py:156-158`). Before the first evaluation the `status` property reports `INSUFFICIENT_DATA` (`device/__init__.py:44-49`).

**Where the per-backend maximum modulation comes from.** `max_modulation_level` is the coordinator's `maximum_relative_modulation_value` (`coordinator/__init__.py:142`). The key differs per backend: EMS-ESP `burnmaxpower` (`coordinator/mqtt/ems.py:26`, `coordinator/mqtt/ems.py:94-96`); OTthing the slave key `"min_modulation"` (`coordinator/mqtt/otthing.py:28`, `coordinator/mqtt/otthing.py:146-151`); serial `DATA_SLAVE_MAX_RELATIVE_MOD` (`coordinator/serial/__init__.py:147-152`); ESPHome the number `max_rel_mod_level` (`coordinator/esphome/__init__.py:195-197`). The full per-backend table is in the I/O section's completeness pass. The reliability tracker skips every update while this value is missing or below 3.0 (`device/modulation.py:26-29`). As stated in section 9, that only affects the status label.

### Not found in the code / unclear

1. **Meaning of `centralheating` and `domestichotwater` on OTGW MQTT.** SAT only compares these topic values with `"ON"` (coordinator/mqtt/opentherm.py:59-69). Which OpenTherm status bit they carry (master enable or slave active) is defined by the gateway firmware, not by SAT. The constant is named `DATA_DHW_ENABLE` (coordinator/mqtt/opentherm.py:22), while the serial backend uses `DATA_SLAVE_DHW_ACTIVE` (coordinator/serial/__init__.py:58-59). The two backends may therefore feed different signals into `hot_water_active`.
2. **`TARGET_MIN_ON_TIME_SECONDS` comment vs. use.** The comment says "Minimum cycle duration required for classification" (cycles/const.py:6-7). The constant is used only for `long_cycle_fraction` (cycles/history.py:180). The classifier has no minimum-duration rule other than duration <= 0 (cycles/classifier.py:19-20).
3. **Unused constants.** `DEFAULT_DUTY_WINDOW_SECONDS` and `DEFAULT_CYCLES_WINDOW_SECONDS` are defined (cycles/const.py:17-18) but not referenced anywhere else in the repository.
4. **Unused statistics.** `SatHeatingControl.cycles` (heating_control.py:113-116) has no reader in the repository. The history counts cycles per 4 h and 24 h window (`sample_count`, cycles/history.py:226), but has no per-hour cycle count. PWM counts its own ON phases per rolling hour (pwm.py:138-141, pwm.py:156-160).
5. **`CycleTracker.reset()` and `SatHeatingControl.reset()`** are never called (cycles/tracker.py:48-53, heating_control.py:192-195). The code does not show that the device or cycle tracking state is ever reset while the integration runs, for example after an HVAC mode change.
6. **Exact listener timing.** The listener call and the debouncer are Home Assistant framework code, not in this repository (coordinator/__init__.py:96-102). The listener has an optional `time` argument and falls back to wall-clock "now" when it is absent (heating_control.py:242, helpers.py:17-19). Whether HA ever passes a time, and how the debouncer merges several changes that arrive within 0.2 s into one pass, is not verified here.
7. **`hot_water_active_fraction` in full-cycle metrics** is computed (cycles/tracker.py:294-296), but only the tail value is used, by the classifier and the Cycle Status sensor (cycles/classifier.py:25, sensor.py:457).
8. **`min_flow_temperature`** is stored in the cycle (cycles/tracker.py:135) but not exposed by the Cycle Status sensor, which shows only `max_flow_temperature` (sensor.py:454).
9. **No energy, gas or ignition counters** exist in this area. No flame-failure or lockout detection based on OpenTherm fault flags exists in cycles/ or device/. `STALLED_IGNITION` is purely time based (device/status.py:135-168).
10. **First pass after load.** Whether Home Assistant triggers a pass when the listener is added (heating_control.py:166), and whether the gateway's MQTT broker delivers retained values right after subscription (coordinator/mqtt/__init__.py:55-60), are outside this repository. So the exact moment of the first tracking pass, and whether it sees restored or fresh values, is not shown in the code.
11. **`CycleTracker._classify_cycle`** (cycles/tracker.py:326-334) is a wrapper around the classifier that only the tests call (tests/test_cycles.py:138, path relative to the repository root, not to custom_components/sat). `_build_cycle_state` calls `CycleClassifier.classify` directly (cycles/tracker.py:121).

## 6. Areas, presets, window detection and thermostat sync

Source: Alexwijn/SAT, branch `releases/thermo-nova`, commit 7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb. All paths are relative to `custom_components/sat`. Every statement below cites a line range that was opened and read. Worked examples are marked "computed" and follow only from the cited formulas. Statements marked "follows from the code, not run" are derived by reading the code; they were not executed.

### Purpose

SAT runs one main thermostat entity (`SatClimate`) that owns one target room temperature, one heating curve and one PID controller (climate.py:77, climate.py:112-114). On top of that it can follow extra rooms ("areas"). Each area is another Home Assistant climate entity with its own heating curve and its own PID (area.py:37-50, area.py:218-220). The area results are folded into the single flow setpoint request that goes to the heating control layer (climate.py:369-394, climate.py:547-555).

This section also covers:

- presets (fixed target temperatures such as home, away, sleep) (climate.py:87, climate.py:867-871, entry_data.py:257-276);
- pushing mode and preset changes out to the linked climate entities (climate.py:570-639);
- following a separate wall thermostat entity (climate.py:750-765);
- window open detection, which switches to the "activity" preset (climate.py:800-834);
- how the target temperature is set, restored and limited (climate.py:169-212, climate.py:557-568, climate.py:641-667).

There are three kinds of linked climate entities in the configuration:

| Config key | Code name | Role | Citation |
|---|---|---|---|
| `main_climates` | `radiators` | Climates that receive SAT's own target temperature; used for valve detection | const.py:58, entry_data.py:157-159, climate.py:651-653, climate.py:418-422 |
| `secondary_climates` | `rooms` (areas) | Rooms with their own target, own PID, combined into one demand | const.py:56, entry_data.py:161-163, area.py:218-220 |
| `thermostat` | `thermostat` | One climate entity whose target is copied into SAT | const.py:46, entry_data.py:165-167, climate.py:730-735 |

All three are picked in the "areas" step of the setup form; the thermostat is single, the other two allow multiple entities (config_flow.py:435-457). All three are read from the config entry `data`, not from the options (entry_data.py:157-167). The options menu has the steps general, presets, system configuration and (advanced users) advanced (config_flow.py:583-592); none of these step forms contains these three lists (config_flow.py:594-802). They can be changed through the reconfigure flow, which copies the entry data, passes through the "areas" step again and writes the result back to `data` (config_flow.py:347-351, config_flow.py:374, config_flow.py:405, config_flow.py:538-544).

### Inputs

Main thermostat:

- Inside temperature sensor `inside_sensor_entity_id` (entry_data.py:192, climate.py:271-279). If "thermal comfort" is on, the value used is `SummerSimmer.index(temperature, humidity)` instead of the raw temperature (climate.py:276-277). A stale, `unknown` or `unavailable` sensor gives no value (climate.py:669-686).
- Outside temperature, one or more entities; entities whose id contains `sensor` are tried before the others (for example `weather.*`); the first non-stale, known value wins (climate.py:317-343).
- Sensor maximum age `sensor_max_value_age_seconds` (entry_data.py:199-201). A state is stale when its age since `last_updated` exceeds this value; a value of 0 or less disables the check (helpers.py:59-69).
- The user-set target temperature and preset (climate.py:557-568, climate.py:604-639).
- Preset temperatures: read from the options for away, home, sleep, comfort and activity, and converted to float (entry_data.py:257-276).

Per area (room):

- The room climate entity state; `unknown` and `unavailable` are treated as no state (area.py:59-65).
- Current room temperature: taken from the climate attribute `current_temperature` (area.py:97-98), unless the climate entity carries an attribute `sensor_temperature_id`. In that case the referenced entity's state is used, provided that entity is not `unknown`, `unavailable` or `off` (area.py:72-80). Its state value is only read when its entity id contains `sensor`; any other entity id gives no temperature (area.py:94-95, area.py:100). Stale sources give no temperature (area.py:76-78, area.py:82-84).
- Room target temperature: the climate attribute `temperature` (area.py:102-108).
- Optional valve position: the climate attribute `current_valve_position` (area.py:29, area.py:131-146).

Window sensors:

- A list of binary sensors (`window_sensors`, read from options) (entry_data.py:169-171). They are picked in the "general" step of the options form, which offers door, window and garage door binary sensors and excludes SAT's own window group sensor (config_flow.py:576, config_flow.py:594, config_flow.py:625-639).
- Minimum open time `window_minimum_open_time_seconds`, parsed from `HH:MM:SS` (entry_data.py:196-198, helpers.py:72-81).

### Behaviour (step by step, in time order)

#### 1. Start-up of the main thermostat

1. The preset table is built from configuration when the entity is created (climate.py:87, entry_data.py:257-276). The available preset list is `none` plus the configured presets (climate.py:101). Supported HVAC modes are only `off` and `heat` (climate.py:100).
2. One `Area` is created per configured room (climate.py:112, area.py:218-220).
3. When the entity is added, the previous state is restored, or defaults are set (climate.py:128, climate.py:169-212):
   - With a saved state: target temperature from the saved `temperature` attribute; if absent, `min_temp` (climate.py:176-182). HVAC mode from the saved state (climate.py:184-185). Preset, `pre_activity_temperature` and `pre_custom_temperature` from saved attributes, each only when the saved value is truthy (climate.py:187-194). Per-room "home" temperatures (`rooms`) from the saved attribute, else read fresh from the room climates (climate.py:196-199).
   - Without a saved state: per-room temperatures read from the room climates (climate.py:201-202), target temperature `min_temp` (climate.py:204-207), HVAC mode `off` (climate.py:209-210).
4. The heating control layer is started, and the main PID loads its stored state (climate.py:130-131, pid.py:141-155).
5. If Home Assistant is already running (for example on an integration reload), SAT now updates the heating curves, registers its listeners, runs the main PID once and schedules the control loop. Otherwise these four actions are registered to run on the `homeassistant_started` event (climate.py:133-144).
6. Services are registered and the coordinator is started (climate.py:146-147).
7. The areas are started (climate.py:148). Each area loads its stored PID state, runs its PID once (now, or on the `homeassistant_started` event) and starts a 60 s PID timer (area.py:177-190, pid.py:20, pid.py:141-155).
8. A stop listener is registered for Home Assistant shutdown (climate.py:150).

Fetching per-room temperatures at start-up (climate.py:836-853): for each room that has a known state and a `temperature` attribute, `_rooms[entity_id] = float(temperature)`.

Start-up gap on a reload (follows from the code, not run): when Home Assistant is already running, the heating curve update in step 5 (climate.py:134) runs before the areas receive their Home Assistant handle in step 7 (area.py:179). Without that handle an area's target temperature is `None` (area.py:105-106), so its curve update is skipped (area.py:261-262). The area PID then skips its updates because its heating curve has no value (pid.py:161-163), and the area is not "available" (pid.py:56-59). The rooms therefore add nothing to the demand until the next heating curve update, which only happens on an inside or outside sensor change, a room target change, or a SAT target change (climate.py:767-773, climate.py:795-796, climate.py:662-663). On a cold Home Assistant boot the areas get their handle before the `homeassistant_started` event fires (climate.py:140, climate.py:148), so this gap does not occur there.

#### 2. Periodic timers (after listeners are registered)

| What | Interval | Citation |
|---|---|---|
| Control loop request (debounced) | every 5 s | climate.py:706-710 |
| Debounce delay before the loop runs | 1 s, a pending run is not re-armed | climate.py:526-531 |
| Main PID update | every 60 s (`PID_UPDATE_INTERVAL`) | climate.py:712-716, pid.py:20 |
| Each area PID update | every 60 s (`PID_UPDATE_INTERVAL`) | area.py:188-190, pid.py:20 |

State-change listeners are registered for: the inside sensor, the outside sensor(s), the thermostat (if configured), all room climates, and every window sensor (if any) (climate.py:718-748).

There is no periodic heating curve update. The main and area curves are recalculated only at start-up, on a SAT target change, on an inside or outside sensor change and on a room target change (climate.py:134, climate.py:140, climate.py:663, climate.py:772, climate.py:796).

#### 3. Per area PID tick (every 60 s)

1. Build the area temperature state from the room's target and current temperature (area.py:110-129). If either is missing or the source is stale, the tick is skipped; the area PID is not reset (area.py:202-206).
2. Otherwise the area PID is updated with that state (area.py:208). The PID formula itself (output = heating curve + P + I + D) is in pid.py:126-132 and belongs to the PID section. After each update the PID state is saved to storage (pid.py:179-181, pid.py:239-251).

Contrast with the main PID: a stale inside sensor resets the main PID and returns (climate.py:468-478).

#### 4. Per control loop tick: combining main and rooms into one requested setpoint

The control loop returns immediately when there is no outside temperature or the HVAC mode is `off` (climate.py:544-545). Otherwise it hands a `HeatingDemand` (HVAC mode, valves open flag, timestamp, requested setpoint, outside temperature) to the heating control layer (climate.py:547-555, heating_control.py:55-62). What heating control then does with it is out of scope here.

The requested setpoint is computed as follows (climate.py:369-394):

1. If the main heating curve has no value yet, return `MINIMUM_SETPOINT` (10.0 °C) (climate.py:372-373, const.py:35).
2. Start from the main PID output (climate.py:375).
3. Heating mode ECO: use only the main PID. Round to 0.1 and clamp between 10.0 °C and the maximum setpoint (climate.py:377-379).
4. Heating mode COMFORT (default) (const.py:183):
   1. `secondary_heating = areas.pids.output` (aggregated room demand, see Formulas) (climate.py:382).
   2. `overshoot_cap = areas.pids.overshoot_cap` (climate.py:383).
   3. `setpoint = max(setpoint, secondary_heating)` (climate.py:385-386).
   4. If a cap exists: `setpoint = min(setpoint, overshoot_cap)` (climate.py:388-389). The cap therefore also limits the main PID's own demand.
   5. If solar gain is active and enabled, subtract `solar_gain.setpoint_offset_celsius` (climate.py:391-392). Solar gain is described in its own section.
   6. Clamp between 10.0 °C and the maximum setpoint (climate.py:394).

The heating mode selector only appears in the options form when at least one room is configured (config_flow.py:604-610).

Which rooms count (area.py:165-175, area.py:278-280):

- A room counts only if its PID is "available" (it has a last error and its heating curve has a value) (pid.py:56-59), and it "requires heat".
- "Requires heat" with a valve position: `valve_position >= 10.0` (area.py:168-170).
- "Requires heat" without a valve position: area PID output `> COLD_SETPOINT` (22 °C) (area.py:172-175, const.py:34).

The room errors are also used outside the setpoint: on each main PID tick, the median of the main error and all room errors is recorded in the temperature history when the mode is `heat` and the valves are open (climate.py:396-412, climate.py:496-497). That history belongs to another section.

#### 5. Event: a room climate changes

On a state change of any room climate (climate.py:775-798):

1. Ignore if there is no new state, if the per-room table is not loaded, or if the new state has no `temperature` attribute (climate.py:779-786).
2. Compare `float(target)` with `_rooms.get(entity_id, target)` and ignore the event when they are equal (climate.py:788-789). For a room that is already in `_rooms`, this ignores an unchanged target. For a room that is not in `_rooms`, the default of the lookup is the new value itself, so for a numeric attribute the two are equal and the event is ignored as well (follows from the code, not run).
3. Store the new target in `_rooms` if the room was not yet in the table, or the current preset is `home` (climate.py:791-793). Because of step 2, in practice only the `home` branch stores a value for a numeric attribute. So `_rooms` holds each room's "home" temperature, learned while the preset is `home`.
4. Reset that room's PID (integral and derivative to 0) and update all heating curves (climate.py:795-796, area.py:335-338, pid.py:134-137).

Step 4 runs on every target change that passed step 2, also when step 3 did not store it (climate.py:791-796). A room can be missing from `_rooms` in two ways: its climate was missing, `unknown` or `unavailable` (or had no `temperature`) when the table was filled at start-up (climate.py:841-853, called from climate.py:199 and climate.py:202), or it was added to the configuration after `_rooms` was restored from a saved state (climate.py:196-197). Such a room never gets an entry through a target change, its PID is not reset on a target change, and a `home` preset sync sends it the home preset temperature instead (climate.py:636).

#### 6. Event: the user sets a temperature

`async_set_temperature` (climate.py:557-568):

1. No temperature given: do nothing (climate.py:559-560).
2. If the value exactly equals a preset temperature, switch to that preset instead (first match wins) (climate.py:562-565).
3. Otherwise set preset `none` and set the target temperature (climate.py:567-568).

Consequence of step 2: if the value equals the temperature of the preset that is already active, `async_set_preset_mode` returns without doing anything (climate.py:609-611). After a thermostat sync the real target can differ from the active preset's value, because the sync does not touch the preset (climate.py:765, climate.py:641-667). Setting the preset's value by hand then leaves the target unchanged (follows from the code, not run).

`async_set_target_temperature(temperature, cascade=True)` (climate.py:641-667):

1. If the value equals the current target, do nothing (climate.py:643-644).
2. Store it as the new target (climate.py:647).
3. If `cascade`: send `climate.set_temperature` with this value to every radiator (`main_climates`) (climate.py:649-653); and if `push_setpoint_to_thermostat` is set, call the coordinator to push it to the thermostat (climate.py:655-657).
4. Reset the main PID and all area PIDs (climate.py:659-660, climate.py:461-464).
5. Update heating curves, schedule the control loop, write the entity state (climate.py:662-667).

A PID reset sets the integral and the raw derivative to 0 and keeps the last error, so the PID stays "available" after a reset (pid.py:134-137, pid.py:56-59).

#### 7. Event: the user sets a preset

`async_set_preset_mode(preset_mode)` (climate.py:604-639):

1. Unknown preset: raise an error (climate.py:606-607).
2. Same as the current preset: do nothing (climate.py:609-611).
3. Preset `none`: set preset `none` and restore the target to `_pre_custom_temperature` (climate.py:613-616).
4. Any other preset:
   1. If leaving `none`, remember the current target as `_pre_custom_temperature` (climate.py:618-620).
   2. Set the preset and set the target to the preset temperature (with cascade to radiators and optional thermostat push, see step 6) (climate.py:622-624).
   3. If `sync_climates_with_preset` is on, for each room that exists and is not `off`: send `climate.set_temperature`. For preset `home` the value is the room's stored home temperature from `_rooms`, falling back to the home preset temperature; for any other preset it is the preset temperature (climate.py:626-639).

#### 8. Event: the user sets the HVAC mode

`async_set_hvac_mode(hvac_mode, cascade=True)` (climate.py:570-602):

1. Only `heat` and `off` are accepted; anything else is logged and ignored (climate.py:572-580).
2. Reset the main PID and all area PIDs (climate.py:582-583).
3. Climates to update: all radiators, plus all rooms if `sync_climates_with_mode` is on (default on) (climate.py:585-588, const.py:139).
4. If `cascade` (the default), call `climate.set_hvac_mode` for each (climate.py:590-598). A grep of all `.py` files in `custom_components/sat` for `async_set_hvac_mode` and `cascade` found no call of this method inside the integration (only its definition at climate.py:570), so inside SAT there is no path that uses `cascade=False`. How Home Assistant itself calls it is outside this repository.
5. Schedule the control loop and write the entity state (climate.py:600-602).

#### 9. Event: the linked wall thermostat changes

`_async_thermostat_changed` (climate.py:750-765):

1. Ignore if the old or new state is missing, `unavailable` or `unknown` (climate.py:752-758).
2. If the thermostat's state or its `temperature` attribute changed, set SAT's target to the thermostat's `temperature` with `cascade=False` (climate.py:760-765).
3. With `cascade=False`, the value is not sent to radiators and not pushed back to the thermostat (climate.py:649-657). The PIDs are still reset and the loop is scheduled (climate.py:659-667). The preset is not changed, because `async_set_target_temperature` does not touch it (climate.py:641-667).

#### 10. Event: a window sensor changes (window open detection)

`_async_window_sensor_changed` (climate.py:800-834):

1. Ignore if the new state is missing, `unavailable` or `unknown` (climate.py:802-804).
2. Sensor `on` (open):
   1. If the preset is already `activity`, do nothing (climate.py:808-810).
   2. Start a sleep of `window_minimum_open_time_seconds` (default 15 s) (climate.py:812-815, const.py:172).
   3. Before waiting, save `_pre_activity_temperature = target_temperature or min_temp` (climate.py:816).
   4. After the wait, switch to preset `activity` (climate.py:818-819). If the wait is cancelled, only a debug message is logged (climate.py:820-821).
3. Sensor `off` (closed):
   1. Cancel a pending wait, if any (climate.py:826-828).
   2. If the preset is `activity`, call `async_set_temperature(_pre_activity_temperature)` (climate.py:830-832). Because this goes through `async_set_temperature`, the preset afterwards is whichever preset exactly matches that value, or `none` (climate.py:562-568).

So "window open" in thermo-nova means: after the minimum open time, SAT switches to the `activity` preset, whose default temperature is 10 °C (const.py:175). There is no separate "heating off" action in this code path. The preset switch sends what any preset switch sends: the activity temperature to the radiators, a thermostat push if enabled, and the activity temperature to the rooms if `sync_climates_with_preset` is on (climate.py:622-639, climate.py:649-657).

Rooms after a window close (follows from the code, not run): if `_pre_activity_temperature` matches no preset, the restore goes through `async_set_target_temperature`, which only reaches the radiators and the thermostat (climate.py:567-568, climate.py:649-657). Rooms that were set to the activity temperature by the preset sync stay at that temperature. Only when the restored value matches a preset does the preset sync run again for the rooms (climate.py:562-565, climate.py:626-639).

A window binary sensor group entity ("Window Sensor", mode `any`) is also created when window sensors are configured (binary_sensor.py:56-57, binary_sensor.py:450-471). The climate entity does not listen to that group; it listens to each configured sensor directly (climate.py:743-748). When the climate entity is removed, a pending window wait is cancelled (climate.py:157-159).

#### 11. Inside or outside sensor changes

Any state change with a new state updates the heating curves and writes the entity state (climate.py:767-773). The check at climate.py:769 compares the new `State` object with the strings `unavailable` and `unknown`, so it never matches; only a missing new state is filtered (follows from the code, not run). Heating curve update for the main curve uses SAT's target; each area curve uses that room's own target and the shared outside temperature (climate.py:688-697, area.py:259-262). Nothing is updated when SAT's target or the outside temperature is missing (climate.py:693-694).

#### 12. Service: reset integral

The `reset_integral` service resets the main PID and all area PIDs (climate.py:217-222).

#### 13. Shutdown

On removal, the control loop schedule and a pending window wait are cancelled, and every area stops its PID timer (climate.py:152-167, area.py:192-196).

### Formulas

Area temperature error (used for overshoot) (temperature/state.py:15-18):

```
error = round(setpoint - current, 3)
```

Aggregated room demand `areas.pids.output` (area.py:272-301), with `percentile = 0.75`, `headroom = 5.0` (area.py:267):

```
outputs = sorted(pid.output of every area that is available and requires heat)
if outputs is empty: return MINIMUM_SETPOINT            # 10.0
index    = max(0, min(len(outputs) - 1, int(len(outputs) * 0.75)))
baseline = outputs[index]
chosen   = min(outputs[-1], baseline + 5.0)
return round(chosen, 1)
```

Computed examples:

- 1 to 4 rooms: `int(n * 0.75)` equals `n - 1`, so `baseline` is the highest output and `chosen` is the highest output. The headroom has no effect.
- 3 rooms with outputs 30, 35, 45: index 2, baseline 45, chosen min(45, 50) = 45.
- 5 rooms with outputs 30, 32, 34, 36, 48: index int(3.75) = 3, baseline 36, chosen min(48, 41) = 41.

Overshoot cap `areas.pids.overshoot_cap` (area.py:303-333), with `COLD_SETPOINT = 22`, `COOLING_HEADROOM = 10.0`, `COOLING_SLOPE = 4.0`, `OVERSHOOT_MARGIN = 0.3` (const.py:34, area.py:32-34):

```
for each area that is available and requires heat and has a temperature state:
    if error >= -0.3: skip                  # not more than 0.3 °C above target
    degrees_over = -error
    caps.append(22 + 10.0 - 4.0 * degrees_over)
if no caps: return None
cap = max(22, min(caps))
return round(cap, 1)
```

Computed examples: 0.5 °C over gives 30.0 °C; 1.0 °C over gives 28.0 °C; 2.5 °C over gives 22.0 °C, which is also the floor; 3.0 °C over gives 20, raised to the floor 22.0 °C.

Requested setpoint, COMFORT mode (climate.py:375-394):

```
setpoint = main_pid.output
setpoint = max(setpoint, areas_output)
if overshoot_cap is not None: setpoint = min(setpoint, overshoot_cap)
if solar_gain active and enabled: setpoint -= solar_gain.setpoint_offset_celsius
requested = clamp(setpoint, 10.0, maximum_setpoint)
```

Requested setpoint, ECO mode (climate.py:377-379):

```
requested = clamp(round(main_pid.output, 1), 10.0, maximum_setpoint)
```

`clamp(value, low, high) = max(low, min(value, high))` (helpers.py:140-145). The maximum setpoint is the configured value, or by default 50 °C for underfloor and 55 °C otherwise (coordinator/__init__.py:274-283, entry_data.py:227-239, helpers.py:97-99). The switch coordinator overrides this and returns its minimum setpoint as the maximum (coordinator/switch/__init__.py:41-43).

Area heating curve (shared formula, own target per room) (heating_curve.py:27-39):

```
value = round(base_offset + (coefficient / 4) * (4*(target - 20) + 0.03*(outside - 20)^2 - 0.4*(outside - 20)), 1)
```

`base_offset` is 20 for underfloor and 27.2 for every other heating system (types.py:116-118). The coefficient is `heating_curve_coefficient`, default 2.0 (heating_curve.py:22-25, entry_data.py:213, const.py:182).

Valves open (climate.py:414-455): with no radiators configured, always `True` (climate.py:420-422). Otherwise, over radiators plus rooms, skipping missing, unknown, unavailable and `off` climates: `True` if any reports `hvac_action == heating`, or, when it has no `hvac_action`, if `target >= current + climate_valve_offset` (climate.py:424-452). Else `False` (climate.py:454-455).

### Constants and defaults

| Name | Value | Unit | Citation |
|---|---|---|---|
| `COLD_SETPOINT` | 22 | °C | const.py:34 |
| `MINIMUM_SETPOINT` | 10.0 | °C | const.py:35 |
| `MAXIMUM_SETPOINT` (constant) | 65.0 | °C | const.py:36 |
| Default maximum setpoint, underfloor / other | 50 / 55 | °C | helpers.py:97-99 |
| `OVERSHOOT_MARGIN` | 0.3 | °C | area.py:33 |
| `COOLING_HEADROOM` | 10.0 | °C | area.py:34 |
| `COOLING_SLOPE` | 4.0 | °C per °C over | area.py:32 |
| Room aggregation percentile | 0.75 | fraction | area.py:267 |
| Room aggregation headroom | 5.0 | °C | area.py:267 |
| Valve "requires heat" threshold | 10.0 | unit of `current_valve_position` (not stated) | area.py:170 |
| `PID_UPDATE_INTERVAL` | 60 | s | pid.py:20 |
| Control loop timer | 5 | s | climate.py:706-710 |
| Control loop debounce | 1 | s | climate.py:530 |
| Heating curve `base_offset`, underfloor / other | 20 / 27.2 | °C | types.py:116-118 |
| `CONF_HEATING_CURVE_COEFFICIENT` default | 2.0 | | const.py:182 |
| `CONF_HEATING_MODE` default | comfort | | const.py:183, types.py:121-123 |
| `CONF_SYNC_CLIMATES_WITH_MODE` default | True | | const.py:139 |
| `CONF_SYNC_CLIMATES_WITH_PRESET` default | False | | const.py:140 |
| `CONF_PUSH_SETPOINT_TO_THERMOSTAT` default | False | | const.py:138 |
| `CONF_WINDOW_SENSORS` default | empty list | | const.py:130 |
| `CONF_WINDOW_MINIMUM_OPEN_TIME` default | 00:00:15 (15) | s | const.py:172 |
| `CONF_SENSOR_MAX_VALUE_AGE` default | 06:00:00 (21600) | s | const.py:170 |
| `CONF_THERMAL_COMFORT` default | False | | const.py:131 |
| `CONF_TARGET_TEMPERATURE_STEP` default | 0.5 | °C | const.py:169 |
| Target temperature step form range | 0.1 to 1, step 0.05 | °C | config_flow.py:730-733 |
| `CONF_CLIMATE_VALVE_OFFSET` default | 0 | °C | const.py:168 |
| Climate valve offset form range | -1 to 1, step 0.1 | °C | config_flow.py:734-737 |
| Preset `activity` default | 10 | °C | const.py:175 |
| Preset `away` default | 10 | °C | const.py:176 |
| Preset `home` default | 18 | °C | const.py:177 |
| Preset `sleep` default | 15 | °C | const.py:178 |
| Preset `comfort` default | 20 | °C | const.py:179 |
| Preset form range (all five) | 5 to 35, step 0.5 | °C | config_flow.py:648-668 |
| Maximum setpoint number entity range | 30 to 80 | °C | number.py:115-123 |
| Hot water setpoint number entity range | 30 to 60 | °C | number.py:62-70 |

Options are the defaults overlaid by the user's saved options (__init__.py:29).

### State kept between passes

| State | Meaning | Persisted | Citation |
|---|---|---|---|
| `_target_temperature` | SAT target room temperature | via HA restore state (`temperature` attribute) | climate.py:90, climate.py:176-182 |
| `_hvac_mode` | `heat` or `off` | via HA restore state | climate.py:92, climate.py:184-185 |
| `_attr_preset_mode` | current preset | via HA restore state | climate.py:99, climate.py:187-188 |
| `_pre_custom_temperature` | target before leaving preset `none` | attribute `pre_custom_temperature` | climate.py:91, climate.py:193-194, climate.py:238 |
| `_pre_activity_temperature` | target before a window switched to `activity` | attribute `pre_activity_temperature` | climate.py:93, climate.py:190-191, climate.py:239 |
| `_rooms` | per-room "home" target temperatures | attribute `rooms` | climate.py:86, climate.py:196-199, climate.py:241 |
| `_window_sensor_handle` | the pending minimum-open-time wait | no | climate.py:94, climate.py:813-815 |
| Area PID state (`integral`, `last_error`, `raw_derivative`, `last_temperature`, `last_derivative_updated`) | per room | HA store `sat.pid.<entity_id>.<device_id>`, saved after every PID update | pid.py:141-155, pid.py:179-181, pid.py:239-251 |

### Outputs (to the boiler, to entities)

To the heating control layer (and from there to the boiler): one `HeatingDemand` with `requested_setpoint`, `hvac_mode`, `valves_open` and `outside_temperature` (climate.py:547-555). Nothing in this section sends OpenTherm commands directly, except the thermostat push below.

To other Home Assistant entities:

- `climate.set_temperature` to every radiator on each target change with cascade (climate.py:649-653).
- `climate.set_temperature` to rooms on preset change, when `sync_climates_with_preset` is on (climate.py:626-639). A window opening is such a preset change (climate.py:819).
- `climate.set_hvac_mode` to radiators, and to rooms when `sync_climates_with_mode` is on (climate.py:585-598).

Thermostat setpoint push (only when `push_setpoint_to_thermostat` is true and cascade is on) (climate.py:655-657):

- MQTT OpenTherm (OTGW) coordinator publishes the command string `TC=<value>` (coordinator/mqtt/opentherm.py:206-209).
- Serial coordinator calls `set_target_temp(value)` unless simulation is on (coordinator/serial/__init__.py:204-208).
- EMS coordinator: marked "Not supported (yet)" (coordinator/mqtt/ems.py:120-122).
- Base coordinator: no action (coordinator/__init__.py:377-379). A grep of the whole integration for `async_set_control_thermostat_setpoint` found only the four definitions above. All other coordinators therefore inherit the no-op: OTThing derives from the generic MQTT coordinator, not from the OpenTherm one (coordinator/mqtt/otthing.py:42, coordinator/mqtt/__init__.py:19), and ESPHome, fake, simulator and switch derive directly from the base coordinator (coordinator/esphome/__init__.py:91, coordinator/fake/__init__.py:23, coordinator/simulator/__init__.py:14, coordinator/switch/__init__.py:22).

Entity attributes published by the climate entity include `pre_custom_temperature`, `pre_activity_temperature`, `rooms`, `setpoint`, `valves_open`, `heating_curve` and `requested_setpoint` (climate.py:234-263). The HVAC action is `off` when mode is off, `idle` when the coordinator is not active, else `heating` (climate.py:358-367).

Sensor entities: one "PID" sensor for the main PID plus one "PID (<room entity id>)" sensor per configured room, whose value is that PID's output and which is available only when that PID is available (sensor.py:46, sensor.py:56-57, sensor.py:101-169). A "Requested Setpoint" sensor shows `requested_setpoint` (sensor.py:51, sensor.py:71-98).

### Edge cases

1. Few rooms: with 1 to 4 rooms calling for heat, the hottest room's PID output always wins; the 5 °C headroom only limits from 5 rooms up (area.py:294-299, computed).
2. No rooms calling for heat: `areas.pids.output` returns 10.0, never `None`, so the `is not None` test in the requested setpoint always passes (area.py:291-292, climate.py:385-386).
3. The overshoot cap only considers rooms that still "require heat" (area.py:308-310). A room that is too warm but whose valve is below 10 or whose PID output is 22 °C or less does not cap the setpoint.
4. In COMFORT mode the overshoot cap also caps the main thermostat's own PID demand (climate.py:375-389).
5. ECO rounds to 0.1 before clamping; COMFORT does not round in `requested_setpoint` (climate.py:379, climate.py:394).
6. HVAC mode sync stops early: when a target climate is missing or does not list the mode in `hvac_modes`, the code does `return`, not `continue`. The remaining climates are skipped, and the control-loop scheduling and state write at the end are also skipped (climate.py:592-602). The PIDs were already reset at that point (climate.py:583). If a climate has no `hvac_modes` attribute, the membership test runs on `None` (climate.py:594), which in Python raises a `TypeError` (follows from the code, not run).
7. Preset auto-selection by temperature: the preset order is activity, away, home, sleep, comfort (climate.py:870). With the defaults activity and away are both 10 °C (const.py:175-176), so setting 10 °C selects `activity` (climate.py:563-565).
8. Window close restores through the same auto-selection (climate.py:832, climate.py:562-568). If the saved value equals the activity temperature, the auto-selection picks `activity`, which is already set, so nothing changes (climate.py:609-611).
9. Window close with no saved `_pre_activity_temperature` (for example when the user chose `activity` by hand before any window opened): `async_set_temperature` receives `None` and does nothing (climate.py:559-560, climate.py:830-832).
10. Several windows: each sensor is handled on its own (climate.py:746). Any single sensor reporting `off` while the preset is `activity` triggers the restore, even if another sensor is still `on` (climate.py:825-832).
11. Window open while the preset is already `activity`: nothing happens (climate.py:809-810).
12. A second window opening during the wait overwrites `_window_sensor_handle` with a new wait (climate.py:813-815); a later close cancels only the stored one (climate.py:826-828). The first wait can still complete and call `async_set_preset_mode(activity)` (climate.py:818-819). At that moment the window is already closed and its `off` event has passed, so SAT stays in `activity` (10 °C by default) until another `off` event or a manual change (climate.py:825-832). The same overwrite happens on a repeated `on` event from the same sensor during the wait, because only a missing, `unknown` or `unavailable` new state is filtered (climate.py:802-815). This follows from the code; it was not run.
13. Preset `none` restores `_pre_custom_temperature` without a check for `None` (climate.py:614-616). If it is `None` and the current target is not, the target becomes `None` (climate.py:643-647) and that `None` is also sent to the radiators and, if enabled, to the thermostat push (climate.py:649-657). After that the main temperature state is `None` and the main PID tick is skipped (climate.py:303-306, climate.py:480-482), and heating curves are not updated (climate.py:693-694).
14. Thermostat sync copies the thermostat's `temperature` attribute even if it is absent (`None`) (climate.py:765). A thermostat HVAC mode change only triggers a target copy; SAT's own HVAC mode is not changed (climate.py:760-765).
15. Room home temperatures are learned only while the preset is `home` (climate.py:788-792, see behaviour step 5). A room missing from `_rooms` is not added by a target change, and its PID is not reset by it (climate.py:788-789). A room target changed in other presets still resets that room's PID, when the room is in `_rooms` (climate.py:795).
16. `valves_open` is always `True` when no radiators are configured, even when rooms are configured (climate.py:420-422).
17. Stale sensors differ: a stale main inside sensor resets the main PID (climate.py:470-478); a stale room source only skips the area tick (area.py:204-206).
18. A room's override sensor (`sensor_temperature_id`) is used only when it is not `unknown`, `unavailable` or `off`; otherwise the climate's own `current_temperature` is used (area.py:73-86). A stale override sensor gives no temperature at all; there is no fallback to the climate in that case (area.py:76-78).
19. Window restore fires on any window sensor event whose new state is `off` while the preset is `activity`, also an `unavailable` to `off` transition or an attribute-only update; only a missing, `unknown` or `unavailable` new state is filtered (climate.py:802-804, climate.py:825-832).
20. On an integration reload, rooms count for nothing until the first heating curve update after start-up (see "Start-up gap on a reload" in step 1; follows from the code, not run).

### Not found in the code / unclear

1. `min_temp` and `max_temp` are used (climate.py:178-179, climate.py:205-206, climate.py:816) but not defined anywhere in `custom_components/sat` (grep for `def min_temp` / `def max_temp` found nothing). Their values come from the Home Assistant `ClimateEntity` base class, which is not in this repository. SAT itself does no range check on a target temperature set through `async_set_temperature` or the thermostat sync (climate.py:557-568, climate.py:641-667). The 5 to 35 °C preset range exists only in the options form (config_flow.py:648-668).
2. `self.pid.control_setpoint = self.min_temp` (climate.py:178, climate.py:205): the `PID` class has no `control_setpoint` member (pid.py:30-251). No effect of this assignment is visible in the code.
3. `Area.weight` (a room demand weight in the range 0 to 2) is defined (area.py:148-163) but no caller was found by grep. It does not influence the setpoint in this code.
4. `COMFORT_BAND = 0.1` (area.py:31) and `ATTR_TEMPERATURE` in area.py (area.py:26) are defined but not used in area.py.
5. `Area.async_outside_entity_changed` (area.py:198-200) has no caller found by grep. It would pass the room temperature in the outside-temperature argument of `heating_curve.update`; since it is not called, it has no effect.
6. `push_setpoint_to_thermostat` is read from `config.data` (entry_data.py:153-155), and `SatConfig.data` is `entry.data` (__init__.py:29). The presets options form writes the key to the options (config_flow.py:670, config_flow.py:804-806). The only places found that write it into `data` are the migrations from the old `sync_with_thermostat` key (__init__.py:150-151, __init__.py:185-187); the only `async_update_entry` call is that migration (__init__.py:198), and the setup and reconfigure flows store `self.data`, in which no step sets this key (config_flow.py:538-549). A grep of the whole integration for `push_setpoint` and `PUSH_SETPOINT` found only the constant and its default (const.py:60, const.py:138), the options form (config_flow.py:670), the reader (entry_data.py:154-155), the use (climate.py:656) and the migration (__init__.py:187). From the code read, the options-form toggle therefore has no effect on the climate; this was not tested.
7. The unit of `current_valve_position` (percent or otherwise) is not stated; the code only compares it with 10.0 (area.py:170).
8. What the OTGW PIC does with `TC=` is outside this repository; only the sent string is known (coordinator/mqtt/opentherm.py:206-207).
9. The thermostat push for coordinators other than MQTT OpenTherm, serial and EMS is the inherited base no-op (see Outputs); whether this is intended for OTThing, which also speaks OpenTherm, is not stated in the code (coordinator/mqtt/otthing.py:42, coordinator/__init__.py:377-379).
10. The exact string values of the HA preset names (`PRESET_ACTIVITY`, `PRESET_HOME`, and so on) come from Home Assistant (climate.py:16-23). The preset lookup builds keys as `f"{preset}_temperature"` (climate.py:870) and must match `away_temperature` and the other config keys (const.py:104-108). The `if key in conf_presets` filter at climate.py:871 is always true.
11. `self.preset_mode` and `self.preset_modes` are used (climate.py:606, climate.py:791, climate.py:809, climate.py:830) but not defined in `custom_components/sat`; they come from the Home Assistant `ClimateEntity` base class. That they return `_attr_preset_mode` and `_attr_preset_modes` is an assumption about Home Assistant, not shown in this repository.
12. There is no window action other than switching to the `activity` preset; for example no direct "heating off" and no notice to the boiler. Whether that is intended design is not stated in the code.
13. Whether the room temperature attributes are numbers or strings depends on the room's own integration. The room-change logic in behaviour step 5 behaves differently for a string attribute (then `float(target)` never equals the string default and a new room is added); which case occurs in practice is not shown in this repository (climate.py:788-792).
14. Heating control (what happens with the `HeatingDemand`), PID internals, solar gain and the temperature history belong to other sections and are not specified here.

## 7. Solar gain, summer simmer, temperature history and health sensors

Source: Alexwijn/SAT, branch `releases/thermo-nova`, commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`. All paths are relative to `custom_components/sat`. Every statement below cites the lines it was read from.

This section covers five separate features:

1. Solar gain compensation (the only feature in this section that changes boiler control directly).
2. Summer simmer index and the "thermal comfort" option (changes the room temperature that the controller sees, only when enabled).
3. Temperature error history and the "Heating Curve Recommendation" sensor (advice only, no control effect).
4. The "Error Value" sensor (display only).
5. The health and synchronisation binary sensors (display only, they report problems but do not change control).

### Purpose

**Solar gain compensation.** Detects that the room is warming up from the sun (passive solar gain) while the boiler is doing little, and then lowers the requested flow setpoint and optionally freezes the PID integral for a short hold time. The class docstring states: "Detect passive solar gain and keep a short-lived suppression hold" (solar_gain.py:40-41). It is off by default (const.py:132).

**Summer simmer index.** A heat-and-humidity comfort index. The docstring says: "The Summer Simmer Index is a measure of heat and humidity" (summer_simmer.py:13). It is always published as an attribute of the climate entity (climate.py:245-246). When the option "thermal comfort" is on, the index replaces the measured room temperature as the controlled value (climate.py:276-277). That option is off by default (const.py:131).

**Temperature history.** A rolling store of the room temperature error, described as "Rolling history of room temperature error samples" (temperature/history.py:40-41). It feeds only the "Heating Curve Recommendation" sensor, which advises to raise, lower or keep the heating curve (sensor.py:317-333). A search of the package for `temperature_statistics` found it read only in sensor.py (sensor.py:319, sensor.py:337-338, sensor.py:360) and defined in climate.py (climate.py:289-292).

**Health sensors.** Binary sensors of device class PROBLEM that flag a mismatch or an unhealthy condition: control setpoint sync, relative modulation sync, central heating sync, pressure health, device health and cycle health (binary_sensor.py:49-63, binary_sensor.py:104, binary_sensor.py:138, binary_sensor.py:175, binary_sensor.py:239, binary_sensor.py:401, binary_sensor.py:430). A window sensor group is also created when window sensors are configured (binary_sensor.py:56-57); its device class is WINDOW, not PROBLEM (binary_sensor.py:466).

### Inputs

**Solar gain controller** receives one `SolarGainSignals` record per PID pass (solar_gain.py:19-28, climate.py:485-494):

| Signal | Source | Citation |
|---|---|---|
| `sample.temperature` | `temperature_state.current`, the room temperature as SAT sees it (the simmer index when thermal comfort is on) | climate.py:487, climate.py:303-311, climate.py:271-279 |
| `sample.timestamp` | time of the PID tick (`event_timestamp(_time)`), falls back to wall clock | climate.py:484, helpers.py:17-19 |
| `valves_open` | `SatClimate.valves_open` | climate.py:488, climate.py:414-455 |
| `sun_elevation` | attribute `elevation` of the Home Assistant entity `sun.sun`, or None | climate.py:489, climate.py:504-513 |
| `flame_active` | coordinator `flame_active` (base class default: equals `active`) | climate.py:490, coordinator/__init__.py:169-171 |
| `is_heating_mode` | `hvac_mode == HEAT` | climate.py:491 |
| `relative_modulation` | coordinator `relative_modulation_value` (may be None; compared with the constant named `..._PERCENT`, so read as percent) | climate.py:492, coordinator/__init__.py:209-211, const.py:31 |

Configuration (`SolarGainConfig`, entry_data.py:88-94, entry_data.py:278-286): `enabled`, `freeze_integral`, `minimum_elevation`, `minimum_rise_per_hour`, `setpoint_offset_celsius`. The two on/off switches (solar gain compensation, freeze integral) and the thermal comfort switch are in the "advanced" options step (config_flow.py:713-724); the three numbers follow in the same step (config_flow.py:738-749).

**Summer simmer index** inputs: room temperature and relative humidity (summer_simmer.py:9, summer_simmer.py:31). The temperature is treated as °C, because it is converted from CELSIUS to FAHRENHEIT (summer_simmer.py:26-28). The code does not state a unit for humidity; the humidity sensor picker only accepts sensors of device class HUMIDITY (config_flow.py:392-397). In the climate entity these are `current_temperature` (raw sensor value) and `current_humidity` from the configured humidity sensor, which is None when no humidity sensor is configured (climate.py:273-277, climate.py:281-287). Both values are read through `_get_entity_state_float`, which also returns None when the sensor state is missing, unknown, unavailable or older than `sensor_max_value_age` (climate.py:669-686). The humidity sensor is an optional field of the sensors setup step and is stored in the config entry data, not in the options (config_flow.py:361-362, config_flow.py:392-397, entry_data.py:193).

**Temperature history** input: `median_error`, the median of the error of the primary room and of every secondary area that has a valid temperature (climate.py:396-412). The error is `setpoint - current`, rounded to 3 decimals (temperature/state.py:15-18). Secondary areas use their own raw sensor or climate temperature (area.py:89-100, area.py:111-129).

**Pressure health** inputs: coordinator `boiler_pressure` in bar (base class returns None, coordinator/__init__.py:194-196), coordinator `active` (binary_sensor.py:336), and `PressureHealthConfig` (entry_data.py:71-76, entry_data.py:248-255).

**Other health sensors** inputs: `heating_control.control_setpoint` vs `coordinator.setpoint` (binary_sensor.py:114-115); `heating_control.relative_modulation_value` vs `coordinator.maximum_relative_modulation_value` (binary_sensor.py:151-152); climate `hvac_action` vs `coordinator.active` (binary_sensor.py:185-186); `heating_control.device_status` (binary_sensor.py:406); `heating_control.last_cycle.classification` (binary_sensor.py:435-442).

### Behaviour (step by step, in time order)

#### Start-up

1. The climate entity is constructed with an inactive solar gain snapshot (`active=False`, no rise, no elevation) (climate.py:95), a new `SolarGainController` (climate.py:115) and an empty `TemperatureHistory` (climate.py:118). Neither the solar gain controller nor the history has any restore or storage code (solar_gain.py:43-46, temperature/history.py:43-44), so both start empty after every restart.
2. When the entity is added, previous state is restored (climate.py:128). If Home Assistant is already running, SAT registers its timers and calls `control_pid()` once immediately (climate.py:133-138). Otherwise these steps run on the `EVENT_HOMEASSISTANT_STARTED` event (climate.py:139-144).
3. The timers are: the heating control loop every 5 s (climate.py:706-710) and `control_pid` every `PID_UPDATE_INTERVAL` = 60 s (climate.py:712-716, pid.py:20). They are registered in `_register_event_listeners` (climate.py:699-716).
   - Ordering detail when Home Assistant is already running: the immediate `control_pid()` call (climate.py:137) runs before the secondary areas are attached to Home Assistant (climate.py:148, area.py:177-179). Until then an area has no `_hass` and returns no temperature state (area.py:67-70, area.py:111-114). So the first temperature history sample, if recorded, contains only the primary room error (climate.py:396-412, climate.py:496-497). In the other branch `control_pid` runs on `EVENT_HOMEASSISTANT_STARTED` (climate.py:143), after `async_added_to_hass` has finished.
4. The pressure health sensor restores `last_pressure`, `last_pressure_timestamp`, `last_seen_pressure_timestamp` and `smoothed_pressure` from its last saved state attributes and, when both last pressure and its timestamp exist, seeds the sample buffer with that one sample (binary_sensor.py:215-229).
5. All SAT binary sensors wait up to 10 s for the climate entity to be ready; if it does not appear, no binary sensors are created (binary_sensor.py:36-44). The same applies to sensors (sensor.py:29-37).

#### Per PID tick (every 60 s, `control_pid`, climate.py:466-502)

1. If `sensor_max_value_age` is above 0 and the inside sensor state is older than that age, the PID is reset and the tick ends (climate.py:468-478). Solar gain and history are not updated in this case.
2. If no `temperature_state` can be built (no inside sensor, no target, or no current temperature), the tick ends (climate.py:480-482, climate.py:295-306).
3. Solar gain update (climate.py:484-494), inside `SolarGainController.update` (solar_gain.py:48-64):
   1. The indoor rise rate is computed from the previous stored sample and the new sample, and the new sample is stored. This happens first, before any enable check, so the stored sample is always refreshed (solar_gain.py:49, solar_gain.py:66-78). On the very first sample the rise is None (solar_gain.py:67-69). If the time between samples is zero or negative, the rise is None (solar_gain.py:74-76).
   2. If the feature is disabled or the HVAC mode is not HEAT: the hold is cleared and the result is inactive (solar_gain.py:52-54).
   3. If the sun elevation is unknown: the hold is cleared and the result is inactive (solar_gain.py:56-58).
   4. A "solar gain event" is checked (solar_gain.py:80-93). All of these must be true:
      - rise is known and sun elevation is known (solar_gain.py:81-82);
      - sun elevation >= `minimum_elevation` (solar_gain.py:84-85);
      - rise >= `minimum_rise_per_hour` (solar_gain.py:87-88);
      - valves are open (solar_gain.py:90-91);
      - boiler output is low: either the flame is off, or the flame is on and relative modulation is known and <= 20 % (solar_gain.py:93, solar_gain.py:95-103). With flame on and unknown modulation, output is not considered low (solar_gain.py:100-101).
   5. On an event the hold end is set to `timestamp + 600 s` (solar_gain.py:60-61). Each new event pushes the hold end forward again.
   6. The result is active while the sample time is at or before the hold end (solar_gain.py:63). Outside the clear conditions in steps 2 and 3 the hold is never cleared early; it simply expires.
4. Temperature history: if HVAC mode is HEAT, valves are open and a median error exists, the median error is recorded with the tick time (climate.py:496-497). Recording also prunes samples older than 24 h, measured from the new sample's time (temperature/history.py:59-62, temperature/history.py:92-94).
5. The PID is updated. The integral is frozen when the solar gain snapshot is active AND the option `freeze_integral` is on (climate.py:499-502). In the PID, freezing skips only the integral update; the derivative is still updated and the last error and temperature are still stored (pid.py:165-171). Note what "skip the integral update" means: the normal integral update sets the integral to 0 whenever |error| > `DEADBAND` = 0.1 °C, and only accumulates inside that band (pid.py:185-192, const.py:16). Freezing skips the whole update, so it also blocks that reset: while frozen, the integral keeps its value even when the sun pushes the room more than 0.1 °C above target (pid.py:167-168). If the heating curve has no value yet, the PID update is skipped entirely, with or without freeze (pid.py:161-163). Steps 3 and 4 have already run at that point.
6. After each PID update the PID sends the dispatcher signal `SIGNAL_PID_UPDATED` (pid.py:183). The "Requested Setpoint" and "Heating Curve Recommendation" sensors listen to it and refresh their state (sensor.py:72-77, sensor.py:303-311).

#### Per heating control loop (every 5 s, debounced by 1 s)

1. The 5 s timer calls `schedule_heating_control_loop`, which schedules one run 1 s later unless a run is already pending (climate.py:515-531, climate.py:706-710).
2. The loop does nothing when there is no outside temperature or the HVAC mode is OFF (climate.py:544-545).
3. Otherwise it builds a `HeatingDemand` with `requested_setpoint` and passes it to heating control (climate.py:547-555). `requested_setpoint` applies the solar gain offset (see Formulas). The loop therefore uses the solar gain snapshot made at the latest 60 s PID tick.
4. Heating control sends the computed control setpoint, maximum relative modulation and heater on/off to the coordinator (heating_control.py:236-238). The heater demand (`HeatingDemand.heater_state`) is OFF when the HVAC mode is not HEAT, when the valves are closed, or when the requested setpoint is not above `COLD_SETPOINT` = 22 °C (heating_control.py:64-72, const.py:34). With demand OFF, heating control disables PWM and sets the control setpoint to `MINIMUM_SETPOINT` = 10 °C and the relative modulation to the configured maximum (heating_control.py:229-234, const.py:35). The heater on/off value actually sent is derived from the control setpoint: ON only when the control setpoint is above 22 °C (heating_control.py:238).

#### Health sensor evaluation

The binary sensors compute their state inside the `is_on` property (binary_sensor.py:112-117, binary_sensor.py:149-154, binary_sensor.py:183-192, binary_sensor.py:253-295, binary_sensor.py:404-406, binary_sensor.py:433-442). They are coordinator entities (entity.py:21-23); the central heating sync sensor also refreshes when the climate entity changes state (entity.py:50-58), and the cycle health sensor refreshes when the event `EVENT_SAT_CYCLE_ENDED` fires (binary_sensor.py:415-420). The code in this section does not set its own evaluation interval, so delays and confirmations (60 s, 120 s) only advance when the property is evaluated.

**Sync sensors (delayed mismatch).** A mismatch must be seen continuously for at least `delay` = 60 s before the sensor reports a problem. The first mismatch stores a wall clock time; any evaluation without mismatch clears it (binary_sensor.py:68-88).

- Control Setpoint Synchronization: problem when SAT's computed control setpoint and the setpoint reported by the device differ after rounding both to 0.1 °C (binary_sensor.py:112-117). Unavailable when either value is None (binary_sensor.py:106-109). Created only when the coordinator supports setpoint management (binary_sensor.py:59-60).
- Relative Modulation Synchronization: problem when SAT's relative modulation value and the device's maximum relative modulation value differ after truncation to integers (binary_sensor.py:149-154). Unavailable when either is None (binary_sensor.py:140-146). Created only when the coordinator supports relative modulation management (binary_sensor.py:62-63).
- Central Heating Synchronization: no problem when (climate action OFF and device not active) or (IDLE and device not active) or (HEATING and device active); every other combination is a mismatch (binary_sensor.py:183-192). SAT's own `hvac_action` is itself derived from `coordinator.active`: OFF when the HVAC mode is OFF, otherwise IDLE when the device is not active and HEATING when it is (climate.py:358-367). So with mode HEAT the two inputs agree by construction, and the combination that can mismatch in the SAT code is mode OFF while the device reports active (derived from climate.py:358-367 and binary_sensor.py:188-192). The sensor reads the action from the climate entity's `state_attributes` (binary_sensor.py:186); how and when Home Assistant fills that dictionary is framework code, not SAT code.

**Pressure Health** (binary_sensor.py:253-295), on every evaluation:

1. Track the device active flag. On the first evaluation it is only stored. When it changes, drop-rate checking is suspended for 600 s and the sample buffer is cleared (binary_sensor.py:262, binary_sensor.py:335-345).
2. If there is no pressure reading now:
   - never seen a pressure: no problem (binary_sensor.py:264-266);
   - `maximum_age_seconds` <= 0: no problem (binary_sensor.py:268-269);
   - otherwise problem when the last reading is older than `maximum_age_seconds` (binary_sensor.py:271). This path returns directly, without the 120 s confirmation, and it does not touch `_problem_first_detected` either (binary_sensor.py:264-271, binary_sensor.py:325-333). So if a raw problem was pending when readings stopped, its start time is kept, and when readings return with the problem still present the confirmation can be met at once (derived from binary_sensor.py:330-333).
3. With a reading: store "last seen" = now (binary_sensor.py:273), update the smoothed pressure (exponential moving average, binary_sensor.py:274, binary_sensor.py:315-323), add the raw sample to the buffer and drop samples older than the drop window (binary_sensor.py:275, binary_sensor.py:357-365).
4. Compute the drop rate from the buffer (binary_sensor.py:277, binary_sensor.py:367-388). Store last pressure and timestamp (binary_sensor.py:279-280). The published attribute `pressure_drop_rate_bar_per_hour` is set to None while the check is suspended, updated (rounded to 3 decimals) when a rate is computed, and otherwise keeps its previous value (binary_sensor.py:286-290).
5. Raw problem = smoothed pressure below minimum, OR smoothed pressure above maximum, OR drop rate above the maximum drop rate (binary_sensor.py:282-293). The drop rate is ignored (set to None) while suspended (binary_sensor.py:284-288, binary_sensor.py:347-355).
6. Confirmation: the raw problem must hold continuously for at least 120 s before the sensor turns on; any evaluation without a raw problem resets the timer (binary_sensor.py:295, binary_sensor.py:325-333).

**Device Health**: problem while `device_status` is `INSUFFICIENT_DATA` (binary_sensor.py:404-406). The device tracker returns `INSUFFICIENT_DATA` only while it has no computed status yet (device/__init__.py:44-49); it computes a status on every device update (device/__init__.py:134-153), and when no snapshot exists it returns OFF, not INSUFFICIENT_DATA (device/__init__.py:155-160). A search of the package found no other place that produces `BoilerStatus.INSUFFICIENT_DATA` (other hits only compare with it or return its name, sensor.py:478, sensor.py:487, binary_sensor.py:406). The device tracker is updated from heating control's coordinator listener (heating_control.py:166, heating_control.py:242-245). In practice the sensor is on from start-up until the first coordinator update reaches that listener. There is no delay.

**Cycle Health**: off when there is no last cycle (binary_sensor.py:435-436). Otherwise problem when the last completed cycle's classification is not GOOD, UNCERTAIN or INSUFFICIENT_DATA (binary_sensor.py:438-442). The remaining classifications are OVERSHOOT, UNDERHEAT and SHORT_CYCLING (types.py:60-72), produced by the cycle classifier (cycles/classifier.py:15-90; covered in the cycle section of this specification).

**Window Sensor**: a binary sensor group over the configured window sensors with mode `any`, so it is on when any window sensor is on (binary_sensor.py:450-456). The control effect is in climate.py: when a window sensor turns on and the preset is not already ACTIVITY, SAT waits `window_minimum_open_time` and then switches to the ACTIVITY preset; if the window closes first, the wait is cancelled; on close while in ACTIVITY, the previous target temperature is restored (climate.py:800-834). Defaults: minimum open time 15 s (const.py:172), activity temperature 10 °C (const.py:175). This control logic listens to each configured window sensor directly, not to the group sensor (climate.py:743-748). An OFF event from any one sensor cancels the wait and restores the pre-activity temperature without checking the other sensors (climate.py:825-834). So with two windows open, closing one ends ACTIVITY while the other is still open (derived from those lines). The pre-activity temperature is the target at the moment the wait started, or `min_temp` if there was none (climate.py:816).

### Formulas

**Indoor rise rate** (solar_gain.py:74-78), in °C per hour:

```
elapsed_seconds = sample.timestamp - previous.timestamp
rise_per_hour   = (sample.temperature - previous.temperature) * 3600 / elapsed_seconds
```

There is no smoothing: it is the slope between two consecutive PID samples. Arithmetic example from this formula: with 60 s between samples, a step of 0.1 °C gives 6 °C/h, and the default threshold 0.6 °C/h corresponds to 0.01 °C per minute.

**Solar gain event** (solar_gain.py:80-103):

```
event = rise_per_hour is not None
    and sun_elevation is not None
    and sun_elevation >= minimum_elevation
    and rise_per_hour >= minimum_rise_per_hour
    and valves_open
    and (not flame_active
         or (relative_modulation is not None and relative_modulation <= 20.0))
```

**Solar gain hold** (solar_gain.py:60-63):

```
if event: hold_until = timestamp + SOLAR_GAIN_HOLD_SECONDS
active = hold_until is not None and timestamp <= hold_until
```

**Requested setpoint with solar gain** (climate.py:369-394):

```
if heating_curve.value is None: return MINIMUM_SETPOINT
setpoint = pid.output
if heating_mode == ECO: return clamp(round(setpoint, 1), MINIMUM_SETPOINT, maximum_setpoint)
if secondary_heating is not None: setpoint = max(setpoint, secondary_heating)
if overshoot_cap is not None:     setpoint = min(setpoint, overshoot_cap)
if solar_gain_active and solar_gain.enabled:
    setpoint -= setpoint_offset_celsius
return clamp(setpoint, MINIMUM_SETPOINT, maximum_setpoint)
```

**Summer simmer index** (summer_simmer.py:9-38):

```
F     = temperature converted from °C to °F
index = 1.98 * (F - (0.55 - 0.0055 * H) * (F - 58.0)) - 56.83
if F < 58: index = F
result = round(index converted from °F to °C, 1)
```

`H` is relative humidity. If temperature or humidity is None the result is None (summer_simmer.py:22-23). The conversions use Home Assistant's `TemperatureConverter` (summer_simmer.py:3-4, summer_simmer.py:26-28, summer_simmer.py:38).

**Summer simmer perception** (summer_simmer.py:40-63), on the index in °C:

| Index (°C) | Label |
|---|---|
| None | Unknown |
| < 21.1 | Cool |
| < 25.0 | Slightly Cool |
| < 28.3 | Comfortable |
| < 32.8 | Slightly Warm |
| < 37.8 | Increasing Discomfort |
| < 44.4 | Extremely Warm |
| < 51.7 | Danger Of Heatstroke |
| < 65.6 | Extreme Danger Of Heatstroke |
| otherwise | Circulatory Collapse Imminent |

**Room error** (temperature/state.py:15-18):

```
error = round(setpoint - current, 3)      (positive = room too cold)
```

**Median error for history** (climate.py:396-412): median of the primary room error and each area error that is available.

**Window statistics** (temperature/history.py:64-108), for each window (4 h "recent", 24 h "daily"):

```
samples        = samples with timestamp >= (latest sample timestamp - window)
sample_count   = len(samples)
median_error   = median(errors)
mean_error     = mean(errors)
mean_abs_error = mean(|error|)
in_band_fraction = count(|error| <= DEADBAND) / sample_count
```

With no samples: count 0, median/mean/mean_abs None, in-band fraction 0.0 (temperature/history.py:68-75). The window end is the timestamp of the newest sample, not the current clock (temperature/history.py:85-90, temperature/history.py:96-100).

**Heating curve recommendation** (sensor.py:300-333):

```
threshold = DEADBAND * 2 = 0.2 °C
if daily.sample_count < 6:           INSUFFICIENT_SAMPLES
elif daily.median_error is None:     HOLD
elif daily.median_error > threshold: INCREASE
elif daily.median_error < -threshold: DECREASE
else:                                HOLD
```

**Pressure smoothing** (binary_sensor.py:315-323), per evaluation with a reading:

```
smoothed = raw                                   (first time)
smoothed = 0.05 * raw + 0.95 * smoothed          (afterwards)
```

**Pressure drop rate** (binary_sensor.py:367-388): ordinary least-squares slope of pressure over time across the buffer, requiring at least 3 samples spanning at least 300 s:

```
slope = (n*Σ(t*p) - Σt*Σp) / (n*Σ(t²) - (Σt)²)
drop_rate_bar_per_hour = -slope * 3600           (positive = pressure falling)
```

Returns None when there are too few samples, the span is under 300 s, or the denominator is 0. The buffer keeps samples within `max(maximum_age_seconds, 3600)` seconds of the newest one, where a `maximum_age_seconds` <= 0 is treated as 3600 (binary_sensor.py:357-365).

**Current consumption sensor** (sensor.py:252-259), shown for completeness since it lives next to the health sensors. It is created only when the coordinator supports relative modulation management and both `minimum_consumption` and `maximum_consumption` are above 0 (sensor.py:59-66). Both default to 0 (const.py:163-164), so by default this sensor does not exist:

```
if not active or not flame_active: 0.0
else: round(minimum_consumption + (relative_modulation_value / 100) * (maximum_consumption - minimum_consumption), 3)
```

### Constants and defaults

| Name | Value | Unit | Citation |
|---|---|---|---|
| `SOLAR_GAIN_HOLD_SECONDS` | 600.0 | s | const.py:24 |
| `SOLAR_GAIN_MAX_RELATIVE_MODULATION_PERCENT` | 20.0 | % | const.py:31 |
| `CONF_SOLAR_GAIN_COMPENSATION` (enabled) default | False | bool | const.py:132 |
| `CONF_SOLAR_GAIN_MIN_ELEVATION` default | 12.0 | degrees | const.py:133 |
| `CONF_SOLAR_GAIN_MIN_ELEVATION` UI range | -10 to 90, step 1 | degrees | config_flow.py:738-741 |
| `CONF_SOLAR_GAIN_MIN_RISE_PER_HOUR` default | 0.6 | °C/h | const.py:134 |
| `CONF_SOLAR_GAIN_MIN_RISE_PER_HOUR` UI range | 0.0 to 4.0, step 0.1 | °C/h | config_flow.py:742-745 |
| `CONF_SOLAR_GAIN_SETPOINT_OFFSET_CELSIUS` default | 2.0 | °C | const.py:135 |
| `CONF_SOLAR_GAIN_SETPOINT_OFFSET_CELSIUS` UI range | 0.0 to 10.0, step 0.1 | °C | config_flow.py:746-749 |
| `CONF_SOLAR_GAIN_FREEZE_INTEGRAL` default | True | bool | const.py:136 |
| `PID_UPDATE_INTERVAL` (solar gain and history sample period) | 60 | s | pid.py:20, climate.py:712-716 |
| Heating control loop timer | 5 | s | climate.py:706-710 |
| Heating control loop debounce | 1 | s | climate.py:530 |
| `MINIMUM_SETPOINT` (lower clamp of requested setpoint) | 10.0 | °C | const.py:35, climate.py:394 |
| `COLD_SETPOINT` (heater demand off at or below) | 22 | °C | const.py:34, heating_control.py:72 |
| `CONF_THERMAL_COMFORT` default | False | bool | const.py:131 |
| `CONF_HUMIDITY_SENSOR_ENTITY_ID` | listed as None in `OPTIONS_DEFAULTS`, but the value used is read from config entry data and set to None when left empty in the sensors step | entity | const.py:137, entry_data.py:193, config_flow.py:361-362 |
| Simmer index pivot | 58.0 | °F | summer_simmer.py:31-35 |
| Simmer index coefficients | 1.98, 0.55, 0.0055, 56.83 | constants of a °F formula (code gives no units) | summer_simmer.py:31 |
| `RECENT_WINDOW_SECONDS` | 14400 (4 h) | s | temperature/history.py:10 |
| `DAILY_WINDOW_SECONDS` | 86400 (24 h) | s | temperature/history.py:11 |
| `DEADBAND` (in-band limit) | 0.1 | °C | const.py:16, temperature/history.py:107 |
| `_MINIMUM_DAILY_SAMPLES` | 6 | samples | sensor.py:300 |
| `_ERROR_THRESHOLD` | `DEADBAND * 2` = 0.2 | °C | sensor.py:301 |
| `SatSyncSensor` delay | 60 | s | binary_sensor.py:71 |
| `PRESSURE_DROP_RATE_SETTLE_SECONDS` | 600 | s | binary_sensor.py:25 |
| `PRESSURE_EMA_ALPHA` | 0.05 | per evaluation | binary_sensor.py:26 |
| `PRESSURE_PROBLEM_CONFIRMATION_SECONDS` | 120 | s | binary_sensor.py:27 |
| `PRESSURE_DROP_RATE_MIN_WINDOW_SECONDS` | 300 | s | binary_sensor.py:28 |
| Drop-rate minimum sample count | 3 | samples | binary_sensor.py:368 |
| Drop-rate buffer window | `max(maximum_age_seconds, 3600)` | s | binary_sensor.py:358-361 |
| `CONF_MINIMUM_BOILER_PRESSURE` default | 0.8 | bar | const.py:157 |
| `CONF_MAXIMUM_BOILER_PRESSURE` default | 2.5 | bar | const.py:158 |
| Min and max pressure UI range | 0 to 4, step 0.1 | bar | config_flow.py:781-788 |
| `CONF_MAXIMUM_PRESSURE_DROP_RATE` default | 0.3 | bar/h | const.py:159 |
| Maximum drop rate UI range | 0 to 3, step 0.05 | bar/h | config_flow.py:789-792 |
| `CONF_PRESSURE_MAX_VALUE_AGE` default | "00:30:00" (1800) | HH:MM:SS (s) | const.py:160, entry_data.py:254, helpers.py:72-79 |
| `CONF_WINDOW_MINIMUM_OPEN_TIME` default | "00:00:15" (15) | HH:MM:SS (s) | const.py:172, entry_data.py:196-198 |
| `CONF_ACTIVITY_TEMPERATURE` default | 10 | °C | const.py:175 |
| `CONF_SENSOR_MAX_VALUE_AGE` default (stale inside sensor stops the PID tick) | "06:00:00" (21600) | HH:MM:SS (s) | const.py:170, entry_data.py:199-201, climate.py:468-478 |
| `CONF_MINIMUM_CONSUMPTION`, `CONF_MAXIMUM_CONSUMPTION` defaults (consumption sensor only exists when both > 0) | 0, 0 | as configured | const.py:163-164, sensor.py:65-66 |
| PID integral reset band (relevant to freeze) | `DEADBAND` = 0.1 | °C | const.py:16, pid.py:187-189 |

With the default pressure age of 1800 s the drop-rate window is `max(1800, 3600)` = 3600 s (arithmetic from binary_sensor.py:361 and const.py:160).

### State kept between passes

| Owner | State | Meaning | Persisted across restart | Citation |
|---|---|---|---|---|
| `SolarGainController` | `_last_sample` | previous temperature and time, for the rise rate | No | solar_gain.py:46, solar_gain.py:66-72 |
| `SolarGainController` | `_hold_until` | end time of the current solar hold | No | solar_gain.py:45, solar_gain.py:61 |
| `SatClimate` | `_solar_gain_snapshot` | last result: active, rise, elevation | No (reset to inactive in constructor) | climate.py:95, climate.py:485 |
| `TemperatureHistory` | `_samples` deque | (timestamp, median error) for up to 24 h | No | temperature/history.py:43-44, temperature/history.py:59-62 |
| `SatSyncSensor` | `_last_mismatch` | wall clock time when the current mismatch started | No | binary_sensor.py:74, binary_sensor.py:82-83 |
| `SatPressureHealthSensor` | `_last_active` | device active flag at the last evaluation | No | binary_sensor.py:205, binary_sensor.py:335-345 |
| `SatPressureHealthSensor` | `_drop_rate_suspended_until` | end of drop-rate suspension | No | binary_sensor.py:206, binary_sensor.py:342 |
| `SatPressureHealthSensor` | `_smoothed_pressure` | EMA of pressure | Yes, via state attribute | binary_sensor.py:211, binary_sensor.py:226, binary_sensor.py:302 |
| `SatPressureHealthSensor` | `_last_pressure`, `_last_pressure_timestamp` | last raw reading and its time | Yes, via state attributes | binary_sensor.py:207, binary_sensor.py:210, binary_sensor.py:223-224, binary_sensor.py:305-306 |
| `SatPressureHealthSensor` | `_last_seen_pressure` | time a reading was last present | Yes, via attribute `last_seen_pressure_timestamp` | binary_sensor.py:209, binary_sensor.py:225, binary_sensor.py:307 |
| `SatPressureHealthSensor` | `_pressure_samples` | (time, raw pressure) buffer for the drop rate | Seeded with one restored sample | binary_sensor.py:213, binary_sensor.py:228-229 |
| `SatPressureHealthSensor` | `_problem_first_detected` | start of the current raw problem | No | binary_sensor.py:212, binary_sensor.py:330-331 |
| `SatPressureHealthSensor` | `_last_drop_rate` | last published drop rate | No | binary_sensor.py:208, binary_sensor.py:288-290 |

The restore of pressure state uses Home Assistant's `RestoreEntity` and `async_get_last_state` (binary_sensor.py:14, binary_sensor.py:200, binary_sensor.py:218).

### Outputs (to the boiler, to entities)

**To the boiler.**

- Solar gain is the only feature here that reaches the boiler. While active (and enabled, and not in ECO mode), it lowers `requested_setpoint` by `setpoint_offset_celsius` (climate.py:391-392). That value goes into `HeatingDemand` (climate.py:547-555), from which heating control computes the control setpoint, maximum relative modulation and heater on/off that are sent to the coordinator (heating_control.py:197-238). If the lowered requested setpoint is at or below 22 °C, the heater demand becomes OFF (heating_control.py:64-72). Heating control then sends control setpoint `MINIMUM_SETPOINT` = 10 °C, the configured maximum relative modulation, and heater OFF, because the heater state sent is `control_setpoint > COLD_SETPOINT` (heating_control.py:229-238, const.py:34-35).
- While active with `freeze_integral` on, solar gain also stops the PID integral from changing, including the reset to 0 that would otherwise happen when |error| > 0.1 °C (climate.py:499-502, pid.py:167-168, pid.py:187-189). The PID output feeds `requested_setpoint` (climate.py:375).
- Thermal comfort (summer simmer) does not send anything by itself, but when enabled it replaces the room temperature used by the PID, the room error, the solar gain sample and the history (climate.py:276-277, climate.py:303-311, climate.py:400-401, climate.py:487, climate.py:499-502).
- Temperature history, the recommendation sensor, the error value sensor and all health binary sensors send nothing to the boiler. Their code only reads values and returns entity state (binary_sensor.py:91-471, sensor.py:299-399).

**To entities.**

| Entity or attribute | Content | Citation |
|---|---|---|
| Climate attribute `summer_simmer_index` | simmer index of `current_temperature` and `current_humidity` (see note below) | climate.py:245 |
| Climate attribute `summer_simmer_perception` | perception label | climate.py:246 |
| Climate attributes `solar_gain_active`, `solar_gain_sun_elevation`, `solar_gain_rise_per_hour` | latest solar gain snapshot | climate.py:253-255 |
| Sensor "Requested Setpoint" | `requested_setpoint` (includes solar offset), °C; refreshed on every PID update signal | sensor.py:71-98 |
| Sensor "Error Value" | `temperature_state.error`, °C; unavailable without temperature state | sensor.py:367-399 |
| Sensor "Heating Curve Recommendation" | INCREASE / DECREASE / HOLD / INSUFFICIENT_SAMPLES; available when the daily window has at least one sample | sensor.py:299-364 |
| Recommendation attributes | threshold, minimum samples, and mean, count, median, mean absolute error and in-band fraction for the recent and daily windows | sensor.py:335-356 |
| Binary "Control Setpoint Synchronization" (PROBLEM) | setpoint mismatch for >= 60 s | binary_sensor.py:91-122 |
| Binary "Relative Modulation Synchronization" (PROBLEM) | modulation mismatch for >= 60 s | binary_sensor.py:125-159 |
| Binary "Central Heating Synchronization" (PROBLEM) | HVAC action vs device active mismatch for >= 60 s | binary_sensor.py:162-197 |
| Binary "Pressure Health" (PROBLEM) | pressure low / high / falling fast (confirmed 120 s), or reading stale | binary_sensor.py:200-313 |
| Pressure Health attributes | `pressure`, `smoothed_pressure`, `pressure_drop_rate_bar_per_hour`, `last_pressure`, `last_pressure_timestamp`, `last_seen_pressure_timestamp` | binary_sensor.py:297-308 |
| Binary "Device Health" (PROBLEM) | device status still INSUFFICIENT_DATA | binary_sensor.py:391-411 |
| Binary "Cycle Health" (PROBLEM) | last cycle OVERSHOOT, UNDERHEAT or SHORT_CYCLING | binary_sensor.py:414-447 |
| Binary "Window Sensor" (WINDOW) | any configured window sensor on | binary_sensor.py:450-471 |
| Sensor "Cycle Status" | last cycle classification name and cycle metrics | sensor.py:421-467 |
| Sensor "Device Status" | device status name; unavailable while INSUFFICIENT_DATA | sensor.py:470-491 |

Note on `summer_simmer_index`: the attribute is computed from `self.current_temperature` (climate.py:245), and when thermal comfort is on, `current_temperature` already returns the simmer index (climate.py:276-277). So with thermal comfort on, the attribute is the index computed from the index, not from the raw temperature. This follows directly from the cited lines; whether this is intended is not stated in the code.

### Edge cases

- **Solar gain in ECO mode.** `requested_setpoint` returns before the solar offset line in ECO mode (climate.py:377-379), so the setpoint is not lowered. The controller still runs, so with `freeze_integral` on the integral is still frozen while active (climate.py:485-502).
- **No radiators configured.** `valves_open` returns True when no main radiators are configured (climate.py:420-422), so the valves condition is always met.
- **No `sun.sun` entity or no elevation attribute.** Elevation is None, solar gain is inactive and any hold is cleared (climate.py:504-513, solar_gain.py:56-58).
- **Flame on with unknown modulation.** Not treated as low output, so no event (solar_gain.py:100-101).
- **Coordinators without a separate flame signal.** The base class reports `flame_active = active` (coordinator/__init__.py:169-171).
- **Hold refresh.** Every qualifying tick extends the hold to 600 s after that tick (solar_gain.py:60-61). After the last event the result stays active for ticks up to and including 600 s later (solar_gain.py:63). With a 60 s tick this is about 10 more ticks (arithmetic).
- **Rise rate after skipped ticks.** When a PID tick ends early (stale sensor or no temperature state), the solar sample is not stored (climate.py:468-482). The next rise rate then spans the longer gap (solar_gain.py:74-78).
- **Setpoint floor.** After the offset, the requested setpoint is clamped to at least 10 °C (climate.py:394, const.py:35). A lowered setpoint at or below 22 °C turns heater demand OFF (heating_control.py:72).
- **Thermal comfort without humidity sensor.** `current_humidity` is None (climate.py:284-285), so the index is None (summer_simmer.py:22-23), `current_temperature` is None (climate.py:276-277), `temperature_state` is None (climate.py:305-306), and every PID tick is skipped (climate.py:480-482).
- **Thermal comfort with a stale or unavailable humidity sensor.** The same chain applies: `_get_entity_state_float` returns None for a missing, unknown, unavailable or stale state (climate.py:669-686), so the PID tick is skipped as above (climate.py:276-277, climate.py:480-482). With the default `sensor_max_value_age` of 6 h (const.py:170), a humidity sensor that has not updated for 6 h stops SAT's PID ticks while thermal comfort is on (derived from those lines).
- **Solar gain freeze and the integral reset.** Without freeze, an error outside ±0.1 °C sets the integral to 0 (pid.py:187-189). With an active solar hold and `freeze_integral` on, `_update_integral` is not called (pid.py:167-168), so the integral survives a sun-driven overshoot and is still there when the hold ends (derived from those lines).
- **Heating curve without a value.** `requested_setpoint` returns `MINIMUM_SETPOINT` = 10 °C and skips the solar offset (climate.py:372-373), and the PID update is skipped (pid.py:161-163). The solar gain controller and history are still updated in that tick (climate.py:484-497).
- **Simmer index below 58 °F (about 14.4 °C).** The index equals the input temperature, only rounded to 0.1 °C after the round trip conversion (summer_simmer.py:33-38).
- **Simmer index for areas.** Secondary areas read their raw temperature; `area.py` does not reference the simmer index (area.py:89-100; a package search for `thermal_comfort` found it only in climate.py:276, const.py:62, entry_data.py:81 and entry_data.py:271).
- **History only while heating with open valves.** No samples are recorded in OFF mode or with closed valves (climate.py:496-497), and the statistics stay anchored to the newest stored sample (temperature/history.py:96-100). After a long pause the 24 h window can still show old values until new samples push them out (temperature/history.py:85-94).
- **History after restart.** It starts empty (climate.py:118), so the recommendation shows INSUFFICIENT_SAMPLES until 6 samples exist in the daily window (sensor.py:321-322). At one sample per 60 s that is at least 6 minutes of heating with open valves (arithmetic).
- **Pressure reading missing.** The stale check returns directly and skips the 120 s confirmation (binary_sensor.py:264-271). If no reading was ever seen, the sensor is unavailable and off (binary_sensor.py:244-250, binary_sensor.py:264-266). After a restart the restored `last_seen_pressure_timestamp` makes the sensor available and can report stale straight away (binary_sensor.py:225, binary_sensor.py:247-250, binary_sensor.py:271).
- **Pressure age set to 0.** Missing readings never report a problem (binary_sensor.py:268-269) and the drop-rate window becomes 3600 s (binary_sensor.py:358-361).
- **Device active flag changes.** The flag is the coordinator's `active`, documented as "Whether the device is actively heating" (coordinator/__init__.py:156-160); the code here does not say whether this is the burner flame. Any change of the device active flag clears the pressure sample buffer and suspends the drop-rate check for 600 s (binary_sensor.py:341-343). Low and high pressure checks continue during the suspension (binary_sensor.py:282-293).
- **Drop rate sign.** Only a falling pressure can trip the drop-rate check, because the rate is the negated slope and compared with `>` (binary_sensor.py:388, binary_sensor.py:292). A rising pressure gives a negative rate.
- **Pressure limits use the smoothed value.** Low and high use the EMA (binary_sensor.py:282-283); the drop rate uses raw samples (binary_sensor.py:275, binary_sensor.py:362).
- **Sync sensor delay uses wall clock.** The 60 s delay is measured with `timestamp()` (UTC wall clock) (binary_sensor.py:83-85, helpers.py:12-14, helpers.py:22-25).
- **Relative modulation sync truncation.** Values are compared after `int()`, so 99.6 and 99 count as equal (binary_sensor.py:151-154; example is arithmetic).
- **Cycle Health before any cycle.** Off, not unavailable (binary_sensor.py:435-436).
- **Device Health vs Device Status.** While status is INSUFFICIENT_DATA, the Device Health problem sensor is on (binary_sensor.py:406) and the Device Status sensor is unavailable (sensor.py:482-487).

### Not found in the code / unclear

1. **Evaluation rate of the binary sensors.** The 60 s sync delay, the 120 s pressure confirmation, the 600 s drop-rate suspension and the EMA all advance only when `is_on` is evaluated (binary_sensor.py:76-88, binary_sensor.py:253-295). How often Home Assistant evaluates these properties (on coordinator updates, state writes or UI reads) is decided by the Home Assistant framework and the coordinator update rate, not by code in these files. The effective EMA time constant in seconds is therefore not defined in the code.
2. **Side effects in a property getter.** The pressure sensor mutates its state inside `is_on` (binary_sensor.py:262-295). If Home Assistant reads the property more than once per update, the EMA and the sample buffer advance more than once. The code does not guard against this.
3. **Intent of the double simmer conversion in the attribute.** With thermal comfort on, `summer_simmer_index` is computed from a value that is already the index (climate.py:245, climate.py:276-277). No comment explains whether this is intended.
4. **Solar gain not applied in ECO mode.** The early return in ECO mode (climate.py:377-379) skips the solar offset; no comment says whether this is deliberate.
5. **Sun elevation source.** Elevation is read from the Home Assistant `sun.sun` entity (climate.py:505-513). How that entity computes elevation is outside the SAT code. The `sun` integration is not listed in SAT's manifest dependencies (manifest.json:8-11), so nothing in SAT ensures `sun.sun` exists; without it solar gain is never active (climate.py:506-507, solar_gain.py:56-58).
6. **What `boiler_pressure`, `active`, `flame_active`, `setpoint` and `maximum_relative_modulation_value` contain per device type.** The base coordinator only defines defaults or abstract properties (coordinator/__init__.py:152-171, coordinator/__init__.py:194-196, coordinator/__init__.py:265-267). The per-backend values (serial, MQTT, ESPHome and others) were not read for this section.
7. **Serial-mode binary sensors.** In serial mode extra binary sensors are added from `coordinator/serial/binary_sensor.py` (binary_sensor.py:46-47). That file was not read for this section.
8. **Cycle classification details.** Which cycles become OVERSHOOT, UNDERHEAT or SHORT_CYCLING depends on cycle metrics and margins in `cycles/` (cycles/classifier.py:15-90). The margin constants in `cycles/const.py` were not read here; see the cycle section of this specification.
9. **How the heating control turns the lowered requested setpoint into the control setpoint.** Only the hand-over is cited here (heating_control.py:197-238); the PWM and continuous setpoint computations belong to the heating control section.
10. **No persistence of solar gain state or temperature history.** No storage code was found in solar_gain.py or temperature/history.py (solar_gain.py:40-103, temperature/history.py:40-108). The documentation does not say whether this is intended.
11. **Summer simmer formula origin.** The code gives the formula (summer_simmer.py:15, summer_simmer.py:31) but no reference to its source.
12. **Humidity unit.** The formula uses humidity as a plain number `H` (summer_simmer.py:31). The code does not state that it must be percent; it only restricts the picker to sensors of device class HUMIDITY (config_flow.py:395).
13. **Central heating sync input.** The sensor reads `hvac_action` from the climate entity's `state_attributes` (binary_sensor.py:186). Filling that dictionary is Home Assistant framework code, not shown in SAT. Whether it is always computed live from `hvac_action` (climate.py:358-367) was not verified here.
14. **Window close with several sensors.** Restoring the temperature on the first OFF event while other windows may still be open (climate.py:825-834) follows from the code; no comment says whether this is intended.
15. **Start-up order and the first history sample.** That the first immediate `control_pid()` runs before areas are attached (climate.py:137, climate.py:148, area.py:179) follows from the code; no comment says whether this is intended. It affects at most one history sample.

## 8. Boiler I/O, DHW, limits and safety, configuration defaults

Source: Alexwijn/SAT, branch `releases/thermo-nova`, commit 7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb. All citations without a prefix are relative to `custom_components/sat/`. A few citations point outside SAT for context. They are labelled and use these prefixes:

- `pyotgw/...` means `other-projects/pyotgw-master/pyotgw/...`. This is the local pyotgw copy, version 2.2.3 (`other-projects/pyotgw-master/setup.py:16`). SAT pins pyotgw 2.2.2 (`manifest.json:29`), so serial-path command mappings are "per local pyotgw 2.2.3". The command letters come from `pyotgw/vars.py:281-282` (`TT`, `TC`) and `pyotgw/vars.py:306-311` (`SH`, `SW`, `MM`, `CS`, `CH`).
- `OTGW-Commands.md` means `other-projects/OTGW-documentation/OTGW-Commands.md`. It describes gateway behaviour, not SAT behaviour.
- `OT-spec` means `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md` in the OTGW-firmware repo.

### Purpose

This section describes the boundary between SAT and the boiler:

- what SAT writes to the boiler, and when;
- how each backend (called a "coordinator") turns those writes into commands;
- what SAT reads back;
- how SAT handles domestic hot water (DHW);
- which limits and fallbacks exist (sensor loss, stale data, maximum setpoint);
- the configuration options and their defaults.

The control algorithms (PID, heating curve, PWM duty cycle, cycle classification) are only referenced here. They belong to other sections.

### Inputs

**Selection of the backend.** The configured `mode` picks one of eight coordinators: fake, simulator, switch, esphome, mqtt_ems, mqtt_opentherm, mqtt_otthing and serial (`entry_data.py:22-30`, `coordinator/__init__.py:52-87`).

**Values SAT reads from the boiler side.** Each coordinator exposes the same properties (`coordinator/__init__.py:130-143`, `coordinator/__init__.py:150-267`):

| Property | OTGW via MQTT, topic key | EMS-ESP via MQTT, key | OTthing via MQTT | Serial (pyotgw key) | ESPHome object_id |
|---|---|---|---|---|---|
| `active` (CH active) | `centralheating` == "ON" (`coordinator/mqtt/opentherm.py:23`, `:59-61`) | `heatingactive` == "on" (`coordinator/mqtt/ems.py:22`, `:54-56`) | thermostat `ch_enable` or slave `ch_mode` (`coordinator/mqtt/otthing.py:65-70`) | `DATA_MASTER_CH_ENABLED` (`coordinator/serial/__init__.py:53-55`) | switch `ch_enable` (`coordinator/esphome/__init__.py:143-145`) |
| `flame_active` | `flame` (`coordinator/mqtt/opentherm.py:63-65`) | `burngas` (`coordinator/mqtt/ems.py:58-60`) | slave status `flame` (`coordinator/mqtt/otthing.py:72-74`) | `DATA_SLAVE_FLAME_ON` (`coordinator/serial/__init__.py:161-163`) | `flame_on` (`coordinator/esphome/__init__.py:147-149`) |
| `hot_water_active` | `domestichotwater` (`coordinator/mqtt/opentherm.py:67-69`) | `tapactivated` (`coordinator/mqtt/ems.py:62-64`) | thermostat `dhw_enable` or slave `dhw_mode` (`coordinator/mqtt/otthing.py:76-81`) | `DATA_SLAVE_DHW_ACTIVE` (`coordinator/serial/__init__.py:57-59`) | `dhw_active` (`coordinator/esphome/__init__.py:151-153`) |
| `setpoint` (control setpoint as reported) | `TSet` (`coordinator/mqtt/opentherm.py:71-76`) | `selflowtemp` (`coordinator/mqtt/ems.py:66-68`) | thermostat `ch_set_t` (`coordinator/mqtt/otthing.py:83-85`) | `DATA_CONTROL_SETPOINT` (`coordinator/serial/__init__.py:77-82`) | number `t_set` (`coordinator/esphome/__init__.py:155-157`) |
| `boiler_temperature` (flow) | `Tboiler` (`coordinator/mqtt/opentherm.py:106-111`) | `curflowtemp` (`coordinator/mqtt/ems.py:74-76`) | slave `flow_t` (`coordinator/mqtt/otthing.py:91-93`) | `DATA_CH_WATER_TEMP` (`coordinator/serial/__init__.py:91-96`) | `t_boiler` (`coordinator/esphome/__init__.py:171-173`) |
| `return_temperature` | `Tret` (`coordinator/mqtt/opentherm.py:113-118`) | `rettemp` (`coordinator/mqtt/ems.py:78-80`) | slave `return_t` (`coordinator/mqtt/otthing.py:95-97`) | `DATA_RETURN_WATER_TEMP` (`coordinator/serial/__init__.py:98-103`) | `t_ret` (`coordinator/esphome/__init__.py:175-177`) |
| `relative_modulation_value` | `RelModLevel` (`coordinator/mqtt/opentherm.py:127-132`) | `curburnpow` (`coordinator/mqtt/ems.py:82-84`) | slave `rel_mod` (`coordinator/mqtt/otthing.py:99-101`) | `DATA_REL_MOD_LEVEL` (`coordinator/serial/__init__.py:126-131`) | `rel_mod_level` (`coordinator/esphome/__init__.py:183-185`) |
| `boiler_pressure` | `CHPressure` (`coordinator/mqtt/opentherm.py:120-125`) | not provided (base returns None, `coordinator/__init__.py:193-196`) | not provided | `DATA_CH_WATER_PRESS` (`coordinator/serial/__init__.py:105-110`) | `ch_pressure` (`coordinator/esphome/__init__.py:179-181`) |
| `maximum_setpoint_value` | `MaxTSet`, else the configured maximum (`coordinator/mqtt/opentherm.py:78-83`) | configured maximum (base, `coordinator/__init__.py:218-221`) | configured maximum (base) | configured maximum (base) | number `max_t_set` (`coordinator/esphome/__init__.py:199-201`) |
| `member_id` (manufacturer code) | `slave_memberid_code` (`coordinator/mqtt/opentherm.py:159-164`) | always None (`coordinator/mqtt/ems.py:98-101`) | slave `memberId` (`coordinator/mqtt/otthing.py:129-137`) | `DATA_SLAVE_MEMBERID` (`coordinator/serial/__init__.py:154-159`) | `device_id` (`coordinator/esphome/__init__.py:203-206`) |

The switch, simulator and fake coordinators produce these values internally (`coordinator/switch/__init__.py:37-54`, `coordinator/simulator/__init__.py:49-71`, `coordinator/fake/__init__.py:49-67`).

**Values from Home Assistant.**

- Inside temperature: read from `inside_sensor_entity_id`. A stale, unknown or unavailable state is returned as None (`climate.py:270-279`, `climate.py:669-686`).
- Outside temperature: read from one or more `outside_sensor_entity_id` entities (`climate.py:317-343`).
- Radiator climates: their state is used to decide whether valves are open (`climate.py:414-455`).
- HVAC mode of the SAT climate entity (`climate.py:350-356`).

**User inputs that write to the boiler directly.** Two number entities do this: "Hot Water Setpoint" and "Maximum Setpoint" (`number.py:16-21`, `number.py:72-74`, `number.py:125-127`).

### Behaviour (step by step, in time order)

#### A. Start-up

1. **Config entry setup.**
   - Options are built as `OPTIONS_DEFAULTS` overlaid with the stored options (`__init__.py:29`).
   - The coordinator and `SatHeatingControl` are created (`__init__.py:32-33`).
   - Then `coordinator.async_setup()` runs, then `coordinator.async_added_to_hass()`, and only then are the platforms set up (`__init__.py:50-52`).
2. **Coordinator setup, per type.**
   - MQTT types: `async_setup` loads the values stored by the previous run from HA storage `sat.mqtt.<device>` into the data cache (`coordinator/mqtt/__init__.py:27`, `coordinator/mqtt/__init__.py:49-50`, `coordinator/mqtt/__init__.py:71-74`).
   - Serial: `async_setup` connects to the gateway with a 5 s timeout. On failure it raises `ConfigEntryNotReady` (`coordinator/serial/__init__.py:169-178`). Its data cache starts from pyotgw `DEFAULT_STATUS` (`coordinator/serial/__init__.py:33`).
   - ESPHome: when the coordinator object is created (`__init__.py:32`), its constructor takes the ESPHome API client from the ESPHome config entry. If that entry is missing or not initialised, it raises `ConfigEntryNotReady` (`coordinator/esphome/__init__.py:94-115`). It has no `async_setup` of its own.
3. **Coordinator "added", including the boot commands.**
   - MQTT: SAT waits for the MQTT client, subscribes to each tracked topic, and then calls `boot()` (`coordinator/mqtt/__init__.py:52-64`).
   - OTGW via MQTT, `boot()` publishes:
     - `PM=3`, `PM=15` and `PM=48` (`coordinator/mqtt/opentherm.py:166-169`);
     - `MI=500` as well, but only when the manufacturer is Ideal, Intergas or Nefit (`coordinator/mqtt/opentherm.py:171-172`).
   - Context, outside SAT: `PM=<Data-ID>` asks the gateway for a one-time priority read of that Data-ID (`OTGW-Commands.md:399-407`). In the OpenTherm spec, ID 3 is the slave configuration flags (`OT-spec:1603`), ID 15 is the maximum boiler capacity and minimum modulation level (`OT-spec:2356`), and ID 48 is the DHW setpoint bounds (`OT-spec:2381`). `MI=<ms>` sets the gateway message interval (`OTGW-Commands.md:846-856`).
   - EMS-ESP and OTthing: `boot()` does nothing (`coordinator/mqtt/ems.py:103-105`, `coordinator/mqtt/otthing.py:156-158`).
   - ESPHome: SAT discovers entity keys by object_id and subscribes to their state updates (`coordinator/esphome/__init__.py:208-248`, `coordinator/esphome/__init__.py:292-303`).
   - The manufacturer used at boot comes from the config (`coordinator/__init__.py:107-108`). If none is configured, SAT fills it in from `member_id` later, in the per-tick hook (`coordinator/__init__.py:345-351`). So at boot, `MI=500` depends on the configured manufacturer only.
4. **The climate entity is added.**
   - SAT restores the last control setpoint, relative modulation value and PWM enabled flag from the previous climate state (`climate.py:169-174`, `heating_control.py:181-190`, `pwm.py:111-115`). It also restores the raw HVAC mode string (`climate.py:184-185`). With no previous state, the HVAC mode starts as OFF (`climate.py:209-210`).
   - It also calls `coordinator.async_added_to_hass()` a second time (`climate.py:147`). For MQTT coordinators this repeats the subscriptions and the boot commands, because step 3 already ran them at `__init__.py:51`. For ESPHome it repeats entity discovery and the state subscriptions (`coordinator/esphome/__init__.py:208-248`).
   - Once HA is running, SAT registers the periodic timers and calls `control_pid()` and `schedule_heating_control_loop()` (`climate.py:133-144`).
5. **No control setpoint is written before the first control tick.** All CS, MM and CH writes come from `SatHeatingControl.update()` (`heating_control.py:236-238`), which only runs from the control tick (`climate.py:540-555`).

#### B. Timers

- The control tick is scheduled every 5 s (`climate.py:706-710`). Each schedule call starts a 1 s debounced run. A second request while one is pending is ignored (`climate.py:515-531`).
- PID updates run every `PID_UPDATE_INTERVAL` = 60 s (`pid.py:20`, `climate.py:712-716`).
- Changes to the target temperature, HVAC mode or preset also schedule a tick (`climate.py:601`, `climate.py:664`).

#### C. Per control tick: what gets written

1. **Gate.** If the outside temperature is None, or the HVAC mode is OFF, the tick returns at once and **nothing is written to the boiler** (`climate.py:544-545`).
2. **Demand object.** SAT builds a `HeatingDemand` (`climate.py:547-555`) from:
   - the HVAC mode;
   - `valves_open`;
   - the timestamp;
   - `requested_setpoint`, clamped to [`MINIMUM_SETPOINT`, `coordinator.maximum_setpoint`] (`climate.py:369-394`);
   - the outside temperature.
3. **Heater-state decision** (`heating_control.py:64-72`):
   - OFF if the HVAC mode is not HEAT;
   - OFF if no valve is open;
   - otherwise ON only if `requested_setpoint > COLD_SETPOINT` (22 °C).
4. **If the demand heater state is ON** (`heating_control.py:202-228`):
   - If `requested_setpoint >= limits.maximum_setpoint`, PWM is disabled (`heating_control.py:203-204`).
   - Then the PWM enable/disable guards run, and the PWM state machine updates (`heating_control.py:206-220`). See the PWM section for both.
   - The relative modulation value is computed (`heating_control.py:222`, `heating_control.py:510-523`):
     - None if the coordinator does not support modulation management;
     - 0 (`MINIMUM_RELATIVE_MODULATION`) when the modulation state is OFF, which happens whenever PWM is enabled and DHW is not active (`heating_control.py:133-145`);
     - otherwise the configured `maximum_relative_modulation`;
     - for Geminox, at least 10.
   - The control setpoint is computed by the PWM branch (`heating_control.py:380-448`) or the continuous branch (`heating_control.py:450-508`), depending on `control_mode` (`heating_control.py:147-152`). Their formulas belong to the PWM and continuous-control sections. Facts that matter for the boiler write:
     - PWM enabled but IDLE: `control_mode` is then CONTINUOUS (`heating_control.py:147-152`), while the modulation state is still OFF because PWM is enabled (`heating_control.py:139-140`). PWM becomes IDLE, for example, when the flow temperature is unknown (`pwm.py:122-123`). The written pair is then the continuous setpoint with MM=0 (`heating_control.py:517-518`, `heating_control.py:227-228`).
     - The PWM branch changes the setpoint only when `overshoot_protection` is true. With it false, it returns without touching `_control_setpoint`, so the previous value is written again (`heating_control.py:385-386`).
     - PWM OFF phase with overshoot protection: setpoint = `MINIMUM_SETPOINT` (10) (`heating_control.py:388-390`).
     - PWM ON, flame off: setpoint = `clamp(return + flame_off_setpoint_offset_celsius, 22, limits.maximum_setpoint)` (`heating_control.py:392-408`).
     - PWM ON, flame on, no flame-on timestamp known: the previous setpoint is kept and a warning is logged (`heating_control.py:410-412`).
     - PWM ON, flame on, less than `modulation_suppression_delay_seconds` since flame on: the setpoint is the last flame-off hold setpoint if one exists, otherwise the previous setpoint is kept (`heating_control.py:414-433`). The elapsed time uses the wall clock `timestamp()` against the flame-on time recorded by the device tracker (`heating_control.py:414`, `device/__init__.py:196-212`).
     - PWM ON, flame on, after `modulation_suppression_delay_seconds`: setpoint = `flow - modulation_suppression_offset_celsius`, with no clamp, and the hold setpoint is cleared (`heating_control.py:435-448`).
     - Continuous: the setpoint is `requested_setpoint` when the boiler temperature is unknown, at or below the request, or the flame is off (`heating_control.py:458-483`). Otherwise `minimum_allowed_setpoint = boiler_temperature - flow_setpoint_offset_celsius` (`heating_control.py:485`), and there are two cases (`heating_control.py:486-489`):
       - request rising (`requested_setpoint > previous_setpoint`): setpoint = `max(requested_setpoint, minimum_allowed_setpoint)`;
       - request falling or unchanged: setpoint = `max(requested_setpoint, min(minimum_allowed_setpoint, previous_setpoint))`, so it is held at most at the previous value and never jumps up. Example: previous 40, request 38, boiler 50, offset 2 gives 40, not 48.
       - No clamp to the maximum setpoint is applied in this branch.
5. **If the demand heater state is OFF** (`heating_control.py:229-234`):
   - PWM is disabled and the guards are reset;
   - the control setpoint = `MINIMUM_SETPOINT` (10);
   - the relative modulation = the configured `maximum_relative_modulation`.
6. **Writes, always in this order, on every tick that passed the gate** (`heating_control.py:236-240`):
   1. `async_set_control_setpoint(control_setpoint)`;
   2. `async_set_control_max_relative_modulation(relative_modulation_value)`;
   3. `async_set_heater_state(ON if control_setpoint > COLD_SETPOINT else OFF)`. The CH bit follows the **computed control setpoint**, not the demand;
   4. `coordinator.async_control_heating_loop(timestamp)`. This fills in the manufacturer from `member_id` (`coordinator/__init__.py:345-351`) and steps the simulator model (`coordinator/simulator/__init__.py:91-117`).
   - Each call is awaited in turn (`heating_control.py:236-240`). On the OTGW-MQTT path one tick therefore publishes `CS`, `PM=25`, optionally `TP` (Immergas), `MM` and `CH`, each followed by the 0.2 s wait (`coordinator/mqtt/opentherm.py:195-222`, `coordinator/mqtt/__init__.py:134-147`).
7. **No change detection.** These writes are repeated on every tick, whether or not the values changed. No code compares them with the previously sent values (`heating_control.py:197-240`).
   - There is no separate keepalive command in SAT. The roughly 5 s re-send is the only repetition.
   - Context, outside SAT: the OTGW requires a `CS` of 8 or higher to be repeated at least every minute, or it reverts (`OTGW-Commands.md:493`). The 5 s re-send covers this, but the code does not name it as the reason.

#### D. Per coordinator: what each write becomes

Base-class write methods only log. The heater-state method always logs (`coordinator/__init__.py:353-355`). The setpoint, hot water setpoint, maximum setpoint and modulation methods log only when the matching `supports_*` flag is true (`coordinator/__init__.py:357-375`). The thermostat-setpoint method does nothing (`coordinator/__init__.py:377-379`). Default support flags in the base class: setpoint False, hot water setpoint False, maximum setpoint False, relative modulation management True (`coordinator/__init__.py:285-331`).

| Write | OTGW via MQTT | EMS-ESP via MQTT | OTthing via MQTT | Serial (pyotgw) | ESPHome | Switch | Simulator | Fake |
|---|---|---|---|---|---|---|---|---|
| Boot | `PM=3`, `PM=15`, `PM=48`, plus `MI=500` for Ideal, Intergas, Nefit (`coordinator/mqtt/opentherm.py:166-172`) | nothing (`coordinator/mqtt/ems.py:103-105`) | nothing (`coordinator/mqtt/otthing.py:156-158`) | connect only (`coordinator/serial/__init__.py:169-178`) | entity discovery (`coordinator/esphome/__init__.py:208-248`) | nothing | nothing | nothing |
| Control setpoint | `CS=<value>` then `PM=25` (`coordinator/mqtt/opentherm.py:195-199`) | `{"cmd":"selflowtemp","value":v}`, with v=0 when the value is exactly 10 (`coordinator/mqtt/ems.py:110-113`) | `chMode1`=`heat`, then `chSetTemp1`=`<v:.1f>` (`coordinator/mqtt/otthing.py:160-164`, `coordinator/mqtt/otthing.py:179-181`) | `set_control_setpoint(v)` (`coordinator/serial/__init__.py:192-196`), which sends `CS` per `pyotgw/pyotgw.py:564-578` | number `t_set` if discovered (`coordinator/esphome/__init__.py:257-262`) | not supported: base flag False and no override (`coordinator/switch/__init__.py:22-64`) | stored internally (`coordinator/simulator/__init__.py:83-85`) | stored internally (`coordinator/fake/__init__.py:112-115`) |
| Max relative modulation | `TP=11:12=<min(v,80)>` for Immergas only, then `MM=<v>` (`coordinator/mqtt/opentherm.py:216-222`) | `{"cmd":"burnmaxpower","value":max(v,20)}` (`coordinator/mqtt/ems.py:129-132`) | not supported (`coordinator/mqtt/otthing.py:61-63`), value None (`heating_control.py:512-515`) | `set_max_relative_mod(v)` (`coordinator/serial/__init__.py:216-220`), sends `MM` per `pyotgw/pyotgw.py:539-560` | number `max_rel_mod_level` (`coordinator/esphome/__init__.py:278-283`) | log only (base) | log only (base) | stored, flag False by default (`coordinator/fake/__init__.py:20`, `coordinator/fake/__init__.py:122-125`) |
| Heater state (CH) | `CH=1` / `CH=0` (`coordinator/mqtt/opentherm.py:211-214`) | `{"cmd":"heatingactivated","value":"on"/"off"}` (`coordinator/mqtt/ems.py:124-127`) | `chMode1`=`heat` / `off` (`coordinator/mqtt/otthing.py:171-177`) | `set_ch_enable_bit(1/0)` (`coordinator/serial/__init__.py:210-214`), sends `CH` per `pyotgw/pyotgw.py:600-615` | switch `ch_enable` (`coordinator/esphome/__init__.py:271-276`) | `switch` or `input_boolean` turn_on/turn_off (`coordinator/switch/__init__.py:56-64`) | internal on/off plus on-since time (`coordinator/simulator/__init__.py:73-81`) | internal (`coordinator/fake/__init__.py:107-110`) |
| Max CH setpoint (user number only) | `SH=<v>` (`coordinator/mqtt/opentherm.py:224-227`) | `{"cmd":"heatingtemp","value":v}` (`coordinator/mqtt/ems.py:134-137`) | not supported (base False, `coordinator/__init__.py:324-331`) | `set_max_ch_setpoint(v)`, sends `SH` per `pyotgw/pyotgw.py:503-513` (`coordinator/serial/__init__.py:222-226`) | number `max_t_set` (`coordinator/esphome/__init__.py:285-290`) | not supported | stored (`coordinator/simulator/__init__.py:87-89`) | stored, flag False (`coordinator/fake/__init__.py:18`) |
| DHW setpoint (user number only) | `SW=<v>` (`coordinator/mqtt/opentherm.py:201-204`) | `{"cmd":"dhw/seltemp","value":v}` (`coordinator/mqtt/ems.py:115-118`) | `dwhSetTemp`=`<v:.1f>` (`coordinator/mqtt/otthing.py:166-169`) | `set_dhw_setpoint(v)`, sends `SW` per `pyotgw/pyotgw.py:521-531` (`coordinator/serial/__init__.py:198-202`) | number `t_dhw_set` (`coordinator/esphome/__init__.py:264-269`) | not supported | not supported | stored, flag False (`coordinator/fake/__init__.py:19`) |
| Thermostat setpoint (push option only) | `TC=<v>` (`coordinator/mqtt/opentherm.py:206-209`) | not supported (`coordinator/mqtt/ems.py:120-122`) | no override (base no-op, `coordinator/__init__.py:377-379`) | `set_target_temp(v)` with the default `temporary=True`, so it sends `TT`, not `TC`, per `pyotgw/pyotgw.py:71-84` (`coordinator/serial/__init__.py:204-208`) | no override | no override | no override | no override |
| On unload | saves the data cache to storage, nothing published (`coordinator/mqtt/__init__.py:66-69`) | same | same | `set_control_setpoint(0)`, `set_max_relative_mod("-")`, then disconnect (`coordinator/serial/__init__.py:180-190`, `coordinator/serial/__init__.py:228-238`) | unsubscribe only (`coordinator/esphome/__init__.py:250-255`) | nothing | nothing | nothing |

Transport details:

- MQTT publish topic:
  - OTGW: `<topic>/set/<device>/command` (`coordinator/mqtt/opentherm.py:232-233`);
  - EMS: `<topic>/boiler` (`coordinator/mqtt/ems.py:142-143`);
  - OTthing: `<topic>/<suffix>` (`coordinator/mqtt/otthing.py:186-187`, `coordinator/mqtt/__init__.py:151-156`).
- MQTT subscribe topics:
  - OTGW: one topic per value, `<topic>/value/<device>/<key>` (`coordinator/mqtt/opentherm.py:174-193`, `coordinator/mqtt/opentherm.py:229-230`);
  - EMS: `<topic>/boiler_data`, whose JSON object is merged into the data cache (`coordinator/mqtt/ems.py:107-108`, `coordinator/mqtt/ems.py:139-149`);
  - OTthing: `<topic>/state` (`coordinator/mqtt/otthing.py:153-154`, `coordinator/mqtt/otthing.py:183-184`).
  - Payloads are JSON-decoded when possible. A processing error is logged and the message is dropped (`coordinator/mqtt/__init__.py:33-47`, `coordinator/mqtt/__init__.py:100-132`).
- Every MQTT publish uses QoS 1 and is followed by a 0.2 s sleep. A publish error is logged and swallowed (`coordinator/mqtt/__init__.py:134-149`).
- Serial (per local pyotgw 2.2.3): each command waits at most `OTGW_DEFAULT_TIMEOUT` = 3 s for the gateway's answer. A timeout, or a gateway error reply after the retries, is logged and the method returns None, so SAT does not see the failure (`pyotgw/vars.py:279`, `pyotgw/pyotgw.py:704-723`). All gateway error replies map to `RuntimeError`, `SyntaxError` or `ValueError` (`pyotgw/vars.py:404-434`), which are the types caught at `pyotgw/pyotgw.py:720-723`. The command processor retries up to 3 times on gateway error replies (`pyotgw/commandprocessor.py:27`, `pyotgw/commandprocessor.py:44-72`).
- Value formatting:
  - OTGW via MQTT: `CS=` uses Python's default float formatting, `f"CS={value}"` (`coordinator/mqtt/opentherm.py:196`). `SW`, `TC` and `SH` are formatted the same way (`coordinator/mqtt/opentherm.py:202`, `:207`, `:225`);
  - Serial (per local pyotgw 2.2.3): a float value is sent with two decimals, `f"{value:.2f}"` (`pyotgw/commandprocessor.py:38-41`). `TT` is pre-formatted with one decimal (`pyotgw/pyotgw.py:83`);
  - OTthing: one decimal (`coordinator/mqtt/otthing.py:180-181`).

#### E. The simulation option

When the `simulation` option is on:

- MQTT publishing is skipped (`coordinator/mqtt/__init__.py:140-141`);
- serial writes are skipped (`coordinator/serial/__init__.py:193`, `:199`, `:205`, `:211`, `:217`, `:223`);
- ESPHome writes are skipped (`coordinator/esphome/__init__.py:258`, `:265`, `:272`, `:279`, `:286`);
- switch service calls are skipped (`coordinator/switch/__init__.py:57`).

The simulator and fake coordinators do not check this option (`coordinator/simulator/__init__.py:73-89`, `coordinator/fake/__init__.py:107-130`). The serial unload sequence (`coordinator/serial/__init__.py:228-238`) does not check it either.

#### F. DHW handling

- **SAT never writes a DHW setpoint or a DHW enable from the control loop.**
  - `async_set_control_hot_water_setpoint` is called only from the "Hot Water Setpoint" number entity (`number.py:72-74`). That entity exists only when the coordinator supports hot water setpoint management (`number.py:20-21`).
  - The entity range is hard-coded to 30-60 °C (`number.py:63-70`). It is shown as available only while the coordinator reports a DHW setpoint (`number.py:48-50`, `number.py:58-60`).
  - The coordinator's boiler-reported DHW bounds (for example `coordinator/mqtt/opentherm.py:92-104`) are not used by any other module. A search found consumers only inside the coordinators.
  - No coordinator has a DHW-enable write. ESPHome discovers and subscribes to a `dhw_enable` switch (`coordinator/esphome/__init__.py:56`, `coordinator/esphome/__init__.py:229-231`), but no property or command uses it.
- **Effect of `hot_water_active` on control:**
  - The relative modulation state becomes HOT_WATER, so MM is written as the configured maximum, even while PWM is enabled (`heating_control.py:136-137`, `heating_control.py:517-520`).
  - The PWM setpoint override is skipped, so the previous control setpoint is kept (`heating_control.py:385-386`).
  - The sustained-overshoot PWM enable is blocked during DHW and for `DHW_OVERSHOOT_GUARD_SECONDS` = 300 s after DHW turns off (`heating_control.py:268-274`, `const.py:19`).
  - The runtime PWM-disable guards are reset during DHW and in the same 300 s window (`heating_control.py:313-315`).
  - The DHW off time comes from the device tracker's ON-to-OFF transition (`device/__init__.py:214-228`, `device/__init__.py:98-100`).
  - The continuous setpoint branch has no DHW check (`heating_control.py:450-508`).
  - Outside the setpoint path, DHW also feeds monitoring and PWM timing. These belong to other sections and are listed only so the DHW picture is complete:
    - the device status becomes `HEATING_HOT_WATER` when the coordinator reports CH active, the flame on and DHW active; with CH not active the status is `OFF` first (`device/status.py:39-42`, `device/status.py:87-88`);
    - cycle samples taken during DHW count as hot water, not space heating, when the cycle kind is decided (`cycles/tracker.py:103-108`), and a cycle whose tail saw any DHW is classified `UNCERTAIN` (`cycles/classifier.py:25-26`);
    - the PWM low-duty special case gives a minimum on time only when the flame is on and DHW is not active (`pwm.py:247-260`).

#### G. Limits and safety fallbacks (what the code actually does)

- **Requested setpoint clamp.**
  - The requested setpoint is clamped to [10, `coordinator.maximum_setpoint`] (`climate.py:379`, `climate.py:394`).
  - `coordinator.maximum_setpoint` returns `limits.maximum_setpoint` (`coordinator/__init__.py:274-283`). That limit is resolved in the config: the options value, else the data value, else the heating-system default of 50 °C for underfloor and 55 °C otherwise (`entry_data.py:227-239`, `helpers.py:97-99`). Because the config always yields a number, the coordinator's own None fallback (`coordinator/__init__.py:280-281`) is not reached.
  - So the clamp's upper bound and the PWM-disable threshold `limits.maximum_setpoint` (`heating_control.py:203`) are the same value, except on the switch coordinator.
  - The clamp does not use the boiler-reported `MaxTSet` or `max_t_set`. Those only feed `maximum_setpoint_value`, which is used by the number entity and `capabilities` (`coordinator/__init__.py:122-128`, `number.py:101-113`).
  - The lower bound is the constant `MINIMUM_SETPOINT` (10), not the configured `minimum_setpoint`.
  - On the switch coordinator, `maximum_setpoint` is replaced by `minimum_setpoint` (`coordinator/switch/__init__.py:41-43`).
- **Limits on the written setpoint.**
  - The flame-off hold setpoint is clamped to [22, `limits.maximum_setpoint`] (`heating_control.py:398`).
  - The suppression setpoint `flow - offset` has no clamp (`heating_control.py:441`, `heating_control.py:448`).
  - The continuous setpoint can be raised to `boiler_temperature - flow_setpoint_offset_celsius` when the request is rising, with no upper clamp (`heating_control.py:485-487`).
  - No final clamp is applied before the write (`heating_control.py:236`).
- **Maximum setpoint versus PWM.** A request at or above `limits.maximum_setpoint` disables PWM (`heating_control.py:203-204`).
- **Outside sensor lost or stale.**
  - Sensor entities are tried first, then weather entities. Each candidate is skipped if it is stale (older than `sensor_max_value_age`), unknown or unavailable (`climate.py:320-343`, `helpers.py:64-69`).
  - If none is usable, the control tick returns before any write (`climate.py:544-545`). The boiler keeps whatever SAT sent last.
  - Context, outside SAT: on an OTGW, a CS of 8 or higher that is not repeated within about one minute is reverted by the gateway (`OTGW-Commands.md:493`).
- **Inside sensor stale.** With `sensor_max_value_age` > 0, `control_pid` resets the PID and returns (`climate.py:466-478`). A missing temperature state skips the PID update (`climate.py:480-482`). The control tick itself still runs on `pid.output` (`climate.py:375`). See the PID section for what that output is after a reset.
- **No heating curve value.** `requested_setpoint` returns 10 (`climate.py:372-373`), which gives the heater state OFF (`heating_control.py:72`).
- **HVAC mode set to OFF.** The tick returns before `update()`, so **no CS=10, MM or CH=0 is sent** (`climate.py:544-545`). The OFF branch at `heating_control.py:229-234` runs only while the HVAC mode is HEAT and either no valve is open or the request is 22 °C or less, or when a restored HVAC state is "unknown" or "unavailable" (see Edge cases).
- **Missing boiler data.**
  - Continuous mode without a boiler temperature writes the request unchanged (`heating_control.py:458-465`).
  - PWM flame-off without a return temperature keeps the previous setpoint and logs a warning (`heating_control.py:393-395`).
  - PWM flame-on without a flow temperature does the same (`heating_control.py:435-437`).
  - The overshoot-enable guard is skipped when the flow temperature is None (`heating_control.py:284-286`).
- **No staleness check on boiler-side values.**
  - The coordinators keep the last received value with no age check.
  - MQTT coordinators even restore the previous run's values from storage at setup (`coordinator/mqtt/__init__.py:71-74`).
  - `MAX_BOILER_TEMPERATURE_AGE` = 60 is defined (`const.py:40`) but not referenced anywhere else. A search found only its definition.
  - ESPHome returns None for entities whose state is missing (`coordinator/esphome/__init__.py:310-312`).
- **Overheating.** No code compares the flow temperature with a hard maximum and cuts the burner.
  - `MAXIMUM_SETPOINT` = 65 is defined (`const.py:36`) but not referenced elsewhere.
  - The only overshoot reactions are the PWM enable and disable guards (`heating_control.py:262-378`) and the cycle classification. Both are covered in other sections.
- **Pressure and synchronisation.** These are display-only binary sensors. Nothing in `heating_control.py` reads them.
  - Pressure Health: on when the EMA-smoothed pressure is below the minimum or above the maximum, or the drop rate exceeds the maximum, confirmed after 120 s (`binary_sensor.py:252-295`, `binary_sensor.py:315-333`). It is also on when no pressure has been received for longer than `pressure_max_value_age` after one was seen (`binary_sensor.py:264-271`). The drop rate is a least-squares slope over the stored samples, needs at least 3 samples spanning 300 s, and is suspended for 600 s after the CH active state changes (`binary_sensor.py:335-388`).
  - Control Setpoint Synchronization and Relative Modulation Synchronization: on after 60 s of mismatch (`binary_sensor.py:68-159`).
  - Central Heating Synchronization: on after 60 s in which the climate `hvac_action` and the coordinator's `active` disagree (`binary_sensor.py:162-197`).
  - Device Health: on only while no device status has been computed yet (`binary_sensor.py:391-411`, `device/__init__.py:44-49`).
- **Manufacturer-specific limits.**
  - Geminox: MM of at least 10 (`heating_control.py:522-523`). This floor is applied only in the demand-ON branch, because `_compute_relative_modulation_value` is called only there (`heating_control.py:222`). The demand-OFF branch writes the configured maximum without the floor (`heating_control.py:234`).
  - EMS: `burnmaxpower` of at least 20 (`coordinator/mqtt/ems.py:130`).
  - Immergas: TSP 11:12 capped at 80 (`coordinator/mqtt/opentherm.py:217-218`).
  - Ideal, Intergas, Geminox and Nefit report `supports_relative_modulation` = False (`coordinator/__init__.py:303-313`). The OTGW-MQTT, EMS, OTthing and serial coordinators override this to True (`coordinator/mqtt/opentherm.py:55-57`, `coordinator/mqtt/ems.py:50-52`, `coordinator/mqtt/otthing.py:57-59`, `coordinator/serial/__init__.py:73-75`). This flag is the read capability only. The MM write is gated by a different flag, `supports_relative_modulation_management`, which is True in the base class (`coordinator/__init__.py:315-322`) and is checked at `heating_control.py:512`.

### Formulas

Variable names are as in the code.

- Requested setpoint, COMFORT mode: `setpoint = pid.output`; `setpoint = max(setpoint, areas.pids.output)` if not None; `setpoint = min(setpoint, areas.pids.overshoot_cap)` if not None; minus `solar_gain.setpoint_offset_celsius` when solar gain is active and enabled; then `clamp(setpoint, MINIMUM_SETPOINT, coordinator.maximum_setpoint)` (`climate.py:375-394`).
- Requested setpoint, ECO mode: `clamp(round(pid.output, 1), MINIMUM_SETPOINT, coordinator.maximum_setpoint)` (`climate.py:378-379`).
- Demand heater state: `ON if hvac_mode == HEAT and valves_open and requested_setpoint > COLD_SETPOINT else OFF` (`heating_control.py:64-72`).
- Written CH state: `ON if _control_setpoint > COLD_SETPOINT else OFF` (`heating_control.py:238`).
- Written relative modulation: `None` if management is unsupported; else `0` if `relative_modulation_state == OFF`, else `config.pwm.maximum_relative_modulation`; then `max(10, value)` for Geminox (`heating_control.py:510-523`). The value is `config.pwm.maximum_relative_modulation` when demand is off (`heating_control.py:234`).
- Relative modulation state: `HOT_WATER` if `hot_water_active`; else `OFF` if PWM is enabled; else `COLD` if the reported setpoint is None or 10 or less; else `PWM_OFF` (`heating_control.py:133-145`).
- Default maximum setpoint: `50 if heating_system == UNDERFLOOR else 55` (`helpers.py:97-99`).
- Coordinator minimum setpoint: `max(limits.minimum_setpoint, MINIMUM_SETPOINT)` (`coordinator/__init__.py:269-272`).
- Boiler power, shown in an entity only: `min_cap = boiler_capacity * minimum_relative_modulation_value / 100`; `power = 0` if the flame is off; else `min_cap + (boiler_capacity - min_cap) * relative_modulation_value / 100` (`coordinator/__init__.py:228-257`).
- EMS setpoint mapping: `0 if value == 10 else value` (`coordinator/mqtt/ems.py:111`).
- Simulator flow model, per tick: move `_boiler_temperature` towards `target` by `simulated_heating` (up) or `simulated_cooling` (down), snapping when within one step. `target` = `minimum_setpoint` if that is at or above the setpoint; else 10 during the first `simulated_warming_up` seconds after switch-on; else the setpoint. Flame = `active and target > _boiler_temperature`; modulation = 100 with the flame on, 0 with it off (`coordinator/simulator/__init__.py:61-67`, `coordinator/simulator/__init__.py:91-129`).

### Constants and defaults

**Constants in code.** A "not referenced elsewhere" note means a search of all `.py` files found only the definition.

| Name | Value | Unit | Citation | Used for |
|---|---|---|---|---|
| `COLD_SETPOINT` | 22 | °C | `const.py:34` | heater ON threshold for demand and for the written CH (`heating_control.py:72`, `:238`) |
| `MINIMUM_SETPOINT` | 10.0 | °C | `const.py:35` | demand-off setpoint (`heating_control.py:233`), PWM OFF setpoint (`heating_control.py:389`), clamp lower bound (`climate.py:379`, `:394`) |
| `MAXIMUM_SETPOINT` | 65.0 | °C | `const.py:36` | not referenced elsewhere |
| `MINIMUM_RELATIVE_MODULATION` | 0 | % | `const.py:37` | MM while PWM is enabled (`heating_control.py:518`) |
| `MAXIMUM_RELATIVE_MODULATION` | 100 | % | `const.py:38` | not referenced elsewhere |
| `MAX_BOILER_TEMPERATURE_AGE` | 60 | (s, by name) | `const.py:40` | not referenced elsewhere |
| `DHW_OVERSHOOT_GUARD_SECONDS` | 300.0 | s | `const.py:19` | PWM guard window after DHW (`heating_control.py:272`, `:313`) |
| `BOILER_DEADBAND` | 2 | °C | `const.py:17` | not referenced elsewhere |
| `HEATER_STARTUP_TIMEFRAME` | 180 | s | `const.py:18` | PWM only (`pwm.py:50`, `pwm.py:155`, `pwm.py:194-202`) |
| `OVERSHOOT_SUSTAIN_SECONDS` | 60.0 | s | `const.py:21` | PWM enable guard (`heating_control.py:296`) |
| `UNDERHEAT_SUSTAIN_SECONDS` | 180.0 | s | `const.py:22` | PWM disable guard (`heating_control.py:345`) |
| `SATURATION_SUSTAIN_SECONDS` | 300.0 | s | `const.py:23` | PWM disable guard (`heating_control.py:367`) |
| `OVERSHOOT_MARGIN_CELSIUS` / `UNDERSHOOT_MARGIN_CELSIUS` | 3.0 / -3.0 | °C | `cycles/const.py:10-11` | PWM guards (`heating_control.py:288`, `:337`) |
| `PWM_ENABLE/DISABLE_MARGIN_CELSIUS`, `PWM_ENABLE/DISABLE_LOW_MODULATION_PERCENT`, `PWM_LOW_MODULATION_PERSISTENCE_TICKS` | 0.5, 1.5, 10, 30, 6 | °C, °C, %, %, ticks | `const.py:26-30` | not referenced elsewhere |
| `OVERSHOOT_PROTECTION_REQUIRED_DATASET`, `OVERSHOOT_PROTECTION_SETPOINT` | 40; heat pump 40, radiators 62, underfloor 45 | -, °C | `const.py:188-193` | not referenced elsewhere |
| Control tick interval | 5 | s | `climate.py:708` | schedules the tick |
| Tick debounce | 1 | s | `climate.py:530` | delay before the tick runs |
| `PID_UPDATE_INTERVAL` | 60 | s | `pid.py:20` | PID timer |
| MQTT post-publish wait | 0.2 | s | `coordinator/mqtt/__init__.py:134`, `:147` | after each publish |
| MQTT QoS | 1 | - | `coordinator/mqtt/__init__.py:144` | all commands |
| Listener notify debounce | 0.2 | s | `coordinator/__init__.py:96-102` | entity updates |
| Serial connect timeout | 5 | s | `coordinator/serial/__init__.py:171` | setup |
| Serial test connect in the config flow | 5 | s | `config_flow.py:264` | config flow only |
| pyotgw command answer timeout (local 2.2.3) | 3 | s | `pyotgw/vars.py:279` | every serial command (`pyotgw/pyotgw.py:704-716`) |
| Base hot water min / max | 30 / 60 | °C | `coordinator/__init__.py:198-206` | defaults, not consumed outside coordinators |
| Number "Hot Water Setpoint" range | 30-60 | °C | `number.py:63-70` | user DHW write |
| Number "Maximum Setpoint" range | 30-80 | °C | `number.py:116-123` | user max CH setpoint write |
| Sync sensor delay | 60 | s | `binary_sensor.py:71` | display |
| Pressure problem confirmation; drop-rate settle; drop-rate min window; EMA alpha | 120; 600; 300; 0.05 | s; s; s; - | `binary_sensor.py:25-28` | display |
| EMS `burnmaxpower` floor | 20 | % | `coordinator/mqtt/ems.py:130` | MM mapping |
| Immergas TSP 11:12 cap | 80 | % | `coordinator/mqtt/opentherm.py:218` | MM mapping |
| Geminox MM floor | 10 | % | `heating_control.py:523` | MM |
| OTGW `MI` for Ideal, Intergas, Nefit | 500 | ms (per `OTGW-Commands.md:846`) | `coordinator/mqtt/opentherm.py:171-172` | boot |
| Manufacturer member IDs | Atag 4, Baxi 4, Brotge 4, DeDietrich 4, Ferroli 9, Geminox 4, Ideal 6, Immergas 27, Intergas 173, Itho 29, Nefit 131, Radiant 41, Remeha 11, Sime 27, Vaillant 24, Viessmann 33, Worcester 95, Other -1 | - | `manufacturer.py:6-25` | manufacturer detection |

**Configuration options and their defaults.**

- "Store" says where the value is read from: data means config-entry data, options means config-entry options.
- Options defaults come from `OPTIONS_DEFAULTS` (`const.py:111-185`), merged at `__init__.py:29`.
- Durations given as "HH:MM:SS" are converted to seconds (`helpers.py:72-81`).

| Option key | Default | Unit | Store (read at) | UI where set | Citation of default |
|---|---|---|---|---|---|
| `mode` | none | - | data (`entry_data.py:134-136`) | config flow menu: mosquitto (then OTGW, EMS or OTthing), esphome, serial, switch, and simulator in advanced mode; `fake` is not offered (`config_flow.py:48-60`, `config_flow.py:127-168`) | - |
| `device` | serial: `socket://otgw.local:25238` | - | data (`entry_data.py:111-113`) | config flow | `config_flow.py:282` |
| `mqtt_topic` | OTGW: `OTGW`, EMS: `ems-esp`, OTthing: none | - | data (`entry_data.py:130-132`) | config flow (`config_flow.py:558-563`, `config_flow.py:214`) | `config_flow.py:177-196` |
| `manufacturer` | first manufacturer matching `member_id`, else -1 | - | data (`entry_data.py:126-128`) | config flow (`config_flow.py:508-535`) | `config_flow.py:517-518` |
| `heating_system` | none in data; radiators as form default | - | data (`entry_data.py:119-124`). When the key is absent the `heating_system` property returns UNKNOWN (`entry_data.py:121-122`), and that value goes to PWM (`heating_control.py:86`) and to the coordinator's default maximum (`coordinator/__init__.py:277`). Only the `limits` fallback uses RADIATORS (`entry_data.py:228`) | config flow, form default RADIATORS (`config_flow.py:401-433`) | `const.py:184` (an options default, used here only as the form default, `config_flow.py:411-413`) |
| `minimum_setpoint` | 10; switch step default 50 | °C | data, else options (`entry_data.py:238`) | switch and simulator steps only (`config_flow.py:302-304`, `:334-339`) | `const.py:147` |
| `maximum_setpoint` | 50 underfloor, 55 otherwise | °C | options, else data, else default (`entry_data.py:229-235`) | options "general", 10-100 (`config_flow.py:601-614`) | `helpers.py:97-99` |
| `maximum_relative_modulation` | 100 | % | options (`entry_data.py:222`) | options "advanced", 0-100, OTGW-MQTT, serial and simulator only (`config_flow.py:726`, `:751-755`) | `const.py:148` |
| `force_pulse_width_modulation` | False | - | options (`entry_data.py:223`) | options "advanced", same modes (`config_flow.py:727-728`) | `const.py:149` |
| `overshoot_protection` | False | - | **data** (`entry_data.py:150-151`) | no UI; set only by migration from old options (`__init__.py:159-162`) | `const.py:122` |
| `flame_off_setpoint_offset_celsius` | 18.0 | °C | options (`entry_data.py:174-175`) | options "advanced", 0-30, OTGW-MQTT, serial and simulator only (`config_flow.py:751`, `:764-767`) | `const.py:151` |
| `modulation_suppression_delay_seconds` | 20 | s | options (`entry_data.py:178-179`) | options "advanced", 0-120, same modes (`config_flow.py:751`, `:768-771`) | `const.py:152` |
| `modulation_suppression_offset_celsius` | 1.0 | °C | options (`entry_data.py:182-183`) | options "advanced", 0-10, same modes (`config_flow.py:751`, `:772-775`) | `const.py:153` |
| `flow_setpoint_offset_celsius` | 2.0 | °C | options (`entry_data.py:186-187`) | options "advanced", 0-10, all modes (`config_flow.py:777-780`) | `const.py:154` |
| `cycles_per_hour` | 4 | 1/h | options (`entry_data.py:219`) | options "system configuration" (`config_flow.py:686-702`) | `const.py:119` |
| `duty_cycle` | 00:13:00 (780 s) | s | options (`entry_data.py:220`) | no UI found | `const.py:167` |
| `push_setpoint_to_thermostat` | False | - | **data** (`entry_data.py:154-155`) | options "presets" writes it to options (`config_flow.py:670`) | `const.py:138` |
| `sensor_max_value_age` | 06:00:00 (21600 s) | s | options (`entry_data.py:199-201`) | options "system configuration" (`config_flow.py:705`) | `const.py:170` |
| `window_minimum_open_time` | 00:00:15 | s | options (`entry_data.py:196-198`) | options "system configuration" (`config_flow.py:706`) | `const.py:172` |
| `simulation` | False | - | options (`entry_data.py:291`) | options "advanced" (`config_flow.py:719`) | `const.py:129` |
| `error_monitoring` | False | - | options (`entry_data.py:116-117`) | options "advanced" (`config_flow.py:720`) | `const.py:116` |
| `minimum_boiler_pressure` / `maximum_boiler_pressure` | 0.8 / 2.5 | bar | options (`entry_data.py:251-252`) | options "advanced", 0-4 (`config_flow.py:781-788`) | `const.py:157-158` |
| `maximum_pressure_drop_rate` | 0.3 | bar/h | options (`entry_data.py:253`) | options "advanced", 0-3 (`config_flow.py:789-792`) | `const.py:159` |
| `pressure_max_value_age` | 00:30:00 | s | options (`entry_data.py:254`) | options "advanced" (`config_flow.py:793-795`) | `const.py:160` |
| `minimum_consumption` / `maximum_consumption` | 0 / 0 | (no unit in code) | options (`entry_data.py:241-242`) | options "advanced", 0-8, OTGW-MQTT, serial and simulator only (`config_flow.py:751`, `:756-763`) | `const.py:163-164` |
| `climate_valve_offset` | 0 | °C | options (`entry_data.py:244`) | options "advanced", -1 to 1 (`config_flow.py:734-737`) | `const.py:168` |
| `target_temperature_step` | 0.5 | °C | options (`entry_data.py:245`) | options "advanced", 0.1-1 (`config_flow.py:730-733`) | `const.py:169` |
| `automatic_gains` | True | - | options (`entry_data.py:211`) | config flow (`config_flow.py:475`) | `const.py:120` |
| `proportional` / `integral` / `derivative` | "45" / "0" / "6000" | - | options (`entry_data.py:207-209`) | config flow step "pid_controller" and options "general", both only when automatic gains is off (`config_flow.py:464-465`, `config_flow.py:492-505`, `config_flow.py:620-623`) | `const.py:113-115` |
| `automatic_gains_value` | 2.0 | - | options (`entry_data.py:212`) | no UI found | `const.py:121` |
| `heating_curve_coefficient` | 2.0 | - | options (`entry_data.py:213`) | options "general", 0.1-12 (`config_flow.py:616-618`) | `const.py:182` |
| `heating_mode` | comfort | - | options (`entry_data.py:259`) | options "general", only with rooms (`config_flow.py:604-610`) | `const.py:183` |
| presets home / comfort / sleep / away / activity | 18 / 20 / 15 / 10 / 10 | °C | options (`entry_data.py:261-276`) | options "presets", 5-35 (`config_flow.py:648-668`) | `const.py:175-179` |
| `sync_climates_with_mode` / `sync_climates_with_preset` | True / False | - | options (`entry_data.py:272-273`) | options (`config_flow.py:704`, `:669`) | `const.py:139-140` |
| `thermal_comfort` | False | - | options (`entry_data.py:271`) | options "advanced" (`config_flow.py:721`) | `const.py:131` |
| solar gain: compensation / freeze integral / min elevation / min rise / offset | False / True / 12.0 / 0.6 / 2.0 | - / - / ° / °C/h / °C | options (`entry_data.py:281-285`) | options "advanced" (`config_flow.py:722-723`, `:738-749`) | `const.py:132-136` |
| simulated heating / cooling / warming up | 20 / 5 / 00:00:15 | °C per tick / °C per tick / s | data, default from options defaults (`entry_data.py:292-294`) | simulator step (`config_flow.py:322-343`) | `const.py:143-144`, `:171` |

### State kept between passes

- `SatHeatingControl` (`heating_control.py:89-101`):
  - `_control_setpoint`, initially 10;
  - `_relative_modulation_value`, initially the configured maximum;
  - `_flame_off_hold_setpoint`;
  - the three sustained-condition timers;
  - `_last_requested_setpoint` and `_last_outside_temperature`.
- The control setpoint and relative modulation value are restored at start-up from the climate state attributes `setpoint` and `relative_modulation_value` (`heating_control.py:186-190`, `climate.py:243`, `climate.py:257`).
- The PWM enabled flag is restored from `pulse_width_modulation_enabled` (`heating_control.py:183-184`, `climate.py:260`).
- MQTT coordinators: the whole data cache is saved to HA storage on removal and reloaded at setup (`coordinator/mqtt/__init__.py:66-78`).
- Device tracker (`device/__init__.py:21-42`, `device/__init__.py:102-132`):
  - the flame on/off and DHW on/off timestamps, in memory only, taken from coordinator updates (`device/__init__.py:134-148`, `device/__init__.py:196-228`);
  - the modulation-reliability flag, the only value persisted, in storage `sat.device.<id>` (`device/__init__.py:105`, `device/__init__.py:127-131`).
- Coordinator: the manufacturer, filled in once from `member_id` if it was not configured (`coordinator/__init__.py:348-351`).
- Simulator: setpoint, boiler temperature, device state and on-since time (`coordinator/simulator/__init__.py:19-27`).

### Outputs (to the boiler, to entities)

**To the boiler.** See the table in Behaviour D. On every control tick that passes the gate, SAT writes the control setpoint, then the maximum relative modulation, then the CH enable (`heating_control.py:236-238`). These are written only on user action:

- DHW setpoint (`number.py:72-74`);
- maximum CH setpoint (`number.py:125-127`);
- thermostat setpoint (TC/TT), written when the target temperature changes and `push_setpoint_to_thermostat` is true (`climate.py:655-657`).

**To entities.**

- The climate entity's attributes include (`climate.py:234-263`):
  - `setpoint`, the computed control setpoint;
  - `requested_setpoint`;
  - `relative_modulation_value` and `relative_modulation_state`;
  - the PWM enabled flag, duty cycle and state;
  - `valves_open`.
- `hvac_action` is OFF, IDLE or HEATING, based on the HVAC mode and `coordinator.active` (`climate.py:358-367`).
- Number entities "Hot Water Setpoint" and "Maximum Setpoint" (`number.py:16-21`).
- Binary sensors: Cycle Health, Device Health, Pressure Health and Central Heating Synchronization, plus Window Sensor, Control Setpoint Synchronization and Relative Modulation Synchronization when they apply (`binary_sensor.py:49-65`).
- Sensors: PID, cycle, device, manufacturer, error, requested setpoint, heating curve and heating curve recommendation, plus power and consumption when modulation management is supported (consumption only when both consumption bounds are above 0) (`sensor.py:45-68`).
- Serial mode adds its own sensor and binary-sensor platforms, and simulator mode adds simulator sensors (`sensor.py:39-43`, `binary_sensor.py:46-47`). Their contents are not covered here.

### Edge cases

- **CH can be written OFF while SAT is "heating".** CH follows `control_setpoint > 22` (`heating_control.py:238`). The following setpoints all give CH=0:
  - a flame-off hold that clamps to exactly 22 (`heating_control.py:398`);
  - a suppression setpoint `flow - 1.0` of 22 or less (`heating_control.py:441`);
  - the PWM OFF setpoint of 10 (`heating_control.py:389`).
- **PWM without overshoot protection.** The setpoint override returns early (`heating_control.py:385-386`). Because `overshoot_protection` is read from data (`entry_data.py:151`) and no config-flow step sets it, a new installation keeps the previous `_control_setpoint`, while MM is still forced to 0 whenever PWM is enabled (`heating_control.py:517-518`).
- **EMS sends 0 for 10.** An EMS setpoint of exactly 10 is sent as 0 (`coordinator/mqtt/ems.py:111`).
- **OTthing, off tick.** The CS write publishes `chMode1=heat` and the heater-state write then publishes `chMode1=off` in the same tick (`coordinator/mqtt/otthing.py:160-164`, `coordinator/mqtt/otthing.py:171-177`).
- **Switch with a `valve` entity.** The config flow accepts `switch`, `valve` and `input_boolean` entities (`config_flow.py:299-301`). The service map has no `valve` entry, so for a valve no service call is made (`coordinator/switch/__init__.py:16-19`, `coordinator/switch/__init__.py:58-62`).
- **Boot commands run twice on MQTT coordinators.** `async_added_to_hass` is called from both `__init__.py:51` and `climate.py:147`, so subscriptions and boot commands are issued twice (`coordinator/mqtt/__init__.py:52-64`). On ESPHome the state subscriptions are registered twice as well (`coordinator/esphome/__init__.py:208-248`).
- **HVAC mode change aborts early.** When cascading the HVAC mode to radiators, a radiator whose state is missing, or whose `hvac_modes` list does not contain the mode, makes `async_set_hvac_mode` return. The tick scheduling and the state write at the end are then skipped (`climate.py:590-602`).
- **Unrounded CS value.** In COMFORT mode the requested setpoint is not rounded (`climate.py:394`, versus `climate.py:379` for ECO). The value is formatted unrounded in `CS={value}` on the OTGW-MQTT path (`coordinator/mqtt/opentherm.py:196`). On the serial path the local pyotgw 2.2.3 sends it with two decimals (`pyotgw/commandprocessor.py:38-41`).
- **Removal behaviour differs by backend.** On removal, serial explicitly releases control with CS=0 and MM="-" (`coordinator/serial/__init__.py:228-233`). MQTT and ESPHome send nothing.
  - The coordinator removal hook is called from the entry unload (`__init__.py:66-68`), from the climate entity's removal (`climate.py:162`) and on HA stop (`climate.py:150`), so the serial release can run more than once.
  - Any options change reloads the entry: unload, then a fresh setup (`__init__.py:55`, `__init__.py:92-100`). On serial this sends the CS=0 release and reconnects.
  - Context, outside SAT: `CS=0` makes the gateway fall back to the thermostat's value or its internal default of 10 and clears the CH enable bit (`OTGW-Commands.md:492`).
- **Simulation option does not cover every write.** The serial unload release ignores the `simulation` option (`coordinator/serial/__init__.py:228-238`), and so do the simulator and fake coordinators.
- **Restored HVAC state "unknown" or "unavailable".** The tick gate compares the raw `_hvac_mode` with OFF (`climate.py:544`), so a restored "unknown" or "unavailable" passes the gate. The `hvac_mode` property maps those states to OFF (`climate.py:353-354`). The demand heater state is then OFF, and every tick writes CS=10, MM=maximum and CH=0 (`heating_control.py:66-67`, `heating_control.py:229-238`).
- **Weather entity without a temperature.** For a weather entity, `float()` is applied to the `temperature` attribute with no None check (`climate.py:340-341`).
- **ESPHome with partial entities.** Each write is skipped when its entity was not discovered (`coordinator/esphome/__init__.py:258`, `:265`, `:272`, `:279`, `:286`). Keys are rediscovered on device updates (`coordinator/esphome/__init__.py:354-363`).
- **Shared member IDs.** Several manufacturers share a member ID: Atag, Baxi, Brotge, DeDietrich and Geminox all have 4, Immergas and Sime both have 27 (`manufacturer.py:6-25`). Lookup by member ID returns all matches in table order, and both the per-tick backfill and the config-flow default take the first one (`manufacturer.py:50-57`, `coordinator/__init__.py:349-351`, `config_flow.py:517-518`). Auto-detection therefore yields Atag for ID 4 and Immergas for ID 27. A Geminox boiler gets its MM floor only if Geminox is chosen explicitly in the manufacturer step (`config_flow.py:508-535`, `heating_control.py:522-523`).
- **Geminox floor not applied when demand is off.** See Behaviour G: the OFF branch writes the configured maximum directly (`heating_control.py:234`).

### Additional behaviour (completeness pass)

This subsection covers coordinator code that the sections above did not cite. It was found by listing every function in `custom_components/sat` and checking it against the citations in the `*-thermonova.md` files.

#### Capability flags per backend, and what they really gate

Each coordinator answers five yes/no questions. Base class defaults:

- setpoint management: False;
- hot water setpoint management: False;
- maximum setpoint management: False;
- relative modulation read (`supports_relative_modulation`): True, except False when the manufacturer is Ideal, Intergas, Geminox or Nefit;
- relative modulation management: True.

Sources: `coordinator/__init__.py:285-331`.

| Flag | OTGW via MQTT | EMS-ESP | OTthing | Serial | ESPHome | Simulator | Fake | Switch |
|---|---|---|---|---|---|---|---|---|
| setpoint management | True (`coordinator/mqtt/opentherm.py:43-45`) | True (`coordinator/mqtt/ems.py:38-40`) | True (`coordinator/mqtt/otthing.py:49-51`) | True (`coordinator/serial/__init__.py:61-63`) | True only if the `t_set` number was discovered (`coordinator/esphome/__init__.py:127-129`) | True (`coordinator/simulator/__init__.py:37-39`) | True (`coordinator/fake/__init__.py:17`, `coordinator/fake/__init__.py:69-74`) | base (False) |
| hot water setpoint management | True (`coordinator/mqtt/opentherm.py:47-49`) | True (`coordinator/mqtt/ems.py:42-44`) | True (`coordinator/mqtt/otthing.py:53-55`) | True (`coordinator/serial/__init__.py:65-67`) | only if `t_dhw_set` was discovered (`coordinator/esphome/__init__.py:131-133`) | base (False) | False (`coordinator/fake/__init__.py:19`, `coordinator/fake/__init__.py:76-81`) | base (False) |
| maximum setpoint management | True (`coordinator/mqtt/opentherm.py:51-53`) | True (`coordinator/mqtt/ems.py:46-48`) | base (False) | True (`coordinator/serial/__init__.py:69-71`) | only if `max_t_set` was discovered (`coordinator/esphome/__init__.py:135-137`) | True (`coordinator/simulator/__init__.py:41-43`) | False (`coordinator/fake/__init__.py:18`, `coordinator/fake/__init__.py:83-88`) | base (False) |
| relative modulation read | True (`coordinator/mqtt/opentherm.py:55-57`) | True (`coordinator/mqtt/ems.py:50-52`) | True (`coordinator/mqtt/otthing.py:57-59`) | True (`coordinator/serial/__init__.py:73-75`) | `rel_mod_level` discovered AND the base manufacturer rule (`coordinator/esphome/__init__.py:139-141`) | True (`coordinator/simulator/__init__.py:45-47`) | equal to the management flag (`coordinator/fake/__init__.py:97-99`) | base rule |
| relative modulation management | base (True) | base (True) | False (`coordinator/mqtt/otthing.py:61-63`) | base (True) | base (True) | base (True) | False (`coordinator/fake/__init__.py:20`, `coordinator/fake/__init__.py:90-95`) | base (True) |

"Base" means a search for `supports_` in that coordinator file found no override. For the switch coordinator, `coordinator/switch/__init__.py` contains no `supports_` line. For the simulator, only the three flags in the table are overridden (`coordinator/simulator/__init__.py:37-47`).

What each flag actually controls (consumers found by searching all `.py` files):

- **Setpoint management.** It decides whether the "Control Setpoint Synchronization" binary sensor is created (`binary_sensor.py:59-60`). In the base write method it only decides whether a log line is written (`coordinator/__init__.py:357-360`). It does not block the boiler write itself. For example, OTGW via MQTT publishes `CS=` and `PM=25` first and only then calls the base method (`coordinator/mqtt/opentherm.py:195-199`). ESPHome checks its own discovered key instead (`coordinator/esphome/__init__.py:257-262`).
- **Hot water and maximum setpoint management.** They decide whether the "Maximum Setpoint" and "Hot Water Setpoint" number entities are created (`number.py:17-21`), plus the base log lines (`coordinator/__init__.py:362-370`).
- **Relative modulation management.** This is the only flag that gates a value SAT computes for the boiler. If it is False, the MM value becomes None (`heating_control.py:512-515`). It also decides whether the "Relative Modulation Synchronization" binary sensor is created (`binary_sensor.py:62-63`), and whether the "Current Power" and "Current Consumption" display sensors are created (`sensor.py:59-66`).
- **Relative modulation read (`supports_relative_modulation`).** A search of all `.py` files found only its definitions and the ESPHome override that calls the base rule (`coordinator/__init__.py:303-313`, `coordinator/esphome/__init__.py:139-141`). Nothing else reads it. So the manufacturer exception for Ideal, Intergas, Geminox and Nefit (`coordinator/__init__.py:310-311`) changes no control decision and no entity in this code.

#### Remaining boiler values, and who reads them

The Inputs table above covers the values used by control. These five values are read by every backend but were not cited yet:

| Property | OTGW via MQTT | EMS-ESP | OTthing | Serial | ESPHome | Base default |
|---|---|---|---|---|---|---|
| `hot_water_setpoint` | `TdhwSet` (`coordinator/mqtt/opentherm.py:16`, `coordinator/mqtt/opentherm.py:85-90`) | `dhw/seltemp` (`coordinator/mqtt/ems.py:15`, `coordinator/mqtt/ems.py:70-72`) | thermostat `dhw_set_t` (`coordinator/mqtt/otthing.py:24`, `coordinator/mqtt/otthing.py:87-89`) | `DATA_DHW_SETPOINT` (`coordinator/serial/__init__.py:84-89`) | number `t_dhw_set` (`coordinator/esphome/__init__.py:159-161`) | None (`coordinator/__init__.py:178-181`) |
| `minimum_hot_water_setpoint` / `maximum_hot_water_setpoint` | `TdhwSetUBTdhwSetLB_value_lb` / `_hb` (`coordinator/mqtt/opentherm.py:30-31`, `coordinator/mqtt/opentherm.py:92-104`) | base | slave `dhwMin` / `dhwMax` (`coordinator/mqtt/otthing.py:25-26`, `coordinator/mqtt/otthing.py:103-123`) | `DATA_SLAVE_DHW_MIN_SETP` / `DATA_SLAVE_DHW_MAX_SETP` (`coordinator/serial/__init__.py:112-124`) | sensors `t_dhw_set_lb` / `t_dhw_set_ub`; a reading of None or 0 falls back to the base value because of the `or` (`coordinator/esphome/__init__.py:163-169`) | 30 / 60 (`coordinator/__init__.py:198-206`) |
| `boiler_capacity` | `MaxCapacityMinModLevel_hb_u8` (`coordinator/mqtt/opentherm.py:26`, `coordinator/mqtt/opentherm.py:134-139`) | `nompower` (`coordinator/mqtt/ems.py:23`, `coordinator/mqtt/ems.py:86-88`) | slave `max_capacity` (`coordinator/mqtt/otthing.py:19`, `coordinator/mqtt/otthing.py:125-127`) | `DATA_SLAVE_MAX_CAPACITY` (`coordinator/serial/__init__.py:133-138`) | sensor `max_capacity` (`coordinator/esphome/__init__.py:187-189`) | None (`coordinator/__init__.py:223-226`) |
| `minimum_relative_modulation_value` | `MaxCapacityMinModLevel_lb_u8`, else the legacy key `MaxCapacityMinModLevell_lb_u8` (`coordinator/mqtt/opentherm.py:27-28`, `coordinator/mqtt/opentherm.py:141-150`) | `burnminpower` (`coordinator/mqtt/ems.py:25`, `coordinator/mqtt/ems.py:90-92`) | slave key `"maxModulation"` (`coordinator/mqtt/otthing.py:30`, `coordinator/mqtt/otthing.py:139-144`) | `DATA_SLAVE_MIN_MOD_LEVEL` (`coordinator/serial/__init__.py:140-145`) | sensor `min_mod_level` (`coordinator/esphome/__init__.py:191-193`) | None (`coordinator/__init__.py:259-262`) |
| `maximum_relative_modulation_value` | `MaxRelModLevelSetting` (`coordinator/mqtt/opentherm.py:29`, `coordinator/mqtt/opentherm.py:152-157`) | `burnmaxpower` (`coordinator/mqtt/ems.py:26`, `coordinator/mqtt/ems.py:94-96`) | slave key `"min_modulation"` (`coordinator/mqtt/otthing.py:28`, `coordinator/mqtt/otthing.py:146-151`) | `DATA_SLAVE_MAX_RELATIVE_MOD` (`coordinator/serial/__init__.py:147-152`) | number `max_rel_mod_level` (`coordinator/esphome/__init__.py:195-197`) | None (`coordinator/__init__.py:264-267`) |

The fake coordinator starts with a relative modulation of 100 and replaces it with every MM write (`coordinator/fake/__init__.py:45`, `coordinator/fake/__init__.py:65-67`, `coordinator/fake/__init__.py:122-125`).

Who reads these values:

- `hot_water_setpoint`: only the "Hot Water Setpoint" number entity, for its value and its availability (`number.py:47-60`).
- `minimum_hot_water_setpoint` and `maximum_hot_water_setpoint`: a search of all `.py` files found no reader outside the coordinator classes. The number entity uses a fixed 30 to 60 °C range instead (`number.py:62-70`). So the boiler's reported DHW bounds limit nothing.
- `boiler_capacity` and `minimum_relative_modulation_value`: only the boiler power estimate (`coordinator/__init__.py:228-257`), which only the "Current Power" sensor shows (`sensor.py:200-211`). Display only.
- `maximum_relative_modulation_value`: it becomes `DeviceState.max_modulation_level` (`coordinator/__init__.py:142`). That feeds the modulation reliability tracker (`device/modulation.py:26-29`), which only shapes the status label (see the cycles and device section). It is also compared with SAT's own MM value by the "Relative Modulation Synchronization" binary sensor (`binary_sensor.py:140-154`). No control decision reads it.

#### How incoming data is stored, per backend

- **Serial.** Every property reads `self.data[BOILER].get(key)`, so only the boiler part of the pyotgw status is used (`coordinator/serial/__init__.py:165-167`).
- **OTthing.** One topic carries a JSON object. The coordinator copies the whole object into the data cache, adds an empty `slave` and an empty `thermostat` object when the message lacks them, and also stores the object under the key `state` (`coordinator/mqtt/otthing.py:189-196`). A payload that is not an object is stored under its topic key (`coordinator/mqtt/otthing.py:198`, `coordinator/mqtt/__init__.py:158-159`). The properties read `slave`, `slave.status`, `thermostat` and `thermostat.status`, each falling back to an empty object (`coordinator/mqtt/otthing.py:200-210`). The cache replaces values per top-level key (`coordinator/__init__.py:34-39`). Derived from the code, not tested: a message without `slave` therefore replaces all stored slave values with an empty object until the next message that carries them.
- **ESPHome.** The sensor, binary sensor and number readers return None when the entity key is unknown, the state is absent, or the state is flagged `missing_state` (`coordinator/esphome/__init__.py:305-336`). The switch reader has no `missing_state` check (`coordinator/esphome/__init__.py:338-347`). `active`, `flame_active` and `hot_water_active` turn None into False (`coordinator/esphome/__init__.py:143-153`).
- **Manufacturer by name.** The configured name is looked up as an exact key of the `MANUFACTURERS` table. An unknown name gives None (`manufacturer.py:43-48`). The config flow offers exactly those keys as values (`config_flow.py:522-533`). When the result is None, the per-tick member ID backfill applies (`coordinator/__init__.py:348-351`). The manufacturer-specific behaviour (`MI=500`, the Immergas `TP` command, the Geminox MM floor) is chosen by class type (`coordinator/mqtt/opentherm.py:171`, `coordinator/mqtt/opentherm.py:217`, `heating_control.py:522-523`).

### Not found in the code / unclear

1. **What the boiler does when SAT stops writing.** SAT stops writing when HVAC is OFF or the outside sensor is lost. What the boiler then does depends on the gateway. The only source seen is gateway documentation (`OTGW-Commands.md:493`), not SAT code.
2. **`overshoot_protection` cannot be enabled from the UI.** It has no config-flow or options-flow field: `config_flow.py` contains no `overshoot` reference (search of the file). The translations still carry a label for it under "system_configuration" (`translations/en.json:335`, `translations/en.json:342`), but the system-configuration step does not add that field (`config_flow.py:678-711`). It is read from data (`entry_data.py:151`) and set only by migration (`__init__.py:159-162`). Unclear whether this is intended. It decides whether the PWM setpoint override is active at all (`heating_control.py:385`).
3. **`push_setpoint_to_thermostat` may not take effect from the options flow.** It is read from data (`entry_data.py:155`), but the options flow saves it into options (`config_flow.py:670`, `config_flow.py:804-806`). The options-flow toggle therefore appears to have no effect, unless migration put it in data (`__init__.py:185-187`).
4. **`cycles_per_hour` dropdown choice.** The options UI picks the dropdown from `options.get(CONF_HEATING_SYSTEM)` (`config_flow.py:686`, `:695`), but `heating_system` is stored in data (`entry_data.py:121-124`). The UI therefore falls back to the radiators default (`const.py:184`, `config_flow.py:808-812`).
5. **`force_pulse_width_modulation` has no effect.** It is read into `PwmConfig` (`entry_data.py:223`), but a search found no code that uses it.
6. **`duty_cycle` and `automatic_gains_value` have no UI field.** They have defaults (`const.py:167`, `const.py:121`), but no config-flow or options-flow field was found for them.
7. **Unregistered services.** `pulse_width_modulation` is declared in `services.yaml:7`, and `set_overshoot_protection_value` and `start_overshoot_protection_calculation` are named in `const.py:200-202`. Only `reset_integral` is registered (`services.py:41-46`, `climate.py:222`).
8. **Unused constants.** `MAXIMUM_SETPOINT`, `MAXIMUM_RELATIVE_MODULATION`, `MAX_BOILER_TEMPERATURE_AGE`, `BOILER_DEADBAND`, the `PWM_*_MARGIN` and `PWM_*_LOW_MODULATION` constants, and the `OVERSHOOT_PROTECTION_*` constants are defined but not used.
9. **OT bit behind two OTGW MQTT topics.** It is not visible in SAT which OpenTherm bit (master CH enable or slave CH mode) the OTGW MQTT topics `centralheating` and `domestichotwater` carry (`coordinator/mqtt/opentherm.py:22-23`).
10. **ESPHome `active` is SAT's own command.** ESPHome `active` reads SAT's own `ch_enable` switch (`coordinator/esphome/__init__.py:143-145`), not a boiler status bit. The `ch_active` binary sensor is subscribed but unused.
11. **OTthing key names look swapped.** The constants map `DATA_MAX_REL_MOD_LEVEL_SETTING` to `"min_modulation"` and `DATA_REL_MIN_MOD_LEVEL` to `"maxModulation"` (`coordinator/mqtt/otthing.py:28`, `:30`). The code does not show whether this is intended.
12. **Why `PM=25` follows every CS.** The OTGW-MQTT coordinator sends `PM=25` after every `CS` (`coordinator/mqtt/opentherm.py:197`). ID 25 is the boiler flow temperature (`OT-spec:2366`), but the code gives no reason for it.
13. **What `MI=500` is for.** The OTGW coordinator sends it for Ideal, Intergas and Nefit (`coordinator/mqtt/opentherm.py:171-172`), and the code gives no reason.
14. **Gateway acceptance of unrounded values.** On the OTGW-MQTT path SAT publishes unrounded float strings such as `CS=47.2345` (`coordinator/mqtt/opentherm.py:196`). Whether the OTGW firmware accepts them is not shown in SAT code. On the serial path the local pyotgw 2.2.3 formats floats with two decimals (`pyotgw/commandprocessor.py:38-41`); whether the pinned pyotgw 2.2.2 does the same was not checked.
15. **Hard overheat cutoff.** No hard overheat cutoff, no DHW priority logic beyond the checks listed in Behaviour F, and no DHW-enable write was found in thermo-nova.
16. **Frost protection.** A search of all `.py` files for "frost", "antifreeze" and "freeze" (other than the solar-gain `freeze_integral` and dataclass `frozen=True`) found nothing. No frost-protection logic was found in thermo-nova. With HVAC OFF the tick returns before any write (`climate.py:544-545`).
