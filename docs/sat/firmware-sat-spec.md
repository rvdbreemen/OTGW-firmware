# SAT specification (OTGW firmware)

This document describes the SAT (Smart Autotune Thermostat) port in the OTGW firmware as it is in the code today. It describes what the firmware does, not what it should do (it is not the target design).

- **Code version.** Branch `dev`, commit `51df6b486`, firmware version 2.0.0-alpha.455 (`version.h:5-10` at that commit).
- **Force PWM removed.** After this commit the Force PWM option (`bForcePWM`, `SATforcepwm`, REST `force_pwm`, MQTT `sat/force_pwm`) was removed (TASK-1332, Sergeant D 2026-10-23, `sergeant-d-corrections.md` entry 13). The passages below that mention it describe the code at the pinned commit; each carries a removal note.
- **Citation convention.** A citation `file:line` or `file:line-line` points to that file at commit `51df6b486`. A bare file name such as `SATcontrol.ino:4836` is relative to `src/OTGW-firmware/`. Exceptions are stated in the section where they occur: a path that starts with `src/libraries/` is relative to the repository root, and `boards.h` is `src/libraries/Platform/src/boards.h`. In a list after a `SATcontrol.ino:` citation, a bare number such as `4839` means `SATcontrol.ino:4839`. Each section opens with its own source note.
- **Ground truth.** The original SAT design exists only as code: Alexwijn/SAT, branch `releases/thermo-nova`, commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb` (2026-03-18). That thermo-nova code is the ground truth; this firmware document is not. Where the two differ, the thermo-nova code is leading.
- **Comparison.** The difference between this firmware and thermo-nova is worked out in `sat-comparison.md`.
- **Companion references.** A reference of the form `aN-<area>-thermonova.md` in the text below means section N of the companion thermo-nova specification, which has the same headings and order.
- **Reading guide.** Every section has the same structure: purpose, inputs, behaviour in time order, formulas, constants and defaults, state kept, outputs, edge cases, and a final list "Not found in the code / unclear". A statement without a citation is not part of this specification.

## Contents

1. [Start-up sequence and control loop timing (OTGW firmware)](#s1)
    - [Purpose](#s1-purpose)
    - [Inputs](#s1-inputs)
    - [Behaviour (step by step, in time order)](#s1-behaviour-step-by-step-in-time-order)
    - [Formulas](#s1-formulas)
    - [Constants and defaults](#s1-constants-and-defaults)
    - [State kept between passes](#s1-state-kept-between-passes)
    - [Outputs (to the boiler, to MQTT)](#s1-outputs-to-the-boiler-to-mqtt)
    - [Edge cases](#s1-edge-cases)
    - [Not found in the code / unclear](#s1-not-found-in-the-code-unclear)
2. [PID controller (OTGW firmware, as implemented)](#s2)
    - [Purpose](#s2-purpose)
    - [Inputs](#s2-inputs)
    - [Behaviour (step by step, in time order)](#s2-behaviour-step-by-step-in-time-order)
    - [Formulas](#s2-formulas)
    - [Constants and defaults](#s2-constants-and-defaults)
    - [State kept between passes](#s2-state-kept-between-passes)
    - [Outputs (to the boiler, to entities)](#s2-outputs-to-the-boiler-to-entities)
    - [Edge cases](#s2-edge-cases)
    - [Not found in the code / unclear](#s2-not-found-in-the-code-unclear)
3. [Heating curve and setpoint selection](#s3)
    - [Purpose](#s3-purpose)
    - [Inputs](#s3-inputs)
    - [Behaviour (step by step, in time order)](#s3-behaviour-step-by-step-in-time-order)
    - [Formulas](#s3-formulas)
    - [Constants and defaults](#s3-constants-and-defaults)
    - [State kept between passes](#s3-state-kept-between-passes)
    - [Outputs (to the boiler, to entities)](#s3-outputs-to-the-boiler-to-entities)
    - [Edge cases](#s3-edge-cases)
    - [Not found in the code / unclear](#s3-not-found-in-the-code-unclear)
4. [PWM, modulation and the suppression mechanism (OTGW firmware, as implemented)](#s4)
    - [Purpose](#s4-purpose)
    - [Inputs](#s4-inputs)
    - [Behaviour (step by step, in time order)](#s4-behaviour-step-by-step-in-time-order)
    - [Formulas](#s4-formulas)
    - [Constants and defaults](#s4-constants-and-defaults)
    - [State kept between passes](#s4-state-kept-between-passes)
    - [Outputs (to the boiler, to entities)](#s4-outputs-to-the-boiler-to-entities)
    - [Edge cases](#s4-edge-cases)
    - [Not found in the code / unclear](#s4-not-found-in-the-code-unclear)
5. [Boiler state, flame and cycle tracking (OTGW firmware, as built)](#s5)
    - [Purpose](#s5-purpose)
    - [Inputs](#s5-inputs)
    - [Behaviour (step by step, in time order)](#s5-behaviour-step-by-step-in-time-order)
    - [Formulas](#s5-formulas)
    - [Constants and defaults](#s5-constants-and-defaults)
    - [State kept between passes](#s5-state-kept-between-passes)
    - [Outputs (to the boiler, to entities)](#s5-outputs-to-the-boiler-to-entities)
    - [Edge cases](#s5-edge-cases)
    - [Not found in the code / unclear](#s5-not-found-in-the-code-unclear)
6. [Areas, presets, window detection and thermostat sync (OTGW firmware, as implemented)](#s6)
    - [Purpose](#s6-purpose)
    - [Inputs](#s6-inputs)
    - [Behaviour (step by step, in time order)](#s6-behaviour-step-by-step-in-time-order)
    - [Formulas](#s6-formulas)
    - [Constants and defaults (table: name | value | unit | citation)](#s6-constants-and-defaults-table-name-value-unit-cit)
    - [State kept between passes](#s6-state-kept-between-passes)
    - [Outputs (to the boiler, to entities)](#s6-outputs-to-the-boiler-to-entities)
    - [Edge cases](#s6-edge-cases)
    - [Not found in the code / unclear](#s6-not-found-in-the-code-unclear)
7. [Solar gain, summer simmer, temperature history and health sensors (OTGW firmware)](#s7)
    - [Purpose](#s7-purpose)
    - [Inputs](#s7-inputs)
    - [Behaviour (step by step, in time order)](#s7-behaviour-step-by-step-in-time-order)
    - [Formulas](#s7-formulas)
    - [Constants and defaults (table: name | value | unit | citation)](#s7-constants-and-defaults-table-name-value-unit-cit)
    - [State kept between passes](#s7-state-kept-between-passes)
    - [Outputs (to the boiler, to entities)](#s7-outputs-to-the-boiler-to-entities)
    - [Edge cases](#s7-edge-cases)
    - [Not found in the code / unclear](#s7-not-found-in-the-code-unclear)
8. [Boiler I/O, DHW, limits and safety, configuration defaults (OTGW firmware, as implemented)](#s8)
    - [Purpose](#s8-purpose)
    - [Inputs](#s8-inputs)
    - [Behaviour (step by step, in time order)](#s8-behaviour-step-by-step-in-time-order)
    - [Formulas](#s8-formulas)
    - [Constants and defaults](#s8-constants-and-defaults)
    - [State kept between passes](#s8-state-kept-between-passes)
    - [Outputs (to the boiler, to monitoring)](#s8-outputs-to-the-boiler-to-monitoring)
    - [Edge cases](#s8-edge-cases)
    - [Not found in the code / unclear](#s8-not-found-in-the-code-unclear)

---

<a id="s1"></a>

## Start-up sequence and control loop timing (OTGW firmware)

Source read: the OTGW firmware, branch `dev`, commit `51df6b486`. All citations are relative to `src/OTGW-firmware`. During this work the branch moved on to `2c194a329`. I compared every cited file against `51df6b486`: the SAT files (`SATcontrol.ino`, `SATpid.ino`, `SATcycles.ino`, `safeTimers.h`) are identical, and the other cited files differ only in version banners or in single lines that do not shift any line cited here. One exception: `OTGW-firmware.h` gained one line at line 125 after `51df6b486`; the `OTGW-firmware.h` line numbers below are those of `51df6b486`.

Reading guide. This document describes what the firmware *does*, not what it should do. It uses the same headings and topic order as `a1-startup-thermonova.md`, so the two can be read side by side. Where thermo-nova has a step that has no counterpart in the firmware, the firmware step that takes its place is described instead. "Tick" means one run of the control body behind the control timer; "pass" means one call of `satControlLoop()` from the main loop, which happens far more often than a tick.

---

<a id="s1-purpose"></a>

### Purpose

This section describes what the firmware SAT controller does from power-on until it controls the boiler in steady state:

- the order of the start-up steps;
- what is restored from before a restart;
- how the first PID and heating curve values are formed;
- when the first setpoint is sent to the boiler;
- which timers run, and what triggers a control tick.

The four most important facts:

1. **SAT is initialised once, at the end of `setup()`**, by `initSAT()` (`OTGW-firmware.ino:567`, `SATcontrol.ino:3681-3738`). It always sends `CS=0` first when an OpenTherm command interface exists, even when SAT is disabled (`SATcontrol.ino:3702-3710`). Only when SAT is enabled at boot does it also send `PM=3`, `PM=15`, `PM=48` and, for some manufacturers, `MI=500` (`SATcontrol.ino:3721-3734`).
2. **Control runs from one timer**, the control tick, every `iControlInterval` seconds (default 30 s, allowed 10 to 300 s; `SATtypes.h:444`, `settingStuff.ino:1001`). The PID is updated inside that tick, not on its own timer (`SATcontrol.ino:4614`, `SATcontrol.ino:4839`). Between ticks, every pass runs some fast work: flame edge detection, cycle sampling, a 30 s CS keepalive on PIC boards and a 3 s PWM suppression refresh (`SATcontrol.ino:4591-4611`).
3. **The PID takes a start-up step before the first tick (TASK-1368, entries 42 and 83).** After the reset in `initSAT()` (or any `satPidReset()`), the next `satPidService()` call is a start-up step and restarts the 60 s timer, so the first tick reads a computed output instead of the stored 0. That output has D = 0 (entry 95 Q39b: `satPidOutput()` leaves D out of the first output after a reset). Without a room temperature yet the start-up step is `satPidCurveOnly()` and the output is the heating curve. The earlier behaviour (stored 0, raised to 10 °C, `CS=10`, `MM=100`, `CH=0` until the third tick) is gone. Open point for Sergeant D: when the start-up step finds no room temperature, `satPidCurveOnly()` zeroes the restored integral and raw derivative (entry 46), where entry 83 asks to send the restored I.
4. **After a restart, little control state is restored.** The PID integral and filtered derivative come back only when the clock is NTP-synced and the saved file is at most 30 minutes old (`SATcontrol.ino:1993`, `SATcontrol.ino:2026-2042`). The control mode, PWM state, cycle history and last-sent command values always start fresh (`SATcontrol.ino:3688-3700`).

A grep for `warm|settle|preheat|startup|start-up|boot` over `SATcontrol.ino`, `SATpid.ino` and `SATcycles.ino` finds no warm-up or settle period in the control path. It finds:

- the boot `CS=0` release and its deferred retry (`SATcontrol.ino:342`, `SATcontrol.ino:3702-3712`, `SATcontrol.ino:4583-4589`);
- the `PM=` boot commands and the `MI=500` quirk (`SATcontrol.ino:3724-3733`, `SATcontrol.ino:128-132`);
- the PWM ignition timeout and minimum on-time named after thermo-nova's `HEATER_STARTUP_TIMEFRAME` (`SATcontrol.ino:59`, `SATcontrol.ino:695-696`);
- boiler status labels (`SATcontrol.ino:508`, `SATcontrol.ino:513`, `SATcontrol.ino:623-624`), a simulator warm-up (`SATcontrol.ino:92`, `SATcontrol.ino:3792`) and a cycle-shape warm-up used only for cycle metrics (`SATcycles.ino:84`);
- the PID restore comments (`SATcontrol.ino:2029`, `SATpid.ino:71`).

<a id="s1-inputs"></a>

### Inputs

| Input | Where it comes from | Citation |
|---|---|---|
| SAT settings (`settings.sat.*`): enabled, target, interval, coefficient, limits, options | settings file on LittleFS, read by `readSettings(true)` before `initSAT()` | `OTGW-firmware.ino:313-316`, `settingStuff.ino:315-345`, `SATtypes.h:436-552` |
| Stored PID state (`i`, `d`, `rd`, `err`, `ts`) | `/sat_pid_state.json` | `SATcontrol.ino:1988`, `SATcontrol.ino:2001-2002` |
| Stored energy totals | `/sat_energy.json`, `/sat_energy_estimate.json` | `SATcontrol.ino:2048`, `SATcontrol.ino:2086` |
| Stored daily-median curve recommendation ring | `/sat/sat_hcr.json` (migrated from `/sat_hcr.json`) | `SATcycles.ino:167-168`, `SATcycles.ino:1135-1138` |
| Wall clock (NTP) | used to judge the age of the PID file | `SATcontrol.ino:2027-2037`, `networkStuff.ino:780-783` |
| Room temperature, in priority order: simulation, BLE, multi-area average, MQTT external, OT MsgID 24 (`Tr`) | state fields and the OT bus | `SATcontrol.ino:1192-1280` |
| Outside temperature: simulation, MQTT external, weather API, OT MsgID 27 (`Toutside`) | state fields and the OT bus | `SATcontrol.ino:1283-1303` |
| Boiler data: flow `Tboiler`, return `Tret`, flame bit, modulation, `MaxTSet`, slave status bits | `OTcurrentSystemState`, filled from decoded OT frames | `SATcontrol.ino:1325-1347`, `SATcontrol.ino:242`, `SATcontrol.ino:4628`, `SATcontrol.ino:4716` |
| Command interface present (PIC or OT-Direct) | `hasOTCommandInterface()` | `OTGW-firmware.h:558-559` |
| MQTT connection state (for the fallback) | `state.mqtt.*` | `SATcontrol.ino:4544-4547` |

Initial values that matter at start-up: `Tr` starts as NAN (`OTGW-Core.h:79`), `Tboiler` and `Toutside` start as 0.0 (`OTGW-Core.h:80`, `OTGW-Core.h:82`), `MaxTSet` starts as 0.0 (`OTGW-Core.h:103`), and the status flags start as 0, so the flame bit reads off (`OTGW-Core.h:52`, `OTGW-Core.h:54`).

<a id="s1-behaviour-step-by-step-in-time-order"></a>

### Behaviour (step by step, in time order)

#### Phase A: device boot up to SAT initialisation (`setup()`)

These steps run in this order inside `setup()`:

1. Mount LittleFS (`OTGW-firmware.ino:313`).
2. **Read all settings**, including the SAT section (`OTGW-firmware.ino:316`). While the file is parsed, a `SATenabled` change does not call `satDisable()` or clear a safety trip, because both are gated on `state.bSetupComplete`, which is still false (`settingStuff.ino:981-993`, `OTGW-firmware.h:519`).
3. Start WiFi, with a 240 s timeout on the configuration portal (`OTGW-firmware.ino:356`). The time this step takes is therefore not fixed.
4. Start NTP (`OTGW-firmware.ino:486`). Whether the clock is synced by the time `initSAT()` runs depends on the network; the code does not wait for it.
5. Start telnet, mDNS, web server, WebSocket and MQTT (`OTGW-firmware.ino:490-503`).
6. Reset the PIC, start the PIC stream and the PIC serial task (`OTGW-firmware.ino:539-547`). The comment at `OTGW-firmware.ino:541-543` states that PIC detection has run by this point.
7. Send the general PIC boot commands (`sendPICBootCommands()`, `OTGW-firmware.ino:557`). These are not SAT commands; I did not read that function (see "Not found in the code / unclear").
8. **`initSAT()`** (`OTGW-firmware.ino:567`), Phase C.
9. Mark setup complete (`OTGW-firmware.ino:572`).

After `setup()`, the main loop calls `satControlLoop()` once per iteration, but only while no firmware or PIC flash is running (`OTGW-firmware.ino:1132`, `OTGW-firmware.ino:1157`). OT frames are decoded later in the same iteration, after `satControlLoop()` (`OTGW-firmware.ino:1167`, `OTGW-firmware.ino:1176`). A pass therefore sees boiler values decoded in earlier iterations.

#### Phase B: objects and initial values before `initSAT()`

The firmware has no separate entity layer. The SAT state exists as global structs and file-scope variables with fixed initial values:

- `state.sat` (transient runtime state, `SATtypes.h:145-149`): `bActive` false, `eControlMode` OFF (`SATtypes.h:150-151`), `fPidOutput` 0.0 (`SATtypes.h:155`), `iCurrentModulation` 100 (`SATtypes.h:210`), `bSafetyTripped` false (`SATtypes.h:233`), `bValvesOpen` true (`SATtypes.h:243`), last-sent command cache invalid (`SATtypes.h:395-396`), detected manufacturer `OTHER` (`SATtypes.h:240`).
- PID statics: all zero, `_pid_initialized` false, `_pid_roomTempSeen` NAN (`SATpid.ino:32-47`).
- PWM statics: effective boiler temperature 0, not waiting for flame (`SATcontrol.ino:686`, `SATcontrol.ino:688`, `SATcontrol.ino:690-694`).
- Previous flame state false (`SATcontrol.ino:473`).
- Three timers are declared at file scope: the control tick from `settings.sat.iControlInterval` with type `CATCH_UP_MISSED_TICKS` (`SATcontrol.ino:582`; it runs as `SKIP_MISSED_TICKS` in practice, item 16), the 60 s window-statistics timer (`SATcontrol.ino:478`) and the 3 s PWM suppression refresh timer (`SATcontrol.ino:480`). A file-scope declaration sets the first due time to the moment of declaration plus the interval (`safeTimers.h:109-114`).

#### Phase C: `initSAT()` step order

Order as written in `SATcontrol.ino:3681-3737`:

1. **Reset the PID** (`SATcontrol.ino:3683`, `SATpid.ino:94-114`): errors, integral, raw derivative, last room temperature and all P, I, D, output and gain fields to 0; the update and derivative timestamps to now; `_pid_initialized` false.
2. **Restore the PID state** from the file (`SATcontrol.ino:3684`, details in Phase D).
3. **Restore energy totals and the curve recommendation ring** (`SATcontrol.ino:3685-3687`).
4. **Initialise the cycle tracker** (`SATcontrol.ino:3688`, `SATcycles.ino:188-219`): flame recorded as off, flame-on start 0, **flame-off start = now**, cycle history and the per-hour cycle counter emptied.
5. **Reset PWM** (`SATcontrol.ino:3689`, `SATcontrol.ino:923-931`): no flame requested, not waiting for flame, no hold setpoint.
6. **Set SAT inactive, mode OFF, safety trip cleared**, skip and PIC-fail counters 0, boot `CS=0` marker false, previous OT-online false (`SATcontrol.ino:3690-3696`).
7. **Invalidate the command cache**, so the first CS, MM, CH and TC are always sent (`SATcontrol.ino:3700`, `SATcontrol.ino:356-362`).
8. **Boot safety release.** If a command interface exists, send `CS=0` and mark it sent (`SATcontrol.ino:3706-3710`). This does not depend on SAT being enabled. If no interface exists yet, the marker stays false and the control loop sends `CS=0` later (Phase E3, step 7).
9. Initialise BLE scanning (`SATcontrol.ino:3716`).
10. **Restart the control timer**: interval from the loaded setting, next due time = now + interval (`SATcontrol.ino:3719`, `safeTimers.h:131-135`). This macro call passes no timer type, so the type becomes 0, which is `SKIP_MISSED_TICKS` (`safeTimers.h:96`, `safeTimers.h:135`, `safeTimers.h:247-258`). From here the control timer no longer catches up missed ticks: each tick sets the next due time to "now + interval" (`safeTimers.h:218-219`).
11. **If SAT is enabled:** set the mode to CONTINUOUS (`SATcontrol.ino:3721-3722`) and, if a command interface exists, queue `PM=3`, `PM=15`, `PM=48` (`SATcontrol.ino:3725-3728`). Add `MI=500` when the effective manufacturer has the `MI_500_BOOT` quirk (Ideal, Intergas, Nefit; `SATcontrol.ino:3729-3733`, `SATcontrol.ino:128-132`). With the manufacturer setting on AUTO, the effective manufacturer is the detected one (`SATcontrol.ino:144-149`), which starts as `OTHER` with no quirks (`SATtypes.h:240`, `SATcontrol.ino:139`) until an OT MsgID 3 frame is decoded (`OTGW-Core.ino:3076`). These commands are sent only here; they are not retried if no interface existed at this moment.
12. Otherwise log "initialized but disabled" (`SATcontrol.ino:3735-3737`).

A grep for `"PM=` and `"MI=` over the firmware sources finds only `SATcontrol.ino:3726-3731`. The firmware sends no `PM=25`.

#### Phase D: what is restored, and how

**PID state** (`SATcontrol.ino:2008-2045`):

1. Read `/sat_pid_state.json`; return if the file is missing (`SATcontrol.ino:2010-2011`).
2. Parse `i` (integral), `d` (D term), `rd` (raw derivative), `err` (error) and `ts` (save time) (`SATcontrol.ino:2017-2024`).
3. **If the clock is not yet NTP-synced** (`time < 1000000`), skip the restore (`SATcontrol.ino:2027-2032`).
4. **If the save time is 0 or more than 1800 s old**, discard it (`SATcontrol.ino:1993`, `SATcontrol.ino:2033-2037`).
5. Otherwise put `d` and `err` in the displayed state fields `fPidD` and `fError` (`SATcontrol.ino:2038-2039`), and put `i` and `rd` in the PID's working integral and raw derivative (`SATcontrol.ino:2042`, `SATpid.ino:75-80`).

What is *not* restored: the last error used for P, the last room temperature and the derivative timestamp. `_pid_initialized` stays false, so the first PID call re-initialises these from live data (`SATpid.ino:72-73`, `SATpid.ino:264-273`). The stored output `fPidOutput` is also not restored; it stays 0 from the reset (`SATpid.ino:109`).

**Deferred retry.** If the restore was skipped because NTP was not synced, there is one retry: on the first pass where `isNTPtimeSet()` is true, `satLoadPidState()` runs again (`SATcontrol.ino:4512-4519`). This retry runs once per boot whatever the outcome at `initSAT()`, and it runs before the enabled check, so also while SAT is disabled.

**Energy** (`SATcontrol.ino:2067-2083`, `SATcontrol.ino:2100-2116`): the kWh totals are restored when the parsed value is ≥ 0. No age check.

**Curve recommendation ring** (`SATcycles.ino:1135-1175`): daily medians and intra-day samples are restored.

**Settings that act as remembered state**: the target temperature, the enabled flag, the interval, force-PWM, overshoot protection and the learned thermal coefficient are persisted settings (`settingStuff.ino:315`, `settingStuff.ino:319`, `settingStuff.ino:322`, `settingStuff.ino:344-345`, `settingStuff.ino:364`), so they come back with `readSettings()` in Phase A.

**Not restored** (always fresh after boot): control mode (OFF, then CONTINUOUS if enabled, `SATcontrol.ino:3691`, `SATcontrol.ino:3722`); requested control mode (`SATtypes.h:208`); active preset (`SATtypes.h:205`); PWM state (`SATcontrol.ino:3689`); cycle history (`SATcycles.ino:200-202`); last-sent command values (`SATcontrol.ino:3700`). A grep over `settingStuff.ino` finds no write of `eControlMode`, `iRequestedControlMode` or `eActivePreset`.

#### Phase E: the first control passes and ticks

**What runs on every pass** (every main-loop iteration while not flashing), in this order (`SATcontrol.ino:4510-4614`):

1. The one-time deferred PID restore, once NTP is synced (`SATcontrol.ino:4515-4519`).
2. Boiler-detected handling for simulation mode (`SATcontrol.ino:4525-4526`).
3. On an OT-bus offline-to-online edge, invalidate the command cache (`SATcontrol.ino:4531-4535`).
4. MQTT fallback check and fallback lift (`satMqttFallbackStep()`, `SATcontrol.ino:4677-4707`, see Phase F).
5. **If SAT is disabled or a flash is running:** call `satDisable()` if SAT was active, and stop (`SATcontrol.ino:4568-4573`).
6. Simulation model and thermal-drop learning (`SATcontrol.ino:4576-4578`).
7. **If the safety trip is set: stop** (`SATcontrol.ino:4581`).
8. **Deferred boot release:** if `CS=0` was not sent in `initSAT()` and a command interface now exists, send it (`SATcontrol.ino:4585-4589`).
9. **Flame edge:** when the flame bit differs from the previous pass, notify the cycle tracker (`SATcontrol.ino:4592-4596`).
10. Sample cycle data (`SATcontrol.ino:4599`); every 60 s update the 4 h and 24 h window statistics (`SATcontrol.ino:4602-4605`).
11. **PIC CS keepalive** (`SATcontrol.ino:4608`, `SATcontrol.ino:405-414`).
12. **PWM suppression refresh**, at most every 3 s (`SATcontrol.ino:4611`, `SATcontrol.ino:894-918`).
13. **If the control timer is not due: stop** (`SATcontrol.ino:4614`).

After every pass, whatever path it took, the core state topics are published (`SATcontrol.ino:4504-4508`, `SATcontrol.ino:2610-2631`).

**E1. Heating curve.** The curve is recomputed on **every tick**, from the effective target and the outside temperature of that tick (`SATcontrol.ino:4831-4836`, `SATcontrol.ino:485-500`). There is no separate first computation and no event that recomputes it between ticks.

**E2. First PID call** (`SATpid.ino:251-320`, called at `SATcontrol.ino:4839`):

1. The room temperature is stamped as "seen now" when it differs from the last seen value; on the very first call it always differs, because the last seen value starts as NAN (`SATpid.ino:256-261`, `SATpid.ino:46`).
2. Because `_pid_initialized` is false, the last error, last room temperature, last curve value and the last update time are set from this call, and the derivative reference is set to this temperature and the time it was first seen (`SATpid.ino:264-273`).
3. The elapsed time since the last update is therefore 0, which is below the 10 s sample limit, so the function **returns the stored output `fPidOutput` unchanged** (`SATpid.ino:276-280`, `SATpid.ino:21`). After the reset in `initSAT()` that stored output is 0 (`SATpid.ino:109`); the restore does not change it (`SATpid.ino:75-83`).
4. On later ticks the PID is skipped, again returning the stored output, when less than 10 s passed, or when the rounded error changed by less than 0.001 °C and less than 60 s passed (`SATpid.ino:276-285`).
5. The first computed update sets gains, integral, derivative, P, I, D and the output (`SATpid.ino:288-319`).

Consequence for the default 30 s interval and a steady room temperature: tick 1 returns 0; tick 2 (about 30 s later) is skipped because the error is unchanged and less than 60 s passed; tick 3 (at least 60 s after tick 1) computes the first real output. If the error changes by 0.001 °C or more, tick 2 already computes. The room value passes a low-pass filter first (step 5 of E3), and that filter is seeded with the first valid reading, so an unchanged raw reading gives an unchanged filtered value (`SATcontrol.ino:4700-4710`).

At the first computed update, the restored values are used as follows:

- **Integral:** zeroed when |error| > deadband (default 0.1 °C); otherwise accumulated on top of the restored value (`SATpid.ino:151-166`, `SATtypes.h:443`).
- **Derivative:** if |error| ≤ deadband, or the temperature has not changed, or the change came within 60 s, the restored raw derivative is kept and used in D (`SATpid.ino:199-219`, `SATpid.ino:299`).
- **P:** from the live error (`SATpid.ino:297`).

**E3. First control tick.** The first tick comes one interval after `initSAT()` restarted the timer (default 30 s, `SATcontrol.ino:3719`, `SATtypes.h:444`), provided the pass reaches the timer check at that time. In order (`SATcontrol.ino:4616-5018`):

1. Set SAT active; if the mode is OFF, set it to CONTINUOUS (`SATcontrol.ino:4616-4619`).
2. **DHW check.** DHW is active when slave status bit 2 is set (or a simulated DHW draw runs) (`SATcontrol.ino:4625-4629`). Power and energy are updated first (`SATcontrol.ino:4632-4633`). While DHW is active the tick sends `SW=` once on DHW entry (only with DHW control enabled and a valid DHW setpoint), publishes, and stops. CS, MM and CH are not touched and the PID does not run (`SATcontrol.ino:4865-4882 [lines refreshed 2026-10-24, TASK-1351]`).
3. Read room, outside and target temperature (`SATcontrol.ino:4656-4658`).
4. **Room temperature check (TASK-1364, entries 38 and 46).** `satRoomTempValid()` is false when the room temperature is NAN or outside -10 to 50 °C. There is no trip and no skip: the tick goes on with the room temperature treated as lost. The room low-pass filter, the MQTT-takeover thermal block, auto-tune, the curve recommendation and the daily median sample are skipped, and PV boost does not start. The PID step is replaced by `satPidCurveOnly()` (P, I, D and the raw derivative are 0, `state.sat.fError` reads 0, the requested setpoint is the heating curve value). The solar offset, clamps, cold cutoff, PWM, MM, CS and CH then run as on any tick. The PIC-fail trip (5 ticks without a command interface) is unchanged. When a valid reading returns, the output stays the curve alone (P, I and D 0) until the next 60 s PID step stores a real error, up to 60 s later. The integral restarts from 0. The first D after recovery is computed over the whole gap, from the last pre-loss temperature to the new one, as thermo-nova does (`pid.py:134-137` keeps `_last_temperature`, `pid.py:209-232`), so D need not start at 0. The status field `room_temp_lost` (REST `sat/status`, MQTT `sat/room_temp_lost`) is true while the loss lasts.
5. **Room low-pass filter** with time constant 45 s. The first valid reading seeds the filter; later ticks move toward the raw value (`SATcontrol.ino:4694-4713`).
6. **Boiler fault flag** (slave status bit 0): stop this tick (`SATcontrol.ino:4716-4719`).
7. Outside temperature outside -40 to 50 °C is replaced by 10 °C (`SATcontrol.ino:4721-4723`).
8. Thermal estimation fallback, only while the MQTT fallback is active (`SATcontrol.ino:4727-4752`).
9. Window timer, pressure, solar gain (`SATcontrol.ino:4755-4762`).
10. **Summer simmer active:** send `CS=10`, `MM=<max>`, `CH=0` and stop (`SATcontrol.ino:4765-4776`). **All valves closed:** the same (`SATcontrol.ino:4779-4790`).
11. Auto-tune, curve recommendation, modulation reliability, setpoint sync, flame status, boiler status, comfort and PV boost (`SATcontrol.ino:4793-4829`).
12. Heating curve and PID (`SATcontrol.ino:5529-5538`); zone block for every configured zone, which raises the main PID output to the zones output and applies the overshoot cap (`SATcontrol.ino:5540-5561`, `satAggregateZoneSetpoint()` at `SATcontrol.ino:2329-2377`, TASK-1440).
13. Subtract `fSolarSetpointOffset` from the PID output when solar gain is active and enabled (TASK-1323; thermo-nova `climate.py:391-392`), then clamp it to 10 °C .. effective maximum (`SATcontrol.ino:4908-4912`). The result is the requested setpoint (thermo-nova `climate.py:394`); it is kept in `_sat_requestedSetpoint` for the 3 s refresh and the `sat/requested_setpoint` topic, while `state.sat.fPidOutput` stays the raw PID output.
14. Switch PWM to continuous when the output reaches the system maximum; force PWM when configured (`SATcontrol.ino:4917-4925`). (Removed in TASK-1332, 2026-10-23: the Force PWM option no longer exists. Removed in TASK-1341, 2026-10-24: the PWM auto switch setting no longer exists, entry 19.)
15. Compute the control setpoint for the active mode (`SATcontrol.ino:4928-4933`), clamp it again (`SATcontrol.ino:4936-4937`) and remember it as the last control setpoint (`SATcontrol.ino:4939`). There is no solar subtraction on the control setpoint any more (moved to step 13, TASK-1323).
16. Auto-switch check between continuous and PWM, only when the PID output is above the cold setpoint, for every heating source and heat pumps too (Sergeant D entry 87; `SATcontrol.ino:5542-5546`).
17. Compute the modulation value (`SATcontrol.ino:4960-4967`).
18. **Cold cutoff:** if the requested setpoint is at or below the cold setpoint (`satColdCutoff()`, `<=` like the PWM gate in `satApplyPWM()` and thermo-nova `heating_control.py:72` / `:385`; TASK-1323) (28.2 °C radiators, 21 °C underfloor), the setpoint becomes 10 °C and the modulation the configured maximum (corrected 2026-10-24, TASK-1351) (`SATcontrol.ino:5206-5214`, `SATcontrol.ino:130-131`, `SATcontrol.ino:339-343`). The cutoff also disables PWM, as thermo-nova's heater-OFF branch does (`pwm.disable()`, `heating_control.py:230`, `pwm.py:106-109`; control setpoint reset to `MINIMUM_SETPOINT`, `heating_control.py:233`): `satPwmDisableOnHeaterOff()` (also called in the valves-closed early return, thermo-nova `heating_control.py:69-70`) always leaves PWM mode (TASK-1332, Sergeant D entry 13) (thermo-nova's `control_mode` is CONTINUOUS while PWM is not enabled, `heating_control.py:148-152`; PWM returns through the overshoot paths, `heating_control.py:304`, mirrored by `satCycleCheckOvershootSwitch()`; the `on_cycle_end()` re-enable of `pwm.py:219-223` is not ported; summer simmer does not call the helper), resets the PWM phase, ignition wait and hold (`satPwmReset()`), clears the sustained overshoot/underheat/saturation timers (`_reset_pwm_disable_guards`, `heating_control.py:231`, `:525-528`; `satCycleResetSustainGuards()`), sets the modulation to the maximum relative modulation (`heating_control.py:234`) and sets the held "previous CS" to 10, so a later PWM tick without an override cannot return the pre-cutoff value (TASK-1327; Sergeant D 2026-10-23, `docs/sat/sergeant-d-corrections.md` entry 12). thermo-nova never reads its `force_pulse_width_modulation` option (`entry_data.py:223` is its only use), so the firmware option had no counterpart there and was removed.
19. **Send** CS, MM and CH through the write-on-change helpers; CH follows `satChEnableDecide()` (`SATchEnable.h`, TASK-1323; Sergeant D 2026-10-23: "If the requested setpoint is lower than the thresholds then we send CH=0 CS=10"; thermo-nova uses COLD_SETPOINT 22): `CH=0` when the requested setpoint is at or below the cold setpoint (the cold cutoff, CS=10) or when PWM drives the CS and the PWM phase is OFF (CS=10); otherwise `CH=1`. That includes the PWM ON phase whatever low CS is sent, continuous mode, and a PWM tick that keeps the previous CS (overshoot protection off, DHW active, no flow temperature) while the requested setpoint is above the cold setpoint, for example a held CS of 24 on radiators with a requested 30 gives `CH=1`. The CH bit does not depend on the sent CS value. Immergas also gets `TP=11:12=<setpoint>` on every tick (`SATcontrol.ino:4986-4990`). With "push setpoint" on, `TC=<target>` is sent (`SATcontrol.ino:4992-4994`). Without a command interface, count a PIC failure; after 5, set the safety trip and disable (`SATcontrol.ino:4996-5004`, `SATcontrol.ino:69`).
20. Record the tick time; save the PID state when 300 s passed since the last save (`SATcontrol.ino:5007-5012`). The last-save time starts at 0 (`SATcontrol.ino:1989`), so the first save happens on the first full tick after 300 s of uptime.
21. Publish the SAT MQTT topics (`SATcontrol.ino:5018`).

**What decides the first setpoint value.** The PID service has taken its start-up step before the first tick (TASK-1368), so the PID output read in step 12 is already computed (curve + P + I, D = 0 on this first output), or the curve alone when there is no room temperature yet. Step 13 raises an output below 10 °C to 10 °C. Step 18 applies when that value is below both cold setpoints: the setpoint becomes 10 °C and the modulation 100, whatever the control mode returned in step 15. Step 19 then sends `CS=10.0`, `MM=100` and `CH=0`.

**E4. First outputs to the boiler.** In time order after boot:

| When | Command | Citation |
|---|---|---|
| `initSAT()`, if a command interface exists (SAT enabled or not) | `CS=0` | `SATcontrol.ino:3706-3710` |
| `initSAT()`, SAT enabled, interface exists | `PM=3`, `PM=15`, `PM=48`, plus `MI=500` for the quirk manufacturers | `SATcontrol.ino:3721-3734` |
| First enabled pass after an interface appears, if the boot `CS=0` was not sent | `CS=0` | `SATcontrol.ino:4585-4589` |
| First tick that reaches the send step, PID not yet computed | `CS=10.0`, `MM=100`, `CH=0` (and `TC=`, `TP=` when configured) | `SATcontrol.ino:4971-4983`, `SATcontrol.ino:4986-4994` |
| First tick with a computed PID output | CS, MM, CH from the control mode | `SATcontrol.ino:4928-4983` |

All SAT commands go through the shared command queue (`addCommandToQueue`). SAT's own `CS=` is marked as SAT's own while it is queued (`SATcontrol.ino:370-379`). MM, CH and TC are queued unmarked (`SATcontrol.ino:422`, `SATcontrol.ino:434`, `SATcontrol.ino:449`).

**Write-on-change.** CS, MM, CH and TC are queued only when the value changed (CS and TC compared at 0.1 °C resolution), when the cache is invalid, or when 300 s passed since that command was last sent (`SATcontrol.ino:351`, `SATcontrol.ino:383-454`). The cache is invalid at boot, after `satDisable()` and on an OT offline-to-online edge (`SATcontrol.ino:3700`, `SATcontrol.ino:2182`, `SATcontrol.ino:4532-4534`). The boot `CS=0` and the disable `CS=0` go directly to the queue and do not fill the cache (`SATcontrol.ino:3707`, `SATcontrol.ino:2179`).

**PIC CS keepalive.** On PIC boards only, once a CS of 8 °C or more is in the cache, the last CS is re-sent when 30 s passed since the last CS send (`SATcontrol.ino:403-414`). The comment states the reason: the PIC drops a CS of 8 °C or more after about 63 to 126 s without a new one (`SATcontrol.ino:398-402`). The first `CS=10.0` therefore starts a 30 s keepalive on PIC boards.

#### Phase F: steady state, timers and triggers

**Timers and periodic work:**

| Timer or trigger | Period or delay | Does | Citation |
|---|---|---|---|
| Main loop pass | every loop iteration, not a fixed period | `satControlLoop()` | `OTGW-firmware.ino:1157` |
| Control tick | `iControlInterval`, default 30 s, range 10 to 300 s; first tick one interval after `initSAT()` | the tick body (E3) | `SATcontrol.ino:4614`, `SATcontrol.ino:3719`, `SATtypes.h:444`, `settingStuff.ino:1001` |
| PID update | on its own 60 s timer `timerSATPid`, independent of the control tick, plus one start-up step; no time gate and no unchanged-error gate (TASK-1368, entries 42, 75, 111). The tick reads the output | `satPidService`, `satPidStep`, `satPidOutput` | `SATpid.ino` |
| Window statistics | 60 s | 4 h and 24 h stats | `SATcontrol.ino:478`, `SATcontrol.ino:4602-4605` |
| PWM suppression refresh | 3 s | CS during the PWM ON phase after the hold | `SATcontrol.ino:480`, `SATcontrol.ino:894-918` |
| PIC CS keepalive | checked every pass, sends after 30 s | re-send last CS | `SATcontrol.ino:403-414` |
| Command refresh | 300 s per command | re-send unchanged CS/MM/CH/TC on the next tick | `SATcontrol.ino:351`, `SATcontrol.ino:387` |
| PID save | first tick after 300 s since the last save | write `/sat_pid_state.json` | `SATcontrol.ino:5010-5012` |
| Energy save | 3600 s; estimated energy at each 0.1 kWh step | write the energy files | `SATcontrol.ino:2053`, `SATcontrol.ino:3254-3265` |

**Timer behaviour.** After `initSAT()` the control timer is of type `SKIP_MISSED_TICKS`: when it fires, the next due time becomes now + interval (`safeTimers.h:218-219`). When the due time is more than 10 intervals in the past, the timer does not fire; it only sets the next due time to now + interval (`safeTimers.h:186-189`). The timer is checked only by a pass that gets past the enabled and safety checks (`SATcontrol.ino:4568-4581`, `SATcontrol.ino:4614`).

**What triggers a control tick:** only the control timer (`SATcontrol.ino:4614`). The tick body sits after that check in `satControlLoopRun()`, and `satControlLoop()` is its only caller (`SATcontrol.ino:4503-4508`).

**What does not trigger a tick:**

- A target temperature change. It only stores the clamped value (5 to 30 °C) (`settingStuff.ino:997`); the next tick reads it (`SATcontrol.ino:4658`).
- Enabling SAT. It only stores the flag and clears a safety trip (`settingStuff.ino:981-993`, `SATcontrol.ino:1950-1958`).
- New OT data. It is decoded in the main loop and read by the next pass or tick (`OTGW-firmware.ino:1176`).
- An interval change restarts the timer, so the next tick comes one new interval later (`settingStuff.ino:1000-1003`, `safeTimers.h:131-135`).

**Target temperature change** (`settingStuff.ino:997`, `SATcontrol.ino:1903-1934`): a plain target change does not reset the PID. When the new target equals a preset value, the preset is activated instead (`SATcontrol.ino:1913-1927`). Activating or clearing a preset sets the target and sets the *displayed* integral `state.sat.fPidI` to 0 (`SATcontrol.ino:990-1007`). The PID's working integral `_pid_integral` is not touched there; the next computed update overwrites `fPidI` from `_pid_integral` (`SATpid.ino:298`, `SATpid.ino:303`).

**Enable and disable** (the firmware's counterpart of the HVAC mode):

- **Disable** (only when setup is complete): `satDisable()` (`settingStuff.ino:985-987`). In order: mode OFF, PWM reset, last control setpoint 0, SAT inactive, final setpoint 0, PID reset, **save the zeroed PID state**, save energy, reset the curve recommendation, send `CS=0`, invalidate the command cache (`SATcontrol.ino:2160-2183`). It publishes nothing itself; the next pass publishes the core state (`SATcontrol.ino:2184-2186`).
- **Enable at runtime:** the flag is set and a trip is cleared (`settingStuff.ino:990-992`, `SATcontrol.ino:1941-1948`). `initSAT()` is not called again, so no `PM=` commands are sent. The next tick sets the mode from OFF to CONTINUOUS (`SATcontrol.ino:4617-4619`). The PID was reset by the last disable, so the first-call behaviour of E2 applies again. The control timer was not checked while SAT was disabled, so its due time is in the past: the first enabled pass fires the tick at once, or, if the due time is more than 10 intervals old, the tick comes one interval later (`safeTimers.h:181-189`).
- **Control mode command** (`SATcontrol.ino:2223-2248`): `off` disables SAT; `continuous`, `pwm` and `auto` record the request and, only while SAT is enabled and not tripped, set the mode (`auto` starts continuous). A mode change resets PWM (`SATcontrol.ino:934-938`).

**MQTT fallback** (`satMqttFallbackStep()`, `SATcontrol.ino:4677-4707`, called at `SATcontrol.ino:4749`): opt-in since TASK-1345 (`settings.sat.bMqttFallback`, key `SATmqttfallback`, default off). When it is on, SAT is disabled by the user, MQTT is enabled, MQTT was connected before and has been disconnected for more than 300 s, `satMqttFallbackStep()` sets `state.sat.bFallbackActive` (state only: `settings.sat.bEnabled` is never written, so a settings save cannot persist it). Every control gate asks `satIsEnabled()` (`bEnabled || bFallbackActive`). The fallback is lifted when MQTT reconnects, when MQTT is switched off, when the opt-in is switched off, or when the user enables or disables SAT; `satDisable()` is called unless the user enabled SAT. A user switch-off during the outage sets `state.sat.bFallbackSuppressed`, which blocks a new takeover until MQTT is connected again.

#### Phase G: boiler data updates (not a control tick)

1. OT frames are decoded in the main loop after `satControlLoop()` (`OTGW-firmware.ino:1157`, `OTGW-firmware.ino:1176`).
2. On each enabled pass, the flame bit is compared with the previous pass (`SATcontrol.ino:4592-4596`). The previous value starts as false (`SATcontrol.ino:473`), and the cycle tracker starts with the flame recorded as off (`SATcycles.ino:190`). A flame that is already burning when SAT starts is therefore counted as a new flame-on at the first enabled pass that sees the flame bit. That sets the flame-on start time and records a cycle in the per-hour counter (`SATcycles.ino:701-724`).
3. The PWM suppression hold is timed from that flame-on start time (`SATcontrol.ino:720-723`). The PWM OFF phase is timed from the flame-off start time, which `satCycleInit()` set to the boot moment (`SATcontrol.ino:836-837`, `SATcycles.ino:192`).
4. Cycle data is sampled every enabled pass (`SATcontrol.ino:4599`).
5. A decoded OT MsgID 3 frame updates the detected manufacturer (`OTGW-Core.ino:3076`, `SATcontrol.ino:175-190`).

#### Phase H: shutdown, disable and resume

- **Disable** sends `CS=0` and saves zeroed PID state (Phase F). The next boot then starts the PID from zero, because the restored values are zeros (`SATcontrol.ino:2170-2171`).
- **Safety trip** (5 ticks without a command interface; a lost room temperature no longer trips, TASK-1364) sets the trip flag and calls `satDisable()`. Each later pass stops at the trip check (`SATcontrol.ino:4581`). Re-enabling clears it (`SATcontrol.ino:1941-1948`). The trip is not persisted: `initSAT()` clears it (`SATcontrol.ino:3692`).
- **Filesystem update:** the update server writes the PID state, energy and curve recommendation from RAM after the new filesystem image is mounted (`SATcontrol.ino:2118-2136`, `OTGW-ModUpdateServer-esp32.h:465`).
- **Reboot or power loss:** a grep for the callers of `satSavePidState()` finds only `satPersistAllToFs()`, `satDisable()`, `satFlushShortLivedData()` and the 5-minute save in the tick (`SATcontrol.ino:2131`, `SATcontrol.ino:2171`, `SATcontrol.ino:2197`, `SATcontrol.ino:5011`). There is no save on a planned restart in that list, so after a reboot the PID file holds the state of the last 5-minute save.
- **Kept only in memory** (lost on every restart), with the line where each is initialised: control mode (`SATcontrol.ino:3691`); PWM phase and hold (`SATcontrol.ino:923-931`); cycle history and per-hour counter (`SATcycles.ino:200-207`); command cache (`SATcontrol.ino:356-362`); room filter (`SATcontrol.ino:4696-4698`); skip and PIC-fail counters (`SATcontrol.ino:3693-3694`); previous flame state (`SATcontrol.ino:473`); last control setpoint (`SATcontrol.ino:693`).

#### Timeline

Times are relative. `I` is the moment `initSAT()` runs. The time from power-on to `I` depends on WiFi and other setup steps and is not fixed by the code. The default interval of 30 s is assumed.

```
POWER-ON
  setup(): mount LittleFS, read settings                   OTGW-firmware.ino:313-316
           WiFi (portal up to 240 s), NTP, MQTT              OTGW-firmware.ino:356, 486, 503
           reset PIC, start PIC serial task                  OTGW-firmware.ino:539-547
  I+0     initSAT():                                         OTGW-firmware.ino:567
            PID reset, PID restore (needs NTP and age <= 1800 s)   SATcontrol.ino:3683-3684
            energy + curve-recommendation restore            SATcontrol.ino:3685-3687
            cycle tracker init (flame-off start = now)       SATcontrol.ino:3688, SATcycles.ino:192
            PWM reset, mode OFF, trip cleared, cache invalid SATcontrol.ino:3689-3700
            CS=0 (if a command interface exists)             SATcontrol.ino:3706-3710
            control timer: next tick at I + 30 s             SATcontrol.ino:3719
            if enabled: mode CONTINUOUS, PM=3, PM=15, PM=48 (+MI=500)   SATcontrol.ino:3721-3734
          setup complete                                     OTGW-firmware.ino:572
  I+...   every main-loop pass (not while flashing):         OTGW-firmware.ino:1132, 1157
            once NTP is synced: PID restore retried once     SATcontrol.ino:4515-4519
            if enabled: deferred CS=0, flame edge, cycle sample,
            PIC keepalive, 3 s PWM refresh                   SATcontrol.ino:4585-4611
  I+30 s  tick 1: PID first call returns 0 -> 10 C -> cold cutoff
            -> CS=10.0, MM=100, CH=0                         SATpid.ino:264-280, SATcontrol.ino:4971-4983
  I+60 s  tick 2: same output unless the error changed       SATpid.ino:283-285
  I+90 s  tick 3: first computed PID output -> CS/MM/CH from the mode   SATpid.ino:288-319
  every 30 s after a CS >= 8 C (PIC boards only): CS keepalive   SATcontrol.ino:403-414
  every 30 s: tick (PID computes only when its own gates allow)  SATcontrol.ino:4614
  every 300 s: unchanged CS/MM/CH re-sent; PID state saved   SATcontrol.ino:351, 5010-5012

RUNTIME ENABLE (SAT was disabled)
  next pass: timer due time is in the past -> tick at once,
             or one interval later if it is more than 10 intervals old   safeTimers.h:181-189
  that tick: mode OFF -> CONTINUOUS, PID first call returns 0 -> CS=10.0, MM=100, CH=0
  no PM= commands (initSAT is not called)
```

The tick times above are earliest times: a tick fires only when a pass reaches the timer check, so it can be later when the main loop is busy.

<a id="s1-formulas"></a>

### Formulas

Variable names are as in the code.

**Effective target** (`SATcontrol.ino:4831-4833`):

```
effectiveTarget = fTargetTemp (+ fPvBoostAppliedC when the PV boost is active)
```

**Heating curve** (`SATcontrol.ino:489-496`), not rounded:

```
diff = outsideTemp - 20
curveValue = 4*(targetTemp - 20) + 0.03*diff^2 - 0.4*diff
setpoint = baseOffset + (fHeatingCurveCoeff / 4) * curveValue
baseOffset = 20 underfloor, 27.2 otherwise                (SATcontrol.ino:49-50, 255-260)
```

**Error** (`SATpid.ino:63-66`): `error = round(target - room, 3 decimals)`.

**Gains** (`SATpid.ino:120-142`):

```
bAutoGains on (default):  kp = fHeatingCurveCoeff * curveValue / divisor   (divisor 4 underfloor, 3 otherwise)
                          ki = kp / 8400
                          kd = 0.07 * 8400 * kp
bAutoGains off:           kp, ki, kd = fKpManual, fKiManual, fKdManual (defaults 5.0, 0.0005, 0.0; SATtypes.h:519-521)
```

**PID terms** (`SATpid.ino:149-246`, `SATpid.ino:297-309`):

```
I: if solar gain active and bSolarFreezeIntegral: no change (no reset, no accumulation)   SATpid.ino:157, (corrected 2026-10-24, TASK-1351)
   else if |error| > fDeadband: integral = 0
   else if ki >= 1e-9:
        integral += ki * error * 60
        integral = clamp(integral, -curveValue, +curveValue), then clamp(integral, -20, +20)
D: dt = (time the current room value first appeared) - (time of the derivative reference)
   only when |error| > fDeadband, the room value changed by >= 0.001 and dt > 60 s:
     rawDeriv = -(room - refRoom) / dt
     if |rawDeriv| >= 5: rawDerivative = clamp(rawDeriv, -5, 5)
     else: alpha = dt / (60 + dt); rawDerivative = clamp(alpha*rawDeriv + (1-alpha)*rawDerivative, -5, 5)
P = kp * error,  D = kd * rawDerivative
output = curveValue + P + I + D        (not rounded)
```

**Room filter** (`SATcontrol.ino:4705-4710`): `alphaF = dt / (45 + dt)`, `filtered += alphaF * (raw - filtered)`.

**Clamp and cold cutoff** (`SATcontrol.ino:4908-4912`, `SATcontrol.ino:4936-4937`, `SATcontrol.ino:4971-4975`):

```
pidOutput = clamp(pidOutput, 10, maxSetpoint)
pidOutput = pidOutput - fSolarSetpointOffset (solar gain active and enabled), before the clamp above (TASK-1323)
finalSetpoint = clamp(mode result, 10, maxSetpoint)
if pidOutput <= coldSetpoint (28.2 radiators, 21 underfloor): finalSetpoint = 10, MM = iMaxRelModulation, PWM left   (corrected 2026-10-24, TASK-1351)
CH = 0 if the cold cutoff fired or PWM drives the CS in its OFF phase, else 1   (satChEnableDecide, SATchEnable.h)
```

**Effective maximum** (`SATcontrol.ino:240-252`, `SATcontrol.ino:228-235`):

```
maxSetpoint = MaxTSet from the boiler, or 75 when below 30
maxSetpoint = min(maxSetpoint, systemCap (62 radiators, 45 underfloor, 40 heat pump), hardMax (80 radiators, 50 underfloor), fMaxSetpoint (65; 65 also when outside 30..80))
```

**Continuous mode** (`SATcontrol.ino:1148-1187`):

```
if flame off, or Tboiler <= 0 or > 100, or Tboiler <= pidOutput: control = pidOutput
else: min = Tboiler - fFlowOffset                            (fFlowOffset default 2.0)
      control = max(pidOutput, min)                if pidOutput > previousCS
      control = max(pidOutput, min(min, previousCS)) otherwise   (heating_control.py:485-489, entry 112)
```

**Modulation** (`SATcontrol.ino:304-314`): continuous gas boiler gives `iMaxRelModulation`; PWM gives 0 (heat pump or hybrid keep the maximum, never 0); Geminox floor 10; Immergas cap 80.

<a id="s1-constants-and-defaults"></a>

### Constants and defaults

| Name | Value | Unit | Citation |
|---|---|---|---|
| Control tick interval `iControlInterval` | 30 (allowed 10 to 300) | s | `SATtypes.h:444`, `settingStuff.ino:1001` |
| Delay of the first tick after `initSAT()` | one interval | s | `SATcontrol.ino:3719`, `safeTimers.h:131-135` |
| Control timer type after `initSAT()` | `SKIP_MISSED_TICKS` (0) | - | `safeTimers.h:96`, `safeTimers.h:135`, `safeTimers.h:247-258` |
| Timer "too far behind" limit | 10 intervals | - | `safeTimers.h:186-189` |
| `SAT_PID_SAMPLE_TIME_LIMIT` | removed: the 10 s minimum between updates is gone (TASK-1368, entry 75) | - | - |
| `SAT_PID_UPDATE_INTERVAL` (integral dt, skip window, derivative gate) | 60 | s | `SATpid.ino:22` |
| `fDeadband` default | 0.1 | °C | `SATtypes.h:443` |
| `SAT_PID_DERIVATIVE_CAP` | 5.0 | °C/s (raw slope) | `SATpid.ino:24` |
| `SAT_PID_AGGRESSION_V3` | 8400 | - | `SATpid.ino:25` |
| Integral absolute cap | 20 | °C | `SATpid.ino:176` |
| `SAT_PID_STALE_SEC` (maximum age of a restored PID file) | 1800 | s | `SATcontrol.ino:1993` |
| "Clock is synced" threshold for the restore | `time >= 1000000` | s since 1970 | `SATcontrol.ino:2028` |
| PID save interval | 300 | s | `SATcontrol.ino:5010` |
| Energy save interval | 3600 | s | `SATcontrol.ino:2053` |
| `SAT_CMD_REFRESH_MS` | 300 | s | `SATcontrol.ino:351` |
| `SAT_PIC_CS_KEEPALIVE_MS`, minimum CS kept alive | 30, 8 | s, °C | `SATcontrol.ino:403`, `SATcontrol.ino:407` |
| PWM suppression refresh | 3 | s | `SATcontrol.ino:480` |
| Window statistics timer | 60 | s | `SATcontrol.ino:478` |
| MQTT loss before fallback | 300 | s | `SATcontrol.ino:4547` |
| `SAT_MAX_SKIP_COUNT` | removed (TASK-1364) | | a lost room temperature no longer trips the safety; SAT heats on the curve only |
| `SAT_MAX_PIC_FAILS` | 5 | ticks | `SATcontrol.ino:69` |
| Room filter time constant | 45 | s | `SATcontrol.ino:4695` |
| Valid room temperature range | -10 to 50 | °C | `SATcontrol.ino:4669` |
| Outside temperature replacement when outside -40 to 50 | 10 | °C | `SATcontrol.ino:4721-4723` |
| `SAT_MIN_SETPOINT` | 10.0 | °C | `SATcontrol.ino:54` |
| Cold setpoint radiators / underfloor | 28.2 / 21.0 | °C | `SATcontrol.ino:55-56` |
| Heating curve base offset underfloor / other | 20.0 / 27.2 | °C | `SATcontrol.ino:49-50` |
| Heating curve reference temperature | 20.0 | °C | `SATcontrol.ino:51` |
| `fHeatingCurveCoeff` default | 2.0 (TASK-1347, entry 24) | - | `SATtypes.h:443` |
| `fTargetTemp` default | 20.0 | °C | `SATtypes.h:441` |
| `bEnabled` default | false | - | `SATtypes.h:437` |
| `bAutoGains` default | true | - | `SATtypes.h:518` |
| `iMaxRelModulation` default | 100 | % | `SATtypes.h:453` |
| `fFlowOffset` default | 2.0 | °C | `SATtypes.h:467` |
| `fModSupDelay` / `fModSupOffset` defaults | 20 / 1.0 | s / °C | `SATtypes.h:455-456` |
| `fFlameOffOffset` default | 18.0 | °C | `SATtypes.h:462` |
| `bOvershootProtection` default (`bForcePWM` was removed in TASK-1332) | true | - | `SATtypes.h:465-466` |
| `fMaxSetpoint` default | 65 | °C | `SATtypes.h:514` |
| Boiler `MaxTSet` fallback | 75 | °C | `SATcontrol.ino:57`, `SATcontrol.ino:243` |
| System caps radiators / underfloor / heat pump | 62 / 45 / 40 | °C | `SATcontrol.ino:231-233` |
| Hard caps radiators / underfloor | 80 / 50 | °C | `SATcontrol.ino:62-63` |
| `iSensorMaxAgeS` (MQTT external room sensor) | 21600 | s | `SATtypes.h:515`, `SATcontrol.ino:1225` |
| BLE / thermostat `Tr` / external outdoor stale after | 300 / 300 / 600 | s | `SATcontrol.ino:65-67` |
| PWM ignition timeout | 180 | s | `SATcontrol.ino:696` |
| PWM minimum on-time, gas boiler | 180 | s | `SATcontrol.ino:289` |
| Thermal safe flow / after estimating for | 45 / 7200 | °C / s | `SATcontrol.ino:82-83` |

<a id="s1-state-kept-between-passes"></a>

### State kept between passes

| State | Initial value at start | Persisted across restart? | Citation |
|---|---|---|---|
| SAT settings (enabled, target, interval, options) | file values or struct defaults | yes (settings file) | `OTGW-firmware.ino:316`, `settingStuff.ino:315-345` |
| PID integral and raw derivative | 0, then restored if NTP synced and file ≤ 1800 s old | yes (`/sat_pid_state.json`, every 300 s, on disable, flush and filesystem update) | `SATpid.ino:94-99`, `SATcontrol.ino:2026-2042`, `SATcontrol.ino:5010-5012` |
| Displayed `fPidD`, `fError` | 0, then restored | yes (same file) | `SATcontrol.ino:2038-2039` |
| PID last error, last room value, derivative reference and time, update time | set from live data on the first PID call | no | `SATpid.ino:264-273` |
| `fPidOutput` (returned while the PID skips) | 0 | no | `SATpid.ino:109`, `SATtypes.h:155` |
| Control mode | OFF, CONTINUOUS if enabled | no | `SATcontrol.ino:3691`, `SATcontrol.ino:3722` |
| Requested control mode | 0 (auto) | no | `SATtypes.h:208` |
| Active preset | NONE | no | `SATtypes.h:205` |
| PWM flame request, ignition wait, hold setpoint | cleared | no | `SATcontrol.ino:923-931` |
| Last control setpoint (PWM "previous CS") | 0 | no | `SATcontrol.ino:693` |
| Command cache (last CS/MM/CH/TC and send times) | invalid | no | `SATcontrol.ino:3700`, `SATtypes.h:395-396` |
| Cycle tracker, history, per-hour counter | flame off, flame-off start = boot, empty | no | `SATcycles.ino:188-219` |
| Previous flame state | false | no | `SATcontrol.ino:473` |
| Room filter value and time | not seeded | no (also not reset by a disable) | `SATcontrol.ino:4696-4703` |
| Skip and PIC-fail counters, safety trip | 0, 0, false | no | `SATcontrol.ino:3692-3694` |
| Fallback active | false | no | `SATtypes.h:235` |
| Energy totals | restored | yes (hourly, and on 0.1 kWh steps for the estimate) | `SATcontrol.ino:3685-3686`, `SATcontrol.ino:3254-3265` |
| Curve recommendation ring | restored | yes | `SATcontrol.ino:3687`, `SATcycles.ino:1135-1175` |
| Thermal drop coefficient | from settings | yes (setting) | `SATcontrol.ino:4119`, `settingStuff.ino:364` |

<a id="s1-outputs-to-the-boiler-to-mqtt"></a>

### Outputs (to the boiler, to MQTT)

**To the boiler** (through the shared command queue, on PIC or OT-Direct):

| When | Command | Citation |
|---|---|---|
| `initSAT()`, command interface present, SAT enabled or not | `CS=0` | `SATcontrol.ino:3706-3710` |
| `initSAT()`, SAT enabled, interface present | `PM=3`, `PM=15`, `PM=48`, `MI=500` for Ideal, Intergas, Nefit | `SATcontrol.ino:3721-3734` |
| First enabled pass with an interface, if boot `CS=0` was missed | `CS=0` | `SATcontrol.ino:4585-4589` |
| Each tick on the main path | `CS=<setpoint>`, `MM=<value>`, `CH=0/1`, write-on-change | `SATcontrol.ino:4978-4983` |
| Same tick, Immergas | `TP=11:12=<setpoint>`, every tick | `SATcontrol.ino:4986-4990` |
| Same tick, push setpoint on | `TC=<target>`, on target change only, no re-push (TASK-1272) | `SATcontrol.ino:5172-5176`, `SATcontrol.ino:540-553` |
| Tick in summer simmer or with all valves closed | `CS=10.0`, `MM=<max>`, `CH=0` | `SATcontrol.ino:4766-4773`, `SATcontrol.ino:4779-4786` |
| Tick in the thermal safe fallback (2 h of estimation) | `CS=45` (capped at the system max), `MM=<max>`, `CH=1` | `SATcontrol.ino:4732-4743` |
| Tick entering DHW with DHW control on | `SW=<DHW setpoint>`, once per DHW entry | `SATcontrol.ino:4640-4648` |
| PIC boards, every 30 s once CS ≥ 8 °C | `CS=<last CS>` | `SATcontrol.ino:405-414` |
| Every 3 s in the PWM ON phase after the hold | `CS=<flow - offset>`, write-on-change | `SATcontrol.ino:894-918` |
| Disable, safety trip, fallback lift | `CS=0` | `SATcontrol.ino:2179`, `SATcontrol.ino:4561`, `SATcontrol.ino:4680`, `SATcontrol.ino:5002` |

**To MQTT:** the core state topics (`sat/enabled`, `sat/active`, `sat/safety_tripped`, `sat/fallback_active`, `sat/mode`, `sat/control_mode`, `sat/preset`, and `sat/external_temp_enable`) after every pass, sent only on change (`SATcontrol.ino:2610-2631`, `SATcontrol.ino:4504-4508`). The full SAT topic set at the end of a tick or on the early-return paths that call `satPublishMQTT()` (`SATcontrol.ino:5018`, `SATcontrol.ino:4650`, `SATcontrol.ino:4744`, `SATcontrol.ino:4774`, `SATcontrol.ino:4788`).

<a id="s1-edge-cases"></a>

### Edge cases

1. **SAT disabled at boot.** `CS=0` is still sent at `initSAT()` when an interface exists (`SATcontrol.ino:3706-3710`). No `PM=` commands are sent (`SATcontrol.ino:3721`). Every pass stops at the enabled check (`SATcontrol.ino:4568-4573`).
2. **No command interface at `initSAT()`.** `CS=0` is sent on the first enabled pass after an interface appears (`SATcontrol.ino:4585-4589`). The `PM=` and `MI=500` commands are not retried (`SATcontrol.ino:3725`).
3. **NTP not synced at `initSAT()`.** The PID restore is skipped and retried once on the first pass with NTP (`SATcontrol.ino:2028-2032`, `SATcontrol.ino:4515-4519`). If that retry comes after the PID already computed, it overwrites the working integral and raw derivative with the file values (`SATpid.ino:77-78`). Whether that happens depends on how long NTP takes.
4. **PID file older than 30 minutes.** It is discarded and the PID starts from zero (`SATcontrol.ino:2033-2037`).
5. **Restored integral.** It survives the first computed update only when |error| ≤ deadband (`SATpid.ino:154-157`).
6. **First ticks after any start.** Until the PID computes, the tick sends `CS=10.0`, `MM=100`, `CH=0`, which commands heating off (E2, E3). With the default interval and a steady room, that lasts two ticks (about 60 s). On PIC boards the `CS=10.0` is then kept alive every 30 s (`SATcontrol.ino:405-414`).
7. **No room temperature source.** `Tr` starts as NAN (`OTGW-Core.h:79`). Without BLE, external, multi-area or a thermostat `Tr`, SAT heats on the heating curve only (TASK-1364): P, I and D are 0, no trip, no time limit. Before TASK-1364 the 10th invalid tick (about 300 s at 30 s) tripped the safety and sent `CS=0`.
8. **No outside temperature source.** The OT value starts as 0.0 and is used as 0 °C (`OTGW-Core.h:82`, `SATcontrol.ino:1302`). The weather value is used instead only when it is valid and the OT value is exactly 0.0 (`SATcontrol.ino:1299-1300`).
9. **Flame already burning when SAT starts.** The first enabled pass that sees the flame bit starts a new cycle there, and the PWM suppression hold counts from that moment (`SATcontrol.ino:4592-4596`, `SATcycles.ino:701-714`, `SATcontrol.ino:720-723`).
10. **DHW active.** The tick stops before reading inputs, so the PID and the room filter are not updated and CS, MM, CH are not sent (`SATcontrol.ino:4865-4882 [lines refreshed 2026-10-24, TASK-1351]`). The PIC keepalive keeps running on every pass (`SATcontrol.ino:4608`).
11. **Boiler fault flag.** The tick stops after the room filter update and before the PID (`SATcontrol.ino:4694-4719`).
12. **Firmware or PIC flash running.** The main loop does not call `satControlLoop()` at all (`OTGW-firmware.ino:1132`, `OTGW-firmware.ino:1157`).
13. **Runtime enable after a long disable.** No `PM=` commands; the first tick comes at once or one interval later, depending on how old the timer's due time is (`safeTimers.h:181-189`).
14. **Room filter after a disable.** Its static state is not reset by `satDisable()`; after a long gap `dt` is large, so `alphaF` is close to 1 and the filter jumps almost to the raw value (`SATcontrol.ino:4696-4710`).
15. **Preset change.** Only the displayed integral `fPidI` is zeroed; the working integral is unchanged (`SATcontrol.ino:998`, `SATpid.ino:298`).
16. **Timer type change.** The control timer is declared `CATCH_UP_MISSED_TICKS` (`SATcontrol.ino:582`) but runs as `SKIP_MISSED_TICKS` after `initSAT()` and after every interval change (`SATcontrol.ino:4277`, `settingStuff.ino:1005`, `safeTimers.h:96`, `:131-135`).

<a id="s1-not-found-in-the-code-unclear"></a>

### Not found in the code / unclear

1. **Upstream pin.** The firmware was read at `51df6b486`; the branch has since moved to `2c194a329`. The cited lines were checked against `51df6b486` (see the top of this document).
2. **When OT frames are first decoded.** The comment at `OTGW-firmware.ino:303-306` says nothing parses OT before `setup()` finishes. I did not trace that gate in `doBackgroundTasks()`. It decides whether a MsgID 3 (manufacturer) frame can be known at `initSAT()`, and so whether `MI=500` can be sent with the manufacturer on AUTO.
3. **What `sendPICBootCommands()` sends** (`OTGW-firmware.ino:557`). It runs before `initSAT()`; I did not read it.
4. **Whether `hasOTCommandInterface()` is already true at `initSAT()`** on each board type. It depends on `state.pic.bAvailable` and `state.hw.eMode` (`OTGW-firmware.h:527-538`), which I did not trace.
5. **Meaning of `PM=3`, `PM=15`, `PM=48` and `MI=500`.** The firmware comment says they follow SAT Python and that `MI=500` is for faster OT polling (`SATcontrol.ino:3724`, `SATcontrol.ino:3729`). What the PIC or OT-Direct does with them is outside the SAT code.
6. **Loop pass rate.** How often the main loop runs is not fixed by the code, so the exact tick and keepalive times can be later than the nominal values.
7. **Persistence of the fallback enable.** Closed by TASK-1345: the MQTT fallback no longer writes `settings.sat.bEnabled`; it sets only `state.sat.bFallbackActive` (see the MQTT fallback paragraph).
8. **Multi-zone start-up.** The zone block runs for every configured zone and can raise the main PID output or cap it (`SATcontrol.ino:5540-5561`, TASK-1440). I did not trace the start-up of the zone PID (`satZonePidAdvance()`, `SATcontrol.ino:2255`).
9. **PWM first cycle after boot.** The PWM OFF phase compares against the flame-off start time set at boot (`SATcontrol.ino:836-837`, `SATcycles.ino:192`). I did not work out the first PWM ON timing for each duty range.
10. **Outside temperature NAN.** The -40 to 50 °C check (`SATcontrol.ino:4721`) does not catch NAN. I did not check whether any outside source can deliver NAN.
11. **What the boiler or PIC does with the last commands** after `CS=0` or during the gap before the first tick. That is gateway and boiler behaviour outside this code.

---

<a id="s2"></a>

## PID controller (OTGW firmware, as implemented)

Source: OTGW-firmware, branch `dev`, commit `51df6b486`. All paths are relative to `src/OTGW-firmware`. This section describes what the firmware does, not what it should do. It follows the headings and order of the thermo-nova section `a2-pid-thermonova.md`, so the two can be read side by side. Where the firmware differs from thermo-nova, this is stated as "Differs from thermo-nova" with the firmware citation; the thermo-nova side is in the other document.

<a id="s2-purpose"></a>

### Purpose

The PID controller turns the room temperature error into a correction on top of the heating curve. Its output is a flow temperature request in degrees: `output = heatingCurveValue + P + I + D` (SATpid.ino:308-310). The control tick uses that output as `pidOutput`, which then drives the control setpoint sent to the boiler (SATcontrol.ino:4839, SATcontrol.ino:4906-4933).

There is one main PID with its own internal state held in file-level static variables (SATpid.ino:31-56). There is a separate, simpler per-zone PID for up to `SAT_MAX_ZONES` zones (SATcontrol.ino:2255-2310, SATcontrol.ino:5540-5561). It runs for every configured zone, also at zone count 1; a zone without usable inputs is excluded, so a setup that pushes no zone data is unchanged (TASK-1440). The zone count default is 1 (SATtypes.h:531). Each zone PID computes its own heating curve from the zone target and the shared outside temperature (satZonePidAdvance(), SATcontrol.ino:2255).

<a id="s2-inputs"></a>

### Inputs

| Input | Where it comes from | Citation |
|---|---|---|
| Room setpoint (`effectiveTarget`) | `settings.sat.fTargetTemp` plus the PV boost when active | SATcontrol.ino:4658, SATcontrol.ino:4831-4833 |
| Room temperature (`roomTemp`) | `satGetRoomTemp()`: simulation, then BLE, then multi-area average, then MQTT external sensor, then OT MsgID 24 (Tr), with Summer Simmer index on the Tr path when thermal comfort is on | SATcontrol.ino:4656, SATcontrol.ino:1192-1280 |
| Room temperature filter | Time-based EMA with tau 45 s, applied before the PID; the first valid reading seeds it | SATcontrol.ino:4694-4713 |
| Sensor change time | Not taken from the sensor. The PID stamps `millis()` when a call sees a room temperature that differs by 0.001 or more from the last one seen | SATpid.ino:256-261, SATpid.ino:46-47 |
| Heating curve value | `satCalcHeatingCurve(effectiveTarget, outsideTemp)`, recomputed every control tick | SATcontrol.ino:4836, SATcontrol.ino:485-500 |
| Outside temperature | `satGetOutsideTemp()`: simulation, external MQTT (10 min stale limit), weather API when OT Toutside is 0.0, else OT MsgID 27; values outside -40 to 50 are replaced by 10.0 | SATcontrol.ino:4657, SATcontrol.ino:1283-1303, SATcontrol.ino:4721-4723 |
| Heating system type | `satGetEffectiveHeatingSystem()`: the setting, with AUTO mapped to radiators | SATcontrol.ino:194-201, SATtypes.h:438 |
| Gains config | `settings.sat.bAutoGains`, `fKpManual`, `fKiManual`, `fKdManual`, `fHeatingCurveCoeff` | SATpid.ino:120-142, SATtypes.h:442, SATtypes.h:518-521 |
| Deadband | `settings.sat.fDeadband`, a setting (not a constant) | SATpid.ino:151, SATpid.ino:189, SATtypes.h:443 |
| Freeze flag | `state.sat.bSolarGainActive` and `settings.sat.bSolarFreezeIntegral` (corrected 2026-10-24, TASK-1351) | SATpid.ino:157 |
| `boilerTemp` argument | gone with `satPidUpdate()` (TASK-1368): `satPidStep()` and `satPidOutput()` do not take the flow temperature | SATpid.ino |

Differs from thermo-nova: the room temperature passes an extra EMA filter before the PID (SATcontrol.ino:4686-4693 states the reason in a comment). Thermal comfort (Summer Simmer index) only applies when the room temperature comes from the OT Tr path, because the BLE, multi-area and external sources return earlier (SATcontrol.ino:1208-1231, SATcontrol.ino:1264-1270).

<a id="s2-behaviour-step-by-step-in-time-order"></a>

### Behaviour (step by step, in time order)

**A. Creation**

1. The PID state is a set of file-level static variables with initial values: error, integral, raw derivative and last room temperature all 0.0, `_pid_initialized = false` (SATpid.ino:31-40). `_pid_roomTempSeen` starts as `NAN` (SATpid.ino:46).
2. There is no object and no constructor. `satPidReset()` is the explicit reset (SATpid.ino:94-114).

**B. Start-up** (boot)

1. `initSAT()` is called from `setup()` (OTGW-firmware.ino:256, OTGW-firmware.ino:567). It first calls `satPidReset()` (SATcontrol.ino:3683). The reset sets error, integral, raw derivative, last room temperature to 0.0, sets `_pid_lastDerivativeMs` to `millis()`, sets `_pid_initialized = false` and `_pid_startupPending = true` (the next `satPidService()` call takes the start-up step, TASK-1368), and zeroes the published values `fPidP`, `fPidI`, `fPidD`, `fPidOutput`, `fKp`, `fKi`, `fKd`, `fRawDerivative` (SATpid.ino:96-113).
2. It then calls `satLoadPidState()` (SATcontrol.ino:3684). This reads `/sat_pid_state.json` (SATcontrol.ino:1988, SATcontrol.ino:2010-2024). If NTP time is not set yet (`time < 1000000`), nothing is restored (SATcontrol.ino:2027-2032). If the saved timestamp is missing or more than 1800 s old, nothing is restored (SATcontrol.ino:1993, SATcontrol.ino:2033-2037). Otherwise it sets the displayed `fPidD` and `fError` (SATcontrol.ino:2038-2039) and calls `satPidRestoreState(i, rd)` (SATcontrol.ino:2042), which sets `_pid_integral` and `_pid_rawDerivative` and their published copies (SATpid.ino:75-80). `_pid_initialized` stays false (SATpid.ino:72-74). `fPidOutput` is not restored (SATpid.ino:75-83).
3. `initSAT()` sends `CS=0` when an OT command interface is available, to release any stale control setpoint (SATcontrol.ino:3702-3710). It sets the control timer to `settings.sat.iControlInterval` (SATcontrol.ino:3719).
4. `loop()` calls `satControlLoop()` (OTGW-firmware.ino:1094, OTGW-firmware.ino:1157). On the first pass where NTP time is set, `satLoadPidState()` runs once more (SATcontrol.ino:4515-4519). This retry sits before the "SAT enabled" check (SATcontrol.ino:4568).
5. Computed from the code (not stated in it): if NTP becomes available only after the PID has already run, this one-time retry loads the file and overwrites `_pid_integral` and `_pid_rawDerivative`, provided the file is no more than 30 minutes old (SATcontrol.ino:4516-4518, SATcontrol.ino:2033-2042, SATpid.ino:77-78). This can only happen when NTP arrives before this boot's first save. `_pidLastSaveMs` starts at 0, so the first periodic save comes on the first full control tick after about 300 s of uptime (SATcontrol.ino:1989, SATcontrol.ino:5010-5011). That save stores `time(nullptr)` (SATcontrol.ino:2000-2001). If NTP is not synced then, and the clock value is below 1000000 as the check at SATcontrol.ino:2028 assumes, the retry finds the file older than 1800 s and discards it (SATcontrol.ino:2033).
6. The PID steps on its own 60 s timer `timerSATPid` (`DECLARE_TIMER_SEC(timerSATPid, 60, SKIP_MISSED_TICKS)` in SATpid.ino), not on the control tick (TASK-1368, Sergeant D entries 42, 28, 111; thermo-nova `climate.py:712-716`, `pid.py:20`). `satControlLoopRun()` calls `satPidService(millis())` on every loop pass, after the SAT-off, flash, trip and OpenTherm-loss returns and before `if (!DUE(timerSATControl)) return;`. The control tick (default 30 s, setting 10 to 300 s) only reads the output.
7. **First PID step after boot or after any `satPidReset()`.** `satPidReset()` sets `_pid_startupPending`, so the first `satPidService()` call takes one start-up step at once (thermo-nova `climate.py:137`, `:143`) and restarts the 60 s timer; later steps follow every 60 s. In that step:
   1. The initialisation block sets `_pid_lastError = error`, `_pid_lastDerivativeMs` = the time the current room temperature was first seen, and `_pid_derivRefTemp = roomTemp`.
   2. The step then runs in full: gains, integral, derivative (no time gate, entry 75).
   3. Until a room temperature exists the start-up step is curve-only (`satPidCurveOnly()`), and the output is the heating curve.
   4. What the first output contains after a restore, and `D = 0` on the first tick (entry 95 Q39b), belong to TASK-1270 and are not changed here: after a restore `satPidOutput()` adds `kd * restored raw derivative` from the first tick on.

**C. Every control tick** (SATcontrol.ino:4510-5019)

The PID is not stepped by the tick (TASK-1368). `satPidService()` runs before the tick's early returns and steps the PID on its 60 s timer; the tick only reads the output (`satPidOutput(curveValue)`). The PID service does not run (and the PID does not step) in these cases: SAT disabled or a flash update running, MQTT takeover, safety trip latched, OpenTherm link lost (an OpenTherm loss turns SAT off, so the next pass after the link returns takes a start-up step). The control tick itself returns, in this order:

1. SAT disabled or a flash update running (SATcontrol.ino:4568-4573).
2. Safety trip latched (SATcontrol.ino:4581).
3. Timer not due (SATcontrol.ino:4614).
4. DHW active (SATcontrol.ino:4865-4882 [lines refreshed 2026-10-24, TASK-1351]).
5. Room temperature invalid (`NaN`, below -10 or above 50): not a stop since TASK-1364. The tick goes on with the heating curve only (P, I, D = 0).
6. Boiler fault flag (OT status bit 0) set (SATcontrol.ino:4716-4719).
7. Thermal estimation fallback longer than 2 h: a fixed 45 C setpoint is sent instead (SATcontrol.ino:4730-4746, SATcontrol.ino:82-83).
8. Summer simmer active (SATcontrol.ino:4766-4776).
9. All valves closed (SATcontrol.ino:4779-4790).

As thermo-nova (entry 111), the PID steps on its own timer also during DHW, with a boiler fault, in the thermal-safe fallback, in summer simmer and with closed valves; it stops only when SAT is off.

When none of these apply:

1. The room temperature is EMA-filtered (SATcontrol.ino:4694-4713).
2. During thermal estimation (fallback active, under 2 h), the deadband setting is temporarily widened by `0.5 * hours of estimation` (SATcontrol.ino:4747-4751). It is restored after the PID and zone steps (SATcontrol.ino:4901-4904).
3. Solar gain detection runs (SATcontrol.ino:4762), see section E for its effect on the integral.
4. The auto-tune step may change the heating curve coefficient (SATcontrol.ino:4793), see "Formulas".
5. `effectiveTarget` is built (SATcontrol.ino:4831-4833) and the heating curve is computed (SATcontrol.ino:4836).
6. `satPidOutput(curveValue)` runs (without a room temperature `satPidCurveOnly(curveValue)`). The 60 s `satPidStep(room, target, curve, nowMs)` that moved the held state ran earlier, in `satPidService()`. Inside the step:
   1. `error = satPidError(target, room)` = `target - room` rounded to 3 decimals (SATpid.ino:253, SATpid.ino:63-66).
   2. If the room temperature differs by 0.001 or more from the last value seen, the "changed" timestamp is set to now. This happens before any early return (SATpid.ino:256-261).
   3. On the first call after a reset, the initialisation block runs (SATpid.ino:264-273).
   4. No time gate (the 10 s minimum was removed, entry 75).
   5. No unchanged-error gate (removed, entry 42; thermo-nova `pid.py:159-171`).
   6. Gains are calculated from the current curve value (SATpid.ino:288), see "Formulas".
   7. The integral is updated (SATpid.ino:291), see section E.
   8. The derivative is updated (SATpid.ino:294), see section D.
   9. The error, the integral and the raw derivative are stored in `state.sat`. P, D and the output are computed by `satPidOutput()` on every control tick from the current curve: `curve + kp(curve) * held error + I + kd(curve) * held raw derivative` (`pid.py:60-132`).
   10. `_pid_lastError` and the other tracking values are stored.

Differs from thermo-nova: the integral is updated before the derivative (SATpid.ino:291, SATpid.ino:294). The two updates do not read each other's state (SATpid.ino:149-247), so the order does not change the result.

Computed from the code: the PID steps once per 60 s whatever the tick interval (10 to 300 s); between steps the output follows the current curve (TASK-1368).

The zone PID (every configured zone, also at zone count 1) takes its integral step in `satZonePidAdvanceAll()` on the same 60 s timer (`area.py:183-190`, entry 111) and its output in `satZonePidOutput()` on every control tick that reaches the zone block.

**D. Derivative update** (SATpid.ino:187-247), in the order the code checks:

1. First time: handled by the initialisation block in `satPidStep`, which sets the timestamp and the reference temperature. There is no separate first-time branch in the derivative function.
2. Error inside the deadband (`fabsf(error) <= deadband`): the timestamp is set to the "changed" time, the reference temperature is set to the current room temperature, and the function stops. `_pid_rawDerivative` keeps its value (SATpid.ino:199-204).
3. `tempDelta = roomTemp - _pid_derivRefTemp`. If `fabsf(tempDelta) < 0.001`: timestamp and reference are updated and the function stops (SATpid.ino:208-213).
4. `deltaTime = (changedMs - _pid_lastDerivativeMs) / 1000` in seconds (SATpid.ino:215). If `deltaTime <= 60`: stop. Neither the timestamp nor the reference temperature moves (SATpid.ino:217-219).
5. `rawDeriv = -tempDelta / deltaTime` (SATpid.ino:223). A rising room gives a negative value.
6. If `fabsf(rawDeriv) >= 5.0`: the value is clamped to [-5.0, +5.0], stored without filtering, timestamp and reference are updated, and the function stops (SATpid.ino:226-233, SATpid.ino:24).
7. Otherwise: `alpha = deltaTime / (60 + deltaTime)`, `filtered = alpha * rawDeriv + (1 - alpha) * _pid_rawDerivative`, clamped to ±5.0, then stored with the new timestamp and reference (SATpid.ino:237-246).

Differs from thermo-nova:
- The temperature delta is measured against `_pid_derivRefTemp`, which moves together with the timestamp. A change that comes within 60 s is kept and carried into the next calculation (SATpid.ino:49-56, SATpid.ino:217-219).
- The "changed" time is when a PID call first saw the new value, so its resolution is one control tick (SATpid.ino:256-261, SATpid.ino:42-45).
- The "no change" test uses a tolerance of 0.001 instead of an exact zero (SATpid.ino:209).
- Computed from the code: because of the room EMA (`satRoomFiltered()`), a sensor step reaches the PID as a series of small changes over several ticks. The comment at SATpid.ino:53-54 states that this is about 8 control ticks.

**E. Integral update** (SATpid.ino:149-179)

1. If `state.sat.bSolarGainActive` and `settings.sat.bSolarFreezeIntegral`: stop without change, so the integral is frozen and not even reset (TASK-1323; thermo-nova `climate.py:499-502`, `pid.py:167-168`).
2. If `fabsf(error) > deadband`: the integral is set to 0.0 and the function stops (SATpid.ino:153-157).
3. If `fKi < 1e-9`: stop without change (SATpid.ino:160).
4. Otherwise: `_pid_integral += fKi * error * 60` (SATpid.ino:165-166). The 60 is the constant `SAT_PID_UPDATE_INTERVAL`, not the measured time between updates (SATpid.ino:22).
5. The integral is clamped to [-curveValue, +curveValue] (SATpid.ino:172-173).
6. Then it is clamped to [-20.0, +20.0] (SATpid.ino:176-178).

So, as in thermo-nova, the integral only accumulates while the error is within the deadband and is wiped when the error leaves it.

Differs from thermo-nova:
- There is an extra absolute ±20 cap (SATpid.ino:176-178).
- Computed from the code: at the default 30 s tick with a changing error, the fixed 60 s step is added up to twice per minute. That is up to twice the thermo-nova accumulation per minute of real time (SATpid.ino:166, SATpid.ino:276-285, SATtypes.h:444).

Solar gain active requires `bSolarGainEnable` (SATcontrol.ino:4217-4223), default false (SATtypes.h:494). It turns on after the room rise rate stays above the threshold, with modulation under 20 % and valves open, for more than 10 minutes. It turns off after the rise rate stays below the threshold for more than 10 minutes (SATcontrol.ino:4264-4298). A low sun elevation forces it off (SATcontrol.ino:4249-4260). The freeze needs `bSolarFreezeIntegral` as well (default true, SATtypes.h:527; thermo-nova `const.py:136` default True), since TASK-1323.

**F. Between PID ticks** (here: between real PID updates)

The firmware stores P, I, D and the output in `state.sat` on each real update (SATpid.ino:302-310). A call that is skipped by the 10 s or 60 s gate returns the stored `fPidOutput` (SATpid.ino:279, SATpid.ino:284). So the output does not follow heating curve changes between real updates.

Differs from thermo-nova: there the output is recomputed on each read. Here nothing is recomputed between updates.

`fPidOutput` is also read between control ticks:
- by the 3 s PWM suppression refresh, as a gate against the cold setpoint (SATcontrol.ino:480, SATcontrol.ino:894-905);
- by the cycle tracker at flame-off (SATcycles.ino:789);
- by the MQTT publisher (SATcontrol.ino:2733, SATcontrol.ino:3053-3058).

**G. Combination into the requested setpoint** (SATcontrol.ino:5538-5585)

1. `pidOutput = satPidOutput(curveValue)` (the 60 s `satPidService()` stepped the PID earlier).
2. Zones, for every configured zone (`iZoneCount` 1 to 4), through `satAggregateZoneSetpoint()` (SATcontrol.ino:5540-5561, SATcontrol.ino:2329-2377). Ported from thermo-nova climate.py:369-394 and area.py:166-175, :264-333 (entry 116, TASK-1440). Each zone step returns its output, or 10.0 when the zone is not usable (OFF, stale, never pushed, out of range: `satZoneUsable()`, SATcontrol.ino:2231). A zone without usable inputs does not count, so a setup without zone data is unchanged.
   1. ECO (`iHeatingMode` 1): `pidOutput` stays the main PID output (climate.py:377-379). The zone outputs are still computed, for the diagnostics.
   2. A zone counts when it is usable and its output is above the cold setpoint: 28.2 °C radiators, 21.0 °C underfloor (`satGetColdSetpoint()`, ADR-199; thermo-nova uses `COLD_SETPOINT` 22 at area.py:175). The firmware has no per-zone valve position, so only the output test of area.py:166-175 applies.
   3. Zones output (area.py:264-301): the counted outputs sorted ascending, `baseline = outputs[int(N * 0.75)]` (clamped to N - 1), `chosen = min(highest, baseline + fZoneAggregationHeadroom)`, rounded to 1 decimal. No counted zone gives 10.0 (`SAT_MIN_SETPOINT`), which leaves the main PID output alone (area.py:291-292).
   4. `pidOutput = max(main PID output, zones output)` (climate.py:385-386). The zones only raise the demand of the main room, they never lower it.
   5. Overshoot cap (area.py:303-333, climate.py:388-389): every counted zone more than 0.3 °C above its target gives `cold + 10 - 4 * degrees over`. The lowest wins, floored at the cold setpoint and rounded to 1 decimal. Then `pidOutput = min(pidOutput, cap)`. The cap can lower `pidOutput` below the main PID output, as in thermo-nova.
3. Clamp to [10.0, `satEffectiveMaxSetpoint()`] (SATcontrol.ino:5583-5585). The ceiling is the lowest of:
   - the boiler's MaxTSet, or 75 when MaxTSet is below 30;
   - the system cap (62 radiators, 45 underfloor, 40 for a heat pump source);
   - the hard maximum (80 radiators, 50 underfloor);
   - the global maximum setting (default 65).

   Citations: SATcontrol.ino:228-252, SATcontrol.ino:57, SATcontrol.ino:62-64, SATtypes.h:514.
4. If `pidOutput >= satGetMaxSetpoint()` while in PWM mode, the mode switches to continuous (SATcontrol.ino:4914-4920). Force-PWM can switch it back (SATcontrol.ino:4922-4925).
5. `satApplyPWM(pidOutput)` or `satApplyContinuous(pidOutput)` produces `finalSetpoint` (SATcontrol.ino:4927-4933). These belong to the PWM and heating control sections.
6. `finalSetpoint` is clamped again to [10.0, max] (SATcontrol.ino:4935-4937).
7. Solar gain active and enabled: `fSolarSetpointOffset` (default 2.0, SATtypes.h:496) is subtracted from the requested setpoint BEFORE the clamp in step 3 and the control mode step, as thermo-nova does (`climate.py:391-392`; TASK-1323). It is no longer subtracted from `finalSetpoint` afterwards, so the PWM hold and the suppression CS carry no solar term.
8. Cold setpoint cut-off: if `pidOutput <= satGetColdSetpoint()` (28.2 radiators, 21.0 underfloor), `finalSetpoint = 10.0`, modulation `iMaxRelModulation` and PWM left (SATcontrol.ino:5206-5214, SATcontrol.ino:1100-1109, SATcontrol.ino:130-131) (corrected 2026-10-24, TASK-1351).

There is no ECO / COMFORT branch in this path. The setting `iHeatingMode` exists (SATtypes.h:524, settingStuff.ino:1104), but in the SAT files it is only read for publishing (SATcontrol.ino:2533, SATcontrol.ino:2978). See "Not found / unclear".

<a id="s2-formulas"></a>

### Formulas

Variable names as in the code.

**Error** (SATpid.ino:63-66):
`error = roundf((target - room) * 1000) / 1000`. Positive means the room is too cold.

**Heating curve** (SATcontrol.ino:485-500, SATcontrol.ino:255-260):
`curveValue = 4 * (targetTemp - 20) + 0.03 * (outsideTemp - 20)^2 - 0.4 * (outsideTemp - 20)`
`setpoint = baseOffset + (coeff / 4) * curveValue`, not rounded.
`baseOffset = 20.0` for underfloor, `27.2` otherwise (SATcontrol.ino:49-50, SATcontrol.ino:258-259).
`coeff` is `settings.sat.fHeatingCurveCoeff` (SATcontrol.ino:488).

**Automatic gains** (when `bAutoGains` is true) (SATpid.ino:130-141):
- `divisor = 4` for underfloor, otherwise `3` (SATpid.ino:133, SATpid.ino:28-29).
- `kp = (coeff * curveValue) / divisor` (SATpid.ino:135).
- `ki = kp / 8400` (SATpid.ino:136, SATpid.ino:25).
- `kd = 0.07 * 8400 * kp`, that is `588 * kp` (SATpid.ino:137).
- No rounding.

**Manual gains** (when `bAutoGains` is false): `fKp = fKpManual`, `fKi = fKiManual`, `fKd = fKdManual` (SATpid.ino:122-127). See "Not found / unclear": no code path that sets `bAutoGains` was found.

**Auto-tune of the coefficient** (only when `bAutoTune` is true, default false (SATtypes.h:511)): once per hour, after at least 6 cycles, the coefficient is multiplied by `(1 - 2*rate)`, `(1 - rate)`, `(1 + rate)` or `(1 + 0.5*rate)`, depending on the counts of oscillation, overshoot, undershoot and slow convergence. It is then clamped to 0.3 to 5.0 and saved (SATcontrol.ino:4384-4385, SATcontrol.ino:4428-4486). `rate` defaults to 0.02 (SATtypes.h:512). Because the automatic gains use the coefficient, this changes Kp, Ki and Kd (SATpid.ino:130-137).

**Proportional term** (SATpid.ino:297): `P = fKp * error`, no rounding.

**Integral term** (SATpid.ino:154-178, SATpid.ino:298):
- if `state.sat.bSolarGainActive && settings.sat.bSolarFreezeIntegral`: return, nothing changes (SATpid.ino:157) (corrected 2026-10-24, TASK-1351); TASK-1323
- else if `fabsf(error) > deadband`: `_pid_integral = 0.0`
- else, if `fKi >= 1e-9`: `_pid_integral = clamp(clamp(_pid_integral + fKi * error * 60, -curveValue, +curveValue), -20, +20)`
- `I = _pid_integral`

**Derivative term** (SATpid.ino:208-246, SATpid.ino:299):
- `tempDelta = roomTemp - _pid_derivRefTemp`
- `deltaTime = (changedMs - _pid_lastDerivativeMs) / 1000` (s)
- `rawDeriv = -tempDelta / deltaTime`
- if `fabsf(rawDeriv) >= 5.0`: `_pid_rawDerivative = clamp(rawDeriv, -5.0, 5.0)`
- else: `alpha = deltaTime / (60 + deltaTime)`; `_pid_rawDerivative = clamp(alpha * rawDeriv + (1 - alpha) * _pid_rawDerivative, -5.0, 5.0)`
- `D = fKd * _pid_rawDerivative`

**Output** (`satPidOutput()`, SATpid.ino:343-363): `output = heatingCurveValue + P + I + D`, computed on every control tick from the current curve, with Kp and Kd recomputed from it and the error, integral and raw derivative held from the last PID step. D is left out of the first output after a reset (entry 95 Q39b). No rounding and no limit inside the PID. Limits are applied in the control tick.

**Zone PID** (SATcontrol.ino:1828-1866), per zone:
- `curveValue` from the zone target and outside temperature.
- `kp = (coeff * curveValue) / divisor` and `ki = kp / 8400`, always automatic, not dependent on `bAutoGains`.
- The same deadband rule for the integral, with the clamps ±curveValue and ±20.
- `output = curveValue + kp * error + integral`, with no derivative term.
- The output is clamped to [10.0, `satGetMaxSetpoint()`].

**Zone aggregation** (`satAggregateZoneSetpoint()`, SATcontrol.ino:2329-2377; ports thermo-nova area.py:264-333 and climate.py:369-394, entry 116, TASK-1440):
- A zone counts when it is usable and its output is above the cold setpoint (`satGetColdSetpoint()`: 28.2 radiators, 21.0 underfloor, ADR-199).
- `baseline = sorted[int(N * 0.75)]`, clamped to N - 1.
- `chosen = min(highest, baseline + fZoneAggregationHeadroom)`, rounded to 1 decimal. No counted zone gives 10.0.
- `requested = max(main PID output, chosen)`.
- Overshoot cap: for each counted zone with `target - room` below -0.3, `cap = cold + 10 - 4 * (room - target)`. The lowest cap wins, floored at the cold setpoint, rounded to 1 decimal. `requested = min(requested, cap)`.
- Computed from the formula (not stated in the code): `int(N * 0.75)` is 0, 1, 2, 3 for N = 1, 2, 3, 4, which is always the highest output. With at most 4 zones the headroom therefore never binds; it would only matter with 5 or more zones.

**Illustrative arithmetic (not from the code, computed from the formulas above, same inputs as the thermo-nova example).** Radiators, coefficient 2.0, target 20, outside 0:
- `curveValue = 20.0`, `setpoint = 27.2 + 0.5 * 20 = 37.2`.
- `kp = 2.0 * 37.2 / 3 = 24.8`, `ki = 0.0029524`, `kd = 14582.4`.
- With error +0.5: P = 12.4, I = 0 (outside the band).
- If the room rose 0.1 degree over 600 s from `_pid_rawDerivative = 0`: alpha = 600/660, `_pid_rawDerivative = -0.0001515`, D = -2.2095.
- Output = 37.2 + 12.4 + 0 - 2.2095 = 47.39 (not rounded).
- Inside the band with error +0.05, each real update adds `0.0029524 * 0.05 * 60 = 0.00886` degree to the integral.

With the firmware default coefficient of 1.5 (SATtypes.h:442) and the same inputs, the curve is `27.2 + 0.375 * 20 = 34.7` and `kp = 1.5 * 34.7 / 3 = 17.35`.

<a id="s2-constants-and-defaults"></a>

### Constants and defaults

| Name | Value | Unit | Citation |
|---|---|---|---|
| `SAT_PID_UPDATE_INTERVAL` (integral step, derivative minimum interval and filter constant) | 60 | s | SATpid.ino:22 |
| `SAT_PID_SAMPLE_TIME_LIMIT` (minimum time between real updates) | removed (TASK-1368, entry 75) | - | - |
| Error-unchanged tolerance | 0.001 | degrees | SATpid.ino:283 |
| `SAT_PID_DERIVATIVE_CAP` | 5.0 | degrees per s (raw derivative) | SATpid.ino:24 |
| `SAT_PID_DEADBAND_DEFAULT` (defined, not used by the code) | 0.1 | degrees | SATpid.ino:20 |
| `settings.sat.fDeadband` default | 0.1 | degrees | SATtypes.h:443 |
| Deadband setting range | 0.05 to 2.0 | degrees | settingStuff.ino:999 |
| Kp divisor, underfloor | 4 | none | SATpid.ino:28 |
| Kp divisor, radiators (and AUTO) | 3 | none | SATpid.ino:29, SATcontrol.ino:198-199 |
| `SAT_PID_AGGRESSION_V3` (Ki divisor) | 8400 | not stated in the code | SATpid.ino:25 |
| Kd factor | 0.07 * 8400 = 588 | not stated in the code | SATpid.ino:137 |
| Integral absolute cap | 20.0 | degrees | SATpid.ino:176 |
| Ki minimum for accumulation | 1e-9 | none | SATpid.ino:160 |
| `bAutoGains` default | true | bool | SATtypes.h:518 |
| `fKpManual` / `fKiManual` / `fKdManual` defaults | 5.0 / 0.0005 / 0.0 | none | SATtypes.h:519-521 |
| `fAutoGainsValue` default (not read by the PID) | 2.0 | none | SATtypes.h:517 |
| `fHeatingCurveCoeff` default | 2.0 (TASK-1347, entry 24) | none | SATtypes.h:443 |
| Heating curve coefficient setting range | 0.1 to 5.0 | none | settingStuff.ino:998 |
| Auto-tune coefficient range | 0.3 to 5.0 | none | SATcontrol.ino:4475-4476 |
| Heating system default | AUTO, treated as radiators | enum | SATtypes.h:438, SATcontrol.ino:198-199 |
| Heating curve base offset, underfloor | 20.0 | degrees | SATcontrol.ino:49 |
| Heating curve base offset, radiators | 27.2 | degrees | SATcontrol.ino:50 |
| Heating curve reference temperature | 20.0 | degrees | SATcontrol.ino:51 |
| Control interval default | 30 | s | SATtypes.h:444 |
| Control interval range | 10 to 300 | s | settingStuff.ino:1001 |
| Room EMA time constant | 45 | s | SATcontrol.ino:4695 |
| `SAT_MIN_SETPOINT` | 10.0 | degrees | SATcontrol.ino:54 |
| Cold setpoint, radiators | 28.2 | degrees | SATcontrol.ino:55 |
| Cold setpoint, underfloor | 21.0 | degrees | SATcontrol.ino:56 |
| System maximum, radiators / underfloor / heat pump source | 62 / 45 / 40 | degrees | SATcontrol.ino:231-233 |
| Default when boiler MaxTSet is below 30 | 75.0 | degrees | SATcontrol.ino:57, SATcontrol.ino:243 |
| Hard maximum, radiators / underfloor | 80 / 50 | degrees | SATcontrol.ino:62-63 |
| `fMaxSetpoint` default (global ceiling) | 65.0 | degrees | SATtypes.h:514 |
| `bSolarGainEnable` default | false | bool | SATtypes.h:494 |
| `fSolarSetpointOffset` default | 2.0 | degrees | SATtypes.h:496 |
| Solar gain on / off hold | 600 | s | SATcontrol.ino:4276, SATcontrol.ino:4289 |
| `iSensorMaxAgeS` default (MQTT external room sensor only) | 21600 | s | SATtypes.h:515, SATcontrol.ino:1225 |
| BLE / Tr room temperature stale limit | 300 | s | SATcontrol.ino:65-66 |
| Invalid room temperature ticks before safety trip | none (TASK-1364: curve only, no trip) | | |
| PID state file | `/sat_pid_state.json` | none | SATcontrol.ino:1988 |
| PID state maximum age on restore | 1800 | s | SATcontrol.ino:1993 |
| PID state periodic save | 300 | s | SATcontrol.ino:5010 |
| `iZoneCount` default | 1 | zones | SATtypes.h:531 |
| `fZoneAggregationHeadroom` default (most the zones may ask above their P75 output) | 5.0 | degrees | SATtypes.h:552 |
| Zone P75 fraction | 0.75 | fraction | SATcontrol.ino:2360 |
| Zone overshoot margin / cooling headroom / cooling slope | 0.3 / 10.0 / 4.0 | degrees, degrees, degrees per degree | SATcontrol.ino:2335-2337 |
| Zone timeout default | 300 | s | SATtypes.h:532 |
| Thermal estimation deadband widening | 0.5 | degrees per hour | SATcontrol.ino:4750 |
| `iHeatingMode` default (not used by control) | 0 (COMFORT) | enum | SATtypes.h:524 |

<a id="s2-state-kept-between-passes"></a>

### State kept between passes

| State | Meaning | Persisted | Cleared by `satPidReset()` | Citation |
|---|---|---|---|---|
| `_pid_integral` | Accumulated integral (already multiplied by Ki) | yes, as `"i"` (from `state.sat.fPidI`) | yes (0.0) | SATpid.ino:34, SATpid.ino:98, SATcontrol.ino:2001-2002 |
| `_pid_rawDerivative` | Filtered temperature slope (degrees per s, negated) | yes, as `"rd"` | yes (0.0) | SATpid.ino:35, SATpid.ino:99, SATcontrol.ino:2001-2002 |
| `_pid_lastError` | Error held by the last PID step; `satPidOutput()` multiplies it by Kp. NAN means no held error (room temperature lost): the output is the curve alone | no (`"err"` is saved but restores only the display value `fError`) | yes (0.0) | SATpid.ino:31, SATpid.ino:105, SATpid.ino:137, SATpid.ino:350, SATcontrol.ino:2039 |
| `_pid_lastUpdateMs` | removed with the 10 s and 60 s gates (TASK-1368); the 60 s cadence is the timer `timerSATPid` | - | - | - |
| `_pid_lastDerivativeMs` | Start of `deltaTime` | no | set to now, then overwritten by the init block | SATpid.ino:39, SATpid.ino:103, SATpid.ino:270 |
| `_pid_derivRefTemp` | Room temperature at `_pid_lastDerivativeMs` | no | no (set by the init block) | SATpid.ino:56, SATpid.ino:271 |
| `_pid_roomTempSeen`, `_pid_roomTempChangedMs` | Last room temperature seen and when it first appeared | no | no | SATpid.ino:42-47, SATpid.ino:258-261 |
| `_pid_initialized` | Initialisation done | no | yes (false) | SATpid.ino:40, SATpid.ino:104 |
| `_pid_previousError`, `_pid_lastRoomTemp`, `_pid_lastCurveValue` | Written, never read | no | yes | SATpid.ino:33, SATpid.ino:36-37, SATpid.ino:313-316 |
| Zone `fPidIntegral`, `fPrevError` | Per-zone integral and last error | no persistence found | cleared when the zone is excluded | SATcontrol.ino:1790-1796, SATcontrol.ino:1845-1853 |

**When the state is saved:**
- every 300 s at the end of a full control tick (SATcontrol.ino:5009-5012);
- in `satPersistAllToFs()` (SATcontrol.ino:2128-2131);
- after `satDisable()` (SATcontrol.ino:2171);
- after `satFlushShortLivedData()` (SATcontrol.ino:2197).

The saved integral is `state.sat.fPidI`, the published copy, not `_pid_integral` itself (SATcontrol.ino:2001-2002).

**When the state is restored:** at `initSAT()` (SATcontrol.ino:3684), and once on the first control pass after NTP is set (SATcontrol.ino:4515-4519). Only the integral and the raw derivative are restored into the controller (SATpid.ino:77-78).

**When the PID is reset, or the integral cleared:**

| Trigger | Effect | Citation |
|---|---|---|
| Boot (`initSAT`) | Full `satPidReset()`, then restore from file if fresh | SATcontrol.ino:3683-3684 |
| `satDisable()`: SAT switched off, fallback lifted, safety trip or OT command failures | Full `satPidReset()`, then zeros saved | SATcontrol.ino:2170-2171, SATcontrol.ino:4561, SATcontrol.ino:4569-4571, SATcontrol.ino:4680, SATcontrol.ino:5002 |
| Flush command (MQTT `sat/flush` or REST `POST /api/v2/sat/flush`) | Full `satPidReset()`, then zeros saved | SATcontrol.ino:2189-2197, restAPI.ino:1377-1380 |
| `reset_integral` command (MQTT, REST `/api/v2/sat/reset_integral`, and the settings-style REST path) | Integral only: `_pid_integral = 0`; the raw derivative is kept | SATpid.ino:86-91, MQTTstuff.ino:620, MQTTstuff.ino:672, restAPI.ino:1372-1375, restAPI.ino:1624-1626 |
| Window open longer than `iWindowMinOpenSec` (target switches to the Activity preset) | Integral only | SATcontrol.ino:1126-1142 |
| Window closed after that (target restored) | Integral only | SATcontrol.ino:1109-1121 |
| Preset change, or preset cleared with a saved target | Display only: `state.sat.fPidI = 0`. `_pid_integral` is not touched, so the next real update writes the old value back to `fPidI`. A search for `_pid_` in all firmware sources outside SATpid.ino found only a comment (SATcontrol.ino:2040) and the generated, untracked build file `OTGW-firmware.ino.cpp`, which is a copy of the same code | SATcontrol.ino:996-999, SATcontrol.ino:1004-1006, SATpid.ino:298, SATpid.ino:303 |
| Target temperature changed (MQTT/REST `target`, settings) | None | SATcontrol.ino:1903-1934, settingStuff.ino:997 |
| Target adopted from the thermostat (follow thermostat) | None | SATcontrol.ino:1034-1045 |
| Heating mode (ECO/COMFORT) changed | None | SATcontrol.ino:2211-2221 |
| Stale room sensor | A stale source is skipped and the next source is used; if none is valid the room temperature counts as lost (TASK-1364): `satPidCurveOnly()` zeroes the integral and the raw derivative each tick, no trip | SATcontrol.ino:1208-1231, SATcontrol.ino:1242-1245 |
| Zone excluded (zone OFF, stale, invalid input) | That zone's integral and last error cleared | SATcontrol.ino:1790-1796, SATcontrol.ino:1814-1825 |

Window detection only acts when `bWindowDetection` is on (SATcontrol.ino:1100), default false (SATtypes.h:463).

Computed from the code: if a periodic save happens between a preset change and the next real PID update, the file gets `"i": 0` while `_pid_integral` still holds the old value (SATcontrol.ino:998, SATcontrol.ino:2001-2002, SATcontrol.ino:5010-5011).

**When the integral is frozen:** on a real update while `state.sat.bSolarGainActive` and `bSolarFreezeIntegral` are true, whatever the error; the integral is neither accumulated nor reset (SATpid.ino, TASK-1323). Solar gain needs `bSolarGainEnable`, default false (SATcontrol.ino:4217-4223, SATtypes.h:494). So with default settings the integral is never frozen. The zone PID has no freeze (SATcontrol.ino:1845-1853).

<a id="s2-outputs-to-the-boiler-to-entities"></a>

### Outputs (to the boiler, to entities)

**To the boiler:** the PID sends nothing directly. Its output becomes `pidOutput` in the control tick (SATcontrol.ino:4839). That value is clamped, may switch PWM to continuous, and goes through `satApplyPWM` or `satApplyContinuous` to `finalSetpoint` (SATcontrol.ino:4906-4937). The commands are queued write-on-change, that is only when the value changed (SATcontrol.ino:4979-4983). The CS override is also kept alive between control ticks by `satPicCSKeepalive()` (SATcontrol.ino:4607-4608); its details belong to the heating control section. The commands are:
- `CS=finalSetpoint`;
- `MM=iCurrentModulation`;
- `CH=0` when the requested setpoint is at or below the cold setpoint or PWM's OFF phase drives the CS, else `CH=1` (`satChEnableDecide()`).

When `pidOutput` is at or below the cold setpoint (28.2 radiators, 21.0 underfloor), `finalSetpoint` is forced to 10.0, so `CH=0` (SATcontrol.ino:4969-4975). The control setpoint path itself belongs to the heating control and PWM sections.

**To entities (MQTT topics and REST):**

| Topic / field | Value | Citation |
|---|---|---|
| `sat/pid_output` | `state.sat.fPidOutput` (main PID, before zone aggregation and clamps), 1 decimal | SATcontrol.ino:2733 |
| `sat/error`, `sat/pid_p`, `sat/pid_i`, `sat/pid_d` | `fError`, `fPidP`, `fPidI`, `fPidD`, 2 decimals | SATcontrol.ino:2737-2740 |
| `sat/pid_attributes` | JSON with `error`, `proportional`, `integral`, `derivative` | SATcontrol.ino:2742-2749 |
| `sat/raw_derivative` | `fRawDerivative`, 4 decimals | SATcontrol.ino:2751 |
| `sat/kp`, `sat/ki`, `sat/kd` | Current gains | SATcontrol.ino:2822-2824 |
| `sat/heating_curve` | `state.sat.fHeatingCurveValue` | SATcontrol.ino:2732 |
| `sat/requested_setpoint` | `fPidOutput` clamped to [10.0, `satGetMaxSetpoint()`] | SATcontrol.ino:3053-3058 |
| `sat/setpoint` | `fFinalSetpoint` | SATcontrol.ino:2731 |
| REST/status JSON `heating_curve`, `pid_output` | Same state values | SATcontrol.ino:2354-2355, restAPI.ino:2393-2394 |

Computed from the code: `satCalcHeatingCurve` returns the curve and writes no state (SATcontrol.ino:591). The control loop stores the main curve, from the main effective target, in `state.sat.fHeatingCurveValue` (SATcontrol.ino:5069). The zone steps compute their own curves from the zone targets without touching it (SATcontrol.ino:2026), so `sat/heating_curve` is the main curve also with zones on (TASK-1353; before that it was the last zone's curve). Thermo-nova does the same: the climate publishes its own curve (climate.py:249, :697; sensor.py:291) and each area keeps a separate one (area.py:49, :262).

<a id="s2-edge-cases"></a>

### Edge cases

1. **No heating curve value.** The firmware always computes a curve value; there is no "no value" branch (SATcontrol.ino:485-500). An outside temperature outside -40 to 50 is replaced by 10.0 before the curve (SATcontrol.ino:4721-4723).
2. **Error exactly 0.1.** The error is rounded to 3 decimals so that 0.1 stays inside the band (SATpid.ino:58-66). The integral uses `>` to reset and the derivative uses `<=` to hold (SATpid.ino:154, SATpid.ino:199).
3. **Error leaves the band.** The integral drops to 0 on the next real update (SATpid.ino:154-157). There is no gradual decay.
4. **Frozen integral with a large error.** The freeze check runs before the deadband test, so while solar gain is active and `bSolarFreezeIntegral` is on the integral keeps its value even outside the deadband, as thermo-nova does (SATpid.ino:157-163; pid.py:167-168, pid.py:187-189) (corrected 2026-10-24, TASK-1351); TASK-1323, entry 11 answer 3.
5. **Integral clamp moves with the curve.** The ±curveValue limit is applied only during an in-band accumulation (SATpid.ino:166-173).
6. **Derivative inside the band.** `_pid_rawDerivative` is held, so D keeps its last value while the error is within the deadband (SATpid.ino:199-204, SATpid.ino:299).
7. **Room temperature unchanged.** A change below 0.001 moves the timestamp and reference and keeps `_pid_rawDerivative` (SATpid.ino:208-213).
8. **Large slope.** At `|rawDeriv| >= 5.0` the filter is bypassed and the value is clamped to ±5.0 (SATpid.ino:226-233).
9. **Stale or missing room temperature.** (TASK-1364) The PID service and the tick call `satPidCurveOnly()` instead of stepping and reading the PID: integral, raw derivative, P, I, D and the published error are 0 and the output is the heating curve. No trip. After recovery the integral restarts from 0; the first D is computed over the whole gap from the last pre-loss temperature (as pid.py:209-232).
10. **Integral time step is fixed.** The integral always adds `ki * error * 60`, whatever the control interval (10 to 300 s) or the actual time since the last update (SATpid.ino:166, settingStuff.ino:1001).
11. **Manual gains.** No setting, REST or MQTT path that writes `bAutoGains` or the manual gains was found, so the manual branch is not reachable at run time (see "Not found / unclear").
12. **Thermal comfort on.** The error is computed against the Summer Simmer index only when the room temperature comes from OT Tr and the humidity is fresh (SATcontrol.ino:1264-1270).
13. **SAT disabled.** The PID does not tick (SATcontrol.ino:4568-4573), and `satDisable()` resets it (SATcontrol.ino:2170).
14. **First tick after boot or reset.** The PID returns the stored 0.0, and the boiler is commanded off for one or two ticks (section B, step 7).
15. **Deadband during thermal estimation.** The deadband grows by 0.5 per hour of estimation for that tick, which affects both the integral and derivative band tests (SATcontrol.ino:4747-4751, SATpid.ino:151, SATpid.ino:189).
16. **Single counted zone.** It no longer replaces the main PID output. The requested setpoint is the maximum of the main PID output and the zones output, so a single zone can only raise it (SATcontrol.ino:2367, thermo-nova climate.py:385-386; changed in TASK-1440, the old replace-the-primary behaviour is gone).

<a id="s2-not-found-in-the-code-unclear"></a>

### Not found in the code / unclear

1. **`bAutoGains` and the manual gains have no writer.** A case-insensitive search of the firmware sources for `autogains`, `auto_gains`, `KpManual`, `kp_manual`, `SATkp`, `SATki`, `SATkd` and assignments to these fields found only the defaults (SATtypes.h:518-521), the reads (SATpid.ino:122-126) and read-only publishing (SATcontrol.ino:2527-2530, restAPI.ino:2312, handleDebug.ino:116). The setting named `SATautogains` writes `fAutoGainsValue`, not `bAutoGains` (settingStuff.ino:1103). So the PID always uses automatic gains.
2. **`fAutoGainsValue` is never read by the PID.** It is stored, settable and published (SATtypes.h:517, settingStuff.ino:1103, SATcontrol.ino:2977). The search found no use in SATpid.ino. This matches thermo-nova unclear item 1.
3. **`boilerTemp` parameter was unused.** `satPidUpdate` received `satGetFlowTemp()` and never read it; the parameter went away with `satPidUpdate` (TASK-1368).
4. **Unused tracking values.** `_pid_previousError`, `_pid_lastRoomTemp` and `_pid_lastCurveValue` are written (SATpid.ino:266-268, SATpid.ino:313-316) but never read. A search for `_pid_` in all firmware sources outside SATpid.ino found no other use (only the comment at SATcontrol.ino:2040 and the generated, untracked `OTGW-firmware.ino.cpp`). `SAT_PID_DEADBAND_DEFAULT` is defined but not used (SATpid.ino:20).
5. **ECO / COMFORT heating mode has no effect on control.** `iHeatingMode` is set by `satHandleHeatingMode` and the settings parser (SATcontrol.ino:2211-2221, settingStuff.ino:1104). In the SAT files it is only read for publishing (SATcontrol.ino:2533, SATcontrol.ino:2978). No ECO branch was found in the requested setpoint path (SATcontrol.ino:4839-4946).
6. **Preset change "integral reset" is display only.** The comments say the integral is reset (SATcontrol.ino:997, SATcontrol.ino:1006), but only `state.sat.fPidI` is written. The controller value `_pid_integral` is a file-level static in SATpid.ino (SATpid.ino:34). Outside SATpid.ino, no firmware source writes it: the `_pid_` search found only the comment at SATcontrol.ino:2040 and the generated `OTGW-firmware.ino.cpp`. The code does not show whether a real reset was intended.
7. **Late NTP restore.** The one-time restore at the first NTP-synced pass can overwrite a live integral and raw derivative (SATcontrol.ino:4515-4519, SATpid.ino:77-78). This needs NTP to arrive before this boot's first PID state save (section B, step 5). What `time(nullptr)` returns before NTP sync was not checked for this section. The code does not state whether the overwrite is intended.
8. **Default differences versus thermo-nova, stated without judgement:**
   - Heating curve coefficient default 2.0 since TASK-1347 (entry 24), as thermo-nova; stored values are kept and the setup wizard no longer writes a per-system slope.
   - Coefficient range 0.1 to 5.0 (settingStuff.ino:998); thermo-nova uses 0.1 to 12.
   - System maximum 62 radiators / 45 underfloor (SATcontrol.ino:231). The code comment explains the 62 (SATcontrol.ino:218-227).
   - Cold setpoint 28.2 / 21.0 (SATcontrol.ino:55-56); thermo-nova uses 22.
   - The firmware does not round the curve, gains, P, I, D or output. Only the error is rounded (SATpid.ino:63-66, SATcontrol.ino:496, SATpid.ino:135-137, SATpid.ino:297-309).
9. **Outside temperature `NaN`.** The range check at SATcontrol.ino:4721 does not catch `NaN`, because comparisons with `NaN` are false. Whether `OTcurrentSystemState.Toutside` or the other outside sources can be `NaN` was not checked for this section (SATcontrol.ino:1283-1303).
10. **Zone overshoot cap.** Ported from thermo-nova area.py:303-333 (TASK-1440): every counted zone more than 0.3 °C above its target gives `cold + 10 - 4 * degrees over`, the lowest wins, floored at the cold setpoint, and the requested setpoint is `min(requested, cap)` (SATcontrol.ino:2335-2337, SATcontrol.ino:2342-2350, SATcontrol.ino:2368-2372). The old rule (subtract the largest overshoot 1:1, no margin, floor 10.0) is gone. The cold setpoint is the per-system one (ADR-199), not thermo-nova's 22.
11. **Integral rate at control intervals other than 60 s.** The fixed 60 s step (SATpid.ino:166), combined with the 10 s and 60 s gates (SATpid.ino:276-285), means the integral gain per minute depends on the control interval and on how often the error changes. The code does not state whether this is intended.
12. **Units of Ki and Kd.** The code does not name units. The origin of 8400 and 0.07 is not explained (SATpid.ino:25, SATpid.ino:137).
13. **Integral reset on a target change.** Thermo-nova resets the PID on a target change. No such reset was found in the firmware target paths (SATcontrol.ino:1903-1934, SATcontrol.ino:1034-1045, settingStuff.ino:997). The window paths are the exception: they clear the integral when they change the target (SATcontrol.ino:1119, SATcontrol.ino:1139).

---

<a id="s3"></a>

## Heating curve and setpoint selection

Source: OTGW-firmware, branch `dev`, git HEAD 51df6b486. All cited files are in `src/OTGW-firmware/`, except `boards.h`, which is `src/libraries/Platform/src/boards.h`. A bare line number like `4836` (in code blocks and in lists after a `SATcontrol.ino:` citation) always means `SATcontrol.ino:4836`. This section describes what the firmware does, not what it should do. Headings and topic order follow the thermo-nova section (a3-curve-setpoint-thermonova.md) so the two can be compared line by line. Anything the code does not show is listed in the last subsection.

<a id="s3-purpose"></a>

### Purpose

The firmware works out a boiler flow temperature in the same three layers, but all three run inside one periodic control tick (SATcontrol.ino:4510-5019):

1. **Heating curve.** A weather-compensation value from the effective room target and the outside temperature (SATcontrol.ino:485-500, SATcontrol.ino:4836).
2. **Requested setpoint.** There is no separate function for it. It is the local variable `pidOutput`: the curve plus the PID correction (SATcontrol.ino:5538, SATpid.ino:309), raised to the zones output and limited by the zone overshoot cap (`satAggregateZoneSetpoint()`, SATcontrol.ino:5540-5561, TASK-1440), then clamped to [10, effective maximum] (SATcontrol.ino:5583-5585).
3. **Control setpoint.** What is sent to the boiler as `CS=`. It is derived from `pidOutput` by the continuous rule or the PWM override (SATcontrol.ino:4928-4933), clamped again (SATcontrol.ino:4936-4937) and forced to 10 °C by the cold-setpoint cutoff; solar gain lowers `pidOutput` before this (TASK-1323), not the control setpoint (SATcontrol.ino:4971-4975).

The order inside one tick is: curve, PID, zone block (raise to the zones output, overshoot cap), solar reduction, clamp, mode (continuous or PWM), final clamp, auto-switch, modulation, cold cutoff, send (SATcontrol.ino:5414-5418, 5423-5486, 5488-5494, 5496-5511, 5513-5534, 5536-5546, 5552-5560, 5562-5570, 5572-5579). The solar reduction is applied to the requested setpoint before the clamp, as in thermo-nova (climate.py:391-394) (corrected 2026-10-24, TASK-1351); TASK-1323.

This section covers layers 1 and 2 and the continuous-mode part of layer 3. The PWM override is only summarised (see "Cross-reference: PWM override").

<a id="s3-inputs"></a>

### Inputs

| Input | Where it comes from | Citation |
|---|---|---|
| Room target temperature | `settings.sat.fTargetTemp`, default 20 °C, accepted range 5 to 30 °C. Set by the MQTT SAT sub-command `target`, by REST, by a preset, by the window-open timer, or by the "follow thermostat" option. Persisted in the settings file under `SATtargettemp` | SATtypes.h:441, settingStuff.ino:997, MQTTstuff.ino:612, MQTTstuff.ino:657, SATcontrol.ino:1903-1934, restAPI.ino:1289, SATcontrol.ino:976-1024, SATcontrol.ino:1126-1143, SATcontrol.ino:1034-1093, settingStuff.ino:319 |
| Effective target (what the curve and PID use) | `targetTemp`, plus `fPvBoostAppliedC` while the PV boost is active | SATcontrol.ino:4831-4833 |
| Room (inside) temperature | `satGetRoomTemp()`, first usable source in this order: simulation, BLE sensor, multi-area weighted average, external MQTT value, OpenTherm MsgID 24 (`Tr`). With thermal comfort on, the Summer Simmer index replaces `Tr` (only on the `Tr` path). The control tick then smooths it with an EMA, tau 45 s | SATcontrol.ino:1192-1280, SATcontrol.ino:1264-1270, SATcontrol.ino:4694-4713 |
| Outside temperature | `satGetOutsideTemp()`: simulation, external MQTT value (sub-command `outdoor_temp`), weather API when `Toutside` is exactly 0.0, otherwise OpenTherm MsgID 27 `Toutside` | SATcontrol.ino:1283-1303, MQTTstuff.ino:614, MQTTstuff.ino:659 |
| Heating system | `settings.sat.iHeatingSystem`: 0 auto, 1 radiators, 2 underfloor. Auto is treated as radiators | SATtypes.h:41-43, SATtypes.h:438, settingStuff.ino:994, SATcontrol.ino:194-201 |
| Heating source | `settings.sat.iHeatingSource`: auto, gas boiler, heat pump, hybrid. Auto is treated as gas boiler. It changes the maximum setpoint, not the curve | SATtypes.h:48-51, SATtypes.h:439, SATcontrol.ino:209-214, SATcontrol.ino:233 |
| Heating curve coefficient | `settings.sat.fHeatingCurveCoeff`, default 2.0 (TASK-1347; 1.5 before), accepted range 0.1 to 5.0. Read on every curve calculation. Auto-tune can change it | SATtypes.h:442, settingStuff.ino:998, SATcontrol.ino:488, SATcontrol.ino:4444-4485 |
| PID terms (P, I, D) | `satPidStep()` on the 60 s timer, fed with the smoothed room temperature and the effective target by `satPidGatherInputs()`; `satPidOutput()` on every tick | SATcontrol.ino, SATpid.ino |
| Zones (secondary rooms) | Up to 4 zones, each with a room temperature and a setpoint pushed over MQTT. Every configured zone (`iZoneCount`, default 1, range 1 to 4) runs its PID each tick. A zone without usable inputs does not count, so with no zone data pushed the zone block changes nothing | SATcontrol.ino:2158-2161, SATcontrol.ino:2185-2216, MQTTstuff.ino:895, SATtypes.h:550, settingStuff.ino:1112, SATcontrol.ino:5540-5561 |
| Heating mode | `settings.sat.iHeatingMode` (0 comfort, 1 eco). Stored and published. The control path reads it only in the zone block, where ECO skips the zone aggregation (`satAggregateZoneSetpoint()`, SATcontrol.ino:2333); the main PID is the same in both modes (see "Not found") | SATtypes.h:524, settingStuff.ino:1104, SATcontrol.ino:2211-2221 |
| Solar gain state | `state.sat.bSolarGainActive` from `satUpdateSolarGain()`, and the offset `fSolarSetpointOffset` | SATcontrol.ino:4215-4299, SATtypes.h:496 |
| Maximum setpoint | Three different values are used (see Formulas): the effective maximum `satEffectiveMaxSetpoint()`, the system cap `satGetMaxSetpoint()`, and the per-system hard ceiling | SATcontrol.ino:228-252 |
| Boiler maximum (MaxTSet) | OpenTherm MsgID 57, an input to the effective maximum | OTGW-Core.ino:4294, SATcontrol.ino:242-243 |
| Boiler flow temperature | OpenTherm MsgID 25 `Tboiler` | SATcontrol.ino:1325-1329 |
| Flame state | OpenTherm status flags bit 3 | SATcontrol.ino:1337-1341 |
| Valve state | `state.sat.bValvesOpen`, default true, set only by the MQTT SAT sub-command `valves_open` | SATtypes.h:243, MQTTstuff.ino:634-638, MQTTstuff.ino:671 |
| SAT on or off (the HVAC mode equivalent) | `settings.sat.bEnabled`, default false, persisted | SATtypes.h:437, settingStuff.ino:315 |
| DHW active | OpenTherm slave status bit 2 | SATcontrol.ino:4628 |

**Outside temperature selection rule.** In simulation mode the simulated value is used (SATcontrol.ino:1286-1288). Otherwise an external MQTT value is used while it is younger than `SAT_STALE_OUTDOOR_MS` = 10 min; when older it is marked invalid (SATcontrol.ino:1289-1297, SATcontrol.ino:67). The external value must be between -50 and 100 °C to be accepted (SATcontrol.ino:1624). Next, the weather API temperature is used, but only when weather data is valid (`state.sat.weather.bValid`) and `OTcurrentSystemState.Toutside` is exactly 0.0 (SATcontrol.ino:1299-1301). Otherwise `Toutside` is returned (SATcontrol.ino:1302). `Toutside` starts at 0.0 (OTGW-Core.h:82). There is no "no outside temperature" value: if no source ever reported, the value 0.0 is used. In the control tick any value below -40 or above 50 °C is replaced by 10 °C (SATcontrol.ino:4721-4723).

**Staleness rules.** Each source has its own age limit:

| Source | Limit | Citation |
|---|---|---|
| BLE room sensor | 5 min | SATcontrol.ino:65, SATcontrol.ino:1210 |
| External MQTT room temperature | `iSensorMaxAgeS`, default 21600 s (6 h), range 60 to 86400 s | SATtypes.h:515, settingStuff.ino:1101, SATcontrol.ino:1225 |
| OpenTherm `Tr` from a thermostat | 5 min after the last thermostat report | SATcontrol.ino:66, SATcontrol.ino:1242-1245 |
| Multi-area sensors | 5 min | SATcontrol.ino:87, SATcontrol.ino:1707 |
| External MQTT outside temperature | 10 min | SATcontrol.ino:67, SATcontrol.ino:1291 |
| Zones | `iZoneTimeoutS`, default 300 s, range 30 to 3600 s | SATtypes.h:532, settingStuff.ino:1113, SATcontrol.ino:1818 |
| Weather API value | No expiry found (see "Not found") | SATweather.ino:376 |

<a id="s3-behaviour-step-by-step-in-time-order"></a>

### Behaviour (step by step, in time order)

#### At start-up

1. `initSAT()` is called from setup (OTGW-firmware.ino:567). It resets the PID (SATcontrol.ino:3683, SATpid.ino:94-114).
2. It tries to restore the PID state from `/sat_pid_state.json` (SATcontrol.ino:3684, SATcontrol.ino:1988). The restore is skipped when NTP time is not yet set (SATcontrol.ino:2027-2031). It is discarded when the file is older than `SAT_PID_STALE_SEC` = 1800 s (SATcontrol.ino:1993, SATcontrol.ino:2033-2037). Otherwise the integral and raw derivative are loaded into the PID (SATcontrol.ino:2042, SATpid.ino:75-83).
3. Energy and heating-curve-recommendation state are restored, cycle tracking and PWM state are reset, the control mode is set to OFF and the command cache is cleared (SATcontrol.ino:3685-3700).
4. If an OpenTherm command interface exists, `CS=0` is sent to release any stale control setpoint (SATcontrol.ino:3706-3710). If not, `CS=0` is sent on the first loop pass that has one (SATcontrol.ino:4585-4589).
5. The control timer is set to `iControlInterval` (SATcontrol.ino:3719). That sets the first due time to now plus one interval (safeTimers.h:131-135), so the first control tick (and the first curve calculation) comes one interval after start-up, 30 s by default (SATtypes.h:444).
6. If SAT is enabled, the mode becomes continuous and `PM=3`, `PM=15`, `PM=48` are queued, plus `MI=500` for boilers with that quirk (SATcontrol.ino:3721-3734).
7. The room target is not restored from a runtime state: it is a setting, written to the settings file (settingStuff.ino:319) and read back at boot through the same key parser (settingStuff.ino:509, settingStuff.ino:555, settingStuff.ino:997).
8. Until the first tick the curve value is 0.0 (SATtypes.h:154).
9. On every loop pass before the timer check, the PID restore is retried once when NTP has become available (SATcontrol.ino:4515-4519).

#### When the heating curve is recalculated

The curve is recalculated on every control tick that passes the early-exit gates (SATcontrol.ino:4836). There is no event-driven recalculation: a target change or an outside temperature change only takes effect at the next tick. A grep for `satCalcHeatingCurve` over all `*.ino`, `*.h` and `*.cpp` files in `src/OTGW-firmware/` (ignoring the generated `OTGW-firmware.ino.cpp` build copy) found only two callers: the control tick (SATcontrol.ino:4836) and the zone step (SATcontrol.ino:1828).

The curve is **not** recalculated, and `state.sat.fHeatingCurveValue` keeps its old value, on ticks that return early:

- DHW active (SATcontrol.ino:4865-4882 [lines refreshed 2026-10-24, TASK-1351]);
- boiler fault flag set (SATcontrol.ino:4716-4719);
- thermal safe fallback active (SATcontrol.ino:4730-4746);
- summer simmer active (SATcontrol.ino:4766-4776);
- valves closed (SATcontrol.ino:4779-4790).

When zones are used, every active zone also calls `satCalcHeatingCurve()` with its own target (SATcontrol.ino:1828), and that function writes `state.sat.fHeatingCurveValue` each time (SATcontrol.ino:498). The zone calls come after the main call (SATcontrol.ino:4836, SATcontrol.ino:4854). With `iZoneCount` > 1 the published `sat/heating_curve` (SATcontrol.ino:2732) therefore shows the curve of the last zone that reached line 1828, not the main curve. The main PID uses its own local copy (SATcontrol.ino:4836-4839).

#### Periodic timers

- **Control tick:** every `iControlInterval` seconds, default 30, range 10 to 300 (SATtypes.h:444, settingStuff.ino:1000-1003). The timer is declared `CATCH_UP_MISSED_TICKS` (SATcontrol.ino:582) but runs as `SKIP_MISSED_TICKS` (item 16 of the deviation list; `initSAT()` and `settingStuff.ino:1005` set the interval without a type), so ticks are never closer than one interval.
- **PID update:** no timer of its own. It runs inside the control tick (SATcontrol.ino:4839) with its own rate limits (see next subsection).
- **PWM suppression refresh:** every 3 s, between ticks, PWM only (SATcontrol.ino:480, SATcontrol.ino:894-918).
- **PIC CS keepalive:** every 30 s on PIC boards (SATcontrol.ino:403-414, SATcontrol.ino:4608).
- **Zones:** no timer of their own. For every configured zone (also at `iZoneCount` 1), each zone PID steps on the 60 s PID timer (`satZonePidAdvanceAll()`, called by `satPidService()`) and the tick only reads the zone outputs (`satZonePidOutput()`).

#### PID step (on its own 60 s timer, TASK-1368)

The PID steps on its 60 s timer with a valid room temperature. There is no stale-sensor reset step inside `satPidStep()`. A lost room temperature is handled before it: the PID service and the control tick call `satPidCurveOnly()` instead (TASK-1364), which sets the integral, the raw derivative, P, I, D and the published error to 0 and returns the heating curve value. `_pid_lastError` is set to NAN, which `satPidOutput()` reads as "no held error": the output is the curve alone, never NaN, until the next step stores a real error.

1. The error is `target - room`, rounded to 3 decimals (SATpid.ino:63-66, SATpid.ino:253).
2. On the first step after a reset the tracking variables are initialised (`satPidStep()`).
3. There is no time gate and no unchanged-error gate (entries 42 and 75): every call of `satPidStep()` moves the state once, and `satPidService()` calls it at start-up and then once per 60 s.
4. The control tick does not step the PID. It reads `satPidOutput()`, which uses the current curve, so a new curve value reaches the output on the next tick while the error, integral and raw derivative stay as the last step left them.
5. Gains are recomputed from the current curve value (SATpid.ino:288, SATpid.ino:120-142).
6. The integral and the derivative are updated (SATpid.ino:291-294). The integral update is skipped entirely, reset included, while solar gain is active and `bSolarFreezeIntegral` is on (SATpid.ino:157) (corrected 2026-10-24, TASK-1351); TASK-1323.
7. Output (`satPidOutput()`) = curve + P + I + D, not rounded; D is 0 on the first output after a reset.

PID internals belong to the PID section. In short: with automatic gains (default on, SATtypes.h:518), `kp = fHeatingCurveCoeff * curveValue / 4` for underfloor and `/ 3` otherwise (SATpid.ino:130-135), `ki = kp / 8400` (SATpid.ino:136), `kd = 0.07 * 8400 * kp` (SATpid.ino:137). The integral is reset to 0 whenever |error| > `fDeadband` (default 0.1, SATtypes.h:443, SATpid.ino:154-156). Inside the deadband it adds `ki * error * 60` per update (SATpid.ino:166), clamped to plus or minus the curve value and to plus or minus 20 (SATpid.ino:172-178).

**Zone PID** (SATcontrol.ino:1806-1867). A zone is excluded (output 10, integral cleared) when it is off, has no valid room or setpoint, is older than `iZoneTimeoutS`, or has a room outside -10 to 50 °C or a setpoint outside 5 to 30 °C (SATcontrol.ino:1790-1796, SATcontrol.ino:1814-1825). Otherwise it computes its own curve (SATcontrol.ino:1828), `kp` with the same divisor rule and `ki = kp / 8400` (SATcontrol.ino:1834-1837), an integral with the same deadband, clamp and 20 °C cap (SATcontrol.ino:1845-1853), and output `curve + P + I` (no D term) clamped to [10, system cap] (SATcontrol.ino:1855-1861). It runs on every tick for every configured zone, also at `iZoneCount` 1 (SATcontrol.ino:5540-5561). Derived from line 1846 and the 30 s default tick: the zone integral step is sized for 60 s but is applied every 30 s.

#### Control loop pass (every `iControlInterval`, 30 s by default)

Before the timer check, every loop pass handles the fallback enable, the disabled and safety-tripped exits, the deferred boot `CS=0`, the PIC keepalive and the PWM refresh (SATcontrol.ino:4537-4611). Then (SATcontrol.ino:4614):

1. **Mark active.** If the mode is OFF it becomes continuous (SATcontrol.ino:4616-4619).
2. **DHW.** If DHW is active, `SW=` is sent on entry when configured, and the tick ends. No CS, MM or CH is sent (SATcontrol.ino:4851-4883 [lines refreshed 2026-10-24, TASK-1351]).
3. **Read inputs**. If the room temperature is NaN or outside -10 to 50 °C the tick does not end any more (TASK-1364): it runs on the heating curve alone (see step 4 in the Phase E list). No skip count, no trip.
4. **Smooth the room temperature** (EMA, tau 45 s) (SATcontrol.ino:4694-4713).
5. **Boiler fault flag** set: the tick ends (SATcontrol.ino:4716-4719).
6. **Outside temperature** out of range becomes 10 °C (SATcontrol.ino:4721-4723).
7. **Thermal safe fallback** (only when the fallback mode is active and the room estimate is 2 h old): `CS` = 45 °C capped at the system cap and truncated to a whole degree, `CH=1`, tick ends (SATcontrol.ino:4730-4746, SATcontrol.ino:82-83).
8. **Window timer, pressure, solar gain** are updated (SATcontrol.ino:4755-4762).
9. **Summer simmer active:** `CS=10`, `CH=0`, tick ends (SATcontrol.ino:4765-4776).
10. **Valves closed:** `CS=10`, `CH=0`, tick ends (SATcontrol.ino:4779-4790).
11. **Housekeeping:** auto-tune, both curve recommendations, modulation reliability, setpoint sync check, flame and boiler status (SATcontrol.ino:4793-4814).
12. **PV boost** is updated (SATcontrol.ino:4818-4829). The humidity comfort offset that used to be updated here was removed in TASK-1282.
13. **Effective target** = target (+ PV boost) (SATcontrol.ino:4831-4833).
14. **Heating curve** (SATcontrol.ino:4836).
15. **PID** (SATcontrol.ino:4839).
16. **Zone block** for every configured zone (SATcontrol.ino:5540-5561, `satAggregateZoneSetpoint()` SATcontrol.ino:2329-2377, see Formulas). The requested setpoint becomes the maximum of the main PID output and the zones output, then the overshoot cap is applied as a minimum. In ECO (`iHeatingMode` 1) the main PID output is kept.
17. **Requested setpoint clamp:** `pidOutput` is clamped to [10, `satEffectiveMaxSetpoint()`] (SATcontrol.ino:4908-4912).
18. **PWM disable:** if `pidOutput` ≥ the system cap `satGetMaxSetpoint()` and the mode is PWM, the mode becomes continuous (SATcontrol.ino:4909, SATcontrol.ino:4917-4920). If `bForcePWM` is on, the mode becomes PWM (SATcontrol.ino:4923-4925). (Removed in TASK-1332, 2026-10-23: the Force PWM option no longer exists.)
19. **Mode rule:** PWM override or continuous rule (SATcontrol.ino:4928-4933).
20. **Final clamp** to [10, effective maximum] (SATcontrol.ino:4936-4937). The result is stored as the "previous control setpoint" (SATcontrol.ino:4939).
21. **Solar gain:** no longer applied here. Since TASK-1323 the offset is subtracted from `pidOutput` before the clamp of item 17 (SATcontrol.ino:5137-5143) (corrected 2026-10-24, TASK-1351).
22. **Auto-switch** between continuous and PWM, for every source (heat pumps too, entry 87); skipped when `pidOutput` ≤ the cold setpoint (SATcontrol.ino:5542-5546).
23. **Modulation value** (SATcontrol.ino:4960-4967, SATcontrol.ino:304-314).
24. **Cold cutoff:** if `pidOutput` is at or below the cold setpoint (28.2 °C radiators, 21 °C underfloor), the control setpoint becomes 10 °C, the modulation value `iMaxRelModulation`, and `satPwmDisableOnHeaterOff()` leaves PWM (SATchEnable.h:26-29, SATcontrol.ino:5206-5214, SATcontrol.ino:1100-1109, SATcontrol.ino:339-343) (corrected 2026-10-24, TASK-1351); entries 7, 11, 12.
25. **Send to the boiler** (SATcontrol.ino:5216-5236 [lines refreshed 2026-10-24, TASK-1351]): `CS=`, `MM=`, `CH=` from `satChEnableDecide` (SATchEnable.h:46-52, SATcontrol.ino:5223) (corrected 2026-10-24, TASK-1351), `TP=11:12=` for Immergas, `TC=` when push-setpoint is on.
26. Without a command interface a failure counter counts up; at 5 SAT trips safety (SATcontrol.ino:69, SATcontrol.ino:4996-5004).
27. PID state is saved every 5 min (SATcontrol.ino:5010-5012), then MQTT is published (SATcontrol.ino:5018).

The "heater off" decisions are made in three places: summer simmer (SATcontrol.ino:4995-5005), valves closed (SATcontrol.ino:5008-5021) and the cold cutoff (SATcontrol.ino:5209-5214) [lines refreshed 2026-10-24, TASK-1351]. The CH bit follows `satChEnableDecide`: the requested setpoint against the cold setpoint and the PWM phase, not the final setpoint (SATchEnable.h:46-52, SATcontrol.ino:5223) (corrected 2026-10-24, TASK-1351). No single heating-demand decision object was found in SATcontrol.ino.

**Modulation value (heater on).** `satComputeMM()`: 0 in PWM mode, the configured maximum `iMaxRelModulation` (default 100) otherwise; a heat pump gets 100 outside PWM; heat pump and hybrid never get 0; Geminox floor 10, Immergas cap 80 (SATcontrol.ino:299-314, SATtypes.h:453). The cold cutoff overwrites it with 100 after these quirks were applied (SATcontrol.ino:4973).

#### Continuous-mode control setpoint rule

`satApplyContinuous(pidOutput, previousCS)` (`SATcontrol.ino`, search the name). In plain terms: the setpoint follows the request, except that while the flame burns and the boiler is hotter than the request, the setpoint is clamped against `boiler - fFlowOffset` and the previous CS, as thermo-nova (`heating_control.py:485-489`; Sergeant D entry 112, TASK-1381).

Step by step:

1. If the flame is off: control setpoint = requested (SATcontrol.ino:1161-1165).
2. Read the boiler flow temperature. If it is ≤ 0 or > 100 °C (treated as unknown): control setpoint = requested (SATcontrol.ino:1167-1172).
3. If boiler temperature ≤ requested: control setpoint = requested (SATcontrol.ino:1174-1177).
4. Otherwise `minAllowed = boilerTemp - fFlowOffset`. If requested > previous CS: control setpoint = `max(requested, minAllowed)`. Else: `max(requested, min(minAllowed, previous CS))`, so a falling request does not follow a rising flow temperature.

**Previous CS** is `_sat_lastControlCS`, the last CS SAT sent: 0 after SAT enable or boot (`satDisable()`, no restore after a reboot), 10 (`SAT_MIN_SETPOINT`) after heater-off (`satPwmDisableOnHeaterOff()`: cold cutoff and valves closed) and after the summer simmer CS=10, the thermal-safe CS (`SAT_THERMAL_SAFE_FLOW`, clamped, whole degrees) while that fallback runs (the last two are our reading of entry 112), and otherwise the final CS of the previous tick or the PWM 3 s suppression refresh. The control tick reads it before it writes this tick's CS.

Differences visible in the code itself: the order of checks is flame first, then boiler validity, then boiler versus request; the previous-CS branch comes last. The result is bounded from above by the final clamp (the clamp right before `_sat_lastControlCS = finalSetpoint;` in the control tick).

Where the previous CS differs from thermo-nova (TASK-1381 review, 2026-10-29): (a) no restore after a reboot (entry 112, option C not chosen); (b) the DHW and boiler-fault returns of the control tick send no CS and leave the previous CS frozen, while thermo-nova keeps computing the continuous setpoint during hot water; (c) thermal-safe and summer simmer write the CS they send as the previous CS (our reading in entry 112), which PWM's no-override paths also return afterwards; (d) the PWM-to-continuous exit is checked after the PWM step in the firmware, so continuous starts one tick later than in thermo-nova (pre-existing, TASK-1468). `fFlowOffset` defaults to 2.0 °C, range 0.5 to 10 (SATtypes.h:467, settingStuff.ino:1040).

#### Cross-reference: PWM override (summary only)

`satApplyPWM(pidOutput)` (SATcontrol.ino:901-925; working tree numbering). It first runs the PWM schedule, whose duty cycle is `(pidOutput - baseOffset) / (effectiveTemp - baseOffset)` with the heating-system base offset (SATcontrol.ino:727-756). If the flow temperature is unknown (`satGetFlowTemp() <= 0`) the schedule has reset the PWM state and `satApplyPWM()` returns the continuous setpoint `satApplyContinuous(pidOutput)` (SATcontrol.ino:741-748, SATcontrol.ino:905-907). Otherwise it returns the previous control setpoint without an override when overshoot protection is off, DHW is active, or `pidOutput` ≤ the cold setpoint (SATcontrol.ino:908-910). The previous control setpoint is `_sat_lastControlCS` when that is above 10 °C, otherwise `pidOutput` (SATcontrol.ino:903). With the override: 10 °C in the OFF phase (SATcontrol.ino:911-914); flame off in the ON phase gives `return temperature + fFlameOffOffset` clamped to [cold setpoint, system cap] (SATcontrol.ino:916-919, SATcontrol.ino:701-707); during the hold delay the hold value or the previous setpoint (SATcontrol.ino:920-922); after the delay `flow - fModSupOffset` clamped to [10, system cap] (SATcontrol.ino:923-924, SATcontrol.ino:711-717), refreshed every 3 s (SATcontrol.ino:930-954). Details belong to the PWM section.

<a id="s3-formulas"></a>

### Formulas

Variable names are as in the code.

**Unscaled curve** (SATcontrol.ino:489-494):

```
diff       = outsideTemp - 20
curveValue = 4 * (targetTemp - 20) + 0.03 * diff * diff - 0.4 * diff
```

**Scaled curve value** (SATcontrol.ino:496-499):

```
setpoint = baseOffset + (fHeatingCurveCoeff / 4) * curveValue
```

No rounding is applied (SATcontrol.ino:496-499).

**Base offset per heating system** (SATcontrol.ino:255-260, SATcontrol.ino:49-50):

```
baseOffset = 20.0   if effective heating system == UNDERFLOOR
baseOffset = 27.2   otherwise (RADIATORS, and AUTO, which resolves to RADIATORS)
```

The heating source (heat pump, hybrid) does not change the base offset (SATcontrol.ino:255-260).

**PID output** (SATpid.ino:309): `output = heatingCurveValue + P + I + D`, not rounded.

**Requested setpoint** (ECO skips the zones only, SATcontrol.ino:5538-5585):

```
pidOutput = satPidOutput(curveValue) | satPidCurveOnly(curveValue)  (the 60 s satPidService() stepped the PID earlier)
pidOutput = satAggregateZoneSetpoint(pidOutput, zones, ...)    (5541-5563; function 2329-2377)
pidOutput = pidOutput - fSolarSetpointOffset  if solar gain is active and enabled
pidOutput = clamp(pidOutput, 10, satEffectiveMaxSetpoint())    (5581-5586)
```

**Zone aggregate** (`satAggregateZoneSetpoint()`, SATcontrol.ino:2329-2377). A port of thermo-nova climate.py:369-394 and area.py:166-175, :264-333 (entry 116, TASK-1440). It runs for every zone count (1 to 4). A zone counts when `satZoneUsable()` is true (SATcontrol.ino:2231) and its output is above the cold setpoint (`satGetColdSetpoint()`: 28.2 radiators, 21.0 underfloor, ADR-199; SATcontrol.ino:2343). For each counted zone, `error = target - room` rounded to 3 decimals (`satPidError()`); the cap uses the ones below -0.3.

```
ECO (iHeatingMode == 1)  : pidOutput unchanged                    (2333)
counted outputs sorted ascending                                  (2354-2359)
idx        = int(N * 0.75), at most N - 1                         (2360-2361)
chosen     = min(highest, outputs[idx] + fZoneAggregationHeadroom)  (2362-2363)
zonesOut   = round1(chosen);  N == 0 gives 10                     (2352, 2364)
setpoint   = max(pidOutput, zonesOut)                             (2367)
per counted zone with error < -0.3:
    c = cold + 10 - 4 * (-error)                                  (2345)
cap        = min of the c values, at least cold, round1           (2346, 2369-2370)
setpoint   = min(setpoint, cap)  if there is a cap                (2371)
```

Derived from line 2360 (computed, not stated in the code): `int(N * 0.75)` is 0, 1, 2, 3 for N = 1, 2, 3, 4, always the highest output, so with at most 4 zones `min(highest, baseline + headroom)` is the highest output and `fZoneAggregationHeadroom` never binds. It would only bind with 5 or more zones. `fZoneAggregationHeadroom` defaults to 5.0 °C, range 0 to 15 (SATtypes.h:552, settingStuff.ino:1110); its meaning changed in TASK-1440: before it was added on top of the P75 value, now it is the most the zones may ask above their P75 output. No settings migration.

Computed examples (radiators, cold 28.2, 4.0 per degree, headroom irrelevant): 1.0 °C over a target gives a cap of 34.2; 4.0 °C over gives 22.2, floored to 28.2; exactly 0.3 °C over gives no cap (the test is strictly below -0.3).

**Effective maximum** used by the clamps (SATcontrol.ino:240-252):

```
m = MaxTSet;  if m < 30: m = 75                          (242-243)
m = min(m, systemCap)                                     (244, 246)
m = min(m, hardMax)   hardMax = 50 underfloor, 80 other   (245, 247, 62-63)
g = fMaxSetpoint;  if g < 30 or g > 80: g = 65            (248-249)
m = min(m, g)                                             (250)
```

**System cap** `satGetMaxSetpoint()` (SATcontrol.ino:228-235): 45 °C underfloor, 62 °C otherwise, and at most 40 °C for a heat-pump source. The code comment at SATcontrol.ino:216-227 says the 62 °C radiator value is intentional. This cap is used for the PWM-disable check (SATcontrol.ino:4909, 4917), the zone clamp (SATcontrol.ino:1859-1861), the PWM override (SATcontrol.ino:879), the thermal safe fallback (SATcontrol.ino:4736-4737) and the published `sat/requested_setpoint` (SATcontrol.ino:3054-3057).

**Cold setpoint** (SATcontrol.ino:264-268, SATcontrol.ino:55-56): 28.2 °C for radiators and auto, 21.0 °C for underfloor.

**Comfort offset.** Removed in TASK-1282 (Sergeant D entry 114). The humidity comfort offset (`bComfortAdjust`, `fComfortHumidity`, `fComfortMaxOffset`) no longer exists; the effective target is the setpoint plus the PV boost.
**PV boost** (SATcontrol.ino:1542-1616): while active, `fPvBoostDeltaC` (default 1.5 °C) is added to the effective target (SATcontrol.ino:1590, SATcontrol.ino:4832-4833, SATtypes.h:545).

**Worked examples** (derived from SATcontrol.ino:492-496; computed, not taken from the code). Coefficient 2.0 for direct comparison with thermo-nova, and the firmware default 1.5 (SATtypes.h:442):

| System | Target °C | Outside °C | curveValue | Curve, coeff 2.0 | Curve, coeff 1.5 |
|---|---|---|---|---|---|
| Radiators (27.2) | 20 | 10 | 7.0 | 30.7 | 29.825 |
| Radiators (27.2) | 20 | 0 | 20.0 | 37.2 | 34.7 |
| Radiators (27.2) | 20 | -10 | 39.0 | 46.7 | 41.825 |
| Radiators (27.2) | 21 | 0 | 24.0 | 39.2 | 36.2 |
| Underfloor (20) | 20 | 10 | 7.0 | 23.5 | 22.625 |
| Underfloor (20) | 20 | 0 | 20.0 | 30.0 | 27.5 |
| Underfloor (20) | 20 | -10 | 39.0 | 39.5 | 34.625 |
| Underfloor (20) | 21 | 0 | 24.0 | 32.0 | 29.0 |

Derived from the curve formula and the cold setpoints (computed, not stated in the code): both cold setpoints are 1.0 °C above their base offset (27.2 + 1 = 28.2, 20 + 1 = 21). At target 20 °C and with P, I and D all zero, the curve drops below the cold setpoint, and the cold cutoff sends 10 °C, when the outside temperature is above about 15.1 °C (coefficient 1.5) or about 16.1 °C (coefficient 2.0), for both heating systems (SATcontrol.ino:492-496, SATcontrol.ino:4971-4975).

<a id="s3-constants-and-defaults"></a>

### Constants and defaults

| Name | Value | Unit | Citation |
|---|---|---|---|
| Curve target gain | 4 | none in code | SATcontrol.ino:492 |
| Curve quadratic factor | 0.03 | none in code | SATcontrol.ino:493 |
| Curve linear factor | -0.4 | none in code | SATcontrol.ino:494 |
| `SAT_HC_REF_TEMP` | 20.0 | °C | SATcontrol.ino:51 |
| Coefficient divisor | 4 | | SATcontrol.ino:496 |
| Curve rounding | none | | SATcontrol.ino:496-499 |
| `SAT_HC_BASE_OFFSET_FLOOR` | 20.0 | °C | SATcontrol.ino:49 |
| `SAT_HC_BASE_OFFSET_RAD` (radiators, auto) | 27.2 | °C | SATcontrol.ino:50 |
| `fHeatingCurveCoeff` default | 2.0 (TASK-1347) | | SATtypes.h:443 |
| `fHeatingCurveCoeff` accepted range | 0.1 to 5.0 | | settingStuff.ino:998 |
| Auto-tune coefficient clamp | 0.3 to 5.0 | | SATcontrol.ino:4475-4476 |
| `bAutoTune` default | off | | SATtypes.h:511 |
| `fAutoTuneRate` default | 0.02 | per tuning | SATtypes.h:512 |
| `iHeatingSystem` default | 0 (auto, treated as radiators) | | SATtypes.h:438, SATcontrol.ino:198-199 |
| `iHeatingMode` default | 0 (comfort), not used in control | | SATtypes.h:524 |
| `bAutoGains` default | on | | SATtypes.h:518 |
| `SAT_MIN_SETPOINT` | 10.0 | °C | SATcontrol.ino:54 |
| `SAT_COLD_SETPOINT_RAD` | 28.2 | °C | SATcontrol.ino:55 |
| `SAT_COLD_SETPOINT_FLOOR` | 21.0 | °C | SATcontrol.ino:56 |
| `SAT_MAX_SETPOINT_DEFAULT` (when MaxTSet < 30) | 75.0 | °C | SATcontrol.ino:57, SATcontrol.ino:243 |
| System cap radiators / underfloor / heat pump | 62 / 45 / 40 | °C | SATcontrol.ino:231-233 |
| `SAT_HARD_MAX_RAD` / `SAT_HARD_MAX_FLOOR` | 80 / 50 | °C | SATcontrol.ino:62-63 |
| `fMaxSetpoint` default (global cap) | 65.0 | °C | SATtypes.h:514 |
| `SAT_GLOBAL_MAX_SETPOINT` (fallback for an out-of-range `fMaxSetpoint`) | 65.0 | °C | SATcontrol.ino:64, SATcontrol.ino:249 |
| `fFlowOffset` default | 2.0 | °C | SATtypes.h:467 |
| `fFlowOffset` accepted range | 0.5 to 10 | °C | settingStuff.ino:1040 |
| `SAT_FLOW_OFFSET_CONTINUOUS` | 5.0 | °C | SATcontrol.ino:58 (defined; no use found by grep) |
| `fDeadband` default | 0.1 | °C | SATtypes.h:443 |
| `fDeadband` accepted range | 0.05 to 2.0 | °C | settingStuff.ino:999 |
| PID sample-time limit | removed (TASK-1368, entry 75) | - | - |
| PID "update interval" factor | 60 | s | SATpid.ino:22 |
| PID integral absolute cap | 20 | °C | SATpid.ino:176 |
| `iControlInterval` default | 30 | s | SATtypes.h:444 |
| `iControlInterval` accepted range | 10 to 300 | s | settingStuff.ino:1001 |
| Room temperature EMA time constant | 45 | s | SATcontrol.ino:4695 |
| Outside temperature replacement when out of range | 10 | °C | SATcontrol.ino:4721-4723 |
| `fTargetTemp` default | 20.0 | °C | SATtypes.h:441 |
| `fTargetTemp` accepted range | 5 to 30 | °C | settingStuff.ino:997 |
| Preset comfort / eco / away / sleep / activity / home | 21 / 18 / 15 / 16 / 10 / 18 | °C | SATtypes.h:446-451 |
| `iZoneCount` default | 1 | | SATtypes.h:531 |
| `fZoneAggregationHeadroom` default (most the zones may ask above their P75 output) | 5.0 | °C | SATtypes.h:552 |
| Zone percentile | 0.75 (index `int(N * 0.75)`, at most N - 1) | | SATcontrol.ino:2360-2361 |
| Zone overshoot margin / cooling headroom / cooling slope | 0.3 / 10.0 / 4.0 | °C, °C, °C per °C | SATcontrol.ino:2335-2337 |
| `fSolarSetpointOffset` default | 2.0 | °C | SATtypes.h:496 |
| `bSolarGainEnable` default | off | | SATtypes.h:494 |
| `bThermalComfort` default | off | | SATtypes.h:522 |
| `bPvBoostEnabled` default, `fPvBoostDeltaC` | off, 1.5 | , °C | SATtypes.h:542, SATtypes.h:545 |
| `iMaxRelModulation` default | 100 | % | SATtypes.h:453 |
| Initial curve value | 0.0 | °C | SATtypes.h:154 |
| Initial "previous control setpoint" | 0.0 (treated as "none" at or below 10) | °C | SATcontrol.ino:693, SATcontrol.ino:870 |
| PID state file max age | 1800 | s | SATcontrol.ino:1993 |
| PID state save interval | 300 | s | SATcontrol.ino:5010 |
| Curve recommendation (hourly): sample interval, buffer, minimum samples, threshold | 1 h, 24, 6, `2 * fDeadband` | | SATcontrol.ino:3488, SATcontrol.ino:3493, SATcontrol.ino:3514, SATcontrol.ino:3537 |
| Curve recommendation (daily median): threshold, days needed | 0.5, 3 | °C, days | SATcycles.ino:165-166 |
| Daily median ring / intra-day ring (ESP32) | 30 days / 1440 samples | | boards.h:123-124 |

<a id="s3-state-kept-between-passes"></a>

### State kept between passes

| State | Kept where | Persisted? | Citation |
|---|---|---|---|
| Heating curve value | `state.sat.fHeatingCurveValue` | No file write found; recomputed every tick | SATtypes.h:154, SATcontrol.ino:498, SATcontrol.ino:4836 |
| Room target temperature | `settings.sat.fTargetTemp` | Yes, settings file key `SATtargettemp` | SATtypes.h:441, settingStuff.ino:319, settingStuff.ino:997 |
| SAT enabled (HVAC mode equivalent) | `settings.sat.bEnabled` | Yes, settings file key `SATenabled` | SATtypes.h:437, settingStuff.ino:315 |
| Heating curve coefficient | `settings.sat.fHeatingCurveCoeff` | Yes; auto-tune writes it and saves the settings | settingStuff.ino:320, SATcontrol.ino:4479-4485 |
| Previous control setpoint (used by PWM without override) | `_sat_lastControlCS` | No. 0.0 at boot and after disable | SATcontrol.ino:693, SATcontrol.ino:4939, SATcontrol.ino:910, SATcontrol.ino:2166 |
| Last control setpoint sent | `state.sat.fLastSentCS` (rounded to 0.1) | No | SATcontrol.ino:383-396 |
| Final setpoint | `state.sat.fFinalSetpoint` | No | SATcontrol.ino:4946, SATcontrol.ino:4974 |
| PID integral and raw derivative | `_pid_integral`, `_pid_rawDerivative` | Yes, `/sat_pid_state.json` every 5 min and on disable; it saves the display copies `fPidI`, `fRawDerivative` | SATpid.ino:34-35, SATcontrol.ino:1995-2006, SATcontrol.ino:5010-5012, SATcontrol.ino:2171 |
| Room temperature EMA | static in the control tick | No | SATcontrol.ino:4695-4712 |
| Zone integral, previous error, output | `satZones[]` | No | SATcontrol.ino:1726, SATcontrol.ino:1846-1864 |
| PV boost state, solar gain state | `state.sat` | No | SATcontrol.ino:1589-1590, SATcontrol.ino:4277 |
| Hourly error ring (curve recommendation) | `_cr_errorBuffer[24]` | No | SATcontrol.ino:3488-3492 |
| Daily median ring and intra-day samples | `_hcr_dailyMedian`, `_hcr_samples` | Yes, `/sat/sat_hcr.json`, on each day change | SATcycles.ino:168-178, SATcycles.ino:1097-1132, SATcycles.ino:1240-1241 |

**What resets state.**

- A target change through `satHandleTargetTemp()` or `updateSetting("SATtargettemp")` resets nothing in the PID (SATcontrol.ino:1903-1934, settingStuff.ino:997).
- A preset change, and clearing a preset, set `state.sat.fPidI = 0` (SATcontrol.ino:998, SATcontrol.ino:1006). `fPidI` is only a display copy: the PID computes with `_pid_integral` (SATpid.ino:298) and overwrites `fPidI` from it on the next update (SATpid.ino:303). A grep found no code that copies `fPidI` back into `_pid_integral`. So a preset change does not reset the integral that is used.
- The window-open switch and the window-close restore call `satResetIntegral()`, which clears `_pid_integral` (SATcontrol.ino:1119, SATcontrol.ino:1139, SATpid.ino:86-91). The MQTT and REST `reset_integral` commands call the same function (MQTTstuff.ino:620, restAPI.ino:1374, restAPI.ino:1625).
- Disabling SAT, and a safety trip, call `satPidReset()`, which clears integral, derivative, gains and the error history, and saves the zeros (SATcontrol.ino:2160-2171, SATpid.ino:94-114).
- `satFlushShortLivedData()` also resets the PID and saves zeros (SATcontrol.ino:2192-2201).
- The integral also resets by itself whenever |error| > deadband (SATpid.ino:154-156).
- Switching control mode resets PWM state only (SATcontrol.ino:934-938).

<a id="s3-outputs-to-the-boiler-to-entities"></a>

### Outputs (to the boiler, to entities)

**To the boiler** (every tick that reaches step 25, SATcontrol.ino:4978-4994):

| What | Value | Command | Citation |
|---|---|---|---|
| Control setpoint | `finalSetpoint` | `CS=%.1f`. Sent only when the value rounded to 0.1 changed, or 5 min after the last send | SATcontrol.ino:4980, SATcontrol.ino:383-396, SATcontrol.ino:351 |
| Max relative modulation | `state.sat.iCurrentModulation` | `MM=<n>`, same write-on-change rule | SATcontrol.ino:4981, SATcontrol.ino:416-427 |
| CH enable | `satChEnableDecide`: 0 when the requested setpoint is at or below the cold setpoint or in the PWM OFF phase, else 1 (corrected 2026-10-24, TASK-1351) | `CH=1` / `CH=0`, same rule | SATchEnable.h:46-52, SATcontrol.ino:5223 |
| Immergas extra | the final setpoint, no decimals | `TP=11:12=<value>`, every tick | SATcontrol.ino:4986-4990 |
| Thermostat display | `settings.sat.fTargetTemp` when `bPushSetpoint` is on | `TC=%.1f`, on target change; no 5-min refresh (TASK-1272) | SATcontrol.ino:5172-5176, SATcontrol.ino:540-553 |

On PIC boards the last `CS=` (when ≥ 8 °C) is re-sent every 30 s (SATcontrol.ino:403-414). The command cache is cleared when the OpenTherm bus comes back online, so the next tick re-sends everything (SATcontrol.ino:4531-4535). A grep for `CS=` over `SAT*.ino` shows every SAT `CS=` send going through `satEnqueueOwnCS()` (SATcontrol.ino:375-379, SATcontrol.ino:391, SATcontrol.ino:412, SATcontrol.ino:2179, SATcontrol.ino:3707, SATcontrol.ino:4586). No `PM=25` after `CS=` and no `SH=` command was found (grep over all `*.ino` for `PM=25` and `SH=`).

**To MQTT and the REST status:**

- `sat/setpoint` = `state.sat.fFinalSetpoint` (after solar and cold cutoff) (SATcontrol.ino:2731).
- `sat/heating_curve` = `state.sat.fHeatingCurveValue` (SATcontrol.ino:2732). With zones, see "When the heating curve is recalculated".
- `sat/pid_output` = `state.sat.fPidOutput`, the raw PID output before zones and clamps (SATcontrol.ino:2733, SATpid.ino:310).
- `sat/target` = `settings.sat.fTargetTemp` (the target without the PV boost) (SATcontrol.ino:2734).
- `sat/requested_setpoint` = `_sat_requestedSetpoint`, the `pidOutput` of the last control tick after the zone block and the solar offset, clamped to [10, effective maximum] (SATcontrol.ino:3692-3696, SATcontrol.ino:5583-5584). With zones, the value therefore includes the zones output and the overshoot cap.
- `sat/max_setpoint` = the system cap `satGetMaxSetpoint()`, not the effective maximum (SATcontrol.ino:3050).
- `sat/heating_curve_coeff` (SATcontrol.ino:3126).
- Per-zone `sat/zone/<n>/active`, `/output`, `/error` (`satPublishZoneDiagnostics()`, SATcontrol.ino:2382-2412). They are published only when `iZoneCount` > 1 (SATcontrol.ino:5561), on purpose: no new retained MQTT traffic for single-room setups, although the zone block now runs at zone count 1 (TASK-1440).
- The REST status JSON carries `heating_curve` and `coefficient` (SATcontrol.ino:2354, SATcontrol.ino:2365).

**Heating curve recommendation** (advisory only). Two separate mechanisms exist:

1. Hourly (SATcontrol.ino:3495-3545): once per hour, while SAT is active, the PID error (target minus room) is stored in a 24-slot ring. With fewer than 6 samples the result is `insufficient`. Otherwise the mean is compared with `2 * fDeadband`: above gives `increase` (room too cold), below the negative gives `decrease`, otherwise `hold`. Published as `sat/curve_recommendation` with attributes (SATcontrol.ino:2899-2914).
2. Daily median (SATcycles.ino:1201-1289): on every control tick a sample `room - target` is stored (SATcontrol.ino:4799, SATcycles.ino:1205), the opposite sign of mechanism 1. At each new UTC day (time / 86400) the median of that day's samples is committed (SATcycles.ino:1214-1233). With 3 days stored, 3 days in a row below -0.5 give `INCREASE`, above 0.5 give `DECREASE`, otherwise `HOLD` (SATcycles.ino:1254-1278). Published as `sat/heating_curve_recommendation` (SATcontrol.ino:2917).

Neither mechanism writes the coefficient. Only auto-tune does (SATcontrol.ino:4444-4485), and it is off by default (SATtypes.h:511).

<a id="s3-edge-cases"></a>

### Edge cases

- **No outside temperature source.** `Toutside` stays at its initial 0.0 (OTGW-Core.h:82), so the curve is computed for 0 °C outside and heating goes on (SATcontrol.ino:1302, SATcontrol.ino:4836). There is no early return for a missing outside value.
- **Weather API as outside source.** It is used only while weather data is valid and `Toutside` is exactly 0.0 (SATcontrol.ino:1299). A real 0.0 °C OpenTherm reading is therefore also replaced by the weather value.
- **Outside value out of range** (below -40 or above 50 °C): 10 °C is used (SATcontrol.ino:4721-4723).
- **SAT disabled.** The tick does not run. On the enabled-to-disabled transition `CS=0` is sent once (SATcontrol.ino:4568-4573, SATcontrol.ino:2179).
- **Curve not yet computed.** There is no "None" state. Before the first tick the value is 0.0 (SATtypes.h:154); from the first tick on it is always computed.
- **Requested setpoint at or below the cold setpoint** (28.2 radiators, 21 underfloor): control setpoint 10 °C, modulation `iMaxRelModulation`, `CH=0`, PWM left (SATchEnable.h:26-29, SATcontrol.ino:5206-5214, SATcontrol.ino:5223, SATcontrol.ino:1100-1109) (corrected 2026-10-24, TASK-1351); entries 7, 11, 12.
- **CH rule.** `satChEnableDecide`: CH=0 when the requested setpoint is at or below the cold setpoint or the PWM OFF phase drives the CS; otherwise CH=1, also in the PWM ON phase whatever CS keeps the flame on (SATchEnable.h:46-52, SATcontrol.ino:5223) (corrected 2026-10-24, TASK-1351): was "any final setpoint above 10 °C gives CH=1"; entries 5, 7, 9, 11, 17.
- **Solar gain and the cutoff.** The solar offset lowers the requested setpoint before the clamp and the cold cutoff, so solar switches the heater off when it brings the request to or below the cold setpoint (SATcontrol.ino:5137-5152, SATcontrol.ino:5209) (corrected 2026-10-24, TASK-1351); TASK-1323, entry 11.
- **No valve information.** `bValvesOpen` stays true (SATtypes.h:243).
- **Valves reported closed.** `CS=10`, `CH=0`, MM `iMaxRelModulation`, PWM left and reset by `satPwmDisableOnHeaterOff()`, and the PID and curve are not run that tick (SATcontrol.ino:5008-5021, SATcontrol.ino:1100-1109) (corrected 2026-10-24, TASK-1351); TASK-1327.
- **Thermal comfort on.** The Summer Simmer index replaces the room temperature only when the room temperature comes from OpenTherm `Tr` and humidity is fresh (SATcontrol.ino:1264-1270). BLE, multi-area and external sources return earlier (SATcontrol.ino:1208-1231). The curve uses the effective target and the outside temperature only (SATcontrol.ino:4836).
- **Requested setpoint at or above the system cap.** PWM is switched to continuous (SATcontrol.ino:4917-4920). Derived from lines 4908-4912 and 4917: the clamp uses the effective maximum, which can be below the system cap (for example when the boiler's MaxTSet is lower), and then `pidOutput` can never reach the system cap, so this switch cannot trigger.
- **Continuous mode, boiler hotter than the request, flame on.** The setpoint stays at least `boiler - 2.0` °C by default (SATcontrol.ino:1179-1184). There is no hold on a falling request.
- **Zones, one counted.** Its output raises the main PID output when it is higher; it never lowers it (SATcontrol.ino:2367). The old replace-the-primary behaviour is gone (TASK-1440).
- **Zones, several counted.** The main PID output is kept and raised to the zones output, which is the highest counted output with up to 4 zones (SATcontrol.ino:2360-2367). An overshooting zone caps the result (SATcontrol.ino:2368-2372).
- **Rounding.** The curve and PID output are not rounded (SATcontrol.ino:496, SATpid.ino:309). The PID error is rounded to 3 decimals (SATpid.ino:65). The `CS=` command is formatted with one decimal (SATcontrol.ino:390). The thermal safe fallback truncates to a whole degree (SATcontrol.ino:4740).
- **Heat pump.** Same base offset as radiators (SATcontrol.ino:255-260), system cap 40 °C (SATcontrol.ino:233), modulation 100 outside PWM (SATcontrol.ino:307), and it enters PWM through the same 60 s auto-switch as every other source (entry 87; SATcontrol.ino:5542-5546). A heat pump has no hourly PWM start limit (entry 81; SATcycles.ino:320-327).
- **Heating system auto.** Treated as radiators for base offset, cold setpoint and caps (SATcontrol.ino:198-199).
- **Room temperature invalid for 10 ticks.** Safety trip, `CS=0` (SATcontrol.ino:4675-4681). With the default 30 s tick that is about 5 minutes (derived from SATtypes.h:444 and SATcontrol.ino:68).

<a id="s3-not-found-in-the-code-unclear"></a>

### Not found in the code / unclear

1. **ECO / COMFORT heating mode has no effect on control.** `iHeatingMode` is stored, parsed and published (SATtypes.h:524, settingStuff.ino:387, settingStuff.ino:1104, SATcontrol.ino:2533, SATcontrol.ino:2978), but a grep over all firmware sources found no other read of it. The requested setpoint is computed the same way in both modes.
2. **No event-driven curve recalculation.** A grep over the firmware sources found `satCalcHeatingCurve()` called only from the control tick (SATcontrol.ino:4836) and the zone step (SATcontrol.ino:1828).
3. **No "no outside temperature" state.** The code has no path that stops control when no outside source exists (SATcontrol.ino:1283-1303, SATcontrol.ino:4721-4723). Whether 0.0 °C as a silent default is intended is not stated.
4. **Weather data never expires.** `state.sat.weather.bValid` is set true after a successful fetch (SATweather.ino:376); a grep found no code that sets it false.
5. **PID reset on target change.** No reset was found on a plain target change (SATcontrol.ino:1903-1934, settingStuff.ino:997). Presets only zero the display copy `fPidI` (SATcontrol.ino:998, SATcontrol.ino:1006). Whether that is intended is not stated; the code comment at SATcontrol.ino:997 says "Reset PID integral to prevent overshoot on large temp jumps".
6. **`bSolarFreezeIntegral`.** Resolved: the PID now reads it, `if (state.sat.bSolarGainActive && settings.sat.bSolarFreezeIntegral) return;` (SATpid.ino:157), default on (SATtypes.h:549) (corrected 2026-10-24, TASK-1351); TASK-1323, entry 11 answer 3.
7. **`bAutoGains` and the manual gains have no settings key.** `bAutoGains` is read by the PID (SATpid.ino:122), but a grep found no settings-file key or handler that writes it. A grep for `fKpManual`, `fKiManual` and `fKdManual` found only their defaults (SATtypes.h:519-521), the PID read (SATpid.ino:124-126) and a JSON status output (SATcontrol.ino:2528-2530), no writer. The settings key `SATautogains` writes `fAutoGainsValue` instead (settingStuff.ino:1103), and a grep found no read of `fAutoGainsValue` in the control path (only restAPI.ino:4018 and settingStuff.ino:386).
8. **`fMaxSetpoint` has no settings key.** It is used by the effective maximum (SATcontrol.ino:248) but a grep found no code that writes it, so it stays at 65 °C (SATtypes.h:514).
9. **`SAT_FLOW_OFFSET_CONTINUOUS` = 5.0** (SATcontrol.ino:58) is defined, but a grep found no use.
10. **Three different maxima.** The clamp uses the effective maximum (SATcontrol.ino:4908-4912); the PWM-disable check, the zone clamp, the PWM override, `sat/requested_setpoint` and `sat/max_setpoint` use the system cap (SATcontrol.ino:4909, 1859, 879, 3055, 3050). The code does not say whether that difference is intended.
11. **Published heating curve with zones.** With `iZoneCount` > 1, `sat/heating_curve` shows the last zone's curve (SATcontrol.ino:498, 1828, 4854, 2732). The code does not say whether that is intended.
12. **Zone integral step size (settled by TASK-1368).** The zone integral adds `ki * error * 60` per zone step, and the zone steps run on the same 60 s PID timer as the main PID (`satZonePidAdvance()`), so the step size equals the interval. The unchanged-error rule of the main PID is gone as well (entry 42).
13. **Daily-median sample rate.** The code comment says "~every 15 min" (SATcycles.ino:1199) but the function is called every control tick (SATcontrol.ino:4799). Derived from the defaults: at a 30 s tick a day has 2880 samples and the 1440-slot ring (boards.h:124) keeps only the last 12 h of each day.
14. **Two recommendation sign conventions.** Mechanism 1 uses `target - room` (SATcontrol.ino:3507), mechanism 2 uses `room - target` (SATcycles.ino:1205). Both map "room too cold" to increase. Which one the user interface treats as leading is outside this section and was not checked.
15. **Cold cutoff and modulation quirks.** The cutoff sets modulation to `iMaxRelModulation` after `satComputeMM()` applied the Geminox and Immergas quirks (SATcontrol.ino:1108, SATcontrol.ino:5196-5214), so neither quirk applies in that case. thermo-nova's heater-off branch does the same (heating_control.py:234) (corrected 2026-10-24, TASK-1351); TASK-1327. The cutoff, summer simmer and valves closed also skip `satComputeMM()`'s heat-pump/hybrid zero guard and the Geminox floor (SATcontrol.ino:385-386). With `SATmaxmodulation` = 0 (allowed, settingStuff.ino:1014) a heat pump receives MM=0, and a Geminox boiler with a maximum below 10 receives less than 10. This conflicts with entry 21 ("never 0 for heat pumps and hybrids, Geminox at least 10") and is open, to put to Sergeant D (corrected 2026-10-24, TASK-1351).
16. **Why the cold setpoints are 28.2 and 21.** Only the values and a code comment naming a person and task are present (SATcontrol.ino:55-56, SATcontrol.ino:262-263).
17. **Why the radiator system cap is 62 °C.** Only a code comment gives a reason (SATcontrol.ino:216-227); it is not checked here.

---

<a id="s4"></a>

## PWM, modulation and the suppression mechanism (OTGW firmware, as implemented)

Source: OTGW-firmware, branch `dev`, commit 51df6b486. Paths are relative to `src/OTGW-firmware/`. This section describes what the firmware code does, not what it should do. Firmware comments that say "matches Python" or "parity" are not used as evidence; only the code statements are.

Reading guide. "Tick" means one pass of the timed SAT control block inside `satControlLoopRun()` (SATcontrol.ino:4614-5007), gated by the timer `timerSATControl` (SATcontrol.ino:476). "Refresh" means the separate 3 s suppression refresh `satPwmSuppressionRefresh()` (SATcontrol.ino:894-918), gated by `timerSATSuppress` (SATcontrol.ino:480). "CS" is the control setpoint command `CS=`, "MM" the maximum relative modulation command `MM=`, "CH" the central-heating enable command `CH=1` / `CH=0` (SATcontrol.ino:383-439). The firmware has three control modes: OFF, CONTINUOUS and PWM (SATtypes.h:53, seen in a search result). In PWM mode the phase is held in one boolean, `state.sat.bPwmFlameRequested`: true is the ON phase, false the OFF phase (SATcontrol.ino:814-855, SATcontrol.ino:3040-3044). There is no separate IDLE status inside PWM mode; the published `sat/pwm_state` is "IDLE" only when the mode is not PWM (SATcontrol.ino:3040-3044).

<a id="s4-purpose"></a>

### Purpose

The firmware computes a requested flow setpoint `pidOutput` from the heating curve and the PID (SATcontrol.ino:4836-4839), clamps it (SATcontrol.ino:4906-4912), and then turns it into a CS in one of two ways:

- **Continuous mode**: `satApplyContinuous()` sends `pidOutput`, with a clamp against `flow - fFlowOffset` and the previous CS while the flame burns (`heating_control.py:485-489`, entry 112, TASK-1381).
- **PWM mode**: `satApplyPWM()` runs the PWM schedule (`satPwmSchedule()`, SATcontrol.ino:727-856) and then overrides CS: CS = 10 °C in the OFF phase, a "flame-off hold" setpoint while the flame is off in the ON phase, and the "suppression" setpoint `flow - fModSupOffset` after a delay once the flame burns (SATcontrol.ino:868-889). MM is 0 in both PWM phases for a gas boiler (SATcontrol.ino:299-314, SATcontrol.ino:4960-4967).

Important framing for the reader: in the firmware, as in the description of the option, "modulation suppression" does **not** write a modulation value. It writes CS (SATcontrol.ino:709-717, SATcontrol.ino:884-888, SATcontrol.ino:907-917). Modulation is limited separately by MM = 0 for the whole time the mode is PWM (SATcontrol.ino:308, SATcontrol.ino:4962). The code comment says so directly: "Modulation suppression is handled via CS sequence in satApplyPWM() (PWM ON only)" (SATcontrol.ino:4955). The state flag `bModSuppressed` is set to false on every tick and never set to true (SATcontrol.ino:4956-4957; a search for `bModSuppressed` finds only this write, the declaration at SATtypes.h:214 and the read-outs in handleDebug.ino:237, restAPI.ino:2405 and SATcontrol.ino:2393). The comment on the offset setting says "°C below setpoint" (SATtypes.h:456), but the code subtracts it from the flow temperature (SATcontrol.ino:713).

<a id="s4-inputs"></a>

### Inputs

Per tick, the PWM code reads:

| Input | Meaning | Source |
|---|---|---|
| `pidOutput` | Requested flow setpoint (heating curve + PID, raised to the zones output and capped by the zone overshoot cap, minus the solar offset, clamped to [10, effective maximum]) | SATcontrol.ino:5529-5538, SATcontrol.ino:5540-5561, SATcontrol.ino:5583-5585 |
| Flow temperature | `OTcurrentSystemState.Tboiler` (OpenTherm MsgID 25 per the comment), or the simulated value | SATcontrol.ino:1325-1329 |
| Return temperature | `OTcurrentSystemState.Tret` (MsgID 28 per the comment), or simulated | SATcontrol.ino:1331-1335 |
| Flame | Status bit 3 of `OTcurrentSystemState.Statusflags`, or simulated | SATcontrol.ino:1337-1341 |
| DHW active | Bit 2 of `OTcurrentSystemState.SlaveStatus`, or a simulated DHW draw | SATcontrol.ino:4625-4629 |
| Fault flag | Bit 0 of `OTcurrentSystemState.SlaveStatus` | SATcontrol.ino:4716-4719 |
| Valves open | `state.sat.bValvesOpen`, default true, set over MQTT | SATtypes.h:243 and MQTTstuff.ino:635 (search results only), SATcontrol.ino:4779 |
| Boiler MaxTSet | `OTcurrentSystemState.MaxTSet`, part of the effective maximum | SATcontrol.ino:240-252 |

Timestamps from the cycle tracker that the PWM code uses: the last real flame-on time `_cycle_flameOnStartMs` and the last real flame-off time `_cycle_flameOffStartMs` (SATcycles.ino:107-108, SATcycles.ino:1039-1040). They are updated on every pass of `satControlLoopRun()` (not only on ticks) through flame-edge detection (SATcontrol.ino:4591-4596, SATcycles.ino:697-714, SATcycles.ino:757-760). The flame-on time is written only at init (to 0) and at a flame-on edge; a flame-off edge does not clear it (SATcycles.ino:191, SATcycles.ino:714, SATcycles.ino:757-843).

Configuration inputs (defaults in the constants table below): `bOvershootProtection`, `fFlameOffOffset`, `fModSupDelay`, `fModSupOffset`, `fOvershootMargin`, `iMaxRelModulation`, `fFlowOffset`, `iControlInterval`, `iHpCycleSeconds` (SATtypes.h:440-466); `iCyclesPerHour` and `fMaxSetpoint` (SATtypes.h:514, SATtypes.h:525); heating system (radiators or underfloor; AUTO means radiators) and heating source (gas boiler, heat pump, hybrid; AUTO means gas boiler) (SATcontrol.ino:194-214); manufacturer quirks (SATcontrol.ino:114-170). (Removed in TASK-1332, 2026-10-23: the Force PWM option no longer exists.)

<a id="s4-behaviour-step-by-step-in-time-order"></a>

### Behaviour (step by step, in time order)

#### 1. Start-up of the firmware (what PWM-related state exists before the first tick)

1. `initSAT()` runs once at boot (OTGW-firmware.ino:567, search result). It calls `satCycleInit()`, which sets the last flame-on time to 0, the last flame-off time to "now", and clears the sustain timers and the per-hour cycle ring (SATcontrol.ino:3688, SATcycles.ino:188-207).
2. It then calls `satPwmReset()`: phase = OFF (`bPwmFlameRequested = false`), no ignition wait, `_pwm_flameOnMs = 0`, stored flame-off hold = 0, last off-time = 1 ms, and the effective boiler temperature `_pwm_effectiveBoilerTemp` back to 0, as thermo-nova's `pwm.reset()` does (SATcontrol.ino:3728, SATcontrol.ino:959-968; pwm.py:100). The variable also starts at 0 from its static initialiser (SATcontrol.ino:686).
3. The mode is set to OFF, then to CONTINUOUS if SAT is enabled (SATcontrol.ino:3691, SATcontrol.ino:3721-3723). PWM is never restored after a restart. The requested control mode (`iRequestedControlMode`) lives in the transient state struct and starts at 0 = "auto" (SATtypes.h:206-208).
4. If an OpenTherm command interface exists, `CS=0` is sent to release any old PIC override (SATcontrol.ino:3702-3710); otherwise it is sent on the first loop pass where one exists (SATcontrol.ino:4583-4589). With SAT enabled, `PM=3`, `PM=15`, `PM=48` and, for some manufacturers, `MI=500` are queued (SATcontrol.ino:3724-3733).
5. The control timer is set to `iControlInterval` (default 30 s) (SATcontrol.ino:3719, SATtypes.h:444).

On the first tick, `bActive` becomes true and an OFF mode becomes CONTINUOUS (SATcontrol.ino:4616-4619). From then on the mode is CONTINUOUS until one of the enable paths in step 3 fires. If `bForcePWM` is set, the first tick that reaches SATcontrol.ino:4923 already switches to PWM (SATcontrol.ino:4923-4925). Because the last flame-off time was set to the init time (SATcycles.ino:192), the first OFF phase after boot is timed from boot. (Removed in TASK-1332, 2026-10-23: the Force PWM option no longer exists.)

#### 2. Every tick (default every 30 s)

The whole of `satControlLoopRun()` runs on every main-loop pass (SATcontrol.ino:4504-4508; called from OTGW-firmware.ino:1157, search result). Before the tick gate it does, in this order: return if SAT is disabled or a flash runs (SATcontrol.ino:4568-4573), return if safety tripped (SATcontrol.ino:4581), flame-edge detection and cycle sampling (SATcontrol.ino:4591-4599), the PIC CS keepalive (SATcontrol.ino:4607-4608), the 3 s suppression refresh (SATcontrol.ino:4610-4611). Then it returns unless the tick timer is due (SATcontrol.ino:4614).

The tick timer is `DECLARE_TIMER_SEC(timerSATControl, settings.sat.iControlInterval, CATCH_UP_MISSED_TICKS)` (SATcontrol.ino:582); `initSAT()` (SATcontrol.ino:4277) and `settingStuff.ino:1005` re-set the interval without a type, so it runs as `SKIP_MISSED_TICKS`. The interval is clamped to 10-300 s when set (settingStuff.ino:1001, search result).

On a tick, in this fixed order:

1. `bActive = true`; OFF becomes CONTINUOUS (SATcontrol.ino:4616-4619).
2. DHW is read. **If DHW is active the tick returns here** (after an optional `SW=`), before any PWM, CS, MM or CH code (SATcontrol.ino:4621-4652).
3. Invalid room temperature: the tick goes on, curve only (TASK-1364); it does not return and does not trip.
4. Boiler fault flag: the tick returns (SATcontrol.ino:4716-4719).
5. Thermal safe fallback (after 2 h without a real room temperature): CS = 45 °C capped, MM = `iMaxRelModulation`, CH = 1, return (SATcontrol.ino:4729-4746, SATcontrol.ino:82-83).
6. Summer simmer active: CS = 10, MM = `iMaxRelModulation`, CH = 0, return (SATcontrol.ino:4764-4776).
7. All valves closed: CS = 10, MM = `iMaxRelModulation`, CH = 0, return (SATcontrol.ino:4778-4790).
8. PID and heating curve give `pidOutput` (SATcontrol.ino:5529-5538), the zone block may raise it to the zones output or cap it (SATcontrol.ino:5540-5561).
9. Clamp `pidOutput` to [10, `satEffectiveMaxSetpoint()`] (SATcontrol.ino:4908-4912).
10. If `pidOutput >= satGetMaxSetpoint()` and the mode is PWM, switch to CONTINUOUS (SATcontrol.ino:4917-4920).
11. If `bForcePWM` and the mode is not PWM, switch to PWM (SATcontrol.ino:4923-4925). (Removed in TASK-1332, 2026-10-23: the Force PWM option no longer exists.)
12. PWM mode: `finalSetpoint = satApplyPWM(pidOutput)` (step 4 and 5 below); otherwise `satApplyContinuous(pidOutput)` (SATcontrol.ino:4928-4933).
13. Clamp `finalSetpoint` to [10, effective maximum] and store it as `_sat_lastControlCS` (SATcontrol.ino:4935-4939).
14. Solar gain: no longer here. The offset is subtracted from `pidOutput` before the clamp of item 9 (SATcontrol.ino:5137-5143) (corrected 2026-10-24, TASK-1351); TASK-1323.
15. Auto-switch check, only if `pidOutput > cold setpoint`, for every source (SATcontrol.ino:5536-5546, details in step 3).
16. `bModSuppressed = false` (SATcontrol.ino:4955-4957).
17. Compute MM with `satComputeMM()`, using the mode **after** the auto-switch of item 15 (SATcontrol.ino:4959-4967, details in step 6).
18. Cold cutoff: if `pidOutput <= cold setpoint`, `finalSetpoint = 10`, MM = `iMaxRelModulation`, and `satPwmDisableOnHeaterOff()` leaves PWM (SATcontrol.ino:5206-5214, SATcontrol.ino:1100-1109) (corrected 2026-10-24, TASK-1351).
19. Send, in this order: CS, MM, CH from `satChEnableDecide` (SATchEnable.h:46-52, SATcontrol.ino:5223) (corrected 2026-10-24, TASK-1351); for Immergas also `TP=11:12=<finalSetpoint>`; optionally `TC=` (SATcontrol.ino:5216-5236). Each of CS, MM, CH is only queued when it changed (CS at 0.1 °C resolution) or when 300 s passed since the last send (`SAT_CMD_REFRESH_MS`, SATcontrol.ino:427, and the `satEnqueueIfChanged*` helpers) [lines refreshed 2026-10-24, TASK-1351].

Consequence of the order in items 15 and 17: on the tick where the auto-switch moves CONTINUOUS to PWM, CS is still the continuous value but MM is already the PWM value (0 for a gas boiler); on the tick where it moves PWM to CONTINUOUS, CS is still the PWM value and MM is already the non-PWM value (SATcontrol.ino:4929-4933, SATcontrol.ino:4951-4967).

The 3 s refresh between ticks is described in step 5.

#### 3. When PWM is used: automatic switching between continuous and PWM

Every mode change goes through `satSetControlMode()`, which calls `satPwmReset()` whenever the mode actually changes, both on entering and on leaving PWM (SATcontrol.ino:933-938). One path writes the mode directly: the tick start, which turns OFF into CONTINUOUS (SATcontrol.ino:4617-4619), and `initSAT()` (SATcontrol.ino:3691, SATcontrol.ino:3722).

PWM is **entered** by exactly these code paths (a search for `satSetControlMode` lists all callers: SATcontrol.ino:1325 (heater off), 2617 (`satDisable`), 2785, 2788, 2791 (control_mode command), 5520 (PID at system max) and SATcycles.ino:1059 (overshoot switch), 1084 and 1096 (underheat and saturation exits) [refreshed 2026-10-28, TASK-1420]):

| Trigger | Conditions (all must hold) | Citation |
|---|---|---|
| Sustained overshoot (per tick) | `pidOutput > cold setpoint` (every source, heat pumps included, Sergeant D entry 87); mode CONTINUOUS; not DHW and not within 300 s after DHW; `flow >= pidOutput + fOvershootMargin` (equality counts, entry 86 Q43 A); at least 60 000 ms of tick time (`tickMs - start`, one `millis()` read per tick) since the first qualifying tick, with every tick in between qualifying (entries 41, 86 Q46 B, 109) | SATcontrol.ino:5531-5535, SATcycles.ino:1043-1062, SATcycles.ino:29, SATcycles.ino:33 [refreshed 2026-10-28, TASK-1420] |
| (removed in TASK-1332) Force PWM | `bForcePWM` set and the mode is not PWM, checked every tick | SATcontrol.ino:4923-4925 |
| Manual command | `control_mode` value "pwm" or 2, while SAT is enabled and not tripped | SATcontrol.ino:2238, SATcontrol.ino:2242-2244 |

PWM is **left** by exactly these code paths. Each one calls `satPwmReset()` through `satSetControlMode()`:

| Trigger | Conditions | Citation |
|---|---|---|
| Requested at system maximum | Mode PWM and `pidOutput >= satGetMaxSetpoint()` (system cap: 62 °C radiators, 45 °C underfloor, 40 °C heat pump) | SATcontrol.ino:4917-4920, SATcontrol.ino:228-235 |
| Sustained underheat (per tick) | every source; `pidOutput > cold setpoint`; mode PWM and ON phase, outside the DHW guard; `flow <= pidOutput - 3.0` (equality counts, entry 86 Q44 A); at least 180 000 ms of tick time since the first qualifying tick (no `dt`, no gap bail-out; entries 86, 95, 109) | SATcontrol.ino:5564-5568, SATcycles.ino:1072-1090, SATcycles.ino:30-31 [refreshed 2026-10-28, TASK-1420] |
| Sustained saturation (per tick) | Same gate as underheat, evaluated when underheat did not fire; last computed off-time is 0 ms; at least 300 000 ms of tick time since the first qualifying tick | SATcycles.ino:1092-1102, SATcycles.ino:32 [refreshed 2026-10-28, TASK-1420] |
| Manual command | `control_mode` "continuous"/1 or "auto"/0 (both set CONTINUOUS), while enabled and not tripped | SATcontrol.ino:2239-2241, SATcontrol.ino:2245-2247 |
| SAT disabled or safety trip | `satDisable()` sets mode OFF | SATcontrol.ino:2160-2165 (call sites in the note below) |

Note: the disable call sites are SATcontrol.ino:4561 (fallback lifted), SATcontrol.ino:4568-4571 (SAT disabled or flashing), SATcontrol.ino:4680 (room temperature lost) and SATcontrol.ino:5002 (no command interface for 5 ticks).

Leave paths since TASK-1327 (entry 12): valves closed and `pidOutput` at or below the cold setpoint call `satPwmDisableOnHeaterOff()`, which sets CONTINUOUS and resets PWM, the sustain guards and the previous CS (SATcontrol.ino:1100-1109, SATcontrol.ino:5008-5021, SATcontrol.ino:5206-5214). Not a leave path: summer simmer sends CS = 10, MM = max, CH = 0 but leaves the mode at PWM and does not reset PWM (SATcontrol.ino:4994-5006). DHW leaves the mode untouched and returns before the schedule (SATcontrol.ino:5181-5186); it only pauses a running ON phase (TASK-1422, see the hot-water paragraph in the PWM section) (corrected 2026-10-24, TASK-1351).

The requested mode "pwm" is not re-applied on later ticks: `iRequestedControlMode` is only written at SATcontrol.ino:2240-2246 and only read for publishing at SATcontrol.ino:2628 (search for `iRequestedControlMode`). So after a manual "pwm", the auto-switch can still move the mode back to CONTINUOUS. With `bForcePWM` set, by contrast, PWM is re-entered on the next tick (SATcontrol.ino:4923-4925), and when `pidOutput >= system cap` the same tick switches to CONTINUOUS at item 10 and back to PWM at item 11, so PWM is reset twice in that tick (SATcontrol.ino:4917-4925, SATcontrol.ino:936). (Removed in TASK-1332, 2026-10-23: the Force PWM option no longer exists.)

`pidOutput` is clamped to `satEffectiveMaxSetpoint()` (SATcontrol.ino:4912), which is never above `satGetMaxSetpoint()` (SATcontrol.ino:244-246). So the "requested at system maximum" path can only fire when the effective maximum equals the system cap, that is when the boiler MaxTSet (or the 75 °C default used when MaxTSet < 30), the hard ceiling and `fMaxSetpoint` are all at or above the system cap (SATcontrol.ino:240-252, SATcontrol.ino:57, SATcontrol.ino:62-64).

How a "sustained" timer works in the firmware (TASK-1420; Sergeant D entries 86, 88, 95, 108). The three timers (overshoot, underheat, saturation) share one helper, `satSustainedFor()` (SATcycles.ino:152-160), the port of thermo-nova `sustained_runtime()` (`helpers.py:51-56`): a running flag plus the start time; a timer that is not running, or whose start lies after the tick, starts at the tick time and never fires on that sample, otherwise it fires once `tickMs - start` reaches the limit. There is no `dt` and no gap bail-out. The clock is the control tick's time `_sat_tickMs`, one `millis()` read per tick (SATcontrol.ino:5200, right after `DUE(timerSATControl)` at :5195; details in the tick-time item below); the switch code itself never reads `millis()`. `satCycleCheckOvershootSwitch()` and `satCycleCheckPwmExits()` replace the former `satCycleCheckAutoSwitch()`.

- **Overshoot (continuous -> PWM), `satCycleCheckOvershootSwitch(requested, tickMs)` (SATcycles.ino:1043-1062; entries 41, 86 Q43 A and Q46 B, 88).** Called once per tick from the gate at SATcontrol.ino:5531-5535 (`pidOutput` > cold setpoint, every heating source, heat pumps included, entry 87), BEFORE `--- Apply control mode ---` (SATcontrol.ino:5537) and after the maximum-setpoint check, as thermo-nova (`heating_control.py:203-215`); so `satApplyPWM()` sends the first PWM CS on the switch tick (Q47 A, entry 88). Flow `>=` requested + `fOvershootMargin` counts, equality included (thermo-nova resets only on `flow < threshold`, `heating_control.py:288-291`; the margin stays 2.0, thermo-nova's own is 3.0). The switch comes once `tickMs - start >= 60 000 ms`, i.e. on the first tick at least 60 s after the first qualifying tick (30 s interval: the third qualifying tick, 60 s later; 120 s interval: the second qualifying tick, 120 s later). It then calls `satSetControlMode(SAT_MODE_PWM)` and `satCycleResetSustainGuards()` (SATcycles.ino:1059-1060; thermo-nova `heating_control.py:304-305`). A tick that does not qualify, a tick outside CONTINUOUS (`:264-266`) and a tick within the hot-water guard (`:268-274`, below) clear the start. A control tick that never reaches the call clears all three starts through `satCycleSkipAutoSwitch()` (SATcycles.ino:193-196, which now calls `satCycleResetSustainGuards()`): boiler fault (SATcontrol.ino:5304), thermal-safe fallback (:5332), summer simmer (:5363), the gate's else branch for requested <= cold setpoint (:5534; thermo-nova `:280-282`), `satDisable()` (:2618), the first tick after it (mode OFF, :5205) and every mode change through `satSetControlMode()` (:1296), which covers the control_mode command. Of these, boiler fault, thermal-safe fallback, summer simmer, the mode command and switch-off / OpenTherm loss / re-enable are firmware choices without a thermo-nova counterpart (thermo-nova keeps the start when HVAC is off, `climate.py:544-545`). Valves closed and the cold cutoff clear all three through `satCycleResetSustainGuards()` (`satPwmDisableOnHeaterOff()`).
- **Tick time (entries 86 Q46 B, 95 Q46b, 109; derived from the code, not measured on a device).** `_sat_tickMs` is one `millis()` read per control tick, taken right after the timer gate and used by all three sustained timers and the DHW guard (SATcontrol.ino:5195, :5200). The timer is declared `CATCH_UP_MISSED_TICKS` (SATcontrol.ino:582), but `initSAT()` (SATcontrol.ino:4277, called from setup) and `settingStuff.ino:1005` set the interval with `CHANGE_INTERVAL_SEC` without a type, which makes the type 0, `SKIP_MISSED_TICKS` (`safeTimers.h:96`, `:131-135`). In the skip branch the next due time is now + interval, so two ticks are never closer than one interval and a tick that runs late pushes every later tick later; there is no catch-up grid and a stalled loop does not run missed ticks back to back. A sustained timer therefore fires on the first tick at least its length after the first qualifying tick (Q46 B), and never earlier. thermo-nova clears the start when the flow temperature is None (`heating_control.py:284-286`); `satGetFlowTemp()` returns the last stored `Tboiler` without a missing state, so a stale reading could start or keep the timer (not traced, pre-existing).
- **Underheat and saturation (PWM -> continuous), `satCycleCheckPwmExits(requested, tickMs)` (SATcycles.ino:1072-1104; entries 86 Q44 A, 95 Q46b, 108).** Called after `satApplyPWM()` (gate at SATcontrol.ino:5564-5568), because saturation reads this tick's off time. Both timers reset (SATcycles.ino:1074-1078) when the mode is not PWM (thermo-nova `heating_control.py:308-311`), while the boiler DHW-active bit is set and for 300 s after it (`:313-315`, entry 108), and outside the PWM ON phase (`:321-323`); at or below the cold setpoint the gate calls `satCycleResetSustainGuards()` (`:317-319`). Underheat counts while flow `<=` requested - 3 and resets on a higher flow (`heating_control.py:337-340`; the firmware used a strict `<` before TASK-1420); it fires 180 s after the first qualifying tick (`:342-345`, `const.py:22`). Only when underheat did not fire, saturation counts while `satPwmLastOffTimeMs() == 0` (reset on a longer off time, `:359-362`) and fires 300 s after the first such tick (`const.py:23`). The former 60 s `dt` bail-out and the float second accumulators are gone, so both switches fire at control intervals above 60 s too.

DHW guard (TASK-1420, entry 108): `satCycleNoteDhwTick(tickMs)` (SATcycles.ino:200-205) is called from the DHW early return of the control tick (SATcontrol.ino:5241). It refreshes `_sustain_dhwEndMs` with the tick time and clears all three starts; before TASK-1420 the guard timestamp was refreshed only inside a function that the DHW tick never reached, so the 300 s guard never armed (audit 2026-10-24 #11). `satDhwGuardActive(tickMs)` (SATcycles.ino:164-168) is `bDhwActive` or (a draw has been seen and `tickMs - _sustain_dhwEndMs < 300 000 ms`); it blocks the overshoot switch (`heating_control.py:272-274`) and resets the PWM exits (`:313-315`). The guard is armed only once a draw has been seen (`_sustain_dhwSeen`, SATcycles.ino:146, set in `satCycleNoteDhwTick()`), so it is inactive from boot and, when no draw ran, also in the 300 s before each wrap of the tick clock (about every 49.7 days); a draw older than 49.7 days can still alias into the window (accepted).

How cycles are classified (TASK-1387, Sergeant D entry 55: `cycles/classifier.py` exactly). A cycle runs from a flame-on edge to the next flame-off edge (SATcycles.ino:711-860). At flame-off `_cycleClassify()` (SATcycles.ino:655-708) gets: duration; the 1 Hz sample count; the cycle kind (SATcycles.ino:571-581); whether a hot-water sample lies in the tail (SATcycles.ino:812-813); PWM enabled (mode is PWM), PWM IDLE (`satPwmIsIdle()`, SATcontrol.ino:1143) and PWM ON (`bPwmFlameRequested`) at flame-off; the last PWM on-time; the 90th and 50th percentiles of the tail ring (with fewer than 10 samples it falls back to the per-cycle ring or max/min, SATcycles.ino:794-800); `controlSp` = `fFinalSetpoint` at flame-on (SATcycles.ino:731); `requestedSp` = `_sat_requestedSetpoint` at flame-off (SATcycles.ino:807). Classification order, as `cycles/classifier.py` (`cls`):

1. Duration <= 0 (cls:19-20), or fewer than 3 samples (firmware: tracker.py:98-100 drops such a cycle): INSUFFICIENT (SATcycles.ino:661).
2. Kind UNKNOWN: UNCERTAIN (cls:22-23, SATcycles.ino:664).
3. A hot-water sample in the tail: UNCERTAIN (cls:25-26, SATcycles.ino:667).
4. PWM enabled and IDLE: UNCERTAIN (cls:28-29, SATcycles.ino:670).
5. PWM (cls:40-66): `requestedSp <= 0` (stand-in for a missing requested-setpoint error) UNCERTAIN; `tailP90 - requestedSp <= -3.0` UNCERTAIN below the cold setpoint, else UNDERHEAT_PWM; only while the PWM ON phase runs: on-time <= 0 or duration < 0.8 x on-time SHORT; otherwise GOOD. PWM has no OVERSHOOT (SATcycles.ino:675-691).
6. Continuous (cls:69-90): `controlSp <= 0` (stand-in for a missing control-setpoint error) UNCERTAIN; `tailP90 - controlSp >= fOvershootMargin` OVERSHOOT, tested first and without an overshoot-time condition; `tailP50 - controlSp <= -3.0` UNCERTAIN below the cold setpoint (requested setpoint, else the control setpoint), else UNDERHEAT; otherwise GOOD (SATcycles.ino:695-707).

There is no SHORT for a continuous cycle and no rule that a short cycle is UNCERTAIN (both removed in TASK-1387).

**No cycle class changes the control mode.** The result goes to the history, the EMA fractions and the 4 h / 24 h windows only (SATcycles.ino:792-835, SATcycles.ino:551-602); the full list of `satSetControlMode` callers above contains no call from the flame-off path.

#### 4. The PWM state machine (`satPwmSchedule`, once per tick in PWM mode)

**Citation exception (TASK-1273).** The passages in this section about the boiler temperature BT of the duty formula (the effective boiler temperature) cite `SATcontrol.ino` as changed by TASK-1273: the working tree, not commit `51df6b486`. Lines above 729 are the same in both. From line 729 on the numbering differs by up to 38 lines. The step list of `satPwmSchedule` and the passages about BT use the working tree; the other passages of this section, such as the CS override in step 5, still use the numbering of `51df6b486`. Exception to the exception (TASK-1366): items 2 and 5 to 9 of the step list, the paragraph after it, the **Timer reference** bullets, the thresholds block, the heat-pump paragraphs, the range table and the PWM rows of the constants table cite the working tree with TASK-1366 applied (`satPwmSchedule()` starts at SATcontrol.ino:843).

`satPwmSchedule(pidOutput)` is called only from `satApplyPWM()` (SATcontrol.ino:903), which is called only from the tick in PWM mode (SATcontrol.ino:4929-4930). So the state machine does not run on ticks that return early (DHW, fault, thermal safe, summer simmer, valves closed, invalid room temperature) (SATcontrol.ino:4635-4790). A draw in an ON phase freezes its ON clock and the end of the draw adjusts the anchor (SATcontrol.ino:5181, :5190; TASK-1422).

Hot water inside an ON phase (TASK-1422; Sergeant D entries 95, 103, our reading entry 104): `satPwmOnDhwStart()` (SATcontrol.ino:909) freezes the ON clock on the first tick with the DHW bit (used ON time stored); `satPwmOnDhwEnd()` (SATcontrol.ino:937, called at SATcontrol.ino:5190) moves the anchor to now minus the used time and, with the flame lit, resumes at once (entry 103 C). Otherwise `satPwmDhwHold()` (SATcontrol.ino:954) holds the clock until the CH flame burns or 180 s after the last draw end have passed (entry 103 A); a second draw in that wait restarts the 180 s from its end. The resume (`satPwmDhwResume()`, SATcontrol.ino:924) sets `_pwm_dhwResumed` and clears `_pwm_waitingForFlame`: no first-ignition sync (compare pwm.py:186-193), no new `_pwm_flameOnMs` anchor (the pwm.py:144-146 path), no start-up EMA and no minimum-burn hold (SATcontrol.ino:1022, :1028, :1113), so BT stays as before the draw (entry 103 B). A draw during the first-ignition wait is treated the same way (entry 104). An OFF phase is not touched: the off wait stays timed from the ON to OFF switch (entry 97). thermo-nova has no pause; its `update()` runs during hot water (heating_control.py:202-215). Phases: ON (`bPwmFlameRequested` true) and OFF (false) (SATcontrol.ino:835-887).

Per call, in order (SATcontrol.ino:727-889):

0. **Unknown flow temperature**: if `satGetFlowTemp() <= 0` (MsgID 25 not seen), the function calls `satPwmReset()`, sets `fPwmDutyCycle` to 0 and returns, so none of the items below run (SATcontrol.ino:741-748). `satApplyPWM()` then returns the continuous setpoint `satApplyContinuous(pidOutput)` instead of any PWM setpoint (SATcontrol.ino:905-907). thermo-nova sets PWM to IDLE when the flow temperature is missing (pwm.py:122-128), and its control mode then reads continuous (heating_control.py:148-152).
1. **Duty cycle** from the current `pidOutput` and the boiler temperature BT (`effectiveTemp`), clamped to [0, 1], published as `fPwmDutyCycle` (SATcontrol.ino:729-756; formulas below). BT is the filtered `_pwm_effectiveBoilerTemp`, or the live flow temperature while that value is still 0, and never below base + 1 (SATcontrol.ino:749-751).
2. **Time thresholds**: minimum on-time 180 s for every heating source, the PWM period `upperMs = satGetPwmPeriodSec() x 1000` (heat pump: `iHpCycleSeconds`, 1800 or 2400 s; otherwise 3600 / cycles per hour), `maxMs = 2 x upperMs` (SATcontrol.ino:875-877, SATcontrol.ino:357-374, SATcontrol.ino:345-355; thermo-nova pwm.py:53-55; Sergeant D entries 40 and 50, TASK-1366).
3. **Effective temperature filter**: if the flame is on, the code is not waiting for ignition, `_pwm_flameOnMs` is not 0 and less than 180 000 ms (`PWM_IGNITION_TIMEOUT_MS`, thermo-nova `HEATER_STARTUP_TIMEFRAME`) have passed since it, `_pwm_effectiveBoilerTemp = 0.3 x flow + 0.7 x _pwm_effectiveBoilerTemp` (SATcontrol.ino:764-771, SATcontrol.ino:696; pwm.py:144-149). `_pwm_flameOnMs == 0` means "no ignition in this ON phase", as `_flame_on_at is None` does in thermo-nova, so nothing is filtered in the OFF phase or before the first ignition (pwm.py:144-146). After the window the value is held, not replaced by the raw flow, until the next ON phase seeds it again (SATcontrol.ino:780, SATcontrol.ino:881). Sergeant D confirmed the 180 s window on 2026-10-22 (`docs/sat/sergeant-d-corrections.md` entry 1).
4. **Flame sync**: if the flame is on and the code was waiting for ignition, waiting ends and the ignition timer is cleared; only when `_pwm_flameOnMs` is 0 are `_pwm_flameOnMs = now` and `_pwm_effectiveBoilerTemp` set to the live flow temperature (SATcontrol.ino:773-782; pwm.py:189-191). In thermo-nova the wait exists only from an ON start without a flame (pwm.py:163), so a relight after a mid-phase flame-out has no sync and `_flame_on_at` stays (pwm.py:145-146). The firmware does set the wait after a flame loss inside the ON phase (see the end of this item), and the zero test keeps such a relight from reseeding BT or reopening the window. If the flame is off, the code is not waiting and the phase is ON, waiting starts and the ignition timer starts now (SATcontrol.ino:783-786). Item 3 comes before item 4 in the code, so the call that first sees the flame takes no filter sample; the reseed sets the value anyway.
5. **Duty thresholds**: `dutyLower = minOn / upper`, `dutyUpper = 1 - dutyLower`, `dutyMin = dutyLower / 2`, `dutyMax = 1 - dutyMin` (SATcontrol.ino:904-907).
6. **On/off times** from the duty (SATcontrol.ino:909-944; table below). **Range 5** (`duty >= dutyMax`) is special: phase = ON, last off-time = 0, last on-time = `maxMs`, and the function **returns** before the state machine (SATcontrol.ino:982-992). In range 5 there is no on-time expiry and no OFF phase. A range-5 ON phase that starts from OFF goes through the same ON start as item 9, `satPwmBeginOn()` (SATcontrol.ino:987, SATcontrol.ino:867-885; thermo-nova range 5 is on = 2P, off = 0 through the same OFF->ON start, pwm.py:160-167, pwm.py:296-301): it is counted once in the per-hour counter at the start, lit or not (SATcontrol.ino:867-885; pwm.py:160), and with the flame burning its ON time and start-up window count from that start and BT is reseeded; with the flame off it waits for ignition. So when the duty later drops into ranges 1-4 the ON phase is timed from the range-5 start (`_pwm_lastUpdateMs`, set in `satPwmBeginOn()`, SATcontrol.ino:870), not from an older flame-on (host test `test_entry_in_range5_then_range3_keeps_its_on_time`). The hourly limit does not hold a range-5 start back (thermo-nova checks its cap before every OFF->ON start, pwm.py:156).
7. Store the off-time and on-time for the saturation check and the cycle classifier (SATcontrol.ino:945-946).
8. **ON phase** (SATcontrol.ino:1021-1035; pwm.py:182-209). Both phases count from one anchor, `_pwm_lastUpdateMs`, thermo-nova's `_last_update`; `elapsedMs = now - _pwm_lastUpdateMs` (SATcontrol.ino:1019-1020). The anchor is set at `satPwmReset()` (SATcontrol.ino:1148; pwm.py:91), at every OFF/IDLE->ON start in `satPwmBeginOn()` (SATcontrol.ino:870; pwm.py:162), at the first ignition while waiting (SATcontrol.ino:963; pwm.py:188) and at ON->OFF in `satPwmEndOn()` (SATcontrol.ino:890; pwm.py:208). No flame edge moves it, so a hot-water flame-off or flame-on cannot (entries 91, 92).
   - **Synced this step** (SATcontrol.ino:1023; pwm.py:187-193): the step that sees the first flame of a waiting phase moves the anchor and the start-up window to now, reseeds BT, and returns.
   - **Waiting for ignition** (SATcontrol.ino:1024-1029; pwm.py:194-199): the phase waits up to `HEATER_STARTUP_TIMEFRAME` 180 s from the anchor (the phase start). After that the wait ends, a debug line is logged ("no flame within 180s, starting the ON timer anyway") and the ON timer runs from the same anchor. There is no abort (entry 80, Q35 A); the old 180 s ignition abort is gone.
   - **Flame-on hold and expiry** (SATcontrol.ino:1030-1036; pwm.py:201-209): while a flame has been on for less than 180 s since the phase's first flame (`_pwm_flameOnMs`), the phase stays ON (pwm.py:201-203). Otherwise, when `elapsedMs >= onTimeMs`, the phase goes OFF through `satPwmEndOn()`: request cleared, stored hold cleared, `_pwm_flameOnMs` cleared (pwm.py:206), anchor set to now. A relight after a mid-phase flame-out keeps the anchor and the window (pwm.py:144-146). A first flame that appears only after the wait timed out sets `_pwm_flameOnMs` (SATcontrol.ino:943-946; pwm.py:144-146) so the 180 s hold applies, but it does not move the anchor (pwm.py:188 fires only while waiting); test_d7/test_d8 in `tests/test_sat_pwm_anchors.py`.
9. **OFF phase** (SATcontrol.ino:1037-1066; pwm.py:154-155): stored hold cleared. The OFF wait counts from the ON->OFF switch, `elapsedMs >= offTimeMs` (anchor `_pwm_lastUpdateMs`, set by `satPwmEndOn()`); the boiler's own flame-off, hot-water flame-offs included, does not move it (entries 91, 92; host test `test_d1_off_wait_ignores_hot_water_flame_offs`). A new ON phase starts only when `onTimeMs >= minOnMs` (180 s) and (the PWM entry applies or the off wait has passed) (SATcontrol.ino:1047; pwm.py:155; Q38, TASK-1383); with on-time 0 (range 1 without flame) no ON phase starts. **PWM entry** (TASK-1369, entry 43): on the first schedule step after `satSetControlMode()` switched into PWM (`_pwm_entryPending`) the step may start ON without the off wait when the on-time is at least 180 s (SATcontrol.ino:1045), as thermo-nova starts ON from IDLE (pwm.py:154-155, const.py:18). The flag is consumed by that step, except that it stays armed while the flow temperature is missing (thermo-nova stays IDLE, pwm.py:123) and after the hour-limit return of an entry ON start (SATcontrol.ino:1061; pwm.py:155-158). If the start applies: when the rolling-hour PWM start limit is reached, log once and stay OFF (SATcontrol.ino:1052), except for the entry ON start with the flame already burning, which is never blocked (maintainer decision 2026-10-26, provisional until Sergeant D answers Q48); otherwise `satPwmBeginOn()` (SATcontrol.ino:1065). The new phase is recorded once in the per-hour counter at the start, lit or not, inside `satPwmBeginOn()` (pwm.py:160; entries 90, 95; TASK-1401, TASK-1421). With the flame already burning there is no wait, `_pwm_flameOnMs` is set to now and BT is reseeded (pwm.py:164-167). With the flame off, the ignition wait starts and `_pwm_flameOnMs` is 0 (pwm.py:163-164).

The on-time and the off-time are recomputed on every tick, so the phase ends at the first tick where the newly computed time has passed (SATcontrol.ino:1033, SATcontrol.ino:1047). Phase changes happen only on ticks, so an ON or OFF phase can run up to one control interval longer than its computed time (derived from SATcontrol.ino:4653 and SATcontrol.ino:904).

**Timer reference.** Both phases are timed from the one anchor `_pwm_lastUpdateMs` (thermo-nova `_last_update`), set at the phase switches and the first ignition, not from the cycle tracker's flame edges (SATcontrol.ino:1019-1020; pwm.py:91, :155, :162, :188, :205, :208; entries 80, 91, 92). The earlier flame-edge timing, with its flame-on and flame-off reference points, `_pwm_onStartMs`, `_pwm_offStartMs`, `_pwm_waitForFlameStartMs` and the 180 s ignition abort, is gone (TASK-1380, TASK-1382, TASK-1383). Consequences, read from the code:

- An ON phase that starts while the flame is off waits for the ignition for up to 180 s, then counts its on-time from the ignition (`_last_update` = ignition, pwm.py:186-188) or, when no flame came, from the phase start (pwm.py:197-199, no abort). The 30 s control tick adds up to one tick to each edge. Host evidence: `tests/test_sat_pwm_anchors.py` D2, D3, D4 (the old `test_r3_on_phase_waits_for_flame` expected failure in `tests/test_sat_hp_pwm.py` is now a passing test).
- A flame loss and relight inside an ON phase keep the anchor, the start-up window and BT; there is no second wait (pwm.py:144-146; SATcontrol.ino:958-967; host test D6 in `tests/test_sat_pwm_anchors.py`).
- The per-hour limit counts PWM's own ON phases in the last 3600 s (TASK-1401, Sergeant D entry 59, pwm.py:159-160): every ON start (OFF/IDLE->ON, range 5, the entry ON phase) is recorded once through `satPwmBeginOn()` -> `satCycleRecordPwmStart()`, at the start, lit or not (SATcontrol.ino:849-885, SATcycles.ino:312-317). Only an ON start counts; a flame edge never adds a count: a CH ignition, a relight inside a phase, a hot-water flame and flame starts outside PWM are not counted, and the flame-on edge handler does not write the ring (entries 59, 89, 90, 95). An ON start is not made while a flame that was lit during a hot-water draw (boiler DHW-active bit set) still burns: the OFF-to-ON start and the duty range 5 start are held ("PWM ON held: hot-water flame still burning", narrated once), and the start comes, and counts, on the first tick after that flame goes out (Sergeant D entries 105 and 106, Q52 D/E; TASK-1424; `_pwm_dhwFlameLit` is set by `satPwmNoteDhwFlameEdge()` when a flame lights with the live DHW bit set (every-loop flame-edge path, so a draw shorter than one control tick counts) and by `satPwmNoteDhwFlame()` in the control loop's DHW branch; any other flame edge, a resume after a draw (entry 103 C) and a new ON start clear it). The hold applies only when PWM drives the setpoint (`satPwmDrivesSetpoint()`: overshoot protection on, no DHW, request above the cold setpoint); without overshoot protection SAT does not switch the burner and nothing is held (entry 107, our reading). A held start keeps the idle state and the PWM-entry skip of the off wait (entry 43). Host tests `test_e4_on_start_at_the_end_of_a_draw_with_flame_lit_is_held_until_the_flame_is_out`, `test_h1`-`test_h4`, `test_k1`-`test_k4`. A CH flame is never held. thermo-nova starts at once and counts at the ON start, also an ON phase whose ignition fails (pwm.py:155-167); the hold is Sergeant D's deviation. The old first-flame counting is gone (TASK-1421); the earlier worry that restarting ON phases would use up the limit no longer applies since the phase anchors (TASK-1382; host evidence: `tests/test_sat_pwm_start_limit.py` `test_slow_ignition_restarts_do_not_use_up_the_limit`). It holds at most 6 entries (SATcycles.ino:37). Reading the count evicts the first stale entry and every older one (SATcycles.ino:297-308), so after a `millis()` wrap an old start cannot look recent again (host test `test_millis_wrap_does_not_revive_old_starts`). The limit is `satGetMaxCyclesPerHour()` for gas boilers and hybrids; a heat pump has none (entry 81) (SATcycles.ino:320-327). The window slides (each start leaves 3600 s after it was recorded) where thermo-nova uses a window that restarts 3600 s after its first update (pwm.py:138-141). The count survives mode changes, SAT disable/enable, a safety trip and the OT-loss hand-back, because only `satCycleInit()` (called from `initSAT()` at boot) clears the ring, where thermo-nova's `reset()` clears it (pwm.py:90); kept because the firmware limit really blocks, so clearing it on every PWM exit would let continuous/PWM switching bypass it. `cycles_this_hour` (REST, MQTT, the SAT page) shows this PWM start count.

**Effective temperature, as coded (TASK-1273).** BT in the duty formula is `_pwm_effectiveBoilerTemp`. It starts at 0 (SATcontrol.ino:686) and `satPwmReset()` clears it to 0, as thermo-nova's `pwm.reset()` does (SATcontrol.ino:967; pwm.py:100). Without a flow temperature (`satGetFlowTemp() <= 0`) `satPwmSchedule()` resets the PWM state and returns before it touches BT, and `satApplyPWM()` returns the continuous setpoint (SATcontrol.ino:741-748, SATcontrol.ino:905-907; pwm.py:122-128). `_pwm_flameOnMs` is the ignition anchor of the current ON phase. The value 0 means "no ignition in this ON phase", as `_flame_on_at is None` does in thermo-nova (pwm.py:145-146, pwm.py:164, pwm.py:206). It is cleared at an ON-to-OFF switch and at an ON start without a flame (SATcontrol.ino:889, SATcontrol.ino:879), and set to now at the first ignition of the phase, at an ON start with a burning flame and at a first flame that appears after the ignition wait timed out (SATcontrol.ino:943-946, SATcontrol.ino:964, SATcontrol.ino:875; pwm.py:144-146, :188). BT is seeded with the live flow temperature at three moments: the first PWM call that sees the flame while the value is still 0 (SATcontrol.ino:749; pwm.py:130-131), the first ignition of an ON phase, which is the flame sync with the anchor at 0 (SATcontrol.ino:775-781; pwm.py:189-191), and an OFF-to-ON start while the flame is already burning (SATcontrol.ino:875-881; pwm.py:164-167). A relight after a mid-phase flame-out keeps BT and does not reopen the window, because the anchor is not 0 and the sync leaves it alone (SATcontrol.ino:778; pwm.py:145-146). Before any seed the duty formula uses the live flow temperature itself (SATcontrol.ino:750; pwm.py:232). It floors BT at `baseOffset + 1`, which only guards against a division by zero or a negative value (SATcontrol.ino:751; pwm.py:235; Sergeant D, `docs/sat/sergeant-d-corrections.md` entry 1). The earlier fixed `baseOffset + 20` replacement and the `baseOffset + 5` threshold no longer exist. The filter (item 3 above) runs on every call where the flame is on, the code is not waiting, the anchor is not 0 and less than 180 000 ms passed since it (SATcontrol.ino:764-771; pwm.py:144-149). Because of the anchor nothing is filtered in the OFF phase (pwm.py:144). The calls are the control ticks, 30 s apart by default, so the window holds about five samples (derived from the interval, not run). Each sample has weight 0.3, so about 17 % of the seed is still in the value when the window closes (0.7 to the power 5; arithmetic, not run). After the window the filtered value is held until the next ON phase seeds it, at its first ignition or at an ON start with the flame already burning (SATcontrol.ino:780, SATcontrol.ino:881). With a control interval of 180 s or more no sample lands and BT stays at the seed (derived from SATcontrol.ino:768). In duty range 5 the ON phase is set without the ON-start step, so a range-5 stint that begins with a lit flame is not reseeded and sets no anchor (SATcontrol.ino:797-802; derived from reading the code, not run).

#### 5. The CS override in PWM mode: flame-off setpoint and modulation suppression

This runs on a tick in PWM mode, right after `satPwmSchedule()` (SATcontrol.ino:868-889). First the "previous CS" is fixed: `prevCS = _sat_lastControlCS` if that is above 10 °C, else `pidOutput` (SATcontrol.ino:870). `_sat_lastControlCS` is the CS of the last tick or refresh; the CS has no solar adjustment any more (SATcontrol.ino:1061, SATcontrol.ino:5182) (corrected 2026-10-24, TASK-1351), and is set to 0 by `satDisable()` (SATcontrol.ino:2166). Before branch 0: if the flow temperature is unknown (`satGetFlowTemp() <= 0`), `satPwmSchedule()` has reset the PWM state and `satApplyPWM()` returns the continuous setpoint `satApplyContinuous(pidOutput)` instead of any branch below (SATcontrol.ino:741-748, SATcontrol.ino:905-907, working tree numbering; pwm.py:122-128, heating_control.py:148-152). Branches, in this order:

| # | Condition | CS result | Citation |
|---|---|---|---|
| 0 | `bOvershootProtection` off, or DHW active, or `pidOutput <= cold setpoint` | `prevCS` (no override) | SATcontrol.ino:872-874 |
| 1 | OFF phase | CS = 10 °C; stored hold cleared | SATcontrol.ino:875-878 |
| 2 | ON phase, flame off | **Flame-off hold**: CS = clamp(return + `fFlameOffOffset`, cold setpoint, `satGetMaxSetpoint()`); stored as `_pwm_flameOffHoldSetpoint` | SATcontrol.ino:879-883, SATcontrol.ino:701-707 |
| 3 | ON phase, flame on, less than `fModSupDelay` since the last real flame-on | **Hold**: the stored hold if it is > 0, else `prevCS` ("pending") | SATcontrol.ino:884-886, SATcontrol.ino:720-723 |
| 4 | ON phase, flame on, delay passed | **Suppression**: stored hold cleared; CS = clamp(flow - `fModSupOffset`, 10, `satGetMaxSetpoint()`) | SATcontrol.ino:887-888, SATcontrol.ino:711-717 |

The DHW part of branch 0 is never true on a tick: a tick with DHW active returns before `satApplyPWM()` (SATcontrol.ino:4865-4882 [lines refreshed 2026-10-24, TASK-1351]). The schedule (step 4) runs before branch 0, so the phases advance even with overshoot protection off (SATcontrol.ino:871-874). Range 5 counts as ON phase here, because it sets `bPwmFlameRequested = true` (SATcontrol.ino:778). The return temperature is used without a validity check (SATcontrol.ino:703); the flow temperature likewise (SATcontrol.ino:713).

After `satApplyPWM()` the tick clamps the result to [10, effective maximum] and applies the cold cutoff; the solar offset was already subtracted from `pidOutput` before (SATcontrol.ino:5178-5183, SATcontrol.ino:5206-5214, SATcontrol.ino:5137-5143) (corrected 2026-10-24, TASK-1351). The CS sent is that final value (SATcontrol.ino:5219).

**The 3 s refresh.** Between ticks, `satPwmSuppressionRefresh()` re-sends the suppression CS every 3 s. It does nothing unless all of these hold (SATcontrol.ino:1045-1056 [lines refreshed 2026-10-24, TASK-1351]): the 3 s timer is due; SAT enabled, active and not tripped; the thermal safe fallback is not active; mode PWM and ON phase; overshoot protection on and `bDhwActive` false (value from the last tick); `SlaveStatus` bits 0 and 2 clear (live fault and DHW bits); valves open and summer simmer not active; flame on and `fModSupDelay` passed since the last real flame-on; the requested setpoint of the last tick `_sat_requestedSetpoint` (PID output after the solar offset, clamped) above the cold setpoint; `fFinalSetpoint > 10` (SATcontrol.ino:1055-1056). Then CS = the same suppression formula, clamped to the effective maximum, stored as `_sat_lastControlCS`, written to `fFinalSetpoint` and queued with the same write-on-change helper; there is no solar term (SATcontrol.ino:1058-1064) (corrected 2026-10-24, TASK-1351): was "`fPidOutput > cold setpoint`" and "solar offset applied"; TASK-1323. The refresh sends **only CS**; it does not send MM or CH and it does not change the phase (SATcontrol.ino:1045-1065). It runs before the tick gate in the same loop pass (SATcontrol.ino:4610-4614).

What triggers suppression: PWM mode, ON phase, flame on, overshoot protection on, and `fModSupDelay` passed since the cycle tracker's last real flame-on edge (SATcontrol.ino:720-723, SATcontrol.ino:884-888, SATcontrol.ino:899-903). The delay uses the same flame-on time as the ON-phase timing (SATcontrol.ino:722, SATcontrol.ino:813).

What is suppressed: only CS is written. CS = measured flow - 1 °C (default) (SATcontrol.ino:713, SATtypes.h:456). `pidOutput` still drives the duty cycle (SATcontrol.ino:734), branch 0 (SATcontrol.ino:872), the auto-switch (SATcontrol.ino:4951-4952) and the cold cutoff (SATcontrol.ino:4971). The flame is not commanded off: CH = 1 as long as the final CS > 10 °C (SATcontrol.ino:4982-4983). The suppressed value is clamped to [10, system cap] and then to the effective maximum (SATcontrol.ino:714-715, SATcontrol.ino:909, SATcontrol.ino:4936-4937).

Holds and timing: the 20 s (default) delay counts from the real flame-on edge (SATcontrol.ino:722, SATtypes.h:455). Inside the delay, the tick returns the stored hold (SATcontrol.ino:885). The refresh then does nothing, because it requires the delay to have passed (SATcontrol.ino:903). After the delay CS follows the flow temperature at the refresh rate of 3 s, as long as the refresh conditions hold, and is recomputed on every tick as well (SATcontrol.ino:480, SATcontrol.ino:907, SATcontrol.ino:887-888). Because the CS command is write-on-change at 0.1 °C, a CS is queued at most every 3 s and only when the rounded value changed or 300 s passed (SATcontrol.ino:383-396, SATcontrol.ino:351).

How suppression ends: there is no timer for it. On the tick where the ON phase expires, CS becomes 10 (branch 1, SATcontrol.ino:816-821, SATcontrol.ino:875-878). When the flame goes off, the refresh stops at once (SATcontrol.ino:903) and the next tick sends the flame-off hold (branch 2). Between that flame-off and the next tick the last suppressed CS stays on the boiler (derived from SATcontrol.ino:903 and SATcontrol.ino:4614). Leaving PWM mode ends it (step 3). When DHW starts (the ON clock is paused, TASK-1422), the refresh stops on the live DHW bit (SATcontrol.ino:901) and ticks return early (SATcontrol.ino:4651), so the last CS stays; on a PIC board the keepalive re-sends that last CS every 30 s while it is 8 °C or more (SATcontrol.ino:403-414, SATcontrol.ino:4608).

The stored hold is cleared at: ON expiry (`satPwmEndOn()`, SATcontrol.ino:885-891), every OFF-phase schedule call (SATcontrol.ino:835), branch 1 (SATcontrol.ino:876), branch 4 (SATcontrol.ino:887) and `satPwmReset()` (SATcontrol.ino:929). So a hold never survives into the next ON phase.

Interaction with the flame: a flame loss in the ON phase switches the next tick's CS to `return + 18 °C` (default) and stores it (SATcontrol.ino:880-882); the schedule starts a new 180 s ignition wait (SATcontrol.ino:762-765). At relight, the new flame-on edge starts a new 20 s hold of the stored value, then suppression (SATcontrol.ino:884-888, SATcycles.ino:714), and also restarts the ON-phase timing (step 4).

Interaction with the PWM minimum times: the minimum on-time (180 s gas) and the suppression delay (20 s) both count from the same cycle-tracker flame-on time (SATcontrol.ino:834-837, SATcontrol.ino:722). `_pwm_flameOnMs` is a separate variable, the ignition anchor used only for the 180 s filter window of the effective boiler temperature (SATcontrol.ino:687, SATcontrol.ino:768, SATcontrol.ino:779, SATcontrol.ino:880).

#### 6. Maximum relative modulation (MM)

Computed every tick that reaches SATcontrol.ino:4960 by `satComputeMM(pwmMode, heatPump, heatPumpOrHybrid, iMaxRelModulation, quirks)` (SATcontrol.ino:4960-4967, SATcontrol.ino:304-314):

1. Heat pump and not PWM: 100.
2. PWM mode (`eControlMode == PWM`, ON and OFF phase alike): heat pump or hybrid gives `iMaxRelModulation`; any other source gives **0**.
3. Otherwise: `iMaxRelModulation` (default 100).
4. Heat pump or hybrid with a result of 0: 100.
5. Geminox quirk (`SAT_QUIRK_MIN_MOD_10`): at least 10. Immergas quirk: at most 80 (SATcontrol.ino:311-312, SATcontrol.ino:127-129).

Then the cold cutoff overrides MM to `iMaxRelModulation` when `pidOutput <= cold setpoint`, after the quirks (SATcontrol.ino:1108, SATcontrol.ino:5206-5214), so an Immergas boiler receives the configured maximum, uncapped, in that case, as in thermo-nova (heating_control.py:234) (corrected 2026-10-24, TASK-1351). The "no relative modulation" quirk does not change MM; it only marks modulation as unreliable (SATcontrol.ino:4801-4805).

MM is sent only through `satEnqueueIfChangedMM()` (SATcontrol.ino:416-427; a search for `MM=` finds only the format string at SATcontrol.ino:421). Its callers are the thermal safe fallback, summer simmer and valves closed (all `iMaxRelModulation`) and the normal tick end (SATcontrol.ino:4741, SATcontrol.ino:4771, SATcontrol.ino:4784, SATcontrol.ino:4981). So during DHW or a fault-flag tick no MM is sent, and the boiler keeps the last MM, for example 0 if PWM was active (SATcontrol.ino:4635-4719).

So for a gas boiler MM = 0 on every PWM tick that reaches the tick end with `pidOutput > cold setpoint`; at or below it the cold cutoff sets `iMaxRelModulation` and leaves PWM (SATcontrol.ino:379-389, SATcontrol.ino:5206-5214) (corrected 2026-10-24, TASK-1351).

#### 7. Continuous mode CS (for comparison)

`satApplyContinuous(pidOutput, previousCS)` (SATcontrol.ino, search the name):

1. Flame off: CS = `pidOutput` (SATcontrol.ino:1161-1165).
2. Flow temperature <= 0 or > 100 (treated as invalid): CS = `pidOutput` (SATcontrol.ino:1167-1172).
3. Flow <= `pidOutput`: CS = `pidOutput` (SATcontrol.ino:1174-1177).
4. Otherwise `minAllowed = flow - fFlowOffset` (default 2.0 °C, SATtypes.h:467). `pidOutput > previousCS`: CS = `max(pidOutput, minAllowed)`; else CS = `max(pidOutput, min(minAllowed, previousCS))` (heating_control.py:485-489; entry 112, TASK-1381). `previousCS` is `_sat_lastControlCS`, see "Continuous-mode control setpoint rule". MM is handled only by step 6.

#### 8. Timeline example (firmware defaults, radiators, gas boiler, `bOvershootProtection` on)

Assumptions for the arithmetic only: `iCyclesPerHour = 3` (default), base offset 27.2 °C, requested `pidOutput` 40 °C constant, the boiler flow temperature constant at 47.2 °C, so BT = 47.2 °C whether it is live, seeded or filtered (an assumed value, chosen so that the numbers stay as in earlier versions of this example), return 30 °C, the flame has been off longer than the off-time when PWM starts, fewer than 3 ignitions in the last hour, control interval 30 s. Constants: SATtypes.h:525, SATcontrol.ino:50, SATcontrol.ino:749-751, SATtypes.h:444.

Duty: (40 - 27.2) / (47.2 - 27.2) = 0.64. Thresholds: upper = 1200 s, max = 2400 s, `dutyLower` = 0.15, `dutyUpper` = 0.85, `dutyMin` = 0.075, `dutyMax` = 0.925. 0.64 is in range 3: on = 0.64 x 1200 = 768 s, off = 432 s (SATcontrol.ino:753, SATcontrol.ino:759-762, SATcontrol.ino:789-792, SATcontrol.ino:819-823). With a flow temperature of 40 °C or less the same request gives a duty of 1 and range 5 applies (arithmetic from SATcontrol.ino:753-755; thermo-nova gives the same, formula at pwm.py:238), so the first stint after boot, when BT is still the live flow of a cold boiler, can run in range 5.

| Time | Event | PWM state | CS sent | MM | CH |
|---|---|---|---|---|---|
| tick N | Auto-switch fires at the end of the tick | Mode PWM, phase reset, PWM entry armed (TASK-1369) | Continuous value of this tick | 0 | 1 if CS > 10 |
| tick N+1 (+30 s) | OFF branch, PWM entry: on-time 768 s >= 180 s, so ON at once without the off wait (entry 43); hour limit checked unless the flame already burns | OFF to ON, waiting for ignition, 180 s timer. With the flame still burning from continuous: ON at once, counted, its ON time timed from this tick, CS flow - 1 (entry 22) | Flame off: 30 + 18 = 48 °C, stored | 0 | 1 |
| t1 (between ticks) | Flame on edge | Cycle tracker flame-on = t1 | 48 °C stays (no new CS between ticks) | 0 | 1 |
| next tick | Flame sync: `_pwm_flameOnMs` = this tick, BT reseeded with the live flow (47.2 °C by assumption) | ON | Hold 48 °C if less than 20 s since t1, else flow - 1 | 0 | 1 |
| t1 + 20 s, then every 3 s | Refresh conditions hold | ON | flow - 1 °C (write-on-change) | 0 (not re-sent by the refresh) | 1 (not re-sent) |
| first tick at or after t1 + 768 s | ON expiry (768 >= 180) | ON to OFF, hold cleared | 10 °C | 0 | 0 |
| t2 | Boiler flame off | Cycle tracker flame-off = t2 | 10 °C | 0 | 0 |
| first tick at or after t2 + 432 s | OFF branch, if fewer than 3 PWM ON phases in the last 3600 s (hot-water ignitions do not count, TASK-1401) | OFF to ON | Flame off: return + 18 °C again | 0 | 1 |

Citations for the rows: SATcycles.ino:976-983, SATcontrol.ino:4962, SATcontrol.ino:833-855, SATcontrol.ino:879-888, SATcontrol.ino:894-918, SATcontrol.ino:812-821, SATcontrol.ino:875-878, SATcontrol.ino:4980-4983, SATcycles.ino:297-326. If the boiler has not ignited by tick N+2, step 4 applies: `sinceFlameStart` is measured from the previous ignition, and if that is at least 768 s ago the ON phase ends at tick N+2 (SATcontrol.ino:813-821).

<a id="s4-formulas"></a>

### Formulas

Duty cycle (SATcontrol.ino:729-756; thermo-nova pwm.py:229-239, formula at pwm.py:238):

```
if flow temperature <= 0: satPwmReset(); duty = 0; return (continuous setpoint is used)  (SATcontrol.ino:741-748, SATcontrol.ino:905-907)
baseOffset    = 20.0 if heating system == UNDERFLOOR else 27.2          (SATcontrol.ino:255-260, SATcontrol.ino:49-50)
effectiveTemp = _pwm_effectiveBoilerTemp if it is > 0, else the live flow temperature   (SATcontrol.ino:750)
if effectiveTemp < baseOffset + 1: effectiveTemp = baseOffset + 1                        (SATcontrol.ino:751)
duty = (pidOutput - baseOffset) / (effectiveTemp - baseOffset)                           (SATcontrol.ino:753)
duty = min(max(duty, 0), 1)
```

Thresholds, recomputed on every call (SATcontrol.ino:875-877, SATcontrol.ino:904-907):

```
minOnMs   = 180 000 ms (every heating source, pwm.py:53)   (SATcontrol.ino:357-363)
cyclesHr  = 2 (heat pump); else iCyclesPerHour if 2..6; else 3 (underfloor) / 4 (radiators)   (SATcontrol.ino:345-355)
upperMs   = iHpCycleSeconds x 1000, 1 800 000 or 2 400 000 (heat pump); else 3 600 000 / cyclesHr   (SATcontrol.ino:365-374)
maxMs     = 2 x upperMs
dutyLower = minOnMs / upperMs
dutyUpper = 1 - dutyLower
dutyMin   = dutyLower / 2
dutyMax   = 1 - dutyMin
```

Heat pump (TASK-1366, Sergeant D entries 40 and 50): at 1800 s the thresholds are lower 0.10, upper 0.90, min 0.05, max 0.95 and `maxMs` 3600 s; at 2400 s they are 0.075, 0.925, 0.0375, 0.9625 and 4800 s, so all five ranges are reachable (duty 0.30 at 1800 s gives 540 s on, 1260 s off). The 2400 s option (1.5 per hour) is Sergeant D's (entry 50) and is not in thermo-nova, whose heat-pump options are 2 or 3 per hour (config_flow.py:686-693); the period is therefore stored in seconds, not as cycles per hour. A hybrid source keeps the gas timing (180 s, 3600 / `iCyclesPerHour`), settled by entry 82. A heat pump has no hourly start limit (Sergeant D entry 81, Q36 A, TASK-1419): `satCycleIsHourLimitReached()` returns false for it (SATcycles.ino:320-327), and `satGetMaxCyclesPerHour()` has no heat-pump branch (SATcontrol.ino:345-353). Its starts are still recorded, so `cycles_this_hour` still counts them. Gas boilers and hybrids keep the limit (entry 59).

Cycle classifier (TASK-1366 side effect, TASK-1387): a PWM cycle that ends during the ON phase is SHORT when it lasted less than 0.8 x the mapped on-time (SATcycles.ino:685-689, fed by `satPwmLastOnTimeMs()` at SATcycles.ino:809), as `cycles/classifier.py:56-64`. For a heat pump the threshold follows the real on-time, for example 432 s at 540 s on.

On and off times, first matching row wins, values cast to whole milliseconds (SATcontrol.ino:909-944):

| Range of duty | onTimeMs | offTimeMs | Citation |
|---|---|---|---|
| duty >= dutyMax (range 5) | phase forced ON; last on-time = maxMs; last off-time = 0; no state machine | | SATcontrol.ino:912-917 |
| duty < dutyMin, flame on and no DHW | minOnMs | maxMs - minOnMs | SATcontrol.ino:918-923 |
| duty < dutyMin, otherwise | 0 | maxMs | SATcontrol.ino:924-928 |
| duty <= dutyLower | minOnMs | minOnMs x (1 - duty) / duty, capped at maxMs - minOnMs | SATcontrol.ino:929-933 |
| duty <= dutyUpper | duty x upperMs, at least minOnMs | upperMs - (duty x upperMs) | SATcontrol.ino:934-938 |
| otherwise (range 4) | minOnMs / (1 - duty) - minOnMs, at least minOnMs | minOnMs | SATcontrol.ino:939-944 |

In range 3 the off-time is computed before the floor on the on-time is applied (SATcontrol.ino:936-938). In range 1 without flame the on-time is 0, so the ON phase ends at the first tick where at least 180 s passed since the last ignition (SATcontrol.ino:952). Range 5 starts at `duty >= dutyMax`; thermo-nova keeps `duty == max` in range 4 (pwm.py:285), so the two differ only at exact float equality.

With the default 3 cycles per hour: upper = 1200 s, max = 2400 s, `dutyLower` = 0.15, `dutyUpper` = 0.85, `dutyMin` = 0.075, `dutyMax` = 0.925 (arithmetic from SATtypes.h:525 and the formulas above). With a heat pump the thresholds are those of the heat-pump paragraph above (TASK-1366). Before TASK-1366 the heat pump used 1800 s as the minimum on-time with the same 1800 s period, which gave `dutyLower` = 1.0 and `dutyMin` = `dutyMax` = 0.5, so only range 5 and range 1 could be reached (audit #9).

Effective boiler temperature seeds, filter, anchor and reset (SATcontrol.ino:749, SATcontrol.ino:764-782, SATcontrol.ino:841, SATcontrol.ino:875-886, SATcontrol.ino:967), in code order. `_pwm_flameOnMs == 0` means "no ignition in this ON phase":

```
if _pwm_effectiveBoilerTemp <= 0 and flame on:
    _pwm_effectiveBoilerTemp = flow                                           (SATcontrol.ino:749)
if flame on and not waiting for ignition and _pwm_flameOnMs != 0
        and (now - _pwm_flameOnMs) < 180 000 ms:
    _pwm_effectiveBoilerTemp = 0.3 x flow + 0.7 x _pwm_effectiveBoilerTemp   (SATcontrol.ino:766-771)
if flame on and waiting for ignition (flame sync):
    waiting ends; if _pwm_flameOnMs == 0:
        _pwm_flameOnMs = now; _pwm_effectiveBoilerTemp = flow                 (SATcontrol.ino:775-781)
ON phase ends (ON to OFF; `satPwmEndOn()` also sets the anchor `_pwm_lastUpdateMs = now`):
    _pwm_flameOnMs = 0                                                        (SATcontrol.ino:889-890)
OFF-to-ON start (not range 5):
    flame on:  no ignition wait; _pwm_flameOnMs = now; _pwm_effectiveBoilerTemp = flow   (SATcontrol.ino:870-876; _pwm_lastUpdateMs = now)
    flame off: ignition wait; _pwm_flameOnMs = 0                              (SATcontrol.ino:877-880; _pwm_lastUpdateMs = now, SATcontrol.ino:870)
satPwmReset():
    _pwm_effectiveBoilerTemp = 0; _pwm_flameOnMs = 0                          (SATcontrol.ino:964, SATcontrol.ino:967)
```

Flame-off hold (SATcontrol.ino:701-707):

```
flameOffHold = clamp(return + fFlameOffOffset, coldSetpoint, satGetMaxSetpoint())
```

Suppression setpoint (SATcontrol.ino:711-717, SATcontrol.ino:722):

```
active when (now - lastRealFlameOn) >= fModSupDelay x 1000 ms
suppressed = clamp(flow - fModSupOffset, 10, satGetMaxSetpoint())
```

Sustain conditions (SATcycles.ino:1043-1104; start = first qualifying tick, clock = tick time, one `millis()` read per tick):

```
overshoot : flow >= pidOutput + fOvershootMargin   60 000 ms since the first qualifying tick (mode CONTINUOUS, outside the DHW guard)
underheat : flow <= pidOutput - 3.0                 180 000 ms since the first qualifying tick (mode PWM, ON phase, outside the DHW guard)
saturation: last offTimeMs == 0                      300 000 ms since the first qualifying tick (mode PWM, ON phase, outside the DHW guard)
no dt and no gap bail-out; the starts are cleared by every tick without a qualifying sample (satCycleSkipAutoSwitch) and by a DHW tick
```

CH command (SATcontrol.ino:5223): `CH = satChEnableDecide(pidOutput, coldSetpoint, pwmDrivesSetpoint, pwmOn)`: 0 at or below the cold setpoint or in the PWM OFF phase, else 1 (SATchEnable.h:46-52) (corrected 2026-10-24, TASK-1351).

<a id="s4-constants-and-defaults"></a>

### Constants and defaults

| Name | Value | Unit | Citation |
|---|---|---|---|
| Minimum on-time, every heating source (heat pumps too, entries 40 and 50) | 180 | s | SATcontrol.ino:357-363 |
| `iHpCycleSeconds` (heat pump PWM cycle length, the period), default / allowed | 1800 / 1800 or 2400 (other values snap to the nearer one, also when settings load) | s | SATtypes.h:465, settingStuff.ino:999 |
| `PWM_IGNITION_TIMEOUT_MS` | 180 000 | ms | SATcontrol.ino:812 |
| Effective temperature filter window (the same constant as the ignition timeout) | 180 000 | ms | SATcontrol.ino:812, SATcontrol.ino:883 |
| Filter weight on the new sample | 0.3 | - | SATcontrol.ino:884 |
| Effective temperature floor | base + 1 | °C | SATcontrol.ino:867 |
| `iCyclesPerHour` default / allowed | 3 / 2 to 6 | per hour | SATtypes.h:525, settingStuff.ino:1105 (search result) |
| Cycles per hour, heat pump (no hourly start limit, entry 81; the PWM period is `iHpCycleSeconds`) | none | | SATcycles.ino:320-327 |
| Cycles per hour when `iCyclesPerHour` is outside 2..6 | 3 underfloor / 4 radiators | per hour | SATcontrol.ino:354 |
| Rolling-hour ring size | 6 | entries | SATcycles.ino:37 |
| Rolling-hour window | 3600 | s | SATcycles.ino:285 |
| `iMaxRelModulation` default / allowed | 100 / 0 to 100 | % | SATtypes.h:453, settingStuff.ino:1012 (search result) |
| MM in PWM, gas boiler | 0 | % | SATcontrol.ino:308 |
| Geminox MM floor / Immergas MM cap | 10 / 80 | % | SATcontrol.ino:311-312 |
| MM in the cold cutoff | 100 | % | SATcontrol.ino:4973 |
| (removed in TASK-1332) `bForcePWM` default | false | - | SATtypes.h:465 |
| `bOvershootProtection` default | true | - | SATtypes.h:466 |
| `fFlameOffOffset` default / allowed | 18.0 / 0 to 30 | °C | SATtypes.h:462, settingStuff.ino:1034 (search result) |
| `fModSupDelay` default / allowed | 20 / 0 to 120 | s | SATtypes.h:455, settingStuff.ino:1014 (search result) |
| `fModSupOffset` default / allowed | 1.0 / 0 to 5 | °C | SATtypes.h:456, settingStuff.ino:1015 (search result) |
| `fFlowOffset` default (continuous clamp) | 2.0 | °C | SATtypes.h:467 |
| `fOvershootMargin` default / allowed (auto-switch and classifier) | 2.0 / 0.5 to 5 | °C | SATtypes.h:454, settingStuff.ino:1013 (search result) |
| `SAT_UNDERSHOOT_MARGIN_C` | 3.0 | °C | SATcycles.ino:30 |
| `SAT_OVERSHOOT_SUSTAIN_MS` | 60000 | ms | SATcycles.ino:29 |
| `SAT_UNDERHEAT_SUSTAIN_MS` | 180000 | ms | SATcycles.ino:31 |
| `SAT_SATURATION_SUSTAIN_MS` | 300000 | ms | SATcycles.ino:32 |
| `SAT_DHW_OVERSHOOT_GUARD_MS` | 300 000 | ms | SATcycles.ino:33 |
| Cold setpoint, radiators / underfloor | 28.2 / 21.0 | °C | SATcontrol.ino:55-56 |
| `SAT_MIN_SETPOINT` (CS in OFF phase, lower clamp) | 10.0 | °C | SATcontrol.ino:54 |
| Base offset, radiators / underfloor | 27.2 / 20.0 | °C | SATcontrol.ino:49-50 |
| System cap, radiators / underfloor / heat pump | 62 / 45 / 40 | °C | SATcontrol.ino:228-235 |
| MaxTSet default when below 30 | 75 | °C | SATcontrol.ino:57, SATcontrol.ino:243 |
| Hard ceiling, underfloor / radiators | 50 / 80 | °C | SATcontrol.ino:62-63 |
| `fMaxSetpoint` default | 65 | °C | SATtypes.h:514 |
| Control tick interval default / allowed | 30 / 10 to 300 | s | SATtypes.h:444, settingStuff.ino:1001 (search result) |
| Suppression refresh interval | 3 | s | SATcontrol.ino:480 |
| Write-on-change refresh (CS, MM, CH) | 300 | s | SATcontrol.ino:351 |
| PIC CS keepalive | 30 (only CS >= 8 °C) | s | SATcontrol.ino:403-409 |
| Classifier: PWM short factor (only PWM ON at cycle end; no continuous SHORT, no floor) | 0.8 of on-time | - | SATcycles.ino:688 |
| `SAT_PWM_MIN_ON_SEC` | 180 (defined, not used) | s | SATcontrol.ino:59 |
| `SAT_FLOW_OFFSET_CONTINUOUS` | 5.0 (defined, not used) | °C | SATcontrol.ino:58 |

The last two constants appear nowhere else in the firmware (search for both names finds only the definitions).

<a id="s4-state-kept-between-passes"></a>

### State kept between passes

PWM state (SATcontrol.ino:685-696, SATtypes.h:201-203):

| Variable | Meaning | Citation |
|---|---|---|
| `state.sat.eControlMode` | OFF / CONTINUOUS / PWM | SATcontrol.ino:933-938 |
| `state.sat.bPwmFlameRequested` | Phase: true = ON, false = OFF | SATtypes.h:203, SATcontrol.ino:778, SATcontrol.ino:818, SATcontrol.ino:830, SATcontrol.ino:851 |
| `state.sat.fPwmDutyCycle` | Last duty value | SATcontrol.ino:737 |
| `_pwm_effectiveBoilerTemp` | Boiler temperature BT of the duty formula: seeded with the live flow at the first flame, at the first ignition of an ON phase and at an OFF-to-ON start with the flame already burning; filtered during the 180 s after that anchor; held until the next ON phase seeds it; cleared by `satPwmReset()` | SATcontrol.ino:686, SATcontrol.ino:749, SATcontrol.ino:769, SATcontrol.ino:780, SATcontrol.ino:881, SATcontrol.ino:967 |
| `_pwm_flameOnMs` | Ignition anchor of the current ON phase, filter window only. 0 = no ignition in this ON phase (thermo-nova `_flame_on_at is None`). Set at the first ignition of the phase and at an ON start with a burning flame; also set at a first flame that appears after the ignition wait timed out (pwm.py:144-146); cleared at ON to OFF, at an ON start without a flame and by `satPwmReset()` | SATcontrol.ino:807, SATcontrol.ino:943-946, SATcontrol.ino:964, SATcontrol.ino:875, SATcontrol.ino:889, SATcontrol.ino:879, SATcontrol.ino:1150; pwm.py:164, pwm.py:206 |
| `_pwm_waitingForFlame`, `_pwm_lastUpdateMs` | Ignition wait (180 s from the phase start, then the ON timer runs anyway, no abort) and the one phase anchor, thermo-nova `_last_update`: set at reset, at OFF/IDLE->ON, at the first ignition while waiting and at ON->OFF; the ON time and the off wait count from it | SATcontrol.ino:808, SATcontrol.ino:815, SATcontrol.ino:870, SATcontrol.ino:890, SATcontrol.ino:958-967, SATcontrol.ino:1019-1036; pwm.py:91, :162, :188, :208 |
| `_pwm_flameOffHoldSetpoint` | Stored flame-off hold | SATcontrol.ino:690, SATcontrol.ino:881 |
| `_pwm_lastOffTimeMs`, `_pwm_lastOnTimeMs` | Last computed off-time and on-time (saturation check, classifier) | SATcontrol.ino:691-692, SATcontrol.ino:809-810 |
| `_sat_lastControlCS` | Previous CS (no solar adjustment of the CS since TASK-1323); set to 10 by `satPwmDisableOnHeaterOff()` (corrected 2026-10-24, TASK-1351) | SATcontrol.ino:1061, SATcontrol.ino:1107, SATcontrol.ino:5182 |
| `_sat_thermalSafeActive` | Thermal safe fallback owns CS; blocks the refresh | SATcontrol.ino:694, SATcontrol.ino:898, SATcontrol.ino:4729-4733 |

Cycle tracker state used by PWM: `_cycle_flameOnStartMs`, `_cycle_flameOffStartMs` (SATcycles.ino:107-108), the rolling-hour ring `_hourCycleTs` (SATcycles.ino:38-40), the sustain state, three `*Running` flag + `*StartMs` pairs (`_sustain_overshootRunning`/`_sustain_overshootStartMs`, `_sustain_underheatRunning`/`_sustain_underheatStartMs`, `_sustain_saturationRunning`/`_sustain_saturationStartMs`; start timestamps on the tick clock, TASK-1420) and `_sustain_dhwEndMs` + `_sustain_dhwSeen` for the hot-water guard (SATcycles.ino:136-146).

Command cache: last sent CS, MM, CH and their send times (SATcontrol.ino:383-439). It is cleared at init, on an OpenTherm offline-to-online edge and in `satDisable()` (SATcontrol.ino:356-362, SATcontrol.ino:3700, SATcontrol.ino:4531-4535, SATcontrol.ino:2182).

Nothing of the PWM state survives a restart: `initSAT()` resets the phase and starts in CONTINUOUS (SATcontrol.ino:3688-3691, SATcontrol.ino:3721-3722); `_pwm_effectiveBoilerTemp` is a RAM variable with initial value 0 (SATcontrol.ino:686).

<a id="s4-outputs-to-the-boiler-to-entities"></a>

### Outputs (to the boiler, to entities)

To the boiler, at the end of a normal tick in this order (SATcontrol.ino:4977-4994):

| Output | Command | Citation |
|---|---|---|
| Control setpoint | `CS=<value with one decimal>`, through `satEnqueueOwnCS()` so the firmware's own CS ownership check accepts it | SATcontrol.ino:383-396, SATcontrol.ino:375-379 |
| Max relative modulation | `MM=<integer>` | SATcontrol.ino:416-427 |
| CH enable | `CH=1` or `CH=0` | SATcontrol.ino:430-439 |
| Immergas only | `TP=11:12=<finalSetpoint, no decimals>`, queued on every tick | SATcontrol.ino:4984-4990 |

Between ticks: the refresh may queue `CS=` every 3 s (SATcontrol.ino:916-917); on PIC boards the keepalive re-queues the last CS every 30 s (SATcontrol.ino:403-414). When SAT is disabled or trips, `CS=0` is sent (SATcontrol.ino:2179). Without an OpenTherm command interface nothing is queued, and after 5 such ticks SAT trips (SATcontrol.ino:4996-5004, SATcontrol.ino:69).

To entities (MQTT, published only when changed): `sat/mode` (actual mode, "off" when disabled or tripped) and `sat/control_mode` (requested mode) (SATcontrol.ino:2620-2629); `sat/pwm_state` "IDLE" / "ON" / "OFF" (SATcontrol.ino:3039-3045); `sat/modulation_state` "OFF" (SAT not active), "HOT_WATER", "PWM_OFF" (PWM OFF phase), "COLD" (flow below 22 °C) or "ACTIVE" (SATcontrol.ino:3024-3037). The following were seen only as search results: `sat/setpoint` (SATcontrol.ino:2731), `sat/pwm_duty` (SATcontrol.ino:2795), `sat/cycles_this_hour` (SATcontrol.ino:2800), `sat/current_modulation` (SATcontrol.ino:2892), `sat/requested_setpoint` (SATcontrol.ino:3058), and the status JSON fields `pwm_duty`, `pwm_flame_req`, `mod_suppressed`, `current_modulation` (SATcontrol.ino:2390-2408).

<a id="s4-edge-cases"></a>

### Edge cases

- **`bOvershootProtection` off**: in PWM mode CS = `prevCS` on every tick (the last control CS if above 10 °C, else `pidOutput`) (SATcontrol.ino:870-874). The schedule still runs (SATcontrol.ino:871) and MM is still 0 for a gas boiler (SATcontrol.ino:308). The refresh does nothing (SATcontrol.ino:900).
- **DHW active during PWM**: the tick returns before PWM, CS, MM and CH (SATcontrol.ino:4865-4882 [lines refreshed 2026-10-24, TASK-1351]). The mode stays PWM, the schedule is frozen, MM keeps its last sent value (0 in PWM), CS keeps its last sent value. An ON phase is paused and resumed with its remaining ON time after the draw (TASK-1422; entries 95, 103, 104; SATcontrol.ino:909-960). The ignition of a DHW burn is a real flame-on edge, so it moves the ON-phase reference and counts in the rolling-hour limit (SATcycles.ino:714, SATcycles.ino:722-724).
- **No ignition after the ON phase starts**: the phase waits up to 180 s from its start (`_pwm_lastUpdateMs`), then the ON timer runs anyway; there is no abort. A flame that comes later still holds the phase for 180 s from that flame (SATcontrol.ino:1024-1032; pwm.py:194-203; entry 80, Q35 A).
- **Duty below minimum without flame**: on-time 0, off-time `maxMs` (SATcontrol.ino:788-792). The OFF to ON start has no minimum on-time condition, so after `maxMs` since the last flame-off an ON phase still starts, with CS = flame-off hold (SATcontrol.ino:836-853, SATcontrol.ino:879-883).
- **Duty at or above maximum (range 5)**: phase ON is forced, no expiry, no ignition timeout, last off-time = 0 (SATcontrol.ino:904-909). 300 s of tick time after the first tick of this state (the first tick only starts the timer, entries 86, 95) the saturation check leaves PWM for every source, heat pumps included (entry 87; SATcycles.ino:1092-1102, SATcycles.ino:32; called from SATcontrol.ino:5564-5568) [lines refreshed 2026-10-28, TASK-1420]; the `bPwmAutoSwitch` setting was removed in TASK-1341 (entry 19) (corrected 2026-10-24, TASK-1351).
- **Requested setpoint equal to the cold setpoint**: `satApplyPWM()` (`<=`, SATcontrol.ino:1023) and the cold cutoff (`satColdCutoff`, `<=`, SATchEnable.h:26-29) both treat it as no demand, so CS = 10, CH = 0 and PWM is left, as thermo-nova (`heating_control.py:72`, `:385`) (corrected 2026-10-24, TASK-1351); TASK-1323. Open: Sergeant D wrote "below" (entry 7).
- **Requested setpoint at or below the cold setpoint**: CS = 10, MM = `iMaxRelModulation`, CH = 0; `satPwmDisableOnHeaterOff()` leaves PWM and resets the PWM state and the sustain guards; the schedule still ran once that tick inside `satApplyPWM()` before the reset (SATcontrol.ino:5168, SATcontrol.ino:5206-5214, SATcontrol.ino:1100-1109). No auto-switch call that tick (SATcontrol.ino:5542-5546) (corrected 2026-10-24, TASK-1351): was "the mode stays PWM"; TASK-1327, entry 12.
- **Flame-off hold clamp**: the lower clamp is the cold setpoint (28.2 °C radiators) (SATcontrol.ino:811 [lines refreshed 2026-10-24, TASK-1351]). CH = 1 in the PWM ON phase whatever the CS (`satChEnableDecide`, SATchEnable.h:46-52, SATcontrol.ino:5223) (corrected 2026-10-24, TASK-1351).
- **Suppressed setpoint low**: it is floored at 10 °C (SATcontrol.ino:821 [lines refreshed 2026-10-24, TASK-1351]); CH stays 1, because in the PWM ON phase CH no longer depends on the sent CS (`satChEnableDecide`, SATchEnable.h:46-52) (corrected 2026-10-24, TASK-1351): was "at exactly 10 °C CH = 0"; entries 9 and 17.
- **Valves closed while in PWM**: CS = 10, MM = max, CH = 0, and `satPwmDisableOnHeaterOff()` leaves PWM and resets the PWM state (SATcontrol.ino:5008-5021, SATcontrol.ino:1100-1109), as thermo-nova (`heating_control.py:69-70`, `heating_control.py:229-234`). **Summer simmer while in PWM**: CS = 10, MM = max, CH = 0, mode and PWM state unchanged (SATcontrol.ino:4994-5006). The refresh is blocked in both (SATcontrol.ino:1053) (corrected 2026-10-24, TASK-1351): valves closed changed in TASK-1327.
- **Heat pump**: enters PWM through the auto-switch like every other source (Sergeant D entry 87, Q45 A, TASK-1419; SATcontrol.ino:5542-5546), and through the manual `control_mode pwm` command, since Force PWM is gone (entry 13). thermo-nova has no heat-pump case there (a grep of heating_control.py for `HEAT_PUMP`, `heating_system`, `HeatingSystem` finds only the PWM constructor at :86). In PWM MM stays at `iMaxRelModulation`, or 100 when that is 0 (SATcontrol.ino:379-389).
- **Forced PWM**: removed in TASK-1332 (entry 13); the code has no such path any more (corrected 2026-10-24, TASK-1351).
- **Control interval above 60 s**: no effect on the sustained switches since TASK-1420. There is no `dt` and no gap bail-out, so underheat and saturation fire at any interval (settingStuff.ino:1004 allows 10-300 s), each on the first tick at least its length after the first qualifying tick; the overshoot switch fires on the second qualifying tick.

<a id="s4-not-found-in-the-code-unclear"></a>

### Not found in the code / unclear

1. **Resolved by TASK-1273: whether the effective-temperature filter ever runs with the default 30 s interval.** The question was whether a tick lands inside a 30 000 ms window after the sync tick. The window is now 180 000 ms (Sergeant D, 2026-10-22, `docs/sat/sergeant-d-corrections.md` entry 1), so the default 30 s tick gives about five samples per ON phase (SATcontrol.ino:768, SATtypes.h:444; derived from the interval, not run). Exactly how many land still depends on timer jitter at the window edge, which was not traced. Only a control interval of 180 s or more gets no sample, and BT then stays at the seed of the ON phase.
2. **DHW post-overshoot guard**: as written it cannot become active (see step 3). Whether that is intended is not stated in the code.
3. **The `iRequestedControlMode` "pwm" request** is not enforced after the auto-switch moves the mode away (see step 3). Whether that is intended is not stated.
4. **`bModSuppressed` and `iModSuppressionSinceMs`** are declared, reset every tick and published, but never set (SATcontrol.ino:4956-4957, SATtypes.h:213-215). Their purpose is not visible in the code.
5. **`SAT_PWM_MIN_ON_SEC` and `SAT_FLOW_OFFSET_CONTINUOUS`** are defined but unused (SATcontrol.ino:58-59).
6. **Comment versus code**: the `fModSupOffset` comment says "below setpoint" (SATtypes.h:456); the code uses "below the flow temperature" (SATcontrol.ino:713). The `fOvershootMargin` comment says the default was lowered from 3.0 (SATtypes.h:454); the reason is not in the code.
7. **What the boiler does with CS = flow - 1 °C and MM = 0** is boiler behaviour and not in the firmware.
8. **OpenTherm message IDs** behind `CS=`, `MM=`, `CH=` are handled by the PIC or the OTDirect layer, which were not read for this section.
9. **Flame-on reference when PWM starts with the flame already burning** (for example after a sustained overshoot in continuous mode): the OFF-phase test uses the last real flame-off, which may be long ago, so ON can start on the first PWM tick; the ON phase is then timed from the ignition of the burn that is already running, which may already exceed the on-time (SATcontrol.ino:813-821, SATcontrol.ino:836-853). The resulting behaviour depends on the boiler's flame history; no scenario test was run.
10. Files read for this section: SATcontrol.ino (lines 40-170, 192-314, 340-487, 600-979, 1140-1203, 1320-1344, 1938-1987, 2140-2269, 2612-2631, 3015-3054, 3655-3763, 4500-5009), SATcycles.ino (lines 1-310, 548-1059), SATtypes.h (lines 195-219, 425-474, 510-527). Seen only as search results: SATtypes.h:53, SATtypes.h:95-97, SATtypes.h:243, OTGW-firmware.ino:567, OTGW-firmware.ino:1157, MQTTstuff.ino:635, settingStuff.ino lines 318-345, 388, 996-1039, 1105, safeTimers.h:96-98, restAPI.ino:2403-2409, handleDebug.ino:235-241, SATcontrol.ino:2390-2408, 2731, 2795-2800, 2892-2894, 3058. Thermo-nova code was not opened for this section; only its specification file was used for headings and order.

---

<a id="s5"></a>

## Boiler state, flame and cycle tracking (OTGW firmware, as built)

Source: OTGW-firmware, branch `dev`, git HEAD 51df6b486. Paths are relative to `src/OTGW-firmware/` unless they start with `src/libraries/`. Every statement below cites lines that were opened and read. The headings follow the thermo-nova section (a5-cycles-device-thermonova.md) so both can be compared line by line. Statements marked "derived from the order of the code" follow from reading statement order; they were not run on a device or in a test. Code comments are not used as evidence of behaviour; where a comment disagrees with the code, the mismatch is listed under "Not found in the code / unclear".

<a id="s5-purpose"></a>

### Purpose

The firmware watches the boiler through four separate pieces of code, not one shared snapshot:

1. **Cycle tracker (SATcycles.ino)**: a "cycle" is one flame-on period, from a flame OFF->ON edge to the next ON->OFF edge (SATcycles.ino:697-757). At flame-off it classifies the cycle (GOOD, OVERSHOOT, UNDERHEAT, UNDERHEAT_PWM, SHORT, UNCERTAIN, INSUFFICIENT, SATtypes.h:62-73), stores it in a 16-entry history, a rolling 4 h window, a 24 h hourly-bucket window and a set of moving averages (SATcycles.ino:551-602, SATcycles.ino:815-835). It also keeps a rolling per-hour flame-on counter (SATcycles.ino:260-301) and a "cycle phase" label (SATcycles.ino:922-937, SATcycles.ino:1043-1051).
2. **Boiler status evaluator (`satUpdateBoilerStatus`, SATcontrol.ino:516-647)**: keeps its own previous flame, modulation and flow values and its own flame on/off timestamps (SATcontrol.ino:464-470), and derives one `SATBoilerStatus` label (SATtypes.h:103-109).
3. **Modulation reliability tracker (`satUpdateModulationReliability`, SATcontrol.ino:3562-3585)**: an 8-value ring of flame-on modulation readings.
4. **Flame status tracker (`satUpdateFlameStatus`, SATcontrol.ino:3622-3676)**, firmware only: a health label (SATtypes.h:99-102) with no counterpart in the thermo-nova section.

There is no event like `sat_cycle_started` / `sat_cycle_ended`: searches for `on_cycle_end|OnCycleEnd` and `cycle_start|cycle_end|CycleStart|CycleEnd|cycle_started|cycle_ended` in the firmware sources found nothing. The places where this area changes what is sent to the boiler are: the flame-on and flame-off timestamps and the per-hour counter used by the PWM schedule (SATcontrol.ino:720-723, SATcontrol.ino:813-816, SATcontrol.ino:836-840), the PWM on-time read back into the classifier (SATcycles.ino:791), and the sustained-condition mode switches `satCycleCheckOvershootSwitch` and `satCycleCheckPwmExits` (SATcycles.ino:1041-1102). The cycle classification itself does not switch PWM on or off (see section 9). `SATcycles.ino` also contains the heating curve daily-median recommendation (SATcycles.ino:1059-1289); that belongs to another area and is not described here.

<a id="s5-inputs"></a>

### Inputs

**Boiler values** (there is no frozen snapshot record; each function reads the live values when it runs):

| thermo-nova `DeviceState` field | Firmware source | Where it is read in this area |
|---|---|---|
| `flame_active` | `satIsFlameOn()`: `OTcurrentSystemState.Statusflags & 0x08`, or `state.sat.bSimFlameOn` in simulation (SATcontrol.ino:1337-1341) | edge detection (SATcontrol.ino:4592), boiler status (SATcontrol.ino:518), modulation reliability (SATcontrol.ino:3565), flame status (SATcontrol.ino:3635) |
| `hot_water_active` | two different sources: `state.sat.bDhwActive` = `SlaveStatus & 0x04`, or forced true by a simulated draw, refreshed only on the control tick (SATcontrol.ino:4998-5002); and, for the cycle kind and the DHW-in-tail test, `Statusflags & 0x04` read per 1 Hz sample on the real bus, `bDhwActive` under SAT simulation (SATcycles.ino:907-929) [lines refreshed 2026-10-25, TASK-1387 review] | `bDhwActive`: boiler status, off-with-demand (SATcycles.ino:725), auto-switch guard. Per-sample bit: cycle kind and DHW-in-tail (SATcycles.ino:923-929) |
| `central_heating` | two different sources: the cycle kind counts the master CH-enable bit, `Statusflags & 0x0100` (thermo-nova `central_heating` = `coordinator.active` = master CH enabled, coordinator/__init__.py:136, coordinator/serial/__init__.py:53-55), or under SAT simulation a commanded CS above `SAT_MIN_SETPOINT` (SATcycles.ino:907-929); the off-with-demand test reads the slave CH-mode bit `Statusflags & 0x02` (SATcycles.ino:726) [lines refreshed 2026-10-25, TASK-1387 review] | cycle kind and off-with-demand. The boiler status evaluator has no central-heating input |
| `setpoint` | `state.sat.fFinalSetpoint`, SAT's own last commanded control setpoint (SATcontrol.ino:4946), not a value reported back by the boiler | boiler status (SATcontrol.ino:521), cycle start setpoint (SATcycles.ino:717), off-with-demand (SATcycles.ino:710), flame status (SATcontrol.ino:3655, SATcontrol.ino:3661) |
| `flow_temperature` | `satGetFlowTemp()`: `OTcurrentSystemState.Tboiler` (MsgID 25), or the simulated flow plus optional noise (SATcontrol.ino:1325-1329) | everywhere in this area |
| `return_temperature` | `satGetReturnTemp()`: `OTcurrentSystemState.Tret` (MsgID 28), or simulated (SATcontrol.ino:1331-1335) | flow-return delta (SATcycles.ino:859-863) |
| `relative_modulation_level` | `OTcurrentSystemState.RelModLevel` read directly (SATcontrol.ino:519, SATcontrol.ino:3566). The simulation-aware wrapper `satGetActualModulation()` (SATcontrol.ino:1343-1347) is not used here | boiler status, modulation reliability |
| `max_modulation_level` | no counterpart found (not read by any function in this area) | |

`Statusflags` holds the master status in the high byte and the slave status in the low byte. It is written through the reference argument of `print_status` for a normal MsgID 0 frame (OTGW-Core.ino:2802, OTGW-Core.ino:2868-2871, OTGW-Core.ino:4548) and by the PS summary path (OTGW-Core.ino:4441, OTGW-Core.ino:2451-2462). In the low byte, bit 1 is slave "CH mode", bit 2 slave "DHW mode" and bit 3 "flame status" (OTGW-Core.ino:2838-2846).

**Controller values read by this area:**

| Value | Source |
|---|---|
| requested setpoint for the classifier | `_sat_requestedSetpoint` at the flame-off moment (SATcycles.ino:807): the requested setpoint of the last control tick, after the multi-zone block and the solar offset, clamped (SATcontrol.ino:808) |
| requested setpoint for the auto-switch | the local `pidOutput` of the control tick, after the zone block and the min/max clamp (SATcontrol.ino:5538-5585) |
| PWM mode | `state.sat.eControlMode == SAT_MODE_PWM` (SATcycles.ino:790) |
| PWM on-time | `satPwmLastOnTimeMs()` (SATcycles.ino:791, SATcontrol.ino:946), set by the PWM schedule (SATcontrol.ino:780, SATcontrol.ino:810) |
| PWM off-time | `satPwmLastOffTimeMs()` (SATcycles.ino:1010, SATcontrol.ino:942) |
| PWM phase | `state.sat.bPwmFlameRequested` (SATcycles.ino:989-993) |
| overshoot margin | `settings.sat.fOvershootMargin`, default 2.0 °C (SATtypes.h:454) |

There is no stored record of outside temperature or commanded maximum modulation per sample; the cycle code does not read them (SATcycles.ino:847-940).

<a id="s5-behaviour-step-by-step-in-time-order"></a>

### Behaviour (step by step, in time order)

#### 1. When a pass runs (sampling cadence)

- `loop()` calls `satControlLoop()` on every pass while no firmware or PIC flash is in progress (OTGW-firmware.ino:1132, OTGW-firmware.ino:1157). `satControlLoop()` runs `satControlLoopRun()` and then `satPublishCoreState()` (SATcontrol.ino:4504-4508).
- Inside `satControlLoopRun()` everything in this area is skipped when SAT is disabled or a flash is in progress (SATcontrol.ino:4568-4573), and when the safety trip is set (SATcontrol.ino:4581).
- Four cadences follow:

| Cadence | What runs | Citation |
|---|---|---|
| every `loop()` pass | flame edge detection, then `satCycleSample()` | SATcontrol.ino:4591-4599 |
| about 1 Hz (inside `satCycleSample`) | flow ring and tail ring sample | SATcycles.ino:868-884 |
| every 60 s (`timerSAT4hStats`) | 4 h and 24 h window statistics | SATcontrol.ino:478, SATcontrol.ino:4602-4605 |
| control tick, `settings.sat.iControlInterval` (default 30 s) | modulation reliability, flame status, boiler status, auto-switch | SATtypes.h:444, SATcontrol.ino:476, SATcontrol.ino:3719, SATcontrol.ino:4614, SATcontrol.ino:4802-4814, SATcontrol.ino:4951-4953 |

- On the control tick, the status functions run only if the tick gets past several early returns: DHW active (SATcontrol.ino:4865-4882 [lines refreshed 2026-10-24, TASK-1351]), boiler fault bit (SATcontrol.ino:4716-4719), the thermal safe fallback (SATcontrol.ino:4730-4746), summer simmer (SATcontrol.ino:4766-4776) and closed valves (SATcontrol.ino:4779-4790). On those ticks the boiler status, flame status and modulation reliability keep their previous values.
- `satUpdateFlameStatus` has its own 10 s rate limit (SATcontrol.ino:3626), but its only caller is the control tick (SATcontrol.ino:4811), so it runs once per tick.
- The cycle tracker and the boiler status evaluator each detect flame edges on their own: the cycle tracker on every `loop()` pass through `_sat_prevFlameState` (SATcontrol.ino:4592-4596), the evaluator only on control ticks through `_bs_prevFlame` (SATcontrol.ino:526-531, SATcontrol.ino:644). The evaluator's flame timestamps can therefore lag the real edge by up to one control interval (derived from the order of the code).
- In simulation, `satUpdateSimulation()` calls `satCycleOnFlameChange()` itself at its own flame transitions (SATcontrol.ino:3825-3839), and the edge detection a few lines later calls it again (SATcontrol.ino:4576, SATcontrol.ino:4592-4596). The second call does nothing because the handler only acts on a change of `_cycle_flameOn` (SATcycles.ino:701, SATcycles.ino:757).

#### 2. Start-up (firmware boot)

In the order the code runs:

1. `setup()` calls `initSAT()` (OTGW-firmware.ino:567).
2. `initSAT()` resets PID and restores PID, energy and heating-curve data from LittleFS, then calls `satCycleInit()` (SATcontrol.ino:3683-3688).
3. `satCycleInit()` (SATcycles.ino:188-258):
   - cycle flame state off, flame-on time 0, flame-off time = `millis()` now (SATcycles.ino:190-192);
   - sustained-condition timers zero, last check time = now (SATcycles.ino:196-199);
   - cycle history, per-hour counter, flow ring, tail ring, 4 h window and 24 h window emptied, and their `state.sat` outputs zeroed (SATcycles.ino:200-240);
   - shape accumulators reset, `_cycle_lastFlameOffMs` = now, off-with-demand = 0 (SATcycles.ino:242-253);
   - the intra-day heating-curve sample buffer is emptied (SATcycles.ino:255-257).
4. `initSAT()` then resets PWM, sets SAT inactive with mode OFF (SATcontrol.ino:3689-3691), sends `CS=0` when a command interface is ready (SATcontrol.ino:3706-3710), and when SAT is enabled sets mode CONTINUOUS and queues `PM=3`, `PM=15`, `PM=48` and, for boilers with the MI quirk, `MI=500` (SATcontrol.ino:3721-3734).
5. Nothing of this area is restored from storage. The 4 h window file `/sat/sat_cycles.json` is named (SATcontrol.ino:1962) but is never written (SATcontrol.ino:2126-2127); the only code that touches it removes it (SATcycles.ino:1296-1302).

State at the first pass:

- `_sat_prevFlameState` = false (SATcontrol.ino:473), `_cycle_flameOn` = false (SATcycles.ino:190). If the flame is already burning at the first enabled pass, that pass sees an OFF->ON edge and starts a cycle at that moment (SATcontrol.ino:4592-4596, SATcycles.ino:701).
- Boiler status = `SAT_BS_OFF` (SATtypes.h:152), its previous flame = false and both flame timestamps = 0 (SATcontrol.ino:465-470).
- Modulation reliable = true until a verdict is made (SATtypes.h:265).
- Flame status = `INSUFFICIENT_DATA` (SATtypes.h:275) for the first 6 calls (SATcontrol.ino:3629-3633).
- Last cycle class `SAT_CYCLE_NONE`, cycle count 0 (SATtypes.h:166-167).
- `satCycleInit()` has no other caller (search for `satCycleInit(`). `satDisable()` does not reset the cycle tracker, the boiler status or the flame trackers (SATcontrol.ino:2160-2187). `satFlushShortLivedData()` empties only the 4 h window (SATcontrol.ino:2192-2201, SATcycles.ino:1296-1302).

#### 3. Device tracker, every pass (firmware: boiler status evaluator, every control tick)

There is no separate device tracker. `satUpdateBoilerStatus()` (SATcontrol.ino:516-647) does, in this order:

1. Read flame, modulation (`RelModLevel`), flow and `fFinalSetpoint`, and remember the previous status (SATcontrol.ino:518-523).
2. Record flame transitions against its own previous flame value: OFF->ON sets `_bs_flameOnMs` = now, ON->OFF sets `_bs_flameOffMs` = now (SATcontrol.ino:526-531). Neither timestamp is ever cleared.
3. Compute `hasDemand` (SATcontrol.ino:534) and the flow change since the previous call (SATcontrol.ino:537-544).
4. Classify (section 4).
5. If the status changed, set `_bs_stateEntryMs` = now (SATcontrol.ino:639-641).
6. Store status, flame, modulation and flow as "previous" for the next call (SATcontrol.ino:643-646).

There is no overshoot-at-flame-off flag, no hot water on/off timestamps and no copy of the last cycle in this evaluator. It reads the last cycle duration from `state.sat.fLastCycleDuration` (SATcontrol.ino:572-575).

Modulation reliability (firmware), once per control tick (SATcontrol.ino:4802-4805, SATcontrol.ino:3562-3585):

1. Skipped, with `bModulationReliable` forced to false, when the manufacturer has the "no relative modulation" quirk (SATcontrol.ino:4802-4805, SATtypes.h:97).
2. Only with the flame on, and only for a modulation value that is not NaN and not negative (SATcontrol.ino:3565-3567).
3. The value goes into an 8-slot ring (SATcontrol.ino:3556-3560, SATcontrol.ino:3569-3570).
4. While fewer than 8 values were counted, it returns without a verdict; the call that brings the count to 8 also returns (SATcontrol.ino:3571-3574). The first verdict is therefore made on the 9th flame-on tick (derived from the order of the code).
5. Verdict: reliable if at least 3 of the 8 values are >= 3.0 (SATcontrol.ino:3577-3583). The count is also stored in `iModChangeCount` (SATcontrol.ino:3584).
6. Not persisted (no save or load call in SATcontrol.ino:3547-3585).

#### 4. Status classification (`satUpdateBoilerStatus`, SATcontrol.ino:546-637)

The branches are tested in this order:

| # | Condition | Status | Citation |
|---|---|---|---|
| 1 | flame on and `bDhwActive` | `HEATING_HOT_WATER` | SATcontrol.ino:548-551 |
| 2 | flame off, previous call flame off, flow > setpoint + `fOvershootMargin` | `OVERSHOOT_COOLING` | SATcontrol.ino:552-558 |
| 3 | (same branch) flame-off time known, less than 60 s since `_bs_flameOffMs`, and previous status was HEATING, AT_SETPOINT, COOLING or POST_CYCLE | `POST_CYCLE` | SATcontrol.ino:559-563 |
| 4 | (same branch) demand, flame-off time known, less than 180 s since flame-off | `ANTI_CYCLING` | SATcontrol.ino:564-566 |
| 5 | (same branch) demand, flame-off time known, time since off > threshold | `STALLED_IGNITION` | SATcontrol.ino:567-577 |
| 6 | (same branch) demand, flame-off time known, otherwise | `WAITING_FLAME` | SATcontrol.ino:578-580 |
| 7 | (same branch) otherwise | `IDLE` | SATcontrol.ino:582-584 |
| 8 | flame on, previous call flame off, flow < setpoint - 6 | `PUMP_STARTING` | SATcontrol.ino:586-589 |
| 9 | flame on, previous call flame off, otherwise | `IGNITION_SURGE` | SATcontrol.ino:590-592 |
| 10 | flame on (both calls), less than 30 s since `_bs_flameOnMs`, flow change >= 0.5 | `IGNITION_SURGE` | SATcontrol.ino:594-601 |
| 11 | flame on, less than 30 s since flame-on, flow falling, setpoint - flow > 6 | `PUMP_STARTING` | SATcontrol.ino:602-606 |
| 12 | flame on, abs(flow - setpoint) <= 1.5 | `AT_SETPOINT` | SATcontrol.ino:607-610 |
| 13 | flame on, setpoint - flow > 6 | `PREHEATING` | SATcontrol.ino:623-624 |
| 14 | flame on, modulation direction > 0 | `MODULATING_UP` | SATcontrol.ino:625-626 |
| 15 | flame on, modulation direction < 0 | `MODULATING_DOWN` | SATcontrol.ino:627-628 |
| 16 | flame on, otherwise | `HEATING` | SATcontrol.ino:629-631 |
| 17 | flame off, previous call flame on | `COOLING` (one call only) | SATcontrol.ino:634-637 |

Notes on the status rules:

- **`OFF` is never assigned by the evaluator.** Every branch above assigns another value (SATcontrol.ino:546-637), and the only other writer of `eBoilerStatus` is its initial default (SATtypes.h:152; search for `eBoilerStatus\s*=` found only SATcontrol.ino:643). So `OFF` holds only until the first evaluation (derived from the order of the code). There is no rule on the central-heating bit.
- **`HEATING_HOT_WATER` is unreachable through the control tick.** The only caller is SATcontrol.ino:4814. It runs after the DHW early return at SATcontrol.ino:4865-4882 [lines refreshed 2026-10-24, TASK-1351], so `bDhwActive` is always false when the evaluator runs (derived from the order of the code; the only writers of `bDhwActive` are SATcontrol.ino:4626 and SATcontrol.ino:4628).
- **"Setpoint" is SAT's own command** (`fFinalSetpoint`, SATcontrol.ino:521) from the previous tick, because the current tick only writes it later (SATcontrol.ino:4946). In the PWM OFF phase with overshoot protection that value is 10 °C (SATcontrol.ino:875-878), so `hasDemand` is false then (derived from the order of the code).
- **Overshoot cooling** uses the live margin test only; there is no "last flame-off was an overshoot" memory and no cold-setpoint guard (SATcontrol.ino:556-558).
- **Timing is per tick.** `_bs_flameOffMs` and `_bs_flameOnMs` are the times of the tick that first saw the change (SATcontrol.ino:526-531). With a 30 s tick, rule 3 can hold on the tick after `COOLING` (about 30 s since the observed flame-off) and no longer one tick later (derived from the order of the code).
- **Modulation direction** (SATcontrol.ino:615-622): when `bModulationReliable`, +1 if modulation rose more than 1.0, -1 if it fell more than 1.0, else 0, with no fall-through to the flow gradient. When not reliable: +1 if the flow change > 0.1, -1 if < -0.1, else 0.
- **Flame status tracker (firmware only)**, every control tick after 6 warm-up calls (SATcontrol.ino:3622-3676), first match wins:

| # | Condition | Flame status | Citation |
|---|---|---|---|
| 1 | fewer than 6 earlier calls | `INSUFFICIENT_DATA` | SATcontrol.ino:3629-3633 |
| 2 | SAT not active (unreachable through its only caller: `bActive` is set true at SATcontrol.ino:4616, before the call at SATcontrol.ino:4811; derived from the order of the code) | `IDLE_OK` | SATcontrol.ino:3637-3640 |
| 3 | safety tripped | `STUCK_OFF` | SATcontrol.ino:3643-3646 |
| 4 | last cycle class is `SHORT`. Since TASK-1387 (entry 55) only a PWM cycle that ends in the ON phase can be `SHORT` (cls:56-64); a short continuous burn is `GOOD` or `OVERSHOOT`, so `SHORT_CYCLING`, `sat/cycle_health` and the `flame_health` problem flag no longer fire for it | `SHORT_CYCLING` | SATcontrol.ino:4004-4007, `cycle_health` SATcontrol.ino:3360-3366 [lines refreshed 2026-10-25, TASK-1387 review] |
| 5 | flame on and `fFinalSetpoint` < 5 °C | `STUCK_ON` | SATcontrol.ino:3655-3658 |
| 6 | flame off, `fFinalSetpoint` > 30 °C, more than 300 s since the cycle tracker's flame-off | `STUCK_OFF` | SATcontrol.ino:3661-3667 |
| 7 | PWM mode, PWM duty < 0.05, flame on | `PWM_SHORT` | SATcontrol.ino:3670-3673 |
| 8 | otherwise | `HEALTHY` | SATcontrol.ino:3675 |

#### 5. Cycle tracker, every pass (`satCycleOnFlameChange` and `satCycleSample`)

On each `loop()` pass, edge detection calls `satCycleOnFlameChange(currentFlame)` when the flame differs from `_sat_prevFlameState` (SATcontrol.ino:4592-4596), then `satCycleSample()` runs (SATcontrol.ino:4599).

1. **Flame OFF->ON** (`flameOn` and `_cycle_flameOn` false, SATcycles.ino:718) [lines refreshed 2026-10-25, TASK-1387 review]:
   - Off-with-demand duration (see Formulas) into `state.sat.fOffWithDemandSec` (SATcycles.ino:723-729).
   - Mark the cycle open, start time = now, max and min flow = current flow, `_cycle_setpointAtStart` = `fFinalSetpoint`, overshoot seconds 0, last sample time = now (SATcycles.ino:730-736).
   - Refresh `iCyclesThisHour` from the per-hour counter. Since TASK-1401 the flame-on edge no longer records into it: the counter holds PWM ON phases only, written by the PWM schedule (SATcycles.ino:747-748, SATcycles.ino:309-317).
   - Debug line, narration line and a WebSocket status message (SATcycles.ino:740-749).
   - Empty the flow ring and seed it with the current flow as its first sample (SATcycles.ino:751-756). Empty the tail ring without a seed and set its last sample time = now (SATcycles.ino:758-761). Zero the kind and DHW-in-tail counters (SATcycles.ino:763-766).
   - Reset the flow-return delta and shape accumulators (SATcycles.ino:768-775).
2. **Flame on (`satCycleSample`, every `loop()` pass, SATcycles.ino:875-986)**: returns at once while the cycle is not open (SATcycles.ino:877). Otherwise:
   - Update max and min flow (SATcycles.ino:882-883).
   - If the return temperature is between 10 and 100 °C, add (flow - return) to the delta sum (SATcycles.ino:887-891).
   - If at least 1000 ms passed since the last ring sample (SATcycles.ino:897): append the flow to the flow ring and the tail ring (SATcycles.ino:898-905), and count the sample for the cycle kind (see Formulas): `_cycle_totalSamples`++, `_cycle_dhwSamples`++ with the time of this DHW sample after flame-on, or `_cycle_chSamples`++ (SATcycles.ino:907-929). These counters are `uint32_t` and count at 1 Hz, inside the gate (TASK-1387).
   - Add the time since the previous `satCycleSample` call to the overshoot seconds when flow > `_cycle_setpointAtStart` + margin (SATcycles.ino:938-942). Diagnostic only since TASK-1387.
   - From 60 s after flame-on: shape metrics (see Formulas) (SATcycles.ino:944-966).
   - The kind counters are no longer counted per `loop()` pass (moved into the 1 Hz gate above, TASK-1387).
   - Update the cycle phase (see the "Firmware only" paragraph at the end of this section).
   - Remember this call's time (SATcycles.ino:985).
3. **Flame ON->OFF** (`!flameOn` and `_cycle_flameOn`, SATcycles.ino:777-871):
   - Mark the cycle closed, `_cycle_flameOffStartMs` = now, phase = COOLDOWN (SATcycles.ino:779-782).
   - `durationSec` = (now - flame-on time) / 1000 (SATcycles.ino:784).
   - Debug line and WebSocket message with the room temperature (SATcycles.ino:785-795).
   - Percentiles (SATcycles.ino:797-803), classification inputs and classification (SATcycles.ino:806-823), record (SATcycles.ino:824), extra metrics to `state.sat` (SATcycles.ino:827-831), narration (SATcycles.ino:832-841).
   - Append to the 4 h window and the 24 h window (SATcycles.ino:844-863).
   - `_cycle_lastFlameOffMs` = now (SATcycles.ino:866).
4. A cycle is always recorded at flame-off, whatever its length or sample count. A cycle with fewer than 3 samples is recorded with class `INSUFFICIENT` (SATcycles.ino:664, SATcycles.ino:824).

**Firmware only: cycle phase.** Per `satCycleSample` call with the flame on (SATcycles.ino:968-983): `STARTUP` when flow < `_cycle_setpointAtStart` - 1.5; `STEADY` when abs(flow - `_cycle_setpointAtStart`) <= 1.5; otherwise the phase is unchanged. Flame-off sets `COOLDOWN` (SATcycles.ino:781). The phase starts as `IDLE` (SATcycles.ino:118); no code sets it back to `IDLE` (search for `_cycle_phase\s*=` found only SATcycles.ino:118, SATcycles.ino:781, SATcycles.ino:981).

#### 6. Building a finished cycle (firmware: flame-off branch, SATcycles.ino:777-871)

1. No start-time check: the branch only runs when a cycle is open (SATcycles.ino:777).
2. `durationSec` = (now - `_cycle_flameOnStartMs`) / 1000 (SATcycles.ino:784). The sample count used by the classifier is `_cycle_totalSamples`, counted in the 1 Hz gate of `satCycleSample()` (SATcycles.ino:923).
3. No discard: a cycle with fewer than 3 samples is classified `INSUFFICIENT` and still recorded (SATcycles.ino:664, SATcycles.ino:824).
4. Cycle kind from the hot water and central heating sample fractions, as tracker.py:101-108 and :314-324 (see Formulas, SATcycles.ino:571-581).
5. Maximum and minimum flow over every `loop()` pass of the cycle (SATcycles.ino:732-733, SATcycles.ino:882-883).
6. Windows: the tail percentiles use whatever the 180-slot tail ring holds, at about 1 sample per second, so the last 180 s, or the whole cycle when it is shorter (SATcycles.ino:897-905, src/libraries/Platform/src/boards.h:122); there is no warm-up exclusion for them (tracker.py:113-116 starts the tail at max(start + min(60, 0.25 x duration), end - 180)). The DHW-in-tail test does apply that tail start (SATcycles.ino:813-819). The flow ring holds the last 256 s (src/libraries/Platform/src/boards.h:121). The shape metrics skip a fixed first 60 s (SATcycles.ino:84, SATcycles.ino:947-948).
7. Metric sets:
   - full-cycle `p90` and `p10` from the flow ring when it has >= 10 samples, else the cycle maximum and minimum (SATcycles.ino:797-798);
   - `tailP90`, `tailP10`, `tailP50` from the tail ring when it has >= 10 samples, else `p90`, `p10` and (p90 + p10) / 2 (SATcycles.ino:801-803);
   - shape metrics accumulated while running (SATcycles.ino:944-966).
8. Classify (section 7).
9. PWM state: `pwmEnabled` = `eControlMode == SAT_MODE_PWM`, IDLE = `satPwmIsIdle()`, ON = `bPwmFlameRequested`, all at the flame-off moment (SATcycles.ino:811, SATcycles.ino:820-822). None is stored in the cycle record.

#### 7. Cycle classification (`_cycleClassify`, SATcycles.ino:658-711) [lines refreshed 2026-10-25, TASK-1387 review]

TASK-1387 (Sergeant D entry 55): the rules and their order are those of `cycles/classifier.py` (`cls`). Inputs: duration, the 1 Hz sample count, the kind (SATcycles.ino:571-581), DHW in the tail (SATcycles.ino:813-819, with tracker.py's warm-up exclusion), PWM enabled / IDLE / ON at flame-off, the PWM on-time, `tailP90`, `tailP50`, `controlSp` = `_cycle_setpointAtStart` (the `fFinalSetpoint` at flame-on, constant for the cycle) and `requestedSp` = `_sat_requestedSetpoint` at flame-off (SATcycles.ino:810-823). The first matching rule wins:

| # | Condition | Result | thermo-nova | Citation |
|---|---|---|---|---|
| 1 | duration <= 0, or fewer than 3 samples | `INSUFFICIENT` | cls:19-20 (the < 3 sample case: tracker.py:98-100 drops the cycle) | SATcycles.ino:664 |
| 2 | kind `UNKNOWN` | `UNCERTAIN` | cls:22-23 | SATcycles.ino:667 |
| 3 | a DHW sample in the tail | `UNCERTAIN` | cls:25-26 | SATcycles.ino:670 |
| 4 | PWM enabled and IDLE | `UNCERTAIN` | cls:28-29 | SATcycles.ino:673 |
| 5 | PWM, `requestedSp` <= 0 | `UNCERTAIN` | cls:42-43 (error percentiles missing) | SATcycles.ino:680 |
| 6 | PWM, `tailP90 - requestedSp` <= -3 | `UNCERTAIN` if `requestedSp` < cold setpoint, else `UNDERHEAT_PWM` | cls:45-54 | SATcycles.ino:682-686 |
| 7 | PWM ON at flame-off, on-time <= 0 or duration < 0.8 x on-time | `SHORT` | cls:56-64 | SATcycles.ino:689-692 |
| 8 | PWM, otherwise | `GOOD` | cls:66 | SATcycles.ino:693 |
| 9 | continuous, `controlSp` <= 0 | `UNCERTAIN` | cls:71-72 (error percentiles missing) | SATcycles.ino:698 |
| 10 | continuous, `tailP90 - controlSp` >= `fOvershootMargin` | `OVERSHOOT` | cls:74, 77-78 | SATcycles.ino:702 |
| 11 | continuous, `tailP50 - controlSp` <= -3 | `UNCERTAIN` if the requested setpoint (else the control setpoint) < cold setpoint, else `UNDERHEAT` | cls:75, 80-88 | SATcycles.ino:704-709 |
| 12 | otherwise | `GOOD` | cls:37, 90 | SATcycles.ino:710 |

Firmware deviations: cold setpoint 28.2 °C radiators / 21.0 °C underfloor (`satGetColdSetpoint()`, entries 7 and 11) instead of `COLD_SETPOINT` 22; margin `fOvershootMargin` 2.0 with `>=` (entries 15 and 37) instead of 3.0; PWM underheat labelled `UNDERHEAT_PWM`; the setpoint `<= 0` stand-ins for missing percentiles; one control and one requested setpoint per cycle instead of per sample (TASK-1405); floor-index percentiles instead of `percentile_interpolated`, and no warm-up exclusion for the tail percentiles (the DHW-in-tail test has it); PWM IDLE is a flag approximation of pwm.py's status, set by `satPwmReset()`, cleared by the first schedule step past the flow check and by a switch into PWM with a flow temperature, kept by the hour-limit return (SATcontrol.ino:807, :868-869, :991, :1093, :1103-1112). Remaining IDLE differences: on the switch tick thermo-nova runs `pwm.update()` in the same pass (heating_control.py:206-215) and with an on-time of at least 180 s starts ON (pwm.py:154-170; entry 43, the first PWM phase is ON), while the firmware reads OFF until its next control tick, so a cycle that ends inside that tick skips the SHORT test (the tick order is Q47, the first ON phase is entry 43's task); the hour-limit return keeps the status, as the code at pwm.py:156-158 does. thermo-nova's check there never blocks (it compares a cycle count with `duty_cycle_seconds`, 780 s by default, pwm.py:156, entry_data.py:220, const.py:167), but entry 59 asks for a real cycles-per-hour limit (TASK-1401), so keeping the status is the faithful reading; thermo-nova only reaches that return with `on_time >= 180` (pwm.py:155), which the firmware has no gate for (Q38, entry 43 Open). SAT simulation has no OT status flags: the kind counter takes DHW from `bDhwActive` and CH from a commanded CS above `SAT_MIN_SETPOINT` (SATcycles.ino:907-920).

Host test: `tests/test_sat_cycle_classifier.py` runs the real `satCycleOnFlameChange()`, `satCycleSample()` and `_cycleClassify()` per rule (also the DHW warm-up, the master CH-enable kind and the simulation inputs) and checks the expected classes against the real `classifier.py`, with tracker.py's tail start in the oracle. `tests/test_sat_pwm_override.py` checks the real IDLE flag (reset, first step, hour limit, flow loss, overshoot switch).

#### 8. Recording into history (`_cycleRecord`, SATcycles.ino:551-602, plus the window block SATcycles.ino:815-835)

1. Write class, kind, duration, maximum flow, overshoot seconds and (maximum flow - start setpoint) into the 16-entry ring (SATcycles.ino:553-563, SATcycles.ino:26).
2. `iCycleCount`++, and last class, maximum flow, overshoot seconds, duration and kind into `state.sat` (SATcycles.ino:565-570).
3. Hot water and CH fractions of the last cycle (SATcycles.ino:572-579).
4. Moving averages (EMA, alpha 0.15) of overshoot, underheat (both kinds) and long-cycle (> 600 s) shares, and of the duty ratio (SATcycles.ino:582-601).
5. 4 h window record: end time, on-time, off-time, full-cycle `p90`, mean flow-return delta (-1 when no data), class (SATcycles.ino:817-831, SATtypes.h:119-126).
6. 24 h window: add to the current hourly bucket (SATcycles.ino:834, SATcycles.ino:402-434).
7. Remember this flame-off for the next cycle's off-with-demand (SATcycles.ino:838).

**Off-time and duty are always zero / one (derived from the order of the code).** The flame-off branch sets `_cycle_flameOffStartMs` = now (SATcycles.ino:760) before `_cycleRecord` runs (SATcycles.ino:796) and before the 4 h block (SATcycles.ino:816-820). Both blocks test `_cycle_flameOnStartMs > _cycle_flameOffStartMs` (SATcycles.ino:591, SATcycles.ino:818), which is then false. So:
- the per-cycle duty used for the EMA is `durationSec / durationSec` = 1.0 for every cycle with a positive duration (SATcycles.ino:590-596), and `fDutyRatio` moves towards 1.0;
- every window record gets `offDurationMs` = 0 (SATcycles.ino:818-820, SATcycles.ino:826), so `f4hAvgOffSec` = 0 and `f4hDutyRatio` = 1.0 whenever the window holds a cycle with on-time (SATcycles.ino:357-365);
- every daily bucket gets `sumOffMs` += 0 (SATcycles.ino:433), so `f24hDutyRatio` = 1.0 (SATcycles.ino:465-466).

This was not checked on a running device.

#### 9. How cycle data feeds control decisions

- **No PWM switch from the cycle verdict.** No code path reads `eLastCycleClass` to change the control mode. The callers of `satSetControlMode` are SATcycles.ino:1059, SATcycles.ino:1084, SATcycles.ino:1096, SATcontrol.ino:1325, SATcontrol.ino:2617, SATcontrol.ino:2785-2791 and SATcontrol.ino:5520 [refreshed 2026-10-28, TASK-1420]. The readers of `eLastCycleClass` are the auto-tune counters (SATcontrol.ino:4398-4413), the flame status rule `SHORT_CYCLING` (SATcontrol.ino:4005), and the MQTT, REST and debug outputs (SATcontrol.ino:2766-2768, SATcontrol.ino:3004-3008, restAPI.ino:1028-1039, networkStuff.ino:571, handleDebug.ino:233).
- **Sustained-condition mode switches (`satCycleCheckOvershootSwitch`, SATcycles.ino:1043-1062, and `satCycleCheckPwmExits`, SATcycles.ino:1072-1104; TASK-1420).** Called on the control tick whenever `pidOutput` > cold setpoint, for every heating source, heat pumps included (SATcontrol.ino:5531-5535 and :5564-5568, Sergeant D entry 87, Q45 A); otherwise `satCycleSkipAutoSwitch()` / `satCycleResetSustainGuards()` clear the starts. They always run (no setting, removed in TASK-1341, Sergeant D entry 19). They compare flow with `requestedSetpoint` on the tick time (one `millis()` read per tick):
  - continuous: flow >= requested + `fOvershootMargin`, outside the hot water guard, 60 000 ms from the first qualifying tick -> PWM (entries 41, 86 Q43 A / Q46 B); runs before the mode is applied, so the first PWM CS goes out on the switch tick (entry 88);
  - PWM OFF phase, DHW guard window or not PWM: underheat and saturation timers reset (SATcycles.ino:1074-1078);
  - PWM ON phase: flow <= requested - 3 for 180 s -> continuous (SATcycles.ino:1081-1090); PWM off-time 0 for 300 s -> continuous (SATcycles.ino:1092-1102), both from the first qualifying tick.
  The full mode logic belongs to the PWM section of this spec.
- **Hot water guard (TASK-1420, entry 108).** `satCycleNoteDhwTick()` (SATcycles.ino:200-205) is called from the DHW early return (SATcontrol.ino:5241): it sets `_sustain_dhwEndMs` to the tick time, sets `_sustain_dhwSeen` and clears the three starts. `satDhwGuardActive()` (SATcycles.ino:164-168) then blocks the overshoot switch and resets the PWM exits during the draw and for 300 s after it (`heating_control.py:272-274`, `:313-315`).
- **Flame-on time for the PWM schedule.** The PWM hold ends `fModSupDelay` (default 20 s, SATtypes.h:455) after the cycle tracker's flame-on time (SATcontrol.ino:720-723, SATcontrol.ino:884-888). The PWM ON phase ends when the time since that flame-on is >= on-time and >= minimum on-time (SATcontrol.ino:813-820). That test does not check whether the flame is on, and it runs before the 180 s ignition timeout (SATcontrol.ino:824-832). While a new ON phase waits for ignition, the cycle tracker's flame-on time still belongs to the previous cycle, so the ON phase can end on the next control tick before the flame lights (derived from the order of the code; see Edge cases). The PWM controller also keeps its own `_pwm_flameOnMs` for the effective temperature EMA (SATcontrol.ino:764-782).
- **Flame-off time and per-hour count for the PWM schedule.** The OFF phase ends when the time since the cycle tracker's flame-off is >= off-time (or at once on the first step after PWM entry, TASK-1369), unless the rolling-hour count of PWM ON phases (TASK-1401; hot-water ignitions excluded) has reached `satGetMaxCyclesPerHour()` (SATcontrol.ino:1043-1078, SATcycles.ino:297-326). A heat pump has no hourly start limit (entry 81, SATcycles.ino:320-327); for other sources the maximum is the configured value when it is 2 to 6, else 3 (underfloor) or 4 (SATcontrol.ino:345-353).
- **PWM on-time into the classifier**: the PWM SHORT test (SATcycles.ino:689-692, SATcycles.ino:812).
- **Last cycle duration into the boiler status**: stall threshold (SATcontrol.ino:571-575).
- **Modulation reliability** is used only for the modulation direction in the boiler status (SATcontrol.ino:616-622) and is published (SATcontrol.ino:2893, restAPI.ino:1021). A search for `bModulationReliable` found no other reader.
- **Not consumed by control:** the 4 h and 24 h windows, the EMAs, the off-with-demand duration, the shape metrics, the cycle phase, the boiler status and the flame status are only published (SATcontrol.ino:2368-2389, SATcontrol.ino:2758-2810, SATcontrol.ino:2826-2847, SATcontrol.ino:3000-3009).

<a id="s5-formulas"></a>

### Formulas

Variable names are as in the code.

**Demand (boiler status)** (SATcontrol.ino:534):
```
hasDemand = fFinalSetpoint > flow + BS_DEMAND_HYSTERESIS      # 0.7
```

**Overshoot cooling** (SATcontrol.ino:556): `!flame && !_bs_prevFlame && flow > fFinalSetpoint + fOvershootMargin`.

**Anti-cycling** (SATcontrol.ino:564): `hasDemand && _bs_flameOffMs > 0 && (now - _bs_flameOffMs) < 180000 ms`, tested after overshoot cooling and post-cycle.

**Stalled ignition threshold** (SATcontrol.ino:571-577):
```
threshold = 600000 ms
if fLastCycleDuration > 0: threshold = max(threshold, fLastCycleDuration * 3000 ms)
stalled   = offDuration > threshold          # strictly greater
```

**Post-cycle settling** (SATcontrol.ino:559-561): `_bs_flameOffMs > 0 && offDuration < 60000 ms && prev in {HEATING, AT_SETPOINT, COOLING, POST_CYCLE}`, tested before the demand rules.

**Flow change per call** (SATcontrol.ino:537-544):
```
if _bs_prevBoilerTemp > 0.001 and (now - _bs_stateEntryMs) > 0.5 s:
    tempRate = flow - _bs_prevBoilerTemp        # per control tick, not per second
else tempRate = 0
```
`_bs_stateEntryMs` is the time of the last status change (SATcontrol.ino:639-641), not of the previous call.

**Ignition surge** (SATcontrol.ino:586-601): on the first flame-on call when flow >= setpoint - 6; later, within 30 s of the evaluator's flame-on time when `tempRate >= 0.5`.

**Modulation direction** (SATcontrol.ino:615-622):
```
if bModulationReliable:  +1 if mod > prevMod + 1.0;  -1 if mod < prevMod - 1.0;  else 0
else:                    +1 if tempRate > 0.1;        -1 if tempRate < -0.1;      else 0
```

**Modulation reliability** (SATcontrol.ino:3562-3585):
```
only when flame on, mod not NaN, mod >= 0
ring of the last 8 values; no verdict until the count has reached 8 on an earlier call
aboveThresh = count(value >= 3.0 in ring)
required    = max(2, int(8 * 0.4)) = 3
bModulationReliable = aboveThresh >= required
```

**Cycle kind** (SATcycles.ino:571-581, tracker.py:101-108 and :314-324; counted per 1 Hz sample, SATcycles.ino:907-929):
```
dhw = samples with Statusflags bit 2 (slave DHW mode); SAT simulation: bDhwActive
ch  = samples without DHW and with Statusflags bit 8 (master CH enable); SAT simulation: fFinalSetpoint > SAT_MIN_SETPOINT
DHW if dhw/n > 0.8 and ch/n < 0.2;  CH if ch/n > 0.8 and dhw/n < 0.2
MIXED if dhw/n > 0.1 and ch/n > 0.1;  else UNKNOWN
```
The fractions shown are `dhw/n` and `ch/n` (SATcycles.ino:607-609); they need not add up to 1. A cycle in which the master CH-enable bit is clear for more than 20 % of the samples and DHW for at most 10 % (for example a PWM flame tail after SAT sent CH=0, entry 5) is `UNKNOWN`, so `UNCERTAIN`, as in thermo-nova. On PIC and OT-Direct units that bit 8 follows SAT's CH= command was not verified on a bench.

**Per-cycle signals at flame-off** (SATcycles.ino:699-700, SATcycles.ino:683, SATcycles.ino:827):
```
ctrlErrP90 = tailP90 - controlSp          # controlSp = fFinalSetpoint at flame-on
ctrlErrP50 = tailP50 - controlSp
reqErrP90  = tailP90 - requestedSp        # requestedSp = _sat_requestedSetpoint at flame-off
fCycleReqSetpointError = tailP90 - requestedSp
```
The error percentiles are flow percentiles shifted by a setpoint that is constant for the cycle.

**Percentiles** (SATcycles.ino:481-508, SATcycles.ino:514-538), no interpolation:
```
sorted copy of the ring (insertion sort)
idx = int(pct * (n - 1) / 100), clamped to n - 1
p   = sorted[idx]
```
See "Tail percentile sort" under Edge cases for a defect in the tail version.

**Overshoot seconds** (SATcycles.ino:938-942), whole cycle, no warm-up: `elapsed` since the previous call is added while `flow > _cycle_setpointAtStart + fOvershootMargin`. Diagnostic only since TASK-1387: it no longer gates `OVERSHOOT`.

**Shape metrics** (SATcycles.ino:944-966), for every `satCycleSample` call from 60 s after flame-on, with `elapsed` = time since the previous call and `ctrlErr = flow - _cycle_setpointAtStart`:
```
abs(ctrlErr) <= 1.0          : _cycle_timeInBandSec += elapsed
ctrlErr >= fOvershootMargin  : _cycle_totalOvershootSec += elapsed
                               first time: _cycle_timeToFirstOvershootSec = sinceOnSec
                               _cycle_overshootStreakSec += elapsed
                               first time streak >= 60 s: _cycle_timeToSustainedOvershootSec = sinceOnSec
otherwise                    : _cycle_overshootStreakSec = 0
```
Each interval gets the error of the sample at its end. `-1` means "none yet" (SATcycles.ino:124-125).

**Off-with-demand duration**, at flame OFF->ON (SATcycles.ino:706-712):
```
offSec = (now - _cycle_lastFlameOffMs) / 1000 if _cycle_lastFlameOffMs > 0 else 0
demandPresent = !bDhwActive and (Statusflags & 0x02) and fFinalSetpoint > flow   # no hysteresis
fOffWithDemandSec = demandPresent ? offSec : 0
```
`_cycle_lastFlameOffMs` starts at the boot time (SATcycles.ino:247), so the first cycle measures from boot. It is never cleared when there is no demand.

**Moving averages** (SATcycles.ino:582-596), per recorded cycle, `EMA_ALPHA` = 0.15:
```
x_new = 0.15 * sample + 0.85 * x_old
overshoot sample = 1 if class OVERSHOOT
underheat sample = 1 if class UNDERHEAT or UNDERHEAT_PWM
long sample      = 1 if durationSec > 600          # strictly greater
duty sample      = durationSec / totalTime, capped at 1  (see section 8: always 1.0)
```

**4 h window statistics** (SATcycles.ino:308-397), computed every 60 s over records with `now - endMs <= 4 h` by `millis()`:
```
i4hCycles            = number of such records (all classes)
f4hAvgOnSec          = sum(onDurationMs) / n / 1000
f4hAvgOffSec         = sum(offDurationMs) / n / 1000
f4hAvgFlow           = mean of the per-cycle full p90 flow
f4hOvershootFraction = count(OVERSHOOT) / n
f4hUnderheatFraction = count(UNDERHEAT or UNDERHEAT_PWM) / n
f4hDutyRatio         = sum(on) / (sum(on) + sum(off))
f4hFlowRetDeltaP50/P90 = percentile (index method above) of per-cycle mean deltas >= 0
```

**24 h window statistics** (SATcycles.ino:402-475): one bucket per `millis() / 3600000` (uptime hour); a cycle goes into the newest bucket when it has the same index, else a new bucket opens. Every 60 s the last 24 bucket indices are summed:
```
i24hCycles            = sum(nCycles)
f24hDutyRatio         = sum(onMs) / (sum(onMs) + sum(offMs))
f24hOvershootFraction = sum(nOvershoot) / sum(nCycles)
f24hUnderheatFraction = sum(nUnderheat) / sum(nCycles)
f24hLongCycleFraction = sum(nLong) / sum(nCycles)     # nLong: durationSec >= 600
```

**Per-hour flame-on count** (SATcycles.ino:262-283): a 6-slot ring of flame-on times; the count is the number of stored times less than 3600000 ms old.

<a id="s5-constants-and-defaults"></a>

### Constants and defaults

| Name | Value | Unit | Citation |
|---|---|---|---|
| `SAT_CYCLE_HISTORY_SIZE` | 16 | cycles | SATcycles.ino:26 |
| PWM short ratio | 0.8 | x PWM on-time | SATcycles.ino:688 |
| Minimum samples for a class | 3 | `loop()` passes | SATcycles.ino:641 |
| Overshoot timer confirmation | > 10 | s | SATcycles.ino:673 |
| `fOvershootMargin` (setting) | 2.0 | °C | SATtypes.h:454 |
| `SAT_UNDERSHOOT_MARGIN_C` | 3.0 | °C | SATcycles.ino:30 |
| `SAT_OVERSHOOT_SUSTAIN_MS` | 60000 | ms | SATcycles.ino:29 |
| `SAT_UNDERHEAT_SUSTAIN_MS` | 180000 | ms | SATcycles.ino:31 |
| `SAT_SATURATION_SUSTAIN_MS` | 300000 | ms | SATcycles.ino:32 |
| `SAT_DHW_OVERSHOOT_GUARD_MS` | 300000 | ms | `SATcycles.ino:33` | 300 s hot-water guard of the overshoot switch and the PWM exits (armed by `satCycleNoteDhwTick`, TASK-1420) |
| `SAT_IN_BAND_MARGIN_C` | 1.0 | °C | SATcycles.ino:82 |
| `SAT_SHAPE_WARMUP_SEC` | 60 | s | SATcycles.ino:84 |
| `SAT_LONG_CYCLE_SEC` | 600 | s | SATcycles.ino:61 |
| `EMA_ALPHA` | 0.15 | per cycle | SATcycles.ino:143 |
| `SAT_MAX_CYCLES_PER_HOUR` (ring size) | 6 | PWM ON-phase starts (TASK-1401) | SATcycles.ino:37 |
| `SAT_WIN4H_SPAN_MS` | 14400000 (4 h) | ms | SATcycles.ino:45 |
| `SAT_WIN4H_SIZE` | 360 | records | src/libraries/Platform/src/boards.h:120 |
| `SAT_FLOW_SAMPLE_SIZE` | 256 | samples (1 Hz) | src/libraries/Platform/src/boards.h:121 |
| `SAT_TAIL_SAMPLE_SIZE` | 180 | samples (1 Hz) | src/libraries/Platform/src/boards.h:122 |
| Ring sample gate | 1000 | ms | SATcycles.ino:869 |
| Percentile minimum ring fill | 10 | samples | SATcycles.ino:777-783 |
| `SAT_DAILY_BUCKET_MS` / `SAT_DAILY_BUCKETS` | 3600000 / 24 | ms / buckets | SATcycles.ino:59-60 |
| Window statistics timer | 60 | s | SATcontrol.ino:478 |
| Control tick `iControlInterval` | 30 | s | SATtypes.h:444 |
| Cycle phase band | 1.5 | °C | SATcycles.ino:925 |
| `BS_ANTI_CYCLE_MIN_OFF_MS` | 180000 | ms | SATcontrol.ino:506 |
| `BS_STALLED_IGNITION_MIN_MS` | 600000 | ms | SATcontrol.ino:507 |
| Stall ratio | 3.0 | x last cycle duration | SATcontrol.ino:573 |
| `BS_POST_CYCLE_SETTLE_MS` | 60000 | ms | SATcontrol.ino:508 |
| `BS_IGNITION_SURGE_WINDOW_MS` | 30000 | ms | SATcontrol.ino:509 |
| `BS_DEMAND_HYSTERESIS` | 0.7 | °C | SATcontrol.ino:510 |
| `BS_AT_SETPOINT_BAND` | 1.5 | °C | SATcontrol.ino:511 |
| `BS_PUMP_START_DELTA` | 6.0 | °C | SATcontrol.ino:512 |
| `BS_PREHEAT_DELTA` | 6.0 | °C | SATcontrol.ino:513 |
| `BS_IGNITION_SURGE_RATE` | 0.5 | °C per control tick | SATcontrol.ino:514, SATcontrol.ino:542 |
| Modulation direction step (reliable) | 1.0 | % per control tick | SATcontrol.ino:617-618 |
| Flow gradient step (unreliable) | 0.1 / -0.1 | °C per control tick | SATcontrol.ino:620-621 |
| `MOD_REL_MIN_SAMPLES` | 8 | samples | SATcontrol.ino:3556 |
| `MOD_REL_DELTA_THRESH` | 3.0 | % | SATcontrol.ino:3557 |
| Modulation required share | 0.4 (min 2) | of 8 | SATcontrol.ino:3581-3582 |
| Flame status warm-up | 6 | calls | SATcontrol.ino:3629 |
| Flame status rate limit | 10 | s | SATcontrol.ino:3626 |
| Stuck-on setpoint limit | 5 | °C | SATcontrol.ino:3655 |
| Stuck-off setpoint / time | 30 / 300 | °C / s | SATcontrol.ino:3661-3663 |
| PWM-short duty limit | 0.05 | duty | SATcontrol.ino:3670 |
| `SAT_COLD_SETPOINT_RAD` / `_FLOOR` | 28.2 / 21.0 | °C | SATcontrol.ino:55-56 |
| `SAT_MIN_SETPOINT` | 10.0 | °C | SATcontrol.ino:54 |
| `fModSupDelay` (setting) | 20 | s | SATtypes.h:455 |

The buffer sizes are the same for the OTGW32, Classic-S3 and combo builds: the combo build defines `BOARD_NODOSHOP_ESP32` (src/libraries/Platform/src/boards.h:36-38) and uses the values at src/libraries/Platform/src/boards.h:120-122; the Classic-S3 values are at src/libraries/Platform/src/boards.h:192-194.

<a id="s5-state-kept-between-passes"></a>

### State kept between passes

**Cycle tracker (SATcycles.ino, file statics):**
- Open cycle: `_cycle_flameOn`, `_cycle_flameOnStartMs`, `_cycle_flameOffStartMs`, `_cycle_maxFlowTemp`, `_cycle_minFlowTemp`, `_cycle_setpointAtStart`, `_cycle_overshootSec`, `_cycle_lastSampleMs`, `_cycle_dhwSamples`, `_cycle_chSamples`, `_cycle_totalSamples`, `_cycle_lastDhwSample`, `_cycle_phase`, `_cycle_phaseStartMs` (SATcycles.ino:105-121).
- Shape metrics and last flame-off: SATcycles.ino:121-126. Flow-return delta sum: SATcycles.ino:87-88.
- Flow ring (256) and tail ring (180) with heads and counts (SATcycles.ino:92-103).
- Sustained-condition timers and the hot water end time (SATcycles.ino:129-136).
- EMAs (SATcycles.ino:139-142), 16-entry history (SATcycles.ino:146-157), per-hour ring (SATcycles.ino:38-40), 4 h window (SATcycles.ino:47-49), 24 h buckets (SATcycles.ino:63-74).
- Edge-detection memory `_sat_prevFlameState` (SATcontrol.ino:473).

**Boiler status evaluator:** `_bs_prevFlame`, `_bs_prevModulation`, `_bs_prevBoilerTemp`, `_bs_stateEntryMs`, `_bs_flameOnMs`, `_bs_flameOffMs` (SATcontrol.ino:465-470), plus `state.sat.eBoilerStatus`.

**Modulation reliability:** 8-value ring, head and count (SATcontrol.ino:3558-3560), `state.sat.bModulationReliable`, `state.sat.iModChangeCount` (SATtypes.h:265-266).

**Flame status:** `_flame_lastUpdateMs`, `_flame_sampleCount` (SATcontrol.ino:3619-3620), `state.sat.eFlameStatus`.

**Persisted:** nothing in this area. `satPersistAllToFs()` saves PID, energy and the heating-curve recommendation only (SATcontrol.ino:2128-2136), and states that `/sat/sat_cycles.json` is never written (SATcontrol.ino:2126-2127). Every reboot starts this area empty (section 2).

<a id="s5-outputs-to-the-boiler-to-entities"></a>

### Outputs (to the boiler, to entities)

**To the boiler:** the cycle tracker, the auto-switch, the boiler status evaluator, the modulation reliability tracker and the flame status tracker send no OpenTherm command themselves (SATcycles.ino:697-1024, SATcontrol.ino:516-647, SATcontrol.ino:3562-3585, SATcontrol.ino:3622-3676 contain no command call). They influence commands indirectly:
- the control mode through `satCycleCheckOvershootSwitch` (SATcycles.ino:1059) and `satCycleCheckPwmExits` (SATcycles.ino:1084, SATcycles.ino:1096); the overshoot switch runs before `satApplyPWM` / `satApplyContinuous` of the same tick (SATcontrol.ino:5537-5551), the exits after it;
- the PWM ON/OFF timing and the hold through the flame-on and flame-off times and the per-hour count (SATcontrol.ino:720-723, SATcontrol.ino:813-854);
- the resulting control setpoint is sent with `satEnqueueIfChangedCS` (for example SATcontrol.ino:917).

**Events:** none. Instead, at flame-on and flame-off the tracker sends a WebSocket status message (SATcycles.ino:730-734, SATcycles.ino:767-775) and a narration line (SATcycles.ino:728-729, SATcycles.ino:811-812).

**MQTT** (published by `satPublishMQTT`, which returns when MQTT or SAT is disabled, SATcontrol.ino:2647-2650; called at the end of the control tick, SATcontrol.ino:5018):
- `sat/boiler_status`: status name (SATcontrol.ino:2758-2759, names at SATcontrol.ino:650-672).
- `sat/cycle_class`: last class name (SATcontrol.ino:2761-2769).
- `sat/cycle_attributes`: JSON with kind, `sample_count`, duration, maximum flow, CH and DHW fractions, republished when the cycle count changes (SATcontrol.ino:2774-2793). The `sample_count` field carries `iCycleCount`, the number of cycles since boot (SATcontrol.ino:2787).
- `sat/duty_ratio`, `sat/overshoot_fraction` (EMAs) (SATcontrol.ino:2796-2797).
- `sat/cycle_phase`, `sat/cycles_this_hour` (SATcontrol.ino:2799-2800).
- `sat/4h_*` (SATcontrol.ino:2802-2810).
- `sat/flame_status`, and `sat/flame_health` ON when STUCK_ON, STUCK_OFF, PWM_SHORT or SHORT_CYCLING, not published while INSUFFICIENT_DATA (SATcontrol.ino:2826-2847).
- `sat/modulation_reliable` (SATcontrol.ino:2893).
- `sat/device_health`: ON when the boiler status is `OFF` (SATcontrol.ino:3000-3002).
- `sat/cycle_health`: ON when the last class is OVERSHOOT, UNDERHEAT, UNDERHEAT_PWM or SHORT (SATcontrol.ino:3003-3009).

**REST / status JSON:** cycle count, cycles this hour, last class, maximum flow, overshoot seconds, EMAs, the TASK-891.4 metrics, the 24 h values, phase and phase duration (SATcontrol.ino:2368-2389). The `detail=full` health JSON reports `sync_modulation` = not reliable, `device_health` = status not OFF and not STALLED_IGNITION, `cycle_health` = class not OVERSHOOT/UNDERHEAT/UNDERHEAT_PWM/SHORT (restAPI.ino:1017-1031), and cycle kind, duration, count and fractions (restAPI.ino:1066-1070). Note that MQTT `device_health` ON means a problem, while REST `device_health` true means healthy (SATcontrol.ino:3000-3002, restAPI.ino:1026-1027).

<a id="s5-edge-cases"></a>

### Edge cases

- **Flame already on at boot or at enable.** The first enabled pass sees OFF->ON and starts a cycle at that moment, not at the real ignition (SATcontrol.ino:473, SATcontrol.ino:4592-4596). Its off-with-demand time is measured from boot (SATcycles.ino:247, SATcycles.ino:707).
- **SAT disabled mid-cycle (derived from the order of the code).** While disabled, edge detection does not run (SATcontrol.ino:4568-4573), and `satDisable()` does not close the open cycle (SATcontrol.ino:2160-2187). After re-enable, the cycle closes at the next observed flame-off, so its duration includes the disabled time (SATcycles.ino:764).
- **Samples.** Since TASK-1387 the INSUFFICIENT test, the cycle kind and the DHW-in-tail test count 1 Hz samples in the same gate as the rings (SATcycles.ino:897-929). Max/min flow, the flow-return delta, the overshoot seconds and the shape metrics still run per `loop()` pass.
- **Short cycles.** Below 10 tail samples (about 10 s) the tail values fall back to the flow ring, and below 10 flow samples to the cycle maximum and minimum (SATcycles.ino:777-783). The flow ring is seeded with the flame-on flow, the tail ring is not (SATcycles.ino:739-746).
- **Tail percentile sort (derived from reading the code, not run).** `_tailPercentile` uses `uint8_t n` and `int8_t j = (int8_t)i - 1` in the insertion sort (SATcycles.ino:519, SATcycles.ino:525-533). For a ring index `i` of 129 or more, `(int8_t)i` is negative, so `j` is negative, the shift loop does not run, and `sorted[j + 1] = key` writes to a negative index outside `sorted`. The tail ring holds up to 180 samples (src/libraries/Platform/src/boards.h:122), so this applies to any cycle whose tail ring holds more than 129 samples, that is a cycle of more than about 130 s. Then only the first 129 (oldest) samples of the copy are sorted; with a full ring the p90 index is 161 (SATcycles.ino:535), which picks an unsorted sample from about 18 s before flame-off rather than the 90th percentile. `tailP50` (index 89) comes from the sorted oldest 129 samples. The flow ring version uses `int16_t j` and is not affected (SATcycles.ino:494-502). This was not verified with a compiler run or on a device.
- **PWM ON phase and a stale flame-on time (fixed by TASK-1382).** The ON phase used to be timed from `satCycleGetFlameOnStartMs()`, which changes only at a flame OFF->ON edge, so an ON phase that started before the ignition ended on its first tick. It is now timed from `_pwm_lastUpdateMs`, set at the ON start and at the first ignition (SATcontrol.ino:870, SATcontrol.ino:963, SATcontrol.ino:1019-1035; pwm.py:162, :188, :205). The PWM mechanism itself is described in the PWM section.
- **Off-time zero.** See section 8: window off-times are always 0 and the duty ratios are 1.0 (derived from the order of the code).
- **Hot water.** Since TASK-1387 a cycle with a hot water sample in its tail (after the warm-up, tracker.py:113-116), or of kind UNKNOWN, is UNCERTAIN (SATcycles.ino:667-670, cls:22-26). A DHW or MIXED cycle without hot water in the tail is classified by the PWM or continuous rules, as in thermo-nova.
- **Hot water and CH sources.** On the real bus the cycle kind and the DHW-in-tail test read `Statusflags` bit 2 and bit 8 per sample; under SAT simulation they read `bDhwActive` (forced true by a simulated draw, SATcontrol.ino:4998-5002) and a commanded CS above `SAT_MIN_SETPOINT` (SATcycles.ino:907-920) [lines refreshed 2026-10-25, TASK-1387 review]. The off-with-demand test still reads the real bus slave CH-mode bit (SATcycles.ino:726), also under simulation.
- **Setpoint used for the cycle.** The control error uses `fFinalSetpoint` captured at flame-on (SATcycles.ino:717). In PWM, `fFinalSetpoint` at the ignition tick is the flame-off hold value (return + offset, SATcontrol.ino:701-707, SATcontrol.ino:880-882) and changes later in the cycle (SATcontrol.ino:884-888), so the captured value is the setpoint of one moment (derived from the order of the code).
- **Requested setpoint.** The classifier uses `_sat_requestedSetpoint` of the last control tick (SATcycles.ino:807; set from the clamped multi-zone `pidOutput` plus the solar offset, TASK-1323), so it no longer differs from the auto-switch input.
- **Low setpoint.** Underheat with `requestedSp` below the cold setpoint (28.2 °C radiators, 21 °C underfloor) gives `UNCERTAIN` (SATcycles.ino:654, SATcycles.ino:684, SATcontrol.ino:55-56).
- **`HEATING_HOT_WATER` and `OFF`** are never produced by the evaluator after the first tick (section 4 notes).
- **Window time base.** Both windows count back from `millis()` at statistics time (SATcycles.ino:310, SATcycles.ino:328, SATcycles.ino:440-447), so old cycles leave the window even without new cycles. The 24 h buckets are uptime hours, not clock hours (SATcycles.ino:405).
- **4 h counter width.** `i4hCycles` is `uint8_t` (SATtypes.h:179) while the window holds up to 360 records (src/libraries/Platform/src/boards.h:120) and the count variable is `uint16_t` (SATcycles.ino:317, SATcycles.ino:342). More than 255 cycles in 4 h would wrap the published count.
- **Auto-switch gaps.** None since TASK-1420: the underheat and saturation timers have no `dt` and no 60 s gap bail-out, a gap between two ticks counts in full, and a skipped tick clears all three starts (SATcycles.ino:179-196).
- **Long-cycle threshold.** The EMA uses `> 600` (SATcycles.ino:586), the 24 h window `>= 600` (SATcycles.ino:431).
- **Heating-curve samples at boot (outside this area).** `initSAT()` loads intra-day samples (SATcontrol.ino:3687, SATcycles.ino:1166-1182) and then `satCycleInit()` empties them (SATcontrol.ino:3688, SATcycles.ino:255-256).

<a id="s5-not-found-in-the-code-unclear"></a>

### Not found in the code / unclear

1. **Comment vs code, overshoot margin.** The comment at SATcontrol.ino:4949-4950 says the auto-switch uses a 3.0 °C margin; the code uses `settings.sat.fOvershootMargin` (SATcycles.ino:973), default 2.0 (SATtypes.h:454). **Resolved by TASK-1367:** the comment (now SATcontrol.ino:5308-5309) names `settings.sat.fOvershootMargin`, default 2.0 (SATtypes.h:478).
2. **Comment vs code, cycle window persistence.** SATcycles.ino:1291-1293 says the 4 h ring is persisted to `/sat/sat_cycles.json`. No write code was found; SATcontrol.ino:2126-2127 says it is never written, and SATcycles.ino:1298 only removes it. `SAT_CYCLES_FILE_BUF_SIZE` is defined (src/libraries/Platform/src/boards.h:125, src/libraries/Platform/src/boards.h:197), but a search of `src/` found no other use of it.
3. **Comment vs code, "wall-clock hour".** SATcycles.ino:400-401 says buckets follow the wall-clock hour; the code uses `millis() / 3600000` (SATcycles.ino:405).
4. **Comment vs code, temperature rate.** SATcontrol.ino:536 says degrees per second; the code computes a difference per call (SATcontrol.ino:542).
5. **Comment vs code, `sample_count`.** The `cycle_attributes` field named `sample_count` carries `iCycleCount` (SATcontrol.ino:2785-2787).
6. **Comment vs code, flame status cadence.** SATcontrol.ino:3625 says "every 10 seconds"; the only caller is the control tick (SATcontrol.ino:4811), default 30 s (SATtypes.h:444).
7. **Rate of `loop()` passes.** Closed by TASK-1387: the INSUFFICIENT test and the kind fractions now count 1 Hz samples (SATcycles.ino:897-929).
8. **OTDirect boards.** Whether the OTDirect path fills `OTcurrentSystemState.Statusflags` through the same `print_status` route (OTGW-Core.ino:4548) was not traced; OTDirect.ino:1622 only reads it.
9. **No counterparts found** for: the cycle start/end events; the 3-sample discard (the firmware records the cycle as `INSUFFICIENT`); the cycle verdict switching PWM on or off; `max_modulation_level` and its >= 3 gate in the reliability tracker; the 50-value modulation history; persistence of the reliability verdict; the 6 h expiry of the last cycle; the separate requested-setpoint and control-setpoint per sample (the firmware uses one constant per cycle for each); interpolated percentiles; warm-up as 25 % of the duration. Searches over `src/OTGW-firmware` (`*.ino`, `*.h`, `*.cpp`): `on_cycle_end|OnCycleEnd` and `cycle_start|cycle_end|CycleStart|CycleEnd|cycle_started|cycle_ended` found nothing; `max_modulation|MaxRelMod` found `MaxRelModLevelSetting` read only outside this area (for example SATcontrol.ino:3098-3099), not in SATcycles.ino or in `satUpdateModulationReliability` (SATcontrol.ino:3562-3585).
10. **Firmware-only parts** with no thermo-nova counterpart in this area: the flame status tracker (SATcontrol.ino:3622-3676), the cycle phase (SATcycles.ino:922-937), the per-hour counter (SATcycles.ino:260-301), the EMAs (SATcycles.ino:139-143), the 16-entry history (SATcycles.ino:146-157), the overshoot-seconds timer (diagnostic only since TASK-1387; it no longer gates OVERSHOOT), and the `UNDERHEAT_PWM` class (SATtypes.h:72). The continuous `SHORT` and "under 120 s" `UNCERTAIN` rules were removed in TASK-1387.
11. **History readers.** `satCycleCountClass()` (SATcycles.ino:1027-1036) reads the 16-entry history; a search for `satCycleCountClass` outside SATcycles.ino found no caller, so the history is written but not read by other code.

---

<a id="s6"></a>

## Areas, presets, window detection and thermostat sync (OTGW firmware, as implemented)

Source: OTGW-firmware, branch `dev`, HEAD 51df6b486, directory `src/OTGW-firmware`. Every statement below cites a line range that was opened and read. The working tree had uncommitted edits in MQTTstuff.ino and settingStuff.ino; each replaces one line by one line in code outside this section (MQTT client id and top topic), so the cited line numbers match HEAD. OTGW-firmware.h has an extra line in the working tree and is not cited. Worked examples are marked "computed" and follow only from the cited formulas. Statements marked "follows from the code, not run" were derived by reading; they were not executed on a device or in a test.

This document describes what the firmware does, not what it should do. It uses the same headings as the thermo-nova section `a6-areas-presets-thermonova.md` so the two can be compared line by line.

<a id="s6-purpose"></a>

### Purpose

The firmware runs one SAT controller with one target room temperature `settings.sat.fTargetTemp` (SATtypes.h:441), one heating curve and one main PID (SATcontrol.ino:4836-4839). There is no Home Assistant climate entity per room and no link to other climate entities. Rooms enter in two separate, unrelated ways:

| Mechanism | What it is | Effect | Citation |
|---|---|---|---|
| Multi-area ("areas", `bMultiArea`) | Up to 4 pushed room temperatures with a weight each | Their weighted average becomes the main room temperature that the single main PID uses | SATcontrol.ino:86, SATcontrol.ino:1217-1222, SATcontrol.ino:1696-1719 |
| Multi-zone ("zones", `iZoneCount`) | Up to 4 pushed (room temperature, setpoint) pairs, each with its own PID | Every counted zone feeds the zones output; the requested setpoint is the maximum of the main PID output and that output, then limited by the zone overshoot cap. A zone without usable inputs does not count | SATcontrol.ino:2158-2161, SATcontrol.ino:5540-5561 |

This section also covers:

- presets (fixed target temperatures: away, eco, comfort, sleep, activity, home) (SATcontrol.ino:949-1024, SATtypes.h:54-58, SATtypes.h:446-451);
- publishing the preset name to a configurable topic ("preset sync") (SATcontrol.ino:964-974);
- following the wall thermostat's own room setpoint (MsgID 16) and pushing SAT's target to the thermostat with `TC=` (SATcontrol.ino:1026-1093, SATcontrol.ino:4991-4994);
- window open detection, which switches to the activity preset after a minimum open time (SATcontrol.ino:1096-1143);
- an external "valves open" flag that forces the boiler off when false (SATcontrol.ino:4778-4790, MQTTstuff.ino:634-639);
- how the target temperature is set and limited (SATcontrol.ino:1903-1934, settingStuff.ino:997).

The thermo-nova table of linked climate entities (radiators, rooms, thermostat) has no counterpart. The firmware does not call any other climate entity. Its only contacts with other devices in this area are the OpenTherm thermostat on the bus (MsgID 16 in, `TC=` out) and one MQTT topic for preset sync.

<a id="s6-inputs"></a>

### Inputs

Main room temperature, chosen per control tick by `satGetRoomTemp()` in this order (SATcontrol.ino:1192-1280):

1. Simulation mode: the simulated room temperature, or no value during a simulated sensor dropout (SATcontrol.ino:1195-1203).
2. BLE sensor, if enabled and valid and updated within 5 min (SATcontrol.ino:1208-1216, SATcontrol.ino:65).
3. Multi-area weighted average, if `bMultiArea` is on and `iMultiAreaCount > 0` and at least one area is valid (SATcontrol.ino:1217-1222).
4. External indoor temperature pushed over MQTT or REST, if `bUseExternalTemp` is on and the value is younger than `iSensorMaxAgeS` (default 21600 s) (SATcontrol.ino:1223-1231, SATtypes.h:515).
5. OpenTherm room temperature `Tr` (MsgID 24). A thermostat-reported `Tr` older than 5 min counts as missing (SATcontrol.ino:1236-1246, SATcontrol.ino:66). With "thermal comfort" on and fresh humidity, the Summer Simmer index replaces the raw `Tr` (SATcontrol.ino:1258-1272). This substitution is only on the `Tr` path; a BLE, multi-area or external value is returned before it (SATcontrol.ino:1208-1231).

Outside temperature: simulation, then an external outdoor value younger than 10 min (`SAT_STALE_OUTDOOR_MS`, SATcontrol.ino:67), then a weather API value when the OT outside temperature is 0 (SATcontrol.ino:1283-1299). In the control tick, a value outside -40 to 50 °C is replaced by 10 °C (SATcontrol.ino:4721-4723).

Per area (multi-area), index 0 to 3:

- A temperature, accepted when strictly between -50 and 100 °C; it marks the area valid and stamps the time (SATcontrol.ino:1665-1692).
- Sources: MQTT command `sat/area/<0-3>` (MQTTstuff.ino:885-894); REST `sat/area/<0-3>` (restAPI.ino:1590-1608); a Dallas DS18B20 whose address is mapped to the area (sensors_ext.ino:296-305); a BLE sensor whose MAC is mapped to the area (SATble.ino:842-852). The mapping is `settings.sat.sSensorArea[4][18]` (SATtypes.h:535-540).
- A weight `fAreaWeight[i]`, default 1.0, settable 0 to 10 (SATtypes.h:509, settingStuff.ino:1094-1097).

Per zone (multi-zone), number 1 to 4:

- `sat/zone/<n>/room_temp`: accepted from -10 to 50 °C (SATcontrol.ino:1753-1767, MQTTstuff.ino:895-902).
- `sat/zone/<n>/setpoint`: accepted from 5 to 30 °C (SATcontrol.ino:1770-1784, MQTTstuff.ino:903-904).
- Each accepted value stamps `iLastUpdateMs`, shared by both values (SATcontrol.ino:1764, SATcontrol.ino:1781).
- Zones are only reachable over MQTT; the MQTT parser knows only `room_temp` and `setpoint` (MQTTstuff.ino:900-907). A grep of restAPI.ino for `satHandleZone` found nothing.

Window input:

- One window state, sent as MQTT command `sat/window` (MQTTstuff.ino:627-632, MQTTstuff.ino:670) or REST `sat/window` (restAPI.ino:1383-1401). Payload `open`, `1` or `ON` (case-insensitive) means open; any other payload means closed (MQTTstuff.ino:628-630, restAPI.ino:1393-1395).
- There is no list of window sensors. The firmware sees whatever single state the sender pushes.
- Detection must be enabled (`bWindowDetection`, default off) (SATcontrol.ino:1100, SATtypes.h:463).
- Minimum open time `iWindowMinOpenSec`, default 60 s, range 10 to 600 s (SATtypes.h:464, settingStuff.ino:1036, restAPI.ino:4052).

Valves open input: MQTT command `sat/valves_open`; `true`, `1` or `open` sets it true, anything else false (MQTTstuff.ino:634-639, MQTTstuff.ino:671). Default true (SATtypes.h:243). A grep of all `.ino` files for `bValvesOpen` found no other writer.

Thermostat setpoint input: every MsgID 16 (`TrSet`) Write-Data frame sent by the thermostat (T frame) that is not skipped, also when the gateway replaced it on the way to the boiler (OTGW-Core.ino:5211-5217).

Preset temperatures: six settings, persisted in the settings file (SATtypes.h:446-451, settingStuff.ino:324-329). Presets are selected by MQTT `sat/preset`, REST `sat/preset` or REST `sat/settings/preset` (MQTTstuff.ino:666, restAPI.ino:1402-1414, restAPI.ino:1638-1640).

<a id="s6-behaviour-step-by-step-in-time-order"></a>

### Behaviour (step by step, in time order)

#### 1. Start-up of the main thermostat

1. Settings are read from the settings file line by line through `updateSetting()`, so the same clamps apply as for a live change (settingStuff.ino:554-555). The target is clamped to 5 to 30 °C (settingStuff.ino:997). Preset temperatures are clamped per preset (settingStuff.ino:1005-1010).
2. If the file has no `SATfollowthermostat` key, `bFollowThermostat` is set equal to `bPushSetpoint` and marked for saving (settingStuff.ino:602-611).
3. `initSAT()` resets and reloads the main PID state, sends `CS=0` if an OT command interface exists, sets the control timer to the configured interval, and, if SAT is enabled, sends `PM=3`, `PM=15`, `PM=48` (SATcontrol.ino:3681-3738). It does nothing with presets, areas, zones or windows.
4. Runtime state starts at its declared defaults: active preset `none`, window closed, no saved pre-window or pre-custom temperature, valves open (SATtypes.h:205, SATtypes.h:243-251). Areas start invalid (SATtypes.h:383-385). Zones start with no valid room temperature or setpoint (SATtypes.h:130-139, SATcontrol.ino:1726). None of these are restored from storage: a grep for `satZones` found no save or load, and the preset and window fields live in `state`, not in the settings file (SATtypes.h:204-205, SATtypes.h:244-251).
5. The follow-thermostat detector starts with no last value, so the first MsgID 16 value after boot is only recorded (SATtypes.h:400-403, SATcontrol.ino:1069).

There is no restore of the active preset, of the pre-custom temperature or of the per-room home temperatures. After a reboot the preset is `none` and the target is the value in the settings file (follows from the code, not run). See also edge case 14 on when a preset or window target reaches that file.

#### 2. Periodic timers (after listeners are registered)

| What | Interval | Citation |
|---|---|---|
| Control loop tick (main PID, zone PIDs, window timer check, commands) | `iControlInterval`, default 30 s, range 10 to 300 s | SATcontrol.ino:476, SATtypes.h:444, settingStuff.ino:1000-1003 |
| Main PID minimum time between updates | 10 s | SATpid.ino:21, SATpid.ino:274-279 |
| Zone PID output | every control tick, for every configured zone (no own timer; the integral step is on the 60 s PID timer) | SATcontrol.ino:5552-5556 |
| Window minimum open time | checked once per control tick | SATcontrol.ino:4755, SATcontrol.ino:1126-1143 |
| Re-send of an unchanged `TC=` | never (TASK-1272); only after a cache reset or push off and on | SATcontrol.ino:540-553 |
| Area value staleness | 5 min | SATcontrol.ino:87, SATcontrol.ino:1706-1711 |
| Zone value staleness | `iZoneTimeoutS`, default 300 s, range 30 to 3600 s | SATtypes.h:532, settingStuff.ino:1113, SATcontrol.ino:1818 |

There are no state-change listeners. MQTT and REST commands call their handler directly when they arrive (MQTTstuff.ino:875-913, restAPI.ino:1383-1414). The heating curve is recalculated on every control tick that reaches it (SATcontrol.ino:4836).

#### 3. Per area PID tick (every 60 s)

There is no per-area PID. Areas only feed the weighted average of step "Inputs" (SATcontrol.ino:1696-1719).

The closest counterpart is the per-zone PID: `satZonePidAdvance()` (integral and error, on the 60 s PID timer via `satZonePidAdvanceAll()`, TASK-1368) and `satZonePidOutput()` (output, on each control tick), for each configured zone, also at `iZoneCount` 1 (SATcontrol.ino:5552-5556, SATcontrol.ino:2255-2310):

1. If the zone is OFF, has no room temperature or no setpoint yet, has not been updated within `iZoneTimeoutS`, has a room temperature outside -10 to 50 °C or a setpoint outside 5 to 30 °C: the zone's integral and previous error are set to 0, its output to 10 °C, and 10 °C is returned (SATcontrol.ino:1790-1796, SATcontrol.ino:1814-1825). Nothing sets the OFF flag (see Not found, item 1).
2. Otherwise compute the zone's heating curve from its own setpoint and the shared outside temperature (SATcontrol.ino:1828).
3. Gains: `kp = coefficient * curve / divisor` (divisor 4 for underfloor, 3 otherwise) and `ki = kp / 8400` (SATcontrol.ino:1834-1837). This automatic formula is used even when the main PID is set to manual gains (compare SATpid.ino:122-128).
4. Error `satPidError(setpoint, room)`, rounded to 3 decimals (SATcontrol.ino:1839, SATpid.ino:63-66).
5. Integral: inside the deadband, add `ki * error * 60`, then clamp to plus or minus the curve value and to plus or minus 20; outside the deadband, set it to 0 (SATcontrol.ino:1840-1853).
6. Output `curve + P + I`, with no D term, clamped between 10 °C and the system maximum (SATcontrol.ino:1855-1866, SATcontrol.ino:228-234).

Zone PID state is kept only in RAM (SATcontrol.ino:1726). It is not saved after each update.

Contrast with the main PID: a lost main room temperature no longer skips the tick or trips safety (TASK-1364): the main output is the heating curve, and a zone with its own valid temperature still uses its own PID. An inactive zone is only excluded and its PID memory cleared (SATcontrol.ino:1790-1796).

#### 4. Per control loop tick: combining main and rooms into one requested setpoint

The control tick returns early, before any of the steps below, when SAT is disabled, safety is tripped, DHW is active, the boiler reports a fault, or the room temperature has been estimated for too long and the thermal safe fallback setpoint is sent instead (SATcontrol.ino:4568-4573, SATcontrol.ino:4581, SATcontrol.ino:4865-4882 [lines refreshed 2026-10-24, TASK-1351], SATcontrol.ino:4716-4719, SATcontrol.ino:4730-4746). A lost room temperature no longer returns early (TASK-1364). Later, it also returns early on summer mode or on `valves_open == false`, in both cases after sending `CS=10`, the max modulation and `CH=0` (SATcontrol.ino:4994-5021 [lines refreshed 2026-10-24, TASK-1351]).

The requested setpoint is formed as follows (SATcontrol.ino:4831-4912):

1. `effectiveTarget = fTargetTemp (+ PV boost when active)` (SATcontrol.ino:4831-4833). The PV boost belongs to another section.
2. Main heating curve from `effectiveTarget` and the outside temperature, then main PID output (SATcontrol.ino:4836-4839).
3. Zone block, for every zone count 1 to 4 (SATcontrol.ino:5540-5561, `satAggregateZoneSetpoint()` SATcontrol.ino:2329-2377; ports thermo-nova climate.py:369-394, area.py:166-175, :264-333, entry 116, TASK-1440):
   1. Run every configured zone's PID. A zone that is not usable (OFF, stale, never pushed, out of range: `satZoneUsable()`) does not count, so a setup without zone data is unchanged (SATcontrol.ino:5552-5556).
   2. ECO (`iHeatingMode` 1): keep the main PID output; the zone outputs are still computed for the diagnostics (SATcontrol.ino:2333).
   3. A zone counts when it is usable and its output is above the cold setpoint: 28.2 °C radiators, 21.0 °C underfloor (`satGetColdSetpoint()`, ADR-199). No counted zone gives a zones output of 10 °C, which leaves the main output alone (SATcontrol.ino:2343, SATcontrol.ino:2352).
   4. Zones output: sort the counted outputs ascending, `baseline = outputs[int(N * 0.75)]` (at most N - 1), `chosen = min(highest, baseline + fZoneAggregationHeadroom)`, rounded to 1 decimal (SATcontrol.ino:2354-2364).
   5. `requested = max(main PID output, zones output)` (SATcontrol.ino:2367). The zones only raise the demand of the main room.
   6. Overshoot cap: each counted zone more than 0.3 °C above its target gives `cold + 10 - 4 * degrees over`; the lowest wins, floored at the cold setpoint, rounded to 1 decimal; `requested = min(requested, cap)`. The cap can lower `requested` below the main PID output (SATcontrol.ino:2342-2350, SATcontrol.ino:2368-2372).
   7. Publish zone diagnostics, only when `iZoneCount` > 1 (SATcontrol.ino:5561).
4. Solar gain offset, then clamp between 10 °C and the effective maximum setpoint (SATcontrol.ino:5578-5585, SATcontrol.ino:240-254).

The main PID always runs. With the zones, `requested` is the maximum of the main PID output and the zones output, then the cap is applied as a minimum (SATcontrol.ino:2367-2372). The old behaviour is gone: the `ceil(0.75 * N) - 1` index, the headroom added on top of the P75 output, the 1:1 subtraction of the largest overshoot with a floor of 10 °C, a single active zone replacing the main output, counting zones above 10 °C, and running only with 2 or more zones and also in ECO. After this point the value goes into PWM or continuous control and the cold-setpoint cutoff; those belong to other sections.

Heating mode ECO or COMFORT: the setting `iHeatingMode` exists (0 = comfort, 1 = eco) and can be set over MQTT and REST (SATtypes.h:524, SATcontrol.ino:2211-2221, settingStuff.ino:1104). The control path reads it in one place: the zone block passes `iHeatingMode == 1` to `satAggregateZoneSetpoint()`, so ECO follows the main PID only (SATcontrol.ino:5558, SATcontrol.ino:2333; thermo-nova climate.py:377-379). Everywhere else ECO and COMFORT behave the same. ECO still applies the solar offset in the firmware, which thermo-nova does not (it returns before it); this is open question Q58 to Sergeant D.

The valve threshold, room "requires heat" test and the median error recording of thermo-nova have no counterpart. The only valve input is the single `valves_open` flag (MQTTstuff.ino:634-639). The valve offset setting `fValveOffset` (range -1 to 1) is stored, published and shown, but a grep found no use in control (SATtypes.h:526, settingStuff.ino:1106, SATcontrol.ino:2535, SATcontrol.ino:2980, handleDebug.ino:121, restAPI.ino:2317, restAPI.ino:4025).

#### 5. Event: a room climate changes

There are no room climate entities. The counterparts are pushed values:

- A new area temperature only updates `fAreaTemp`, `bAreaValid` and the timestamp (SATcontrol.ino:1665-1692). It takes effect at the next control tick through the weighted average.
- A new zone room temperature or setpoint only updates the zone record (SATcontrol.ino:1753-1784). It takes effect at the next control tick. A zone setpoint change does not reset the zone PID.
- There is no table of per-room "home" temperatures and nothing is learned from room changes.

#### 6. Event: the user sets a temperature

`satHandleTargetTemp(value)` is called from MQTT `sat/target` and REST `sat/target` (SATcontrol.ino:1903-1934, MQTTstuff.ino:612, MQTTstuff.ino:657, restAPI.ino:1280-1294):

1. Missing or non-numeric value: rejected (SATcontrol.ino:1905-1908). Over REST this gives HTTP 400 (restAPI.ino:1289-1292).
2. Value outside 5 to 30 °C: rejected (SATcontrol.ino:1909, SATcontrol.ino:1933).
3. If the value is within 0.05 °C of a preset temperature, call `satHandlePreset()` with that preset. The order is comfort, eco, away, sleep, activity, home; the first match wins (SATcontrol.ino:1913-1926).
4. Otherwise store it through `updateSetting("SATtargettemp")`, which clamps to 5 to 30 °C and schedules a deferred save (SATcontrol.ino:1928-1931, settingStuff.ino:997, settingStuff.ino:1283-1287).

In step 4 the active preset is not changed and the main PID is not reset (SATcontrol.ino:1928-1931, settingStuff.ino:997). So after choosing "comfort" and then setting 19.3 °C, the preset still reads `comfort` (follows from the code, not run).

No push to other climates happens here. A changed target reaches the thermostat with `TC=` at the next control tick, if push-setpoint is on (SATcontrol.ino:4991-4994).

#### 7. Event: the user sets a preset

`satHandlePreset(value)` (SATcontrol.ino:976-1024):

1. Unknown name: return without any change (SATcontrol.ino:988). REST still answers 200 OK (restAPI.ino:1412-1413).
2. Set the active preset (SATcontrol.ino:990). There is no "same as current, do nothing" check, so re-selecting the active preset sets the target to the preset value again.
3. Any preset other than `none`:
   1. If no pre-custom temperature is saved (`fPreCustomTemp == 0`), save the current target there (SATcontrol.ino:992-995). Moving from one preset to another therefore keeps the first saved value.
   2. Write the preset temperature directly into `settings.sat.fTargetTemp` (SATcontrol.ino:996).
   3. Set `state.sat.fPidI = 0` (SATcontrol.ino:997-998). This is the published copy of the integral; the integral itself is `_pid_integral` (SATpid.ino:34), which this line does not touch. The next main PID update copies `_pid_integral` back into `fPidI` (SATpid.ino:298-303). The integral is cleared only if the new error is outside the deadband (SATpid.ino:153-156). So the stored integral survives a preset change when the error stays inside the deadband (follows from the code, not run).
4. Preset `none`: if a pre-custom temperature is saved, restore it as the target, set `fPidI = 0` (same remark as above) and publish `sat/target` at once; then clear the saved value (SATcontrol.ino:1000-1021).
5. Publish the preset name to the preset sync topic, if enabled (SATcontrol.ino:1023, SATcontrol.ino:965-974).

Preset changes do not go through `updateSetting()`, so they do not mark the settings for saving (see edge case 14). There is no per-room preset sync: the only sync is one MQTT message with the preset name (SATcontrol.ino:965-974).

#### 8. Event: the user sets the HVAC mode

There is no HVAC mode setter with heat and off and no sync of modes to other climates. The nearest counterparts:

- `sat/enabled` switches SAT on or off through `updateSetting("SATenabled")`; an enable also clears a safety trip (SATcontrol.ino:1950-1958). Switching off calls `satDisable()`, which releases the boiler with `CS=0` (settingStuff.ino:981-991, SATcontrol.ino:2160-2187).
- `sat/control_mode off` does the same as `sat/enabled 0` (SATcontrol.ino:2225-2230).
- The Home Assistant climate entity that the firmware announces lists modes `off`, `heat`, `cool` and reads the mode from the `hvac_mode` topic (MQTTHaDiscovery.cpp:3009, MQTTHaDiscovery.cpp:3021). A grep of MQTTHaDiscovery.cpp for `mode_cmd_t` found nothing, so this entity has no mode command.

#### 9. Event: the linked wall thermostat changes

"Follow thermostat" (`bFollowThermostat`, default off) (SATtypes.h:461). On every MsgID 16 T frame, `satOnThermostatSetpoint(tr)` runs a detector (OTGW-Core.ino:5211-5217, SATcontrol.ino:1085-1093, SATcontrol.ino:1064-1079):

1. Not active (SAT disabled or option off): forget the last value and any candidate; adopt nothing (SATcontrol.ino:1067, SATcontrol.ino:1087).
2. Value outside 5 to 30 °C: ignore (SATcontrol.ino:1068).
3. No last value yet: record it, adopt nothing (SATcontrol.ino:1069).
4. Change smaller than 0.095 °C from the last value: ignore and drop any candidate (SATcontrol.ino:1070).
5. Debounce: a new value must be seen in two consecutive frames, within 0.051 °C of each other. The first sighting only becomes the candidate (SATcontrol.ino:1071).
6. Confirmed: the value becomes the new last value (SATcontrol.ino:1072-1074).
7. Push-setpoint off: adopt (SATcontrol.ino:1075).
8. Push-setpoint on: adopt once SAT has a `TC=` reference (`fLastSentTC > 0`, i.e. a `TC=` was sent or a value was adopted since boot) and the new value is more than 0.25 °C away from that reference; a value within 0.25 °C is SAT's own echo and is recorded, not adopted (`satFollowThermostatDecide`, SATcontrol.ino:1239-1253, rule comment SATcontrol.ino:1230-1237). The reference is the last sent `TC=`, or the thermostat value SAT last adopted, because an adoption sets `fLastSentTC` to that value without sending `TC=` (`satTCNoteAdopted`, SATcontrol.ino:534-539, SATtypes.h:401). TASK-1298 dropped the earlier condition that the previous thermostat value had to be within 0.25 °C of the last `TC=` (maintainer decision, entry 2) (corrected 2026-10-24, TASK-1351).

Adopting (`satAdoptThermostatSetpoint`) (SATcontrol.ino:1205-1217 [lines refreshed 2026-10-24, TASK-1351]):

- set the active preset to `none` and clear the saved pre-custom temperature, without restoring it;
- store the value through `updateSetting("SATtargettemp")`, so it is clamped to 5 to 30 °C and saved;
- publish `none` to the preset sync topic if a preset was active.

No PID is reset (SATcontrol.ino:1205-1217). The value is not pushed back to the thermostat: `satTCNoteAdopted` sets `fLastSentTC` to the adopted target and marks it accounted, so the write-on-change `TC=` of the next tick has nothing to send (SATcontrol.ino:1213, SATcontrol.ino:534-539, SATcontrol.ino:541-544, SATcontrol.ino:5231-5235; TASK-1272) (corrected 2026-10-24, TASK-1351): was "the normal `TC=` at the next control tick sends the new target".

#### 10. Event: a window sensor changes (window open detection)

`satHandleWindow(isOpen)` (SATcontrol.ino:1098-1123) and `_satCheckWindowTimer()` (SATcontrol.ino:1126-1143):

1. Detection off: the call returns at once, for both open and closed (SATcontrol.ino:1100).
2. Open while not yet open: mark open, store the open time, log "Window open detected: heating paused" (SATcontrol.ino:1102-1108). Nothing else changes at this moment; the target and preset stay as they are.
3. Open while already open: no change (SATcontrol.ino:1102).
4. On each control tick that reaches the timer check (SATcontrol.ino:4755), if the window is open, not yet switched, and open for at least `iWindowMinOpenSec`:
   1. Save the current target in `fPreWindowTarget` and `fPreActivityTemp`, and the current preset in `iPreWindowPreset` (SATcontrol.ino:1134-1136).
   2. Set the preset to `activity` and write `fPresetActivity` (default 10 °C) into the target (SATcontrol.ino:1137-1138, SATtypes.h:450).
   3. Reset the main integral with `satResetIntegral()`, which clears `_pid_integral` and `fPidI` (SATcontrol.ino:1139, SATpid.ino:86-91).
5. Closed while open: mark closed, clear the open time, log "Window closed: heating resumes" (SATcontrol.ino:1109-1113). If the switch to activity had happened: restore the saved target and the saved preset directly (not through the preset handler), clear both saved values and reset the main integral (SATcontrol.ino:1114-1121).
6. Closed while already closed: no change (SATcontrol.ino:1109).

Timing (follows from the code, not run): the main loop reads `targetTemp` at SATcontrol.ino:4658, before the window check at SATcontrol.ino:4755. The heating curve uses that local copy (SATcontrol.ino:4831, SATcontrol.ino:4836), so the activity target drives the PID from the next tick. The `TC=` at SATcontrol.ino:4993 reads `settings.sat.fTargetTemp` directly, so, when push-setpoint is on and the tick reaches the command block (SATcontrol.ino:4977-4994), it already sends the activity temperature on the same tick.

So "window open" in the firmware means: after the minimum open time, measured in control ticks, SAT changes its own target to the activity temperature. There is no direct heating-off action. The only other code that reads the open flag is the PV boost guard, which blocks the boost while the window is open (SATcontrol.ino:1570-1571). Neither the switch to activity nor the restore calls the preset sync (SATcontrol.ino:1126-1143, SATcontrol.ino:1109-1122).

#### 11. Inside or outside sensor changes

There are no sensor-change events. Room and outside temperatures are read at the start of every control tick (SATcontrol.ino:4656-4658), and the main heating curve is recalculated each tick from the effective target and the outside temperature (SATcontrol.ino:4836). Each zone curve uses its own setpoint and the same outside temperature (SATcontrol.ino:1828).

`satCalcHeatingCurve()` writes its result into `state.sat.fHeatingCurveValue` on every call (SATcontrol.ino:485-499). The zone steps call it after the main curve (SATcontrol.ino:4836, SATcontrol.ino:4854, SATcontrol.ino:1828). With zones in use, that state field therefore holds the last zone's curve, not the main curve, when the tick ends (follows from the code, not run).

#### 12. Service: reset integral

MQTT `sat/reset_integral` and REST `sat/reset_integral` or `sat/settings/reset_integral` call `satResetIntegral()` (MQTTstuff.ino:620, MQTTstuff.ino:672, restAPI.ino:1372-1376, restAPI.ino:1624-1627). It clears the main integral only (SATpid.ino:86-91). Zone integrals are not touched.

#### 13. Shutdown

There is no removal step. `satDisable()` (SAT switched off, safety trip) sets mode off, resets the PWM state and the main PID, saves the PID state and sends `CS=0` (SATcontrol.ino:2160-2187). It does not change the preset, the window state, the area values or the zone records. On a reboot all of those runtime values are lost (see step 1).

<a id="s6-formulas"></a>

### Formulas

PID error (main and zone) (SATpid.ino:63-66):

```
error = round((target - room) * 1000) / 1000
```

Heating curve, main and zone (SATcontrol.ino:485-499, SATcontrol.ino:49-51):

```
diff  = outside - 20
curve = 4*(target - 20) + 0.03*diff^2 - 0.4*diff
value = base_offset + (coefficient / 4) * curve        # not rounded
```

`base_offset` is 20.0 for underfloor and 27.2 otherwise (SATcontrol.ino:49-50, SATcontrol.ino:254-260). The coefficient default is 2.0 (SATtypes.h:443, TASK-1347; it was 1.5 before).

Multi-area weighted room temperature (SATcontrol.ino:1696-1719):

```
for i in 0 .. min(iMultiAreaCount, 4) - 1:
    skip if area i is not valid
    if now - last_update_i > 300 s: mark invalid, skip
    w = fAreaWeight[i]; skip if w <= 0
    sum_wt += temp_i * w;  sum_w += w
if sum_w <= 0: no value (fall through to the next source)
room = sum_wt / sum_w
```

Zone PID output (SATcontrol.ino:1834-1866):

```
curve = heating_curve(zone_setpoint, outside)
kp = coefficient * curve / (4 if underfloor else 3)
ki = kp / 8400
error = round3(zone_setpoint - zone_room)
if |error| <= deadband:
    I = clamp(I + ki * error * 60, -curve, +curve); I = clamp(I, -20, 20)
else:
    I = 0
output = clamp(curve + kp * error + I, 10, system_max)
```

Zone aggregation (`satAggregateZoneSetpoint()`, SATcontrol.ino:2329-2377), with `N` counted zones (usable and output above the cold setpoint `cold`: 28.2 radiators, 21.0 underfloor), `headroom = fZoneAggregationHeadroom` (default 5.0), `primary` the main PID output:

```
if ECO: result = primary
if N == 0: zones = 10
else:
    outputs sorted ascending
    idx    = min(int(N * 0.75), N - 1)
    chosen = min(outputs[N-1], outputs[idx] + headroom)
    zones  = round1(chosen)
result = max(primary, zones)
for each counted zone with error = round3(target - room) < -0.3:
    c = cold + 10 - 4 * (-error)
cap = min of the c values, at least cold, round1
if there is a cap: result = min(result, cap)
```

Computed examples (radiators, cold 28.2, default headroom 5.0, main PID output 30, no zone above its target):

- N = 2, zone outputs 30, 40: idx 1, chosen min(40, 40 + 5) = 40, result max(30, 40) = 40.
- N = 4, zone outputs 30, 32, 34, 48: idx 3, chosen min(48, 48 + 5) = 48, result 48. The highest zone is served (before: 34 + 5 = 39).
- One zone with output 25 (not above 28.2): not counted, result 30.
- One zone with output 40, 1.0 °C over its target: zones 40, cap 28.2 + 10 - 4 = 34.2, result min(40, 34.2) = 34.2.
- Same zone 4.0 °C over: cap 22.2, floored to 28.2, result 28.2.

The result is then reduced by the solar offset when solar gain is active, and clamped between 10 °C and the effective maximum setpoint (SATcontrol.ino:5578-5585).

System maximum for the zone output clamp: 45 °C underfloor, 62 °C otherwise, 40 °C for a heat-pump source (SATcontrol.ino:228-234). Effective maximum for the requested setpoint: the boiler's `MaxTSet` (75 °C if below 30), limited by the system maximum, a hard ceiling (50 underfloor, 80 otherwise) and the global maximum `fMaxSetpoint` (default 65) (SATcontrol.ino:240-254, SATcontrol.ino:57, SATcontrol.ino:62-64, SATtypes.h:514).

Follow-thermostat decision (SATcontrol.ino:1064-1079): see behaviour step 9. Window timer (SATcontrol.ino:1131-1132):

```
switch when (millis() - iWindowOpenSinceMs) / 1000 >= iWindowMinOpenSec
```

<a id="s6-constants-and-defaults-table-name-value-unit-cit"></a>

### Constants and defaults (table: name | value | unit | citation)

| Name | Value | Unit | Citation |
|---|---|---|---|
| `SAT_MIN_SETPOINT` | 10.0 | °C | SATcontrol.ino:54 |
| `SAT_MAX_AREAS` | 4 | areas | SATcontrol.ino:86 |
| `SAT_AREA_STALE_MS` | 300000 (5 min) | ms | SATcontrol.ino:87 |
| `SAT_MAX_ZONES` | 4 | zones | SATcontrol.ino:1729 |
| Zone aggregation percentile | 0.75, index `int(N * 0.75)` clamped to N - 1 | fraction | SATcontrol.ino:2360-2361 |
| Zone overshoot margin / cooling headroom / cooling slope | 0.3 / 10.0 / 4.0 | °C, °C, °C per °C | SATcontrol.ino:2335-2337 |
| `fZoneAggregationHeadroom` default / range (most the zones may ask above their P75 output) | 5.0 / 0 to 15 | °C | SATtypes.h:552, settingStuff.ino:1110 |
| `iZoneCount` default / range | 1 / 1 to 4 | zones | SATtypes.h:531, settingStuff.ino:1112 |
| `iZoneTimeoutS` default / range | 300 / 30 to 3600 | s | SATtypes.h:532, settingStuff.ino:1113 |
| Zone room temperature accepted | -10 to 50 | °C | SATcontrol.ino:1760 |
| Zone setpoint accepted | 5 to 30 | °C | SATcontrol.ino:1777 |
| Zone integral step | 60 | s | SATcontrol.ino:1846 |
| Zone integral absolute cap | 20 | °C | SATcontrol.ino:1849-1850 |
| Zone Kp divisor, underfloor / other | 4 / 3 | | SATcontrol.ino:1835 |
| Zone Ki aggression | 8400 | | SATcontrol.ino:1837 |
| Area temperature accepted | above -50, below 100 | °C | SATcontrol.ino:1668, SATcontrol.ino:1684 |
| `fAreaWeight` default / range | 1.0 / 0 to 10 | | SATtypes.h:509, settingStuff.ino:1094-1097 |
| `iMultiAreaCount` default / range | 0 / 0 to 4 | areas | SATtypes.h:508, settingStuff.ino:1093 |
| `bMultiArea` default | false | | SATtypes.h:507 |
| `fTargetTemp` default / range | 20.0 / 5 to 30 | °C | SATtypes.h:441, settingStuff.ino:997 |
| Preset match tolerance | 0.05 | °C | SATcontrol.ino:1922 |
| Preset `comfort` default / range | 21.0 / 15 to 28 | °C | SATtypes.h:446, settingStuff.ino:1005 |
| Preset `eco` default / range | 18.0 / 10 to 22 | °C | SATtypes.h:447, settingStuff.ino:1006 |
| Preset `away` default / range | 15.0 / 5 to 18 | °C | SATtypes.h:448, settingStuff.ino:1007 |
| Preset `sleep` default / range | 16.0 / 5 to 25 | °C | SATtypes.h:449, settingStuff.ino:1008 |
| Preset `activity` default / range | 10.0 / 5 to 20 | °C | SATtypes.h:450, settingStuff.ino:1009 |
| Preset `home` default / range | 18.0 / 10 to 25 | °C | SATtypes.h:451, settingStuff.ino:1010 |
| `bWindowDetection` default | false | | SATtypes.h:463 |
| `iWindowMinOpenSec` default / range | 60 / 10 to 600 | s | SATtypes.h:464, settingStuff.ino:1036 |
| `bPushSetpoint` default | false | | SATtypes.h:460 |
| `bFollowThermostat` default | false; equals `bPushSetpoint` when the key is absent from the file | | SATtypes.h:461, settingStuff.ino:606-611 |
| Follow-thermostat accepted range | 5 to 30 | °C | SATcontrol.ino:1068 |
| Follow-thermostat minimum change | 0.095 | °C | SATcontrol.ino:1070 |
| Follow-thermostat debounce match | 0.051, two consecutive frames | °C | SATcontrol.ino:1071 |
| Follow-thermostat echo window | 0.25 | °C | SATcontrol.ino:1076-1077 |
| `SAT_CMD_REFRESH_MS` (unchanged CS/MM/CH re-send; not TC= since TASK-1272) | 300000 (5 min) | ms | SATcontrol.ino:427 |
| `bPresetSync` default | false | | SATtypes.h:485 |
| `sPresetSyncTopic` | empty, max 64 characters | | SATtypes.h:486 |
| `iHeatingMode` default / range | 0 (comfort) / 0 to 1, no effect on control | | SATtypes.h:524, settingStuff.ino:1104 |
| `fValveOffset` default / range | 0.0 / -1 to 1, no effect on control | °C | SATtypes.h:526, settingStuff.ino:1106 |
| `bValvesOpen` default | true | | SATtypes.h:243 |
| `iSensorMaxAgeS` default / range | 21600 / 60 to 86400 | s | SATtypes.h:515, settingStuff.ino:1101 |
| `fTargetTempStep` default / range | 0.5 / 0.1 to 1.0 | °C | SATtypes.h:468, settingStuff.ino:1037 |
| `iControlInterval` default / range | 30 / 10 to 300 | s | SATtypes.h:444, settingStuff.ino:1001 |
| Heating curve base offset, underfloor / other | 20.0 / 27.2 | °C | SATcontrol.ino:49-50 |
| Heating curve coefficient default | 1.5 | | SATtypes.h:442 |
| HA climate entity range and step | 12 to 30, step 0.5 | °C | MQTTHaDiscovery.cpp:3065 |

<a id="s6-state-kept-between-passes"></a>

### State kept between passes

| State | Meaning | Persisted | Citation |
|---|---|---|---|
| `settings.sat.fTargetTemp` | SAT target room temperature | settings file (key `SATtargettemp`); marked for saving only when set through `updateSetting()` | SATtypes.h:441, settingStuff.ino:319, settingStuff.ino:1283-1287 |
| `state.sat.eActivePreset` | current preset | no (RAM) | SATtypes.h:205 |
| `state.sat.fPreCustomTemp` | target before the first preset left `none`; 0 = none | no | SATtypes.h:250, SATcontrol.ino:993-995 |
| `state.sat.bWindowOpen`, `iWindowOpenSinceMs` | window state and open time | no | SATtypes.h:245-246 |
| `state.sat.fPreWindowTarget`, `iPreWindowPreset` | target and preset before the window switch | no | SATtypes.h:247-248 |
| `state.sat.fPreActivityTemp` | copy of the pre-window target, for publishing | no | SATtypes.h:251, SATcontrol.ino:1135 |
| `state.sat.fAreaTemp[4]`, `bAreaValid[4]`, `iAreaLastMs[4]` | area values | no | SATtypes.h:383-385 |
| `satZones[4]` (room, setpoint, output, integral, previous error, last update, valid flags, OFF flag) | zone records and zone PID memory | no | SATcontrol.ino:1726, SATtypes.h:130-139 |
| `state.sat.bValvesOpen` | external valves flag | no | SATtypes.h:243 |
| `state.sat.fLastSentTC` and the follow-thermostat detector fields | last `TC=`, last and candidate thermostat setpoint | no | SATtypes.h:399-403 |
| Preset temperatures, window settings, zone and area settings, sync flags | configuration | settings file | settingStuff.ino:324-329, settingStuff.ino:338-343, settingStuff.ino:359-360, settingStuff.ino:375-380, settingStuff.ino:395-397 |

<a id="s6-outputs-to-the-boiler-to-entities"></a>

### Outputs (to the boiler, to entities)

To the boiler, through the command queue, on each control tick that reaches the end (SATcontrol.ino:4977-4994):

- `CS=<setpoint>`, `MM=<modulation>`, `CH=0/1` with the requested setpoint from step 4 after PWM or continuous control (SATcontrol.ino:4980-4983). These belong to other sections; this section only shapes the requested setpoint.
- On `valves_open == false`: `CS=10`, `MM=<max modulation>`, `CH=0`, and the PID is not updated (SATcontrol.ino:4778-4790).
- Window open has no command of its own; it only changes the target (SATcontrol.ino:1134-1139).

Thermostat setpoint push (only when `bPushSetpoint` is true and an OT command interface exists) (SATcontrol.ino:4991-4994, SATcontrol.ino:439-453):

- `TC=<fTargetTemp>` with one decimal, sent when the target rounded to 0.1 changed (TASK-1272, Sergeant D entries 14 and 16; as thermo-nova, `climate.py:641-657`). There is no periodic refresh. A target adopted from the thermostat (follow thermostat) is not sent back (`climate.py:765`, `cascade=False`). There is no re-push, no MsgID 9 check and no 5-minute refresh (maintainer decision 2026-10-24, option D: exactly thermo-nova). A thermostat that refuses the remote override keeps its own setpoint and SAT does not keep asking. A cache reset (OT bus back online) and switching push off and on again also send the current target once. `TC=` push works only on thermostats that accept a remote override; SAT pushes on a target change. An adopted thermostat value also becomes `fLastSentTC`, so the echo guard compares against it and a later dial-back is adopted too. Code: `SATcontrol.ino:529-559` (`satTCNoteAdopted`, `satEnqueueIfChangedTC`, `satTCPushOff`), call site `SATcontrol.ino:5172-5176`.
- It is skipped on every tick that returns before line 4977: DHW active, boiler fault, thermal safe fallback, summer mode, valves closed (SATcontrol.ino:4865-4882 [lines refreshed 2026-10-24, TASK-1351], SATcontrol.ino:4716-4719, SATcontrol.ino:4730-4746, SATcontrol.ino:4766-4790).
- It is sent on the same path for PIC and OT-Direct boards; the code does not branch on the board here (SATcontrol.ino:4978-4994).

To MQTT (under the device's own namespace; `sendMQTTData` prefixes every topic with `MQTTPubNamespace`, MQTTstuff.ino:1427-1429):

- `sat/preset` with the preset name, retained (SATcontrol.ino:2630, SATcontrol.ino:951-962). The `publishIfChanged*` helpers publish through `sendMQTTData` with the passed retain flag, on a value change or a heartbeat (SATmqttPublish.cpp:47-62, SATmqttPublish.cpp:93-104, SATmqttPublish.cpp:129-146, SATmqttPublish.cpp:210-224).
- `sat/target`, retained (SATcontrol.ino:2734), plus an immediate publish when preset `none` restores a target (SATcontrol.ino:1013-1018).
- `sat/pre_custom_temperature` and `sat/pre_activity_temperature`, only while above 0 (SATcontrol.ino:2851-2857).
- `sat/window_open` and `sat/valves_open` (SATcontrol.ino:2849, SATcontrol.ino:2859).
- Per zone, retained: `sat/zone/<n>/active`, `sat/zone/<n>/output`, `sat/zone/<n>/error` (unrounded `setpoint - room`) (SATcontrol.ino:1871-1901).
- Preset sync: the preset name, retained, to `<namespace>/<sPresetSyncTopic>`, when `bPresetSync` is on, the topic is not empty and MQTT is connected (SATcontrol.ino:965-974, MQTTstuff.ino:1427-1429).
- The SAT status JSON includes `active_preset` (number), `window_open`, `window_detection`, `valves_open`, `push_setpoint`, `follow_thermostat`, `preset_sync` (SATcontrol.ino:2392, SATcontrol.ino:2426-2429, SATcontrol.ino:2433, SATcontrol.ino:2457).

Home Assistant discovery: one climate entity "Thermostat" whose temperature command is sent as `TT=<value>` to the gateway command topic, whose shown setpoint is `TrSet` and whose current temperature is `Tr` (MQTTHaDiscovery.cpp:3035, MQTTHaDiscovery.cpp:3045, MQTTHaDiscovery.cpp:3054-3057). A grep of MQTTHaDiscovery.cpp for `preset` found only the preset sync switch (MQTTHaDiscovery.cpp:3420-3421), so no preset selector is announced. A `sat/target` discovery entry exists (MQTTHaDiscovery.cpp:294).

<a id="s6-edge-cases"></a>

### Edge cases

1. Zones without data change nothing: the zone block runs at every zone count (1 to 4), but a zone that was never pushed, is OFF, stale or out of range does not count, and with no counted zone the zones output is 10 °C, which leaves the main PID output alone (SATcontrol.ino:5540-5561, SATcontrol.ino:2231, SATcontrol.ino:2352). Zone MQTT diagnostics are still published only at `iZoneCount` > 1 (SATcontrol.ino:5561).
2. One counted zone: it can raise the main PID output, never lower it (`max()`, SATcontrol.ino:2367). It does not replace it.
3. With up to 4 counted zones the percentile index is always the highest output, so the highest zone is served and the headroom never binds (computed example in Formulas). The headroom would only bind with 5 or more zones.
4. A warm zone caps the result instead of being subtracted: more than 0.3 °C over its target gives `cold + 10 - 4 * degrees over`, floored at the cold setpoint, and the result is the minimum of it and the requested setpoint. The cap can lower the setpoint below the main PID output (SATcontrol.ino:2342-2350, SATcontrol.ino:2368-2372). Exactly 0.3 °C over gives no cap.
5. A zone counts only when it is usable and its output is above the cold setpoint (28.2 °C radiators, 21.0 °C underfloor), not above 10 °C (SATcontrol.ino:2343). The firmware has no per-zone valve position, so the valve test of thermo-nova area.py:166-175 is absent.
6. Zone timeout uses one timestamp for both values (SATcontrol.ino:1764, SATcontrol.ino:1781, SATcontrol.ino:1818). A zone whose setpoint keeps arriving but whose room temperature stopped stays active on its last room temperature (follows from the code, not run).
7. The zone integral adds a 60 s step on every control tick (SATcontrol.ino:1846). With the default 30 s tick it therefore grows twice as fast in real time as a step-per-60 s integral would (follows from the code, not run).
8. Zone gains always use the automatic formula, also when the main PID is set to manual gains (SATcontrol.ino:1834-1837, SATpid.ino:122-128).
9. Multi-area overrides the external indoor temperature and `Tr`, but not BLE (SATcontrol.ino:1208-1231). When every area is stale or has weight 0, the next source is used (SATcontrol.ino:1717, SATcontrol.ino:1219-1221).
10. Preset auto-selection by temperature: the order is comfort, eco, away, sleep, activity, home (SATcontrol.ino:1913-1920). With the defaults eco and home are both 18 °C, so setting 18 °C selects `eco` (SATtypes.h:447, SATtypes.h:451).
11. A manual target that matches no preset leaves the old preset name active and keeps `fPreCustomTemp` (SATcontrol.ino:1928-1931). A later preset `none` then restores the value saved before the first preset, not the manual value (SATcontrol.ino:1004-1005; follows from the code, not run).
12. Re-selecting the active preset is not ignored: it writes the preset temperature into the target again (SATcontrol.ino:990-996).
13. The "integral reset" on a preset change clears only the published copy `fPidI` (SATcontrol.ino:998, SATcontrol.ino:1006, SATpid.ino:298-303). The window switch and restore clear the real integral (SATcontrol.ino:1119, SATcontrol.ino:1139, SATpid.ino:86-91). A manual target and a follow-thermostat adoption reset nothing (SATcontrol.ino:1928-1931, SATcontrol.ino:1034-1045).
14. Preset, preset-none restore, window switch and window restore write `settings.sat.fTargetTemp` directly (SATcontrol.ino:996, SATcontrol.ino:1005, SATcontrol.ino:1115, SATcontrol.ino:1138), not through `updateSetting()`, which is where `settingsDirty` is set (settingStuff.ino:1283-1287). `flushSettings()` writes only when that flag is set (settingStuff.ino:60-66). The new target reaches flash only if some other path writes the whole settings file afterwards. A reboot before that brings back the last saved target (follows from the code, not run).
15. Window open while the preset is already `activity`: the switch still happens and saves `activity` as the preset to restore (SATcontrol.ino:1128-1138).
16. A preset or target change during the window-open period after the switch is overwritten by the restore when the window closes (SATcontrol.ino:1114-1117; follows from the code, not run).
17. Detection switched off while the window is open: the timer still fires (it does not check the setting) (SATcontrol.ino:1126-1132), but a close is ignored (SATcontrol.ino:1100), so SAT stays at the activity temperature until detection is switched on again and a close arrives, or the preset or target is changed by hand (follows from the code, not run).
18. Any payload other than `open`, `1` or `ON` closes the window, for example `true` or `opened` (MQTTstuff.ino:628-630).
19. The window timer is checked only on control ticks that get past the DHW, room-temperature, boiler-fault and thermal-safe returns (SATcontrol.ino:4635-4752, SATcontrol.ino:4755). During a long DHW draw the switch to activity waits (follows from the code, not run).
20. Several windows: the firmware has one window state. If several senders push to the same command, the last message wins (SATcontrol.ino:1098-1123, MQTTstuff.ino:627-632).
21. Follow thermostat with push-setpoint on: a thermostat change made before the first `TC=` since boot is only recorded (SATcontrol.ino:1076).
22. Follow thermostat clears an active preset and the saved pre-custom temperature without restoring it (SATcontrol.ino:1038-1040).
23. Preset sync goes to a topic under the gateway's own namespace, not to an arbitrary absolute topic (SATcontrol.ino:970, MQTTstuff.ino:1427-1429). Reaching another device's topic would need that device to listen there; this is not shown in the code.
24. `valves_open` false holds the boiler off (`CS=10`, `CH=0`) until a `true` arrives; there is no timeout on this flag (SATcontrol.ino:4778-4790, MQTTstuff.ino:634-639, SATtypes.h:243).

<a id="s6-not-found-in-the-code-unclear"></a>

### Not found in the code / unclear

1. The zone OFF flag `bOff` is read (SATcontrol.ino:1814, SATcontrol.ino:1885, SATcontrol.ino:3945) but an unfiltered grep of `src/OTGW-firmware` for `bOff` found no line that sets it to true. The MQTT zone parser accepts only `room_temp` and `setpoint` (MQTTstuff.ino:901-907). An OFF zone is therefore not reachable from outside.
2. Heating mode ECO or COMFORT has no effect on control (see behaviour step 4). Whether that is intended is not stated in the code.
3. `fValveOffset` has no effect on control (see behaviour step 4).
4. There is no per-room "home" temperature table, no pushing of targets or HVAC modes to other climates, and no per-room preset sync. A grep of SATcontrol.ino for `preset|window|area|room|sync` found no such code; the only sync is the single preset sync topic (SATcontrol.ino:964-974).
5. No `cascade` concept: SAT never sends its target to radiators. The only outward target push is `TC=` (SATcontrol.ino:4991-4994).
6. What the PIC or OT-Direct does with `TC=` and with the Home Assistant climate entity's `TT=` is outside the code read here. Whether a `TT=` change ever reaches SAT's target is not shown: the follow path reads only thermostat (T) frames of MsgID 16 (OTGW-Core.ino:5211-5217).
7. Units of `fValveOffset` are not stated beyond the range -1 to 1 (SATtypes.h:526).
8. Zone simulation code exists under simulation mode (SATcontrol.ino:3931-3953) and was not analysed for this section.
9. PID internals, PWM and continuous control, solar gain, comfort offset, PV boost and the cold-setpoint cutoff belong to other sections and are not specified here.

---

<a id="s7"></a>

## Solar gain, summer simmer, temperature history and health sensors (OTGW firmware)

Source: OTGW-firmware, branch `dev`, commit `51df6b486`, directory `src/OTGW-firmware`. Every statement below cites the firmware lines it was read from. This section describes what the firmware actually does, not what thermo-nova does. Headings and topic order follow `a7-features-thermonova.md` so the two can be compared line by line.

Important naming difference before reading on. In thermo-nova, "summer simmer" is a heat-and-humidity comfort index. In the firmware the setting `bSummerSimmer` is a different feature: an outdoor-temperature "summer mode" that switches heating off ("auto-disable heating when outdoor temp stays warm", SATtypes.h:498-501, SATcontrol.ino:4302-4358). The comfort index itself exists in the firmware too (SATcontrol.ino:2574-2595), but it is used through the separate setting `bThermalComfort` (SATtypes.h:522).

This section covers these firmware features:

1. Solar gain compensation (changes boiler control directly).
2. The Summer Simmer index with the "thermal comfort" option (changes the room temperature the PID sees, only when enabled), plus the firmware-only outdoor-temperature summer mode (`bSummerSimmer`), which also changes control. The humidity comfort offset (`bComfortAdjust`) was removed in TASK-1282.
3. Temperature error history: two separate recommendation mechanisms (advice only, no control effect).
4. The "Error Value" output (display only).
5. Health and synchronisation outputs (display only), and the window detection that thermo-nova lists next to them.

<a id="s7-purpose"></a>

### Purpose

**Solar gain compensation.** Detects that the room is warming while the boiler modulates low, and then lowers the requested setpoint by a fixed offset before the clamp, the mode step and the cold cutoff, and, with `bSolarFreezeIntegral` on, skips the PID integral update (corrected 2026-10-24, TASK-1351); TASK-1323. The code comment names it "Solar Gain Compensation (Task #23)" (SATcontrol.ino:4206-4207). It is off by default (SATtypes.h:494).

**Summer Simmer index and thermal comfort.** The index function carries the comment "Matches SAT Python summer_simmer.py formula exactly" (SATcontrol.ino:2576-2577). It is published as MQTT topics whenever a fresh indoor humidity value is present (`satHumidityFresh()`, TASK-1441). When `bThermalComfort` is on, the index replaces the room temperature used by the controller (SATcontrol.ino:1258-1270). That option is off by default (SATtypes.h:522).

**Summer mode (`bSummerSimmer`, firmware only).** When the outside temperature has stayed at or above a threshold for a number of hours, SAT stops heating: it sends control setpoint 10 °C and CH off and skips the rest of the control tick (SATcontrol.ino:4764-4776). Off by default (SATtypes.h:499).

**Humidity comfort offset (`bComfortAdjust`).** Removed in TASK-1282 (Sergeant D entry 114). The feature, its settings and its state field are gone.

**Temperature history.** Two separate mechanisms, neither of which is the thermo-nova rolling 4 h / 24 h error store:

- an hourly error sample ring with mean and standard deviation, called "Heating Curve Recommendation (Task #11)" (SATcontrol.ino:3485-3545);
- a daily-median recommendation over days, "Daily median recommendation (Task #228)" (SATcontrol.ino:4798-4799, SATcycles.ino:1199-1289).

The firmware also has "Rolling 4-hour window statistics" and a "Rolling 24-hour DAILY window" (SATcycles.ino:303-308, SATcycles.ino:399-475). These are statistics of heating cycles (on/off times, overshoot and underheat fractions), not of the room error (SATcycles.ino:325-340, SATcycles.ino:445-454). They belong to the cycle section of this specification.

**Health sensors.** MQTT topics with payload "ON"/"OFF" for setpoint sync, modulation sync, CH sync, pressure health, device health and cycle health (SATcontrol.ino:3083-3118, SATcontrol.ino:2886, SATcontrol.ino:3000-3009). A tree-wide search of `src/OTGW-firmware` for the strings `pressure_health`, `device_health`, `cycle_health`, `setpoint_sync`, `modulation_sync` and `ch_sync` found them only in SATcontrol.ino and SATmqttPublish.h, plus `device_health` and `cycle_health` in restAPI.ino (the generated `OTGW-firmware.ino.cpp` build copy aside), so no Home Assistant discovery entry for them was found (search result, see "Not found" item 1). Window detection is a single flag set by an MQTT or REST command, not a group of sensors (MQTTstuff.ino:627-632, restAPI.ino:1383-1397, SATcontrol.ino:1098-1123).

<a id="s7-inputs"></a>

### Inputs

**Solar gain controller** (`satUpdateSolarGain`, SATcontrol.ino:4215-4299) reads its inputs directly, once per control tick:

| Signal | Source | Citation |
|---|---|---|
| Room temperature | `satGetRoomTemp()`, the unfiltered room value (the Summer Simmer index when thermal comfort is active, see below) | SATcontrol.ino:4225, SATcontrol.ino:1192-1280 |
| Time | `millis()` of the control tick | SATcontrol.ino:4226 |
| Valves open | `state.sat.bValvesOpen`, default true, set by MQTT command `valves_open` | SATcontrol.ino:4270, SATtypes.h:243, MQTTstuff.ino:634-639, MQTTstuff.ino:671 |
| Sun elevation | `state.sat.fSunElevation` with validity flag and timestamp, set only by MQTT `set/<nodeId>/sat/sun_elevation` | SATcontrol.ino:4249-4261, SATcontrol.ino:1650-1662, MQTTstuff.ino:618, MQTTstuff.ino:669, MQTTstuff.ino:868 |
| Relative modulation | `OTcurrentSystemState.RelModLevel`, the boiler-reported value, default 0.0 before any report | SATcontrol.ino:4267, OTGW-Core.h:72 |
| Flame | not read by the solar gain code | SATcontrol.ino:4264-4298 |
| HVAC mode | not read by the solar gain code; it runs on every control tick that reaches it | SATcontrol.ino:4761-4762 |

The weather module does not compute a sun elevation: a search of SATweather.ino for `sun` and `elevation` found no match. The sun elevation value is accepted in the range -90 to 90 degrees (SATcontrol.ino:1656).

Configuration: `bSolarGainEnable`, `fSolarMinRiseRate`, `fSolarSetpointOffset`, `fSolarMinElevation`, `bSolarFreezeIntegral` (SATtypes.h:493-497, SATtypes.h:527). They are settable through the settings store (settingStuff.ino:1082-1085, settingStuff.ino:1109) and MQTT commands such as `solar_gain`, `solar_freeze_integral`, `solar_min_elevation` (MQTTstuff.ino:726, MQTTstuff.ino:734, MQTTstuff.ino:737). A Home Assistant switch is announced for the enable (MQTTHaDiscovery.cpp:3364-3368).

**Summer Simmer index** inputs: a temperature in °C and a humidity value, which the function treats as percent (it rejects values outside 0 to 100) (SATcontrol.ino:2577-2582). The humidity is `state.sat.fHumidity`, which comes from:

- MQTT `set/<nodeId>/sat/humidity` (MQTTstuff.ino:617, MQTTstuff.ino:667, MQTTstuff.ino:868) or the REST SAT sub-route `humidity` (restAPI.ino:1547-1560), both through `satHandleHumidity`, which accepts 0 to 100 and stamps the time (SATcontrol.ino:1635-1647);
- the BLE room sensor humidity, through `satNoteBleHumidity` (SATcontrol.ino), once per new reading (SATble.ino `iHumidityMs` is stamped only when an advert carried a humidity). The most recent valid reading from either source wins.

There is no weather fallback since TASK-1441 (Sergeant D entry 117, Q57 A): the Open-Meteo `relative_humidity_2m` is outdoor humidity and is no longer copied into `fHumidity`. Without a fresh indoor humidity Thermal comfort uses the raw room temperature (thermo-nova climate.py:282-287).

The BLE sensor humidity is stored in `state.sat.fBleHumidity` (SATble.ino) and published as `sat/ble_humidity`. Since TASK-1441 a new humidity reading of the selected BLE sensor (the same slot that supplies the BLE room temperature) also feeds `fHumidity`, `bHumidityValid` and `iHumidityLastMs`, so it feeds the index and thermal comfort. One validity rule, `satHumidityFresh()`: `bHumidityValid` and the reading is no older than `iHumidityTimeoutS`. It gates the thermal comfort input and the published index (entry 117, TASK-1441).

**Summer mode** inputs: `satGetOutsideTemp()` (SATcontrol.ino:4326), which returns, in order, the simulated value, a non-stale MQTT external outdoor temperature, the weather temperature when the OT outside temperature is exactly 0.0, else the OT outside temperature (SATcontrol.ino:1283-1302). The OT outside temperature defaults to 0.0 (OTGW-Core.h:82). Settings: `fSummerThreshold`, `iSummerMinHours` (SATtypes.h:500-501).

**Comfort offset.** Removed in TASK-1282 (Sergeant D entry 114).

**Temperature history** input: `state.sat.fError`, the error of the most recent primary PID update (SATcontrol.ino:3507, SATcycles.ino:1205). It is computed as `target - room` rounded to 3 decimals (SATpid.ino:63-66, SATpid.ino:253) and stored at SATpid.ino:305. There is no median over areas: in single-zone mode the PID receives the single value from `satGetRoomTemp()` after a filter (SATcontrol.ino:4656, SATcontrol.ino:4694-4713, SATcontrol.ino:4839). Multi-area mode instead makes `satGetRoomTemp()` return a weighted area temperature (SATcontrol.ino:1217-1222).

**Pressure health** inputs: `OTcurrentSystemState.CHPressure` in bar (default 0.0) (SATcontrol.ino:3340, OTGW-Core.h:73), `state.sat.bActive` (SATcontrol.ino:3344), settings `fMinPressure`, `fMaxPressure`, `fMaxPressureDrop` (SATtypes.h:469-471).

**Other health outputs** inputs: `state.sat.fFinalSetpoint` vs `OTcurrentSystemState.TSet` (SATcontrol.ino:3086-3088, SATcontrol.ino:3593-3594); setting `iMaxRelModulation` vs `OTcurrentSystemState.MaxRelModLevelSetting` (SATcontrol.ino:3098-3100); `state.sat.bActive` vs boiler status bit mask 0x02, which the code names `boilerActive` (SATcontrol.ino:3110-3112); `state.sat.eBoilerStatus` (SATcontrol.ino:3000-3002); `state.sat.eLastCycleClass` (SATcontrol.ino:3004-3008).

<a id="s7-behaviour-step-by-step-in-time-order"></a>

### Behaviour (step by step, in time order)

The firmware has one control loop, not a separate 60 s PID tick and 5 s heating loop. `satControlLoop()` is called from the main loop (OTGW-firmware.ino:1157) and runs its main body only when the timer `timerSATControl` is due (SATcontrol.ino:4613-4614). That timer uses `iControlInterval` (SATcontrol.ino:476), default 30 s (SATtypes.h:444), settable from 10 to 300 s (settingStuff.ino:1001-1002). The thermo-nova sub-headings "Per PID tick" and "Per heating control loop" are therefore both covered by "Per control tick" below.

#### Start-up

1. `initSAT()` resets the PID, restores PID, energy and the daily-median recommendation ring from LittleFS, then calls `satCycleInit()` (SATcontrol.ino:3681-3688). `satCycleInit()` empties the intra-day sample buffer of the daily-median recommendation, keeps the restored daily ring and sets the recommendation text to "insufficient" (SATcycles.ino:254-257).
2. Solar gain state is in static RAM variables that start at zero (SATcontrol.ino:4209-4213). Its state fields start inactive (SATtypes.h:368-373). No save or load code for them was found; `satPersistAllToFs()` saves only PID, energy, estimated energy and the daily-median ring (SATcontrol.ino:2128-2136).
3. Summer mode starts inactive with 0 hours (SATtypes.h:375-376). Humidity starts invalid (SATtypes.h:378-380). Sun elevation starts invalid (SATtypes.h:371-373).
4. The hourly error ring (`_cr_*`) starts empty (SATcontrol.ino:3488-3492). The recommendation enum starts at "insufficient" (SATtypes.h:268).
5. Pressure state starts empty: ring, timers and the "last active" flag are static zero values (SATcontrol.ino:3326-3336), and the smoothed pressure starts at 0.0 (SATtypes.h:253). Nothing is restored.
6. On the first `satControlLoop()` calls, before the timer is due, the loop already runs other per-call work (cycle sampling, the 60 s stats timer, the 3 s PWM refresh) (SATcontrol.ino:4591-4611). The features in this section run only inside the timed body (SATcontrol.ino:4614, SATcontrol.ino:4755-4823).

#### Per PID tick (firmware: per control tick, default every 30 s, `satControlLoopRun`)

Order inside one tick (SATcontrol.ino:4614-5018):

1. Early exits that skip everything in this section: SAT disabled or flashing (SATcontrol.ino:4568-4573), safety tripped (SATcontrol.ino:4581), DHW active (SATcontrol.ino:4865-4882 [lines refreshed 2026-10-24, TASK-1351]), boiler fault bit set (SATcontrol.ino:4716-4719), thermal-estimation safe setpoint (SATcontrol.ino:4730-4746).
2. Window timer check (SATcontrol.ino:4755). See "Window" below.
3. Pressure update, then pressure status text (SATcontrol.ino:4758-4759).
4. Solar gain update, `satUpdateSolarGain()` (SATcontrol.ino:4762, SATcontrol.ino:4215-4299):
   1. If the feature is disabled: active flag false, rise rate shown as 0, condition timer and "was active" cleared, return. The stored previous sample and the rise-rate average are not cleared (SATcontrol.ino:4217-4223).
   2. Read the unfiltered room temperature. If it is NaN, keep the previous rise rate and return (SATcontrol.ino:4225-4232).
   3. Rise rate: on the very first sample only store temperature and time. Afterwards, only when more than 30 s have passed since the stored sample, compute the raw rate in °C/h, blend it into an average (new = 0.3 x raw + 0.7 x old) and store the new sample (SATcontrol.ino:4234-4246). With the default 30 s tick, whether every tick or every second tick passes the "more than 30 s" test depends on exact tick timing (derived from SATcontrol.ino:4235 and SATcontrol.ino:476; not determinable from the code alone).
   4. Sun gate (TASK-1408, thermo-nova solar_gain.py:56-58): if a sun elevation was received but is older than 1 h, mark it invalid. Without a valid sun elevation (never received, or stale), or with an elevation below `fSolarMinElevation`, set inactive, clear "was active" and the condition timer, and return. The rise-rate average above keeps updating and stays published. There is no rise-rate-only fallback.
   5. Detection terms: rising fast = average rise rate > `fSolarMinRiseRate`; low modulation = boiler relative modulation < 20 %; valves open = `bValvesOpen` (SATcontrol.ino:4267-4270).
   6. If not active: all three terms must hold continuously for more than 600 s (10 min). The first qualifying tick starts the timer; any tick where one term fails resets it. After more than 600 s the feature becomes active (SATcontrol.ino:4272-4284).
   7. If active: it stays active while rising fast. It clears only after the rise rate has been not-fast for more than 600 s continuously (SATcontrol.ino:4285-4297). Modulation and valves are not checked while active (SATcontrol.ino:4285-4298). The sun gate in step 4 can still clear it at once.
5. Summer mode update, `satUpdateSummerSimmer()` (SATcontrol.ino:4765, SATcontrol.ino:4307-4358):
   1. If `bSummerSimmer` is off: inactive, hours 0, check timer cleared, return (SATcontrol.ino:4309-4314).
   2. The first call after enabling only stores the time (SATcontrol.ino:4317-4320). After that the logic runs at most once per 5 min (SATcontrol.ino:4305, SATcontrol.ino:4321).
   3. Not active: if outside >= `fSummerThreshold`, add the elapsed hours to the counter; activate when the counter reaches `iSummerMinHours`. If outside is below the threshold, reset the counter to 0 (SATcontrol.ino:4329-4341).
   4. Active: if outside < threshold - 2 °C, subtract the elapsed hours; deactivate when the counter reaches 0. Otherwise refill the counter to `iSummerMinHours` (SATcontrol.ino:4342-4357).
   5. Back in the tick: while summer mode is active and enabled, the tick sets the final setpoint to 10 °C, queues CS=10, MM=`iMaxRelModulation` and CH=0 (write-on-change), publishes, and returns (SATcontrol.ino:4766-4776). Everything below is skipped, including the PID and both history mechanisms.
6. Valves closed: same off-commands and return, with the comment "Don't update PID integral or record error statistics" (SATcontrol.ino:4778-4790).
7. Hourly error sample, `satUpdateCurveRecommendation()` (SATcontrol.ino:4796, SATcontrol.ino:3495-3545). It runs before this tick's PID update (SATcontrol.ino:4839), so it samples the error of the previous PID update (derived from the call order). Details under "Formulas".
8. Daily-median intra-day sample, `satHCRAddSample()` (SATcontrol.ino:4799, SATcycles.ino:1201-1243). Called every control tick (not every 15 min as its comment says, SATcycles.ino:1199), also before the PID update. Details under "Formulas".
9. Setpoint sync check `satCheckSetpointSync()` (SATcontrol.ino:4808). It runs before this tick's new final setpoint is computed, so it compares the previous tick's final setpoint with the boiler's reported TSet (derived from SATcontrol.ino:4808, SATcontrol.ino:4946, SATcontrol.ino:3593-3594).
10. Boiler status update (feeds device health) (SATcontrol.ino:4814).
11. Humidity weather fallback: removed in TASK-1441 (Sergeant D entry 117). The step no longer exists.
12. PV boost. The effective target is `fTargetTemp`, plus a PV boost when active (SATcontrol.ino:4831-4833). This effective target drives both the heating curve and the PID (SATcontrol.ino:4836-4839). The comfort offset step (`satUpdateComfort()`) was removed in TASK-1282.
13. PID update with the filtered room temperature (SATcontrol.ino:4839). Integral freeze (corrected 2026-10-24, TASK-1351); TASK-1323: at the start of the integral update, before the deadband test, `if (state.sat.bSolarGainActive && settings.sat.bSolarFreezeIntegral) return;` (SATpid.ino:157). Order inside `_pidUpdateIntegral`:
    - if solar gain is active and the freeze setting is on: return, nothing changes (SATpid.ino:157);
    - if |error| > deadband: integral set to 0 and return (SATpid.ino:160-163);
    - if Ki is near zero: return (SATpid.ino:166).
    So during a freeze the reset to 0 outside the deadband is skipped too, as thermo-nova (pid.py:167-168, pid.py:187-189). With the setting off, solar gain does not touch the integral. The derivative is still updated (SATpid.ino:294). The multi-zone PID has no solar freeze (SATcontrol.ino:1842-1853).
14. PWM or continuous mode produces `finalSetpoint` from `pidOutput`, which already carries the solar offset (SATcontrol.ino:5162-5183); it is stored as `_sat_lastControlCS` (SATcontrol.ino:5182) (corrected 2026-10-24, TASK-1351).
15. Solar offset: if solar gain is active and enabled, `pidOutput -= fSolarSetpointOffset` before the clamp to [10, effective maximum] (SATcontrol.ino:5137-5152); this happens before step 14 in the code (corrected 2026-10-24, TASK-1351); TASK-1323. There is no ECO-mode exception: a search for `iHeatingMode` found it only in settings, publish, REST and debug code (SATcontrol.ino:2533, SATcontrol.ino:2978, restAPI.ino:2315, settingStuff.ino:1104, handleDebug.ino:119).
16. The continuous/PWM auto-switch and the cold-setpoint cutoff use `pidOutput` after the offset (SATcontrol.ino:5542-5546, SATcontrol.ino:5562-5570). Commands sent: CS = `finalSetpoint`, MM = computed modulation, CH from `satChEnableDecide` (SATcontrol.ino:5572-5579, SATchEnable.h:46-52) (corrected 2026-10-24, TASK-1351).
17. Publish (SATcontrol.ino:5018). The sync debouncers and other health outputs are evaluated inside `satPublishMQTT()` (SATcontrol.ino:3083-3118).

#### Per heating control loop (firmware: the 3 s suppression refresh)

The firmware has no 5 s heating loop. The closest equivalent that touches this section is `satPwmSuppressionRefresh()`, timer every 3 s (SATcontrol.ino:480, SATcontrol.ino:894-918). It runs only in PWM mode during the flame-on phase after the hold, and not while valves are closed or summer mode is active (SATcontrol.ino:899-905). It applies no solar offset; it stores the refreshed CS as `_sat_lastControlCS` (`satPwmSuppressionRefresh`, SATcontrol.ino:1045-1065) (corrected 2026-10-24, TASK-1351); TASK-1323.

#### Health sensor evaluation

Evaluation rate: the sync debouncers and the health topics live in `satPublishMQTT()` (SATcontrol.ino:2647, SATcontrol.ino:3083-3118). A search found calls to `satPublishMQTT()` only from the control loop (SATcontrol.ino:4650, SATcontrol.ino:4744, SATcontrol.ino:4774, SATcontrol.ino:4788, SATcontrol.ino:5018). So these timers advance once per control tick (default 30 s) (derived). Pressure is evaluated in the tick at SATcontrol.ino:4758, after the early exits listed above.

**Sync sensors (delayed mismatch).** Each keeps a "mismatch since" time; any evaluation without mismatch clears it; a problem is reported once the mismatch has lasted at least 60 s (SATcontrol.ino:3084-3117). The first mismatching tick only starts the timer, so with a 30 s tick the topic turns ON at the third consecutive mismatching tick (about 60 s), or the fourth if the tick spacing lands just under 30 s (derived).

- `sat/setpoint_sync`: mismatch when |`fFinalSetpoint` - boiler `TSet`| > 0.5 °C and SAT is active (SATcontrol.ino:3086-3093).
- `sat/modulation_sync`: mismatch when the integer of the setting `iMaxRelModulation` differs from the integer of the boiler-reported `MaxRelModLevelSetting` and SAT is active (SATcontrol.ino:3098-3105). This compares the configured maximum, not the MM value actually sent, which can differ (for example MM=100 at the cold cutoff, SATcontrol.ino:4971-4974).
- `sat/ch_sync`: mismatch when "SAT active" differs from the boiler status bit with mask 0x02 (SATcontrol.ino:3110-3117). `bActive` is set true on every control tick (SATcontrol.ino:4616) and stays true while the tick commands CH=0 (summer mode, valves closed, cold cutoff) (SATcontrol.ino:4766-4790, SATcontrol.ino:4971-4983). In those states the topic can report a problem after 60 s (derived).
- A second, separate setpoint check `satCheckSetpointSync()` sets `bSetpointMismatch` (topic `sat/setpoint_mismatch`) when |`fFinalSetpoint` - `TSet`| > 1.0 °C for at least 60 s, and does nothing while `fFinalSetpoint` < 1.0 (SATcontrol.ino:3590-3614, SATcontrol.ino:2894).

**Pressure** (`satUpdatePressure`, SATcontrol.ino:3338-3483), once per control tick:

1. Track SAT's own `bActive` flag (not the boiler's heating state). On the first call only store it. On a change, suspend the drop-rate check for 600 s and clear the sample ring (SATcontrol.ino:3343-3355).
2. If the raw pressure is below 0.01 bar, return without changing anything else (SATcontrol.ino:3358). There is no "stale reading" alarm: `iLastSeenPressureMs` is written (SATcontrol.ino:3360) but an unfiltered search of the firmware found no reader of it, only this write and the declaration (SATtypes.h:260).
3. Smoothed pressure: initialise to the raw value when the smoothed value is below 0.01, else EMA with alpha 0.05 (SATcontrol.ino:3362-3368).
4. Store a raw sample in a 20-slot ring at most once per 30 s, and evict samples older than 3600 s (SATcontrol.ino:3326, SATcontrol.ino:3370-3399).
5. Drop rate by least squares when not suspended, with at least 3 samples spanning at least 300 s (SATcontrol.ino:3401-3437). See "Formulas".
6. Published drop rate: 0 while suspended; the new value when it is >= 0; otherwise the previous value is kept (SATcontrol.ino:3439-3444). Expired suspension is cleared (SATcontrol.ino:3446-3449).
7. Raw alarm = smoothed < `fMinPressure`, or smoothed > `fMaxPressure`, or (not suspended and drop rate >= 0 and drop rate > `fMaxPressureDrop`) (SATcontrol.ino:3455-3460).
8. Confirmation: the raw alarm must hold for at least 120 s; then `bPressureAlarm` = true and `bPressureHealthy` = false. Any tick without raw alarm clears the timer and, if set, the alarm (SATcontrol.ino:3462-3482).
9. `satPressureHealthUpdate()` copies the raw pressure into `fBoilerPressure` and sets the text status "ok", "low" or "high"; a confirmed alarm caused only by the drop rate is reported as "ok" (SATpressure.ino:33-54).

**Device health**: `sat/device_health` is ON when `eBoilerStatus` is `SAT_BS_OFF` (SATcontrol.ino:3000-3002). `SAT_BS_OFF` is the initial value (SATtypes.h:152). In `satUpdateBoilerStatus()` every branch assigns another status (SATcontrol.ino:546-637), and a search found no other assignment of `eBoilerStatus` (SATcontrol.ino:643). A search found `satUpdateBoilerStatus()` called only at SATcontrol.ino:4814. So the topic is ON from boot until the first control tick that reaches that call, and OFF afterwards (derived). There is no delay.

**Cycle health**: `sat/cycle_health` is ON when the last cycle class is OVERSHOOT, UNDERHEAT, UNDERHEAT_PWM or SHORT (SATcontrol.ino:3003-3009). Before any cycle the class is NONE (SATtypes.h:166), so the topic is OFF. The class is set in the cycle code (SATcycles.ino:566), covered in the cycle section.

**Flame health** (firmware only): ON for the flame states STUCK_ON, STUCK_OFF, PWM_SHORT and SHORT_CYCLING; not published at all while the flame status is INSUFFICIENT_DATA (SATcontrol.ino:2837-2847, SATcontrol.ino:3622-3676).

**Window**: window detection must be enabled (default off, SATtypes.h:463), else the input is ignored (SATcontrol.ino:1100). Input is one flag, set by the MQTT command `window` or the REST SAT sub-route `window` (POST or PUT); in both, the value "open", "1" or "ON" means open and anything else closed (MQTTstuff.ino:627-632, MQTTstuff.ino:670, restAPI.ino:1383-1397). A search found no other caller of `satHandleWindow` in the source files (MQTTstuff.ino:631, restAPI.ino:1396).

1. Open: set `bWindowOpen` and start the timer (SATcontrol.ino:1102-1108).
2. At each control tick, once the window has been open at least `iWindowMinOpenSec` (default 60 s, range 10 to 600 s), save the current target and preset, switch to the Activity preset and target `fPresetActivity` (default 10 °C), and reset the PID integral (SATcontrol.ino:1126-1142, SATcontrol.ino:4755, SATtypes.h:450, SATtypes.h:464, settingStuff.ino:1036). The delay is therefore rounded up to the next control tick (derived).
3. Close: clear the flag; if the switch had happened, restore the saved target and preset and reset the integral (SATcontrol.ino:1109-1122).

<a id="s7-formulas"></a>

### Formulas

**Indoor rise rate** (SATcontrol.ino:4234-4246), in °C per hour:

```
if prevMs > 0 and (now - prevMs) > 30000 ms:
    dtHours  = (now - prevMs) / 3600000
    rawRate  = (roomTemp - prevRoomTemp) / dtHours
    riseRateEma = 0.3 * rawRate + 0.7 * riseRateEma
    prevRoomTemp = roomTemp; prevMs = now
```

The average starts at 0.0 (SATcontrol.ino:4211). Arithmetic example from this formula: a constant raw rate of 1.0 °C/h starting from 0 gives 0.3, 0.51, 0.657 after three samples.

**Solar gain detection** (SATcontrol.ino:4267-4298):

```
risingFast    = riseRateEma > fSolarMinRiseRate
lowModulation = RelModLevel < 20.0
valvesOpen    = bValvesOpen
if not wasActive:
    if risingFast and lowModulation and valvesOpen: active after > 600 s continuously
    else: condition timer reset
else:
    if not risingFast: inactive after > 600 s continuously
    else: condition timer reset
sun gate (only with fresh sun data): elevation < fSolarMinElevation -> inactive at once
```

**Setpoint with solar gain** (SATcontrol.ino:5137-5152, SATchEnable.h:46-52) (corrected 2026-10-24, TASK-1351); TASK-1323:

```
if bSolarGainActive and bSolarGainEnable:
    pidOutput = pidOutput - fSolarSetpointOffset
pidOutput = clamp(pidOutput, 10, effectiveMax)          # the requested setpoint
finalSetpoint = PWM or continuous result from pidOutput, clamped to [10, effectiveMax]
if pidOutput <= coldSetpoint: finalSetpoint = 10, PWM left
CH = satChEnableDecide(pidOutput, coldSetpoint, pwmDrivesSetpoint, pwmOn)
```

**Summer Simmer index** (SATcontrol.ino:2577-2583):

```
if humidity < 0 or humidity > 100: return tempC
F = tempC * 9/5 + 32
if F < 58: return tempC
idx = 1.98 * (F - (0.55 - 0.0055 * humidity) * (F - 58.0)) - 56.83
return (idx - 32) * 5/9
```

No rounding is applied to the result (SATcontrol.ino:2582).

**Summer Simmer perception** (SATcontrol.ino:2585-2595), on the index in °C:

| Index (°C) | Label |
|---|---|
| < 21.1 | Cool |
| < 25.0 | Slightly Cool |
| < 28.3 | Comfortable |
| < 32.8 | Slightly Warm |
| < 37.8 | Increasing Discomfort |
| < 44.4 | Extremely Warm |
| < 51.7 | Danger Of Heatstroke |
| < 65.6 | Extreme Danger Of Heatstroke |
| otherwise | Circulatory Collapse Imminent |

There is no "Unknown" label; the topics are simply not published without valid humidity (SATcontrol.ino:3014).

**Thermal comfort substitution** (SATcontrol.ino:1264-1272): only in the OT room temperature path, and only when the room value is not a "ghost" and thermal comfort is on and the indoor humidity is fresh (`satHumidityFresh()`: valid and at most `iHumidityTimeoutS` old; pushed or BLE only, no weather fallback, TASK-1441): return the index of (OT room temperature, humidity). Otherwise the raw OT room temperature is used. BLE, multi-area and MQTT external room temperatures return earlier and are never converted (SATcontrol.ino:1208-1231).

**Humidity comfort offset.** Removed in TASK-1282 (Sergeant D entry 114). The formula and `effectiveTarget = fTargetTemp + offset` no longer apply; `effectiveTarget = fTargetTemp (+ PV boost)`.

Arithmetic example from this formula with defaults (reference 50 %, max 1.0 °C): humidity 70 % gives offset -0.4 °C.

**Summer mode counter** (SATcontrol.ino:4323-4357), per check (at most every 5 min):

```
dtHours = time since last check, in hours
not active: outside >= threshold -> hoursAbove += dtHours; active when hoursAbove >= iSummerMinHours
            outside <  threshold -> hoursAbove = 0
active:     outside <  threshold - 2 -> hoursAbove -= dtHours; inactive when hoursAbove <= 0
            otherwise               -> hoursAbove = max(hoursAbove, iSummerMinHours)
```

**Room error** (SATpid.ino:63-66):

```
error = round((target - room) * 1000) / 1000      (positive = room too cold)
```

**Hourly error ring** (SATcontrol.ino:3495-3545):

```
once per hour (first call samples at once): if bActive: append fError to 24-slot ring
if count < 6: recommendation = INSUFFICIENT, mean = 0
mean   = sum(samples) / count
stddev = sqrt(sum((s - mean)^2) / count)
threshold = fDeadband * 2            (0.2 °C with default deadband 0.1)
mean >  threshold -> INCREASE
mean < -threshold -> DECREASE
else              -> HOLD
```

The hour timer is restarted before the `bActive` check (SATcontrol.ino:3500-3504). The function is reached only on ticks that pass the summer and valve exits (SATcontrol.ino:4766-4796), so a tick that would have been due during summer mode or closed valves just delays the sample (derived).

**Daily-median recommendation** (SATcycles.ino:1201-1289):

```
every control tick (when bActive): append roomError = -fError to intra-day ring (1440 slots)
on the first NTP-valid call: remember dayNum = unixTime / 86400, no commit
when dayNum increases:
    if intra-day count >= 2: dailyMedian[head] = median(intra-day), ring of 30 days
    clear intra-day ring; recompute recommendation; save to LittleFS
recommendation (needs >= 3 stored days):
    last 3 daily medians all < -0.5 °C -> INCREASE
    last 3 daily medians all > +0.5 °C -> DECREASE
    else -> HOLD
median = sorted[n / 2] (upper middle element for even n)
```

The sign is the opposite of the hourly ring: here positive means the room is warmer than the target (SATcycles.ino:1200, SATcycles.ino:1205). The day boundary is `unixTime / 86400`, which is a UTC day (derived from SATcycles.ino:1214-1216). With a 30 s tick the 1440-slot intra-day ring holds about 12 h, so the daily median covers roughly the last 12 h of the day (derived from SATcycles.ino:1207-1209 and ../libraries/Platform/src/boards.h:124). The ring sizes are 30 days and 1440 samples (../libraries/Platform/src/boards.h:123-124). The code comment says "lower-middle for even n" (SATcycles.ino:1092); `sorted[n / 2]` with zero-based indexing is the upper of the two middle elements for even n (arithmetic, SATcycles.ino:1093).

**Pressure smoothing** (SATcontrol.ino:3362-3367), per control tick with a reading:

```
smoothed = raw                               (when smoothed < 0.01)
smoothed = 0.05 * raw + 0.95 * smoothed      (afterwards)
```

With a 30 s tick, 1 / 0.05 = 20 ticks, about 10 min (arithmetic).

**Pressure drop rate** (SATcontrol.ino:3405-3435): least-squares slope over all samples in the ring, with time in seconds from the oldest sample:

```
slope = (n*Σ(t*p) - Σt*Σp) / (n*Σ(t²) - (Σt)²)      (skipped when |denominator| <= 0.001)
dropRate = -slope * 3600                              (bar/h, positive = falling)
```

The ring holds 20 samples taken at least 30 s apart (SATcontrol.ino:3326, SATcontrol.ino:3371). In steady operation the ring is full long before the 3600 s eviction applies, so the regression spans about the last 20 samples, roughly 10 min at a 30 s tick (derived).

**Gas consumption** (SATcontrol.ino:3061-3077): the code exists, but `minCons` and `maxCons` are hard-coded 0.0, so the block never publishes; the comment calls it "currently dead".

<a id="s7-constants-and-defaults-table-name-value-unit-cit"></a>

### Constants and defaults (table: name | value | unit | citation)

| Name | Value | Unit | Citation |
|---|---|---|---|
| Control tick `iControlInterval` default | 30 | s | SATtypes.h:444 |
| Control tick range | 10 to 300 | s | settingStuff.ino:1001 |
| PWM suppression refresh timer | 3 | s | SATcontrol.ino:480 |
| `bSolarGainEnable` default | false | bool | SATtypes.h:494 |
| `fSolarMinRiseRate` default | 0.6 | °C/h | SATtypes.h:495 |
| `fSolarMinRiseRate` range | 0.1 to 5.0 | °C/h | settingStuff.ino:1083 |
| `fSolarSetpointOffset` default | 2.0 | °C | SATtypes.h:496 |
| `fSolarSetpointOffset` range | 0.5 to 10.0 | °C | settingStuff.ino:1084 |
| `fSolarMinElevation` default | 12.0 | degrees | SATtypes.h:497 |
| `fSolarMinElevation` range | -10 to 45 | degrees | settingStuff.ino:1085 |
| `bSolarFreezeIntegral` default (read by the PID since TASK-1323, corrected 2026-10-24, TASK-1351) | true | bool | SATtypes.h:549 |
| Solar rise-rate minimum sample gap | > 30 | s | SATcontrol.ino:4235 |
| Solar rise-rate EMA weights | 0.3 new, 0.7 old | - | SATcontrol.ino:4238 |
| Solar modulation limit | < 20 | % | SATcontrol.ino:4269 |
| Solar activate / clear sustain time | > 600 | s | SATcontrol.ino:4276, SATcontrol.ino:4289 |
| Sun elevation stale age | 3600 | s | SATcontrol.ino:4251 |
| `SAT_MIN_SETPOINT` (lower clamp after offset) | 10.0 | °C | SATcontrol.ino:54 |
| Cold setpoint radiators / underfloor (compared with `pidOutput`) | 28.2 / 21.0 | °C | SATcontrol.ino:55-56, SATcontrol.ino:4971 |
| `bThermalComfort` default | false | bool | SATtypes.h:522 |
| `iHumidityTimeoutS` default | 1800 | s | SATtypes.h:523 |
| `iHumidityTimeoutS` range | 60 to 65535 | s | settingStuff.ino:1108 |
| Simmer index pivot | 58.0 | °F | SATcontrol.ino:2580-2581 |
| Simmer index coefficients | 1.98, 0.55, 0.0055, 56.83 | constants of a °F formula | SATcontrol.ino:2581 |
| `bSummerSimmer` (summer mode) default | false | bool | SATtypes.h:499 |
| `fSummerThreshold` default / range | 18.0 / 5 to 35 | °C | SATtypes.h:500, settingStuff.ino:1087 |
| `iSummerMinHours` default / range | 6 / 1 to 48 | h | SATtypes.h:501, settingStuff.ino:1088 |
| Summer check interval | 300 | s | SATcontrol.ino:4305 |
| Summer deactivation hysteresis | 2.0 | °C | SATcontrol.ino:4327 |
| `fDeadband` default (integral band, history threshold base) | 0.1 | °C | SATtypes.h:443 |
| Hourly ring size | 24 | samples | SATcontrol.ino:3488 |
| Hourly sample interval | 3600 | s | SATcontrol.ino:3493 |
| Hourly ring minimum samples | 6 | samples | SATcontrol.ino:3514 |
| Hourly threshold | `fDeadband * 2` = 0.2 | °C | SATcontrol.ino:3537 |
| `HCR_DAYS` | 30 | days | libraries/Platform/src/boards.h:123 |
| `HCR_INTRADAY_SIZE` | 1440 | samples | libraries/Platform/src/boards.h:124 |
| `HCR_THRESHOLD_C` | 0.5 | °C | SATcycles.ino:165 |
| `HCR_SUSTAIN_DAYS` | 3 | days | SATcycles.ino:166 |
| Intra-day samples saved to file | at most 96 | samples | SATcycles.ino:1116-1117 |
| Sync sensor delay | 60 | s | SATcontrol.ino:3092, SATcontrol.ino:3104, SATcontrol.ino:3116 |
| Setpoint sync tolerance (`sat/setpoint_sync`) | 0.5 | °C | SATcontrol.ino:3088 |
| Setpoint mismatch tolerance (`sat/setpoint_mismatch`) | 1.0 | °C | SATcontrol.ino:3600 |
| Pressure drop-rate suspension | 600 | s | SATcontrol.ino:3349 |
| Pressure EMA alpha | 0.05 | per control tick | SATcontrol.ino:3366 |
| Pressure alarm confirmation | 120 | s | SATcontrol.ino:3467 |
| Pressure drop-rate minimum span | 300 | s | SATcontrol.ino:3414 |
| Pressure drop-rate minimum samples | 3 | samples | SATcontrol.ino:3405 |
| Pressure ring size | 20 | samples | SATcontrol.ino:3326 |
| Pressure sample interval | >= 30 | s | SATcontrol.ino:3371 |
| Pressure sample eviction age | 3600 | s | SATcontrol.ino:3383 |
| `fMinPressure` default / range | 0.8 / 0 to 3 | bar | SATtypes.h:469, settingStuff.ino:1041 |
| `fMaxPressure` default / range | 2.5 / 1 to 4 | bar | SATtypes.h:470, settingStuff.ino:1042 |
| `fMaxPressureDrop` default / range | 0.3 / 0.05 to 2 | bar/h | SATtypes.h:471, settingStuff.ino:1043 |
| `bWindowDetection` default | false | bool | SATtypes.h:463 |
| `iWindowMinOpenSec` default / range | 60 / 10 to 600 | s | SATtypes.h:464, settingStuff.ino:1036 |
| `fPresetActivity` default / range | 10 / 5 to 20 | °C | SATtypes.h:450, settingStuff.ino:1009 |
| BLE room temperature stale age | 300 | s | SATcontrol.ino:65 |
| `iSensorMaxAgeS` default (MQTT external room temperature only) | 21600 | s | SATtypes.h:515, SATcontrol.ino:1223-1231 |

<a id="s7-state-kept-between-passes"></a>

### State kept between passes

| Owner | State | Meaning | Persisted across restart | Citation |
|---|---|---|---|---|
| Solar gain | `_solar_prevRoomTemp`, `_solar_prevMs` | previous sample for the rise rate | No | SATcontrol.ino:4209-4210 |
| Solar gain | `_solar_riseRateEma` | averaged rise rate | No | SATcontrol.ino:4211 |
| Solar gain | `_solar_conditionMs` | start of the 10 min activate or clear condition | No | SATcontrol.ino:4212 |
| Solar gain | `_solar_wasActive`, `bSolarGainActive` | active flag | No | SATcontrol.ino:4213, SATtypes.h:368 |
| Sun input | `fSunElevation`, `bSunElevationValid`, `iSunElevLastMs` | last MQTT sun elevation | No | SATtypes.h:371-373 |
| Summer mode | `bSummerActive`, `fSummerHoursAbove`, `_summer_lastCheckMs` | mode and hour counter | No | SATtypes.h:375-376, SATcontrol.ino:4304 |
| Humidity | `fHumidity`, `bHumidityValid`, `iHumidityLastMs` | last humidity | No | SATtypes.h:378-381 |
| Hourly ring | `_cr_errorBuffer`, count, index, last sample time | 24 hourly errors | No; not cleared by `satDisable()` | SATcontrol.ino:3488-3492, SATcontrol.ino:2160-2187 |
| Hourly ring outputs | `eCurveRecommendation`, `fMeanError`, `fErrorStdDev`, `iErrorSampleCount` | result | No | SATtypes.h:268-271 |
| Daily median | `_hcr_dailyMedian`, head, count | up to 30 daily medians | Yes, `/sat/sat_hcr.json` on each day commit and in `satPersistAllToFs()`; cleared in RAM by `satDisable()` | SATcycles.ino:167-173, SATcycles.ino:1097-1133, SATcycles.ino:1241, SATcontrol.ino:2134, SATcontrol.ino:2174 |
| Daily median | `_hcr_samples` intra-day ring | today's room errors | Up to 96 saved and loaded, but emptied again by `satCycleInit()` at start-up | SATcycles.ino:1116-1126, SATcycles.ino:1166-1182, SATcycles.ino:254-256, SATcontrol.ino:3687-3688 |
| Daily median | `_hcr_lastDayNum` | last committed day | No | SATcycles.ino:180 |
| Pressure | `_press_ring`, count, head, last sample time | raw samples | No | SATcontrol.ino:3326-3331 |
| Pressure | `_press_lastActive`, `_press_lastActiveInit`, `_press_suspendUntilMs` | SAT active tracking and suspension end | No | SATcontrol.ino:3334-3336 |
| Pressure | `fSmoothedPressure`, `fPressureDropRate`, `bPressureAlarm`, `bPressureHealthy`, `iPressureAlarmSinceMs` | smoothed value, rate, alarm | No | SATtypes.h:253-257, SATcontrol.ino:3463-3479 |
| Sync | three static "mismatch since" times | start of each mismatch | No | SATcontrol.ino:3084, SATcontrol.ino:3096, SATcontrol.ino:3108 |
| Sync | `iMismatchSinceMs`, `bSetpointMismatch` | second setpoint check | No | SATcontrol.ino:3603-3612 |
| Window | `bWindowOpen`, `iWindowOpenSinceMs`, `fPreWindowTarget`, `iPreWindowPreset` | window state and saved target | No | SATcontrol.ino:1102-1139 |

The "No" entries mean that no save code for them was found; the persistence entry point `satPersistAllToFs()` saves only PID, energy, estimated energy and the daily-median ring (SATcontrol.ino:2128-2136).

<a id="s7-outputs-to-the-boiler-to-entities"></a>

### Outputs (to the boiler, to entities)

**To the boiler.**

- Solar gain lowers the requested setpoint (`pidOutput`) by `fSolarSetpointOffset` before the clamp, on every control tick; the 3 s suppression refresh has no solar term (SATcontrol.ino:5137-5152, SATcontrol.ino:1045-1065). The cold-setpoint cutoff, the CH rule and the mode switches see the lowered value, so solar can switch the heater off (SATcontrol.ino:5536-5579) (corrected 2026-10-24, TASK-1351); TASK-1323.
- With `bSolarFreezeIntegral` on, solar gain skips the whole integral update, the reset to 0 outside the deadband included (SATpid.ino:157) (corrected 2026-10-24, TASK-1351); TASK-1323.
- Summer mode (`bSummerSimmer`) sends CS=10, MM=`iMaxRelModulation`, CH=0 and skips the PID while active (SATcontrol.ino:4766-4776).
- Thermal comfort replaces the OT room temperature with the index, which then drives the room filter, the PID, the error and both history mechanisms (SATcontrol.ino:1264-1270, SATcontrol.ino:4656, SATcontrol.ino:4839). It also feeds the solar rise rate (SATcontrol.ino:4225).
- Window detection changes the target to the Activity preset and resets the integral (SATcontrol.ino:1134-1139).
- The history mechanisms, the error output and the health outputs send nothing to the boiler. Their code only reads values and writes state or MQTT (SATcontrol.ino:3495-3545, SATcycles.ino:1201-1289, SATcontrol.ino:3083-3118, SATcontrol.ino:3590-3614).

**To entities.** MQTT topics below are published by `satPublishMQTT()` with on-change plus heartbeat logic (SATcontrol.ino:2655-2659).

| Topic or field | Content | Citation |
|---|---|---|
| `sat/solar_gain` | solar gain active flag | SATcontrol.ino:2943 |
| `sat/indoor_rise_rate` | averaged rise rate, °C/h | SATcontrol.ino:2944 |
| `sat/solar_gain_sun_elevation` | last sun elevation, only while valid | SATcontrol.ino:2945-2947 |
| `sat/summer_active`, `sat/summer_hours_above` | summer mode and hour counter | SATcontrol.ino:2949-2950 |
| `sat/humidity`, `sat/humidity_valid` | humidity input | SATcontrol.ino:2952-2954 |
| `sat/ssi`, `sat/summer_simmer_index`, `sat/summer_simmer_perception` | index of `satGetRoomTemp()` and humidity, and its label; only when the indoor humidity is fresh (`satHumidityFresh()`, TASK-1441) and > 0 | SATcontrol.ino:3012-3019 |
| `sat/error` | `fError`, °C | SATcontrol.ino:2737 |
| `sat/curve_recommendation` | hourly ring: insufficient / increase / decrease / hold | SATcontrol.ino:2899-2905 |
| `sat/curve_recommendation_attributes` | JSON: `error_threshold`, `daily_mean_error` (the hourly ring mean), `daily_sample_count` (the hourly ring count), `recent_mean_error` (the last `fError`) | SATcontrol.ino:2906-2915 |
| `sat/heating_curve_recommendation` | daily-median text: insufficient / INCREASE / DECREASE / HOLD (retained) | SATcontrol.ino:2917, SATcycles.ino:1271-1280 |
| `sat/error_mean`, `sat/error_stddev` | hourly ring mean and standard deviation | SATcontrol.ino:2919-2920 |
| `sat/setpoint_sync`, `sat/modulation_sync`, `sat/ch_sync` | "ON" = problem after 60 s | SATcontrol.ino:3083-3118 |
| `sat/setpoint_mismatch`, `sat/modulation_reliable` | second setpoint check; modulation reliability | SATcontrol.ino:2893-2894 |
| `sat/pressure`, `sat/pressure_drop_rate`, `sat/pressure_alarm` | smoothed pressure, drop rate, confirmed alarm (true = problem) | SATcontrol.ino:2882-2884 |
| `sat/pressure_health` | "ON" when `bPressureHealthy` is true, that is when there is no alarm | SATcontrol.ino:2885-2886, SATcontrol.ino:3468-3479 |
| `sat/ch_pressure`, `sat/ch_pressure_status` | raw pressure and "ok"/"low"/"high" | SATpressure.ino:62-70 |
| `sat/device_health` | "ON" while boiler status is OFF | SATcontrol.ino:3000-3002 |
| `sat/cycle_health` | "ON" for OVERSHOOT, UNDERHEAT, UNDERHEAT_PWM, SHORT | SATcontrol.ino:3003-3009 |
| `sat/flame_health` | "ON" for a flame problem state | SATcontrol.ino:2837-2847 |
| `sat/window_open` | window flag | SATcontrol.ino:2859 |
| `sat/solar_gain_enable`, `sat/summer_simmer_enable`, `sat/thermal_comfort`, `sat/humidity_timeout_s`, `sat/solar_freeze_integral` | setting echoes | SATcontrol.ino:3154-3158, SATcontrol.ino:2981 |
| Home Assistant switches | solar gain and summer simmer enables | MQTTHaDiscovery.cpp:3364-3380 |
| REST health JSON (`satSendHealthJSON`) | `sync_setpoint` = `bSetpointMismatch`; `sync_modulation` = NOT `bModulationReliable`; `sync_ch` = undelayed CH comparison; `device_health` true = healthy (not OFF and not STALLED_IGNITION); `cycle_health` true = healthy; plus pressure, error statistics and the daily-median text | restAPI.ino:1016-1082 |

<a id="s7-edge-cases"></a>

### Edge cases

- **Solar gain in ECO mode.** No ECO exception exists; the offset is applied in both modes (SATcontrol.ino:5141-5142 [lines refreshed 2026-10-24, TASK-1351]; `iHeatingMode` search result under "Behaviour" step 15).
- **No valve data.** `bValvesOpen` defaults to true (SATtypes.h:243), so the valve term is met unless an MQTT `valves_open` command says otherwise (MQTTstuff.ino:634-639).
- **No sun elevation.** Solar gain still works on rise rate and modulation alone (SATcontrol.ino:4262). Thermo-nova instead keeps solar gain off without sun data (a7-features-thermonova.md:343). Sun data older than 1 h is dropped and the same rule applies (SATcontrol.ino:4251-4253).
- **Flame off.** The solar code does not look at the flame. It uses the boiler's reported relative modulation; before any report this is 0.0, which counts as low (SATcontrol.ino:4267-4269, OTGW-Core.h:72).
- **Slow activation and slow release.** Activation needs more than 10 min of continuous qualifying ticks, and release needs more than 10 min of continuous not-fast ticks (SATcontrol.ino:4276, SATcontrol.ino:4289). A single tick that is not "rising fast" during the activation phase restarts the 10 min (SATcontrol.ino:4282-4284).
- **Disable and re-enable.** Disabling solar gain does not clear the stored sample or the average (SATcontrol.ino:4217-4223). After a long disabled period the first new rise rate spans the whole gap (derived from SATcontrol.ino:4235-4238).
- **Solar setpoint floor.** After the offset the requested setpoint is clamped to at least 10 °C (SATcontrol.ino:5150). CH follows `satChEnableDecide`, not the CS value: a lowered request at or below the cold setpoint gives CH=0 and CS=10 (SATchEnable.h:46-52) (corrected 2026-10-24, TASK-1351).
- **Continuous mode floor.** The continuous-mode lower limit (boiler temperature minus `fFlowOffset`) is applied after the solar offset, so the offset can no longer take the CS below that limit (SATcontrol.ino:5137-5176) (corrected 2026-10-24, TASK-1351); TASK-1323.
- **Integral during solar gain.** With `bSolarFreezeIntegral` on (default) the integral is neither accumulated nor reset while solar gain is active; with it off, the normal deadband rules apply (SATpid.ino:157-163) (corrected 2026-10-24, TASK-1351); TASK-1323.
- **Thermal comfort with a BLE, multi-area or external room sensor.** No index is used, because those sources return before the thermal comfort code (SATcontrol.ino:1208-1231, SATcontrol.ino:1264).
- **Thermal comfort without fresh humidity.** The PID keeps running on the raw OT room temperature and a debug line is written (SATcontrol.ino:1271). Thermo-nova instead skips the PID tick (a7-features-thermonova.md:349-350).
- **Thermal comfort with only weather data.** Not used since TASK-1441 (Sergeant D entry 117): outdoor weather humidity is not an indoor source, so the raw room temperature is used.
- **Weather humidity loop.** Removed in TASK-1282 (Sergeant D entry 114). The 30 min stale check that marked humidity invalid lived in `satUpdateComfort()` and is gone. The weather humidity fallback was removed in TASK-1441 (entry 117).
- **Summer mode without any outdoor source.** The OT outside temperature defaults to 0.0 (OTGW-Core.h:82), so the threshold (minimum 5 °C) is never reached (derived from SATcontrol.ino:4331 and settingStuff.ino:1087). Summer mode reads `satGetOutsideTemp()` directly, not the clamped copy used by the heating curve (SATcontrol.ino:4326, SATcontrol.ino:4721-4723).
- **Summer mode release time.** Any check with outside at or above threshold - 2 °C refills the counter, so release needs `iSummerMinHours` of continuous checks below threshold - 2 °C (derived from SATcontrol.ino:4344-4356).
- **Simmer index below 58 °F (about 14.4 °C).** The index returns the input temperature unchanged and unrounded (SATcontrol.ino:2580).
- **Simmer index topic with thermal comfort on.** The published index is computed from `satGetRoomTemp()`, which already returns the index when thermal comfort is active, so the topic shows the index of the index (derived from SATcontrol.ino:3015 and SATcontrol.ino:1264-1270). Thermo-nova has the same effect (a7-features-thermonova.md:337).
- **History while not heating.** Both mechanisms are skipped on ticks that exit early for DHW, summer mode or closed valves (SATcontrol.ino:4865-4882 [lines refreshed 2026-10-24, TASK-1351], SATcontrol.ino:4766-4799).
- **History sample is one tick old.** Both history functions run before this tick's PID update and read `fError` from the previous update (SATcontrol.ino:4796-4799, SATcontrol.ino:4839, SATpid.ino:305) (derived).
- **Daily recommendation after restart.** The text is "insufficient" until the next UTC day boundary recomputes it, even when 3 or more days were restored (SATcycles.ino:254-257, SATcycles.ino:1224-1242) (derived).
- **Daily recommendation after a SAT disable.** `satDisable()` clears the daily ring in RAM (SATcontrol.ino:2174, SATcycles.ino:1189-1197). The file is overwritten at the next save (SATcycles.ino:1097-1133) (derived).
- **Pressure reading missing.** Values below 0.01 bar return early, so alarm and status keep their last state and no stale problem is raised (SATcontrol.ino:3358).
- **SAT active flag, not boiler heating.** The drop-rate suspension follows SAT's own active flag, which a search found written only at start-up, in `satDisable()` (disable, trip, fallback lift) and on each control tick (SATcontrol.ino:3344, SATcontrol.ino:3690, SATcontrol.ino:2168, SATcontrol.ino:4616) (derived). Burner starts and stops do not suspend the drop-rate check (derived).
- **Drop rate sign.** Only falling pressure counts; negative rates are neither stored nor alarmed (SATcontrol.ino:3442-3443, SATcontrol.ino:3456-3457). The field comment "negative when suspended" (SATtypes.h:254) conflicts with the code, which writes 0 while suspended (SATcontrol.ino:3440-3441).
- **Pressure limits use the smoothed value, drop rate uses raw samples** (SATcontrol.ino:3373-3374, SATcontrol.ino:3458-3459).
- **Drop-rate-only alarm shows "ok" in `sat/ch_pressure_status`** while `sat/pressure_alarm` is true (SATpressure.ino:49-52).
- **Polarity differs between outputs.** `sat/pressure_health` ON = healthy (SATcontrol.ino:2886). `sat/device_health`, `sat/cycle_health` and the sync topics ON = problem (SATcontrol.ino:3000-3009, SATcontrol.ino:3083-3118). In the REST JSON `device_health` and `cycle_health` true = healthy (restAPI.ino:1026-1031).
- **Device health counts STALLED_IGNITION only in REST.** MQTT checks only OFF (SATcontrol.ino:3001); REST also treats STALLED_IGNITION as unhealthy (restAPI.ino:1026-1027).
- **Window with several sensors.** There is one input flag; any "closed" message ends the Activity preset (MQTTstuff.ino:627-632, restAPI.ino:1393-1396, SATcontrol.ino:1109-1122).
- **Setpoint mismatch latch.** `satCheckSetpointSync()` returns early when `fFinalSetpoint` < 1.0 (SATcontrol.ino:3597), and `satDisable()` sets it to 0 (SATcontrol.ino:2169). A `bSetpointMismatch` that was true at disable therefore stays true until SAT controls again (derived).
- **Humidity validity is never cleared.** `bHumidityValid` stays true once a value has arrived, but since TASK-1441 every use goes through `satHumidityFresh()`, which also checks the age against `iHumidityTimeoutS`: the thermal comfort input and the published index both stop on a stale value.
- **Simulation scenario `solar_gain`.** In simulation mode an injected event warms the simulated room by a set amount per minute for a set time (SATcontrol.ino:3882-3897, SATcontrol.ino:4032-4041). It only changes the simulated temperature; detection runs through the normal code above (derived).

<a id="s7-not-found-in-the-code-unclear"></a>

### Not found in the code / unclear

1. **Home Assistant discovery for the health topics, the index and the recommendations.** A tree-wide search of `src/OTGW-firmware` for `pressure_health`, `device_health`, `cycle_health`, `setpoint_sync`, `modulation_sync`, `ch_sync`, `curve_recommendation`, `heating_curve_recommendation`, `summer_simmer_index`, `sat/ssi` and `indoor_rise_rate` found no discovery code. The SAT discovery list (MQTTHaDiscovery.cpp:300-357) includes `sat/flame_health` but none of these. This conflicts with a code comment that calls `sat/device_health`, `sat/cycle_health`, the three sync topics and `sat/pressure_health` "HA binary_sensor entities" (SATmqttPublish.h:171-176). The two sources disagree; this section does not pick one. If no discovery exists, the device class and how Home Assistant shows them are not defined in the firmware. For `sat/pressure_health` the ON = healthy polarity matters if some external configuration declares it a PROBLEM sensor.
2. **`bSolarFreezeIntegral`.** Resolved: read by the PID since TASK-1323 (SATpid.ino:157; entry 11 answer 3) (corrected 2026-10-24, TASK-1351).
3. **Boiler modulation with the flame off.** The solar rule uses the reported relative modulation without a flame check (SATcontrol.ino:4267-4269). What a given boiler reports while the flame is off is not in the firmware.
4. **Exact solar sampling cadence at a 30 s tick.** The "more than 30 s" test (SATcontrol.ino:4235) against a 30 s timer (SATcontrol.ino:476) makes the cadence depend on loop timing, which the code does not fix.
5. **NaN index publish.** If `satGetRoomTemp()` returns NaN while humidity is valid, the index is computed from NaN and passed to `publishIfChangedF` (SATcontrol.ino:3014-3017). How that helper handles NaN was not read.
6. **`satHCRAddSample` comment says "~every 15 min"** (SATcycles.ino:1199), but it is called every control tick (SATcontrol.ino:4799). The comment at SATcycles.ino:175-176 says "ESP32: 1440 samples at 1-min intervals", which also does not match the default 30 s tick.
7. **Median comment vs code.** "lower-middle for even n" (SATcycles.ino:1092) vs `sorted[n / 2]` (SATcycles.ino:1093).
8. **Two setpoint sync checks with different tolerances** (0.5 °C at SATcontrol.ino:3088 and 1.0 °C at SATcontrol.ino:3600). No comment explains why both exist.
9. **Modulation sync compares the configured maximum** (SATcontrol.ino:3098-3099), not the MM value sent (SATcontrol.ino:4981). Whether this is intended is not stated.
10. **Meaning of boiler status mask 0x02.** The code names it `boilerActive` / `boilerCHActive` (SATcontrol.ino:3110, restAPI.ino:1023) but does not say which OpenTherm status bit it is in a comment there.
12. **Weather humidity is outdoor humidity** (SATweather.ino:63, SATweather.ino:261). It is no longer used as an indoor humidity fallback (TASK-1441, entry 117).
13. **No persistence of solar, summer, hourly ring and pressure state.** No save code was found for them (SATcontrol.ino:2128-2136). The documentation does not say whether this is intended.
14. **REST and MQTT health definitions differ** (restAPI.ino:1020-1031 vs SATcontrol.ino:3000-3009 and SATcontrol.ino:3083-3118). No comment explains the difference.
15. **Summer simmer formula origin.** The code says it matches the SAT Python formula (SATcontrol.ino:2576) but gives no primary reference.

---

<a id="s8"></a>

## Boiler I/O, DHW, limits and safety, configuration defaults (OTGW firmware, as implemented)

Source: OTGW-firmware, branch `dev`, HEAD 51df6b486. All citations are `file:line` relative to `src/OTGW-firmware/`. The working tree has staged changes in `SATtypes.h`, `settingStuff.ino` and `MQTTstuff.ino`; they replace lines one for one (a version banner and an MQTT top-topic default), so the line numbers cited here are the same as in HEAD.

This section describes what the firmware does, not what it should do. Where it differs from thermo-nova, the difference is stated in a short "Compare" note. The thermo-nova facts in those notes come from the companion section `a8-io-dhw-safety-thermonova.md`, not from a new reading.

<a id="s8-purpose"></a>

### Purpose

This section describes the boundary between the SAT port and the boiler:

- what SAT writes to the boiler, and when;
- how those writes reach the boiler (PIC serial queue or OT-Direct);
- what SAT reads back;
- how SAT handles domestic hot water (DHW);
- which limits and fallbacks exist (sensor loss, communication loss, maximum setpoint);
- the configuration options and their defaults.

The PID, heating curve, PWM schedule and cycle classification are only referenced. They belong to other sections.

<a id="s8-inputs"></a>

### Inputs

**Selection of the backend.** There is no coordinator choice. SAT writes PIC-style command strings into one function, `addCommandToQueue()` (`SATcontrol.ino:377`, `SATcontrol.ino:422`, `SATcontrol.ino:434`, `SATcontrol.ino:449`). That function sends them to the PIC command queue when a PIC is present, or straight to the OT-Direct command handler on a board without a PIC (`OTGW-Core.ino:3617-3628`). "A command interface exists" means a PIC or OT-Direct is active (`OTGW-firmware.h:559-561`).

**Values SAT reads from the boiler side.** All come from the shared OpenTherm state `OTcurrentSystemState`, unless simulation is on (see Behaviour E).

| Value | Source in firmware | Citation |
|---|---|---|
| Flame on | `Statusflags & 0x08` (slave status bit 3) | `SATcontrol.ino:1337-1341`, `OTGW-Core.h:203` |
| DHW active | `SlaveStatus & 0x04` (slave status bit 2), sampled once per control tick | `SATcontrol.ino:4628`, `OTGW-Core.h:204` |
| Boiler fault | `SlaveStatus & 0x01` (slave status bit 0) | `SATcontrol.ino:4716` |
| Flow temperature | `Tboiler` (MsgID 25) | `SATcontrol.ino:1325-1329` |
| Return temperature | `Tret` (MsgID 28) | `SATcontrol.ino:1331-1335` |
| Relative modulation | `RelModLevel` (MsgID 17) | `SATcontrol.ino:1343-1347` |
| Control setpoint as reported | `TSet` (MsgID 1), used only for the sync check | `SATcontrol.ino:3590-3594`, `OTGW-Core.ino:4272` |
| Boiler maximum CH setpoint | `MaxTSet` (MsgID 57), default 0 until received | `SATcontrol.ino:242`, `OTGW-Core.ino:4294`, `OTGW-Core.h:103` |
| CH water pressure | `CHPressure` | `SATcontrol.ino:3340` |
| Manufacturer (member ID) | MsgID 3 low byte, fed to `satDetectManufacturer()` | `OTGW-Core.ino:3076`, `SATcontrol.ino:175-190` |
| Outside temperature | simulation, else external MQTT/REST value (stale after 10 min), else the weather feed when `Toutside` is exactly 0, else `Toutside` (MsgID 27, default 0.0) | `SATcontrol.ino:1283-1303`, `SATcontrol.ino:67`, `OTGW-Core.h:82` |
| Room temperature | simulation, BLE (stale after 5 min), multi-area average, external MQTT/REST (stale after `iSensorMaxAgeS`), thermostat `Tr` (MsgID 24, stale 5 min after the thermostat last sent it) | `SATcontrol.ino:1192-1280`, `SATcontrol.ino:65-66` |

**User and integration inputs that act on the boiler path.**

- MQTT topics `set/<nodeId>/sat/<command>` are dispatched through the table `kSatMqttCmds` (`MQTTstuff.ino:655-747`, `MQTTstuff.ino:754-769`, `MQTTstuff.ino:868-917`). A row either calls a handler or passes the payload to `updateSetting()` under a settings key (`MQTTstuff.ino:760-764`).
- Rows that matter for this section: `enabled` (`MQTTstuff.ino:664`), `control_mode` (`MQTTstuff.ino:665`), `valves_open` (`MQTTstuff.ino:671`), `max_modulation` (`MQTTstuff.ino:679`), `dhw_setpoint`, `dhw_enabled`, `dhw_enable` (`MQTTstuff.ino:680-682`), `interval` (`MQTTstuff.ino:683`), `push_setpoint` (`MQTTstuff.ino:684`), `overshoot_protection` (`MQTTstuff.ino:687`), `simulation` (`MQTTstuff.ino:699`), `manufacturer` (`MQTTstuff.ino:678`), `heating_system` (`MQTTstuff.ino:677`). (Removed in TASK-1332, 2026-10-23: the Force PWM option no longer exists.)
- The REST settings path calls the same `updateSetting()` keys, for example for the three DHW options (`restAPI.ino:1653-1661`).
- `valves_open` is the only writer of `state.sat.bValvesOpen` found (`MQTTstuff.ino:634-639`). Its default is true (`SATtypes.h:243`).

<a id="s8-behaviour-step-by-step-in-time-order"></a>

### Behaviour (step by step, in time order)

#### A. Start-up

1. **Order in `setup()`.** The PIC boot commands are sent first (`OTGW-firmware.ino:557`), then `initSAT()` runs (`OTGW-firmware.ino:567`).
2. **`initSAT()` resets SAT.**
   - The PID is reset, then the saved PID state is loaded (`SATcontrol.ino:3683-3684`). The load is skipped when NTP time is not yet set, and a saved state older than 1800 s is discarded (`SATcontrol.ino:2026-2037`, `SATcontrol.ino:1993`). The control loop retries the load once after NTP is set (`SATcontrol.ino:4515-4519`).
   - Energy totals, the daily recommendation ring, the cycle tracker and the PWM state are restored or reset (`SATcontrol.ino:3685-3689`).
   - `bActive` = false, control mode OFF, safety trip cleared, skip and PIC-failure counters zeroed, the boot-CS=0 flag cleared (`SATcontrol.ino:3690-3696`).
   - The write-on-change cache is invalidated (`SATcontrol.ino:3700`, `SATcontrol.ino:356-362`).
3. **Boot release, `CS=0`.** If a command interface exists, SAT sends `CS=0` at once (`SATcontrol.ino:3706-3710`). The comment gives the reason: after a crash the PIC may still hold the last CS (`SATcontrol.ino:3702-3705`). Otherwise the `CS=0` is sent on the first control-loop pass that finds a command interface (`SATcontrol.ino:4585-4589`). That deferred send is reached only while SAT is enabled, no flash update runs and no safety trip is set (`SATcontrol.ino:4568`, `SATcontrol.ino:4581`).
   - `CS=0` is sent whether or not SAT is enabled (`SATcontrol.ino:3706-3710`, before the enabled check at `SATcontrol.ino:3721`).
4. **Control timer.** The control timer is set to `iControlInterval` seconds and its first due time is one full interval later (`SATcontrol.ino:3719`, `safeTimers.h:131-135`). Default interval 30 s (`SATtypes.h:444`).
5. **Boot commands, only when SAT is enabled at boot.**
   - Control mode becomes CONTINUOUS (`SATcontrol.ino:3721-3722`).
   - If a command interface exists: `PM=3`, `PM=15`, `PM=48`, and `MI=500` when the manufacturer has the `SAT_QUIRK_MI_500_BOOT` flag (`SATcontrol.ino:3725-3733`). That flag is set for Ideal, Intergas and Nefit (`SATcontrol.ino:128`, `SATcontrol.ino:130`, `SATcontrol.ino:132`).
   - The manufacturer is the configured one, or the detected one when the setting is AUTO (`SATcontrol.ino:144-149`). The detected value starts as OTHER (`SATtypes.h:240`) and changes only when a MsgID 3 frame arrives (`OTGW-Core.ino:3076`). So at boot, with the default AUTO setting (`SATtypes.h:472`), `MI=500` is sent only if MsgID 3 was already seen.
   - **PIC path: one `PM=` at a time** (corrected 2026-10-24, TASK-1351); TASK-1315. `initSAT()` calls `pmChainStart()` (`SATcontrol.ino:3946-3948`). The PIC keeps one pending priority message, so the chain queues `PM=3` first; the next `PM=` is queued when the boiler answered the pending ID (`pmChainOnBoilerResponse`, called from `processOT()`) or after 60 s, the chain removes only its own previous `PM=` from the queue, and an ID that timed out is asked once more after the last one (`OTGW-Core.ino:3895-3982`). Before TASK-1315 the three calls ran back to back and only `PM=48` survived the queue. This follows from reading the code; it was not checked on hardware in this correction.
   - **OT-Direct path:** `pmChainStart()` queues all three at once (`OTGW-Core.ino:3950-3958`) (corrected 2026-10-24, TASK-1351); each command goes straight to `handleOTDirectCommand()` (`OTGW-Core.ino:3618-3624`). `PM=<id>` marks that MsgID for an immediate poll, or queues a one-shot READ if it is not in the schedule (`OTDirect.ino:3369-3392`). So all three are handled. `MI=` sets the OT-Direct message interval (100 to 1275 ms) and stores it in settings (`OTDirect.ino:3572-3582`).
   - Enabling SAT later (MQTT `enabled`, REST, settings page) does not send `PM=` or `MI=`: those paths only set the flag and clear a safety trip (`SATcontrol.ino:1950-1958`, `settingStuff.ino:981-992`).
   - Compare: thermo-nova sends `PM=3`, `PM=15`, `PM=48` (and `MI=500`) on the OTGW-MQTT path on every coordinator start, twice per start.
6. **No setpoint is computed before the first control tick.** All CS, MM, CH, TP and TC writes of the regulation come from the timer-guarded part of the control pass (`SATcontrol.ino:4614`, `SATcontrol.ino:4978-4994`), plus the special paths in Behaviour C.

#### B. Timers

- `satControlLoop()` is called on every `loop()` pass while no flash update runs (`OTGW-firmware.ino:1132`, `OTGW-firmware.ino:1157`). The work before the timer gate runs on every pass (`SATcontrol.ino:4510-4611`).
- **Control tick:** `timerSATControl`, `iControlInterval` seconds, default 30, settable 10 to 300 (`SATcontrol.ino:476`, `SATtypes.h:444`, `settingStuff.ino:1000-1003`). It catches up missed ticks (`SATcontrol.ino:476`, `safeTimers.h:192-194`), unless it is more than 10 intervals late, then it skips (`safeTimers.h:186-189`).
- **PWM suppression refresh:** every 3 s (`SATcontrol.ino:480`, `SATcontrol.ino:894-918`). See Behaviour C.
- **PIC CS keepalive:** checked on every pass, sends at most every 30 s (`SATcontrol.ino:403-414`, `SATcontrol.ino:4608`).
- **Write-on-change refresh window:** 300 s (`SATcontrol.ino:351`).
- **4-hour and 24-hour window statistics:** every 60 s (`SATcontrol.ino:478`, `SATcontrol.ino:4602-4605`).
- **PID state save:** every 300 s inside the control tick (`SATcontrol.ino:5010-5012`).
- **PIC command queue:** drained once per second; a queued command is re-sent every 5000 ms until the PIC answers with the same two letters, at most 5 sends (`OTGW-firmware.ino:756-758`, `OTGW-Core.ino:3588-3589`, `OTGW-Core.ino:3741-3755`, `OTGW-Core.ino:3790-3806`). The queue pauses 2000 ms after serial-bridge traffic (`OTGW-Core.ino:3590`, `OTGW-Core.ino:3739`).
- Compare: thermo-nova ticks every 5 s and writes every tick. The firmware ticks every 30 s by default and writes only on change (see C.7).

#### C. Per control pass: what gets written

Steps 1 to 9 run on every pass. Steps 10 onward run only when the control timer is due.

1. **Boiler-detected teardown of simulation** (`SATcontrol.ino:4525-4526`). See Behaviour E.
2. **OT bus back online.** On an offline-to-online edge of the OT bus, the write-on-change cache is invalidated, so the next tick re-sends everything (`SATcontrol.ino:4531-4535`).
3. **MQTT fallback (auto-enable).** If SAT is disabled, MQTT is enabled, MQTT was connected before and has now been disconnected for more than 300 s, and the opt-in `SATmqttfallback` is on, SAT sets `state.sat.bFallbackActive` only; `bEnabled` is never written (`satMqttFallbackStep()`, `SATcontrol.ino:4677-4707`). It does not start after the user switched SAT off during the same outage (`bFallbackSuppressed`, cleared when MQTT is back). The fallback is lifted when MQTT is connected again or switched off, when the opt-in is switched off, or when the user switches SAT on or off; `satDisable()` runs unless the user enabled SAT, which sends `CS=0` (`SATcontrol.ino:2179`).
4. **Disabled or flashing.** If SAT is not enabled, or a flash update runs, and SAT was active, `satDisable()` runs; then the pass returns (`SATcontrol.ino:4568-4573`).
5. Simulation model and thermal learning update (`SATcontrol.ino:4576-4578`).
6. **Safety trip.** If tripped, the pass returns. Nothing more is written until the trip is cleared (`SATcontrol.ino:4581`).
   - **OpenTherm loss (TASK-1365, entries 39 and 47).** Right after the trip check, `satOtLinkStep()` runs (`SATcontrol.ino:4920`, block `SATcontrol.ino:2431-2503`). A tripped pass rebases the silence window (`SATcontrol.ino:4915`), as a switch-off does. See G, "OpenTherm lost". While the link is lost the pass returns here, before the deferred boot `CS=0`, the PIC keepalive, the PWM refresh and the control tick.
7. Deferred boot `CS=0` (step A.3) (`SATcontrol.ino:4585-4589`).
8. Flame edge detection, cycle sampling, window statistics (`SATcontrol.ino:4592-4605`).
9. **Between-tick writes.**
   - PIC CS keepalive (`SATcontrol.ino:4608`): on a PIC board, if a CS of 8 or more was sent, it is re-sent unchanged when 30 s have passed since the last CS (`SATcontrol.ino:405-414`). The comment says the PIC drops a CS of 8 or more about 63 to 126 s after the last one (`SATcontrol.ino:398-402`).
   - PWM suppression refresh (`SATcontrol.ino:4611`): every 3 s, only in PWM ON phase with the flame on, after the hold delay, with overshoot protection on, no DHW and no fault flag (`SATcontrol.ino:894-905`). It sends `CS = flow - fModSupOffset`, clamped to [10, system cap] and then to the effective maximum; no solar offset (`SATcontrol.ino:1045-1065`) (corrected 2026-10-24, TASK-1351). This belongs to the PWM section.
10. **Control tick gate.** If the timer is not due, the pass ends (`SATcontrol.ino:4614`). Otherwise `bActive` = true, and control mode OFF becomes CONTINUOUS (`SATcontrol.ino:4616-4619`).
11. **DHW check.** `bDhwActive` is set from slave status bit 2, or forced true during a simulated draw (`SATcontrol.ino:4625-4629`). Power and energy are updated (`SATcontrol.ino:4632-4633`). If DHW is active, SAT sends `SW=` on the transition only (see Behaviour F) and returns (`SATcontrol.ino:4865-4882 [lines refreshed 2026-10-24, TASK-1351]`). **No CS, MM or CH is written during DHW.**
12. **Room temperature check.** If the room temperature is NaN or outside [-10, 50] °C the tick goes on as room-temperature-lost (TASK-1364): curve only, no trip. See Behaviour G.
13. Room temperature EMA filter, tau 45 s (`SATcontrol.ino:4694-4713`). PID section.
14. **Boiler fault flag.** If slave status bit 0 is set, the tick returns without writing (`SATcontrol.ino:4716-4719`).
15. **Outside temperature sanity.** Outside [-40, 50] °C it is replaced by 10 °C (`SATcontrol.ino:4721-4723`). There is no "outside temperature missing" gate: with no source it is `Toutside`, which starts at 0.0 (`OTGW-Core.h:82`, `SATcontrol.ino:1302`).
16. **Thermal safe fallback.** Only while `bFallbackActive`: after 2 h (`SAT_THERMAL_MAX_EST_MS`) since the last real room reading, SAT writes `CS=45` (capped to the system cap, truncated to an integer), `MM=iMaxRelModulation` and `CH=1`, then returns (`SATcontrol.ino:4730-4746`, `SATcontrol.ino:82-83`). Before 2 h the deadband is widened by 0.5 °C per hour (`SATcontrol.ino:4747-4751`).
17. Window timer, pressure monitoring, solar gain (`SATcontrol.ino:4755-4762`).
18. **Summer simmer.** If summer mode is active and enabled: `CS=10`, `MM=iMaxRelModulation`, `CH=0`, return (`SATcontrol.ino:4765-4776`).
19. **Valves closed.** If `bValvesOpen` is false: `CS=10`, `MM=iMaxRelModulation`, `CH=0`, return (`SATcontrol.ino:4779-4790`).
20. Auto-tune, curve recommendation, modulation reliability (skipped for `SAT_QUIRK_NO_REL_MOD`), setpoint sync check, flame status, boiler status, comfort, PV boost (`SATcontrol.ino:4793-4829`).
21. Heating curve and PID give `pidOutput`; the zone block may raise it or cap it (`SATcontrol.ino:5529-5561`). PID and curve sections.
22. **Clamp of `pidOutput`** to [10, effective maximum] (`SATcontrol.ino:4908-4912`). See G.
23. **Mode choice.** `pidOutput >= sysMax` while in PWM switches to CONTINUOUS (`SATcontrol.ino:4917-4920`). `bForcePWM` then forces PWM (`SATcontrol.ino:4923-4925`). (Removed in TASK-1332, 2026-10-23: the Force PWM option no longer exists.)
24. **Setpoint.** PWM branch `satApplyPWM()` or continuous branch `satApplyContinuous()` (`SATcontrol.ino:4928-4933`). Facts that matter for the write:
    - PWM without overshoot protection, during DHW, or with `pidOutput` at or below the cold setpoint: no override; the previous control setpoint is kept, or `pidOutput` if the previous one is 10 or less (`SATcontrol.ino:870`, `SATcontrol.ino:872-874`).
    - PWM OFF phase: 10 (`SATcontrol.ino:875-878`).
    - PWM ON, flame off: `return + fFlameOffOffset`, clamped to [cold setpoint, system cap] (`SATcontrol.ino:880-882`, `SATcontrol.ino:701-707`).
    - PWM ON, flame on, inside `fModSupDelay`: the hold setpoint, else the previous one (`SATcontrol.ino:884-886`).
    - PWM ON, flame on, after the delay: `flow - fModSupOffset`, clamped to [10, system cap] (`SATcontrol.ino:887-888`, `SATcontrol.ino:711-717`).
    - Continuous: `pidOutput` when the flame is off, the flow is 0 or less or above 100, or the flow is at or below `pidOutput`; otherwise `max(pidOutput, flow - fFlowOffset)` (`SATcontrol.ino:1161-1186`). There is no rising/falling distinction.
    - Compare: thermo-nova's continuous branch holds a falling request at the previous setpoint; the firmware does not compare with the previous setpoint.
25. **Final clamp** of the setpoint to [10, effective maximum] (`SATcontrol.ino:5178-5183`). The solar offset is no longer applied here; it lowers `pidOutput` before step 22 (`SATcontrol.ino:5137-5143`) (corrected 2026-10-24, TASK-1351); TASK-1323.
26. Auto-switch check, only when `pidOutput` is above the cold setpoint, for every source (`SATcontrol.ino:5542-5546`). PWM section.
27. **Modulation value** `iCurrentModulation = satComputeMM(...)` (`SATcontrol.ino:4960-4967`):
    - heat pump outside PWM: 100;
    - PWM: 0, or the configured maximum for heat pump and hybrid;
    - otherwise the configured maximum `iMaxRelModulation`;
    - heat pump or hybrid never get 0 (it becomes 100);
    - Geminox (`SAT_QUIRK_MIN_MOD_10`): at least 10; Immergas (`SAT_QUIRK_IMMERGAS_TP`): at most 80 (`SATcontrol.ino:304-314`).
28. **Cold-setpoint cutoff.** If `pidOutput` is at or below the cold setpoint (28.2 °C radiators, 21 °C underfloor), the setpoint becomes 10, the modulation `iMaxRelModulation`, and `satPwmDisableOnHeaterOff()` leaves PWM (`SATcontrol.ino:5206-5214`, `SATcontrol.ino:1100-1109`, `SATcontrol.ino:130-131`) (corrected 2026-10-24, TASK-1351); TASK-1327.
29. **Writes, in this order, when a command interface exists** (`SATcontrol.ino:4978-4994`):
    1. `CS=<setpoint>` via `satEnqueueIfChangedCS()` (`SATcontrol.ino:4980`);
    2. `MM=<value>` via `satEnqueueIfChangedMM()` (`SATcontrol.ino:4981`);
    3. `CH=` from `satChEnableDecide()` via `satEnqueueIfChangedCH()` (`SATcontrol.ino:5223`, `SATchEnable.h:46-52`) (corrected 2026-10-24, TASK-1351);
    4. Immergas only: `TP=11:12=<setpoint, 0 decimals>`, sent on every tick, not write-on-change (`SATcontrol.ino:4986-4990`);
    5. `TC=<fTargetTemp>` via `satEnqueueIfChangedTC()` when `bPushSetpoint` is on (`SATcontrol.ino:4992-4994`).
    - Compare: thermo-nova sets CH from `control_setpoint > 22`; the firmware sets CH from the requested setpoint against 28.2 / 21 and the PWM phase (entries 7, 9, 11, 17) (corrected 2026-10-24, TASK-1351). Thermo-nova sends `TP=11:12=min(MM, 80)` for Immergas; the firmware sends the control setpoint in `TP=11:12`. Thermo-nova sends `PM=25` after every CS on the OTGW-MQTT path; no `PM=25` was found in the firmware (search of all `.ino`, `.h` and `.cpp` files).
30. **No command interface.** The PIC-failure counter grows. At 5 the safety trips (`SATcontrol.ino:4996-5005`, `SATcontrol.ino:69`). See G.
31. PID state save every 300 s, MQTT publish (`SATcontrol.ino:5007-5018`).

**C.7 Write-on-change.** CS, MM, CH and TC are sent only when the value changed (CS and TC compared at 0.1 °C), or when 300 s have passed since that command was last sent, or when the cache is invalid (`SATcontrol.ino:383-454`, `SATcontrol.ino:351`). The cache is invalidated at `initSAT()`, on the OT-bus online edge and in `satDisable()` (`SATcontrol.ino:3700`, `SATcontrol.ino:4533`, `SATcontrol.ino:2182`).

- Compare: thermo-nova re-sends CS, MM and CH on every 5 s tick with no change detection.

#### D. Per transport: what each write becomes

`addCommandToQueue()` routes by board (`OTGW-Core.ino:3617-3628`).

| Write | PIC board (serial queue) | OT-Direct board |
|---|---|---|
| `CS=v` (v > 0) | queued, sent to the PIC (`OTGW-Core.ino:3698-3705`, `OTGW-Core.ino:3745`) | MsgID 1 write with v as f8.8; refreshes the CS expiry timer (`OTDirect.ino:2753-2768`) |
| `CS=0` | queued, sent to the PIC | clears the MsgID 1 override; per the comment it does not touch the CH bit (`OTDirect.ino:2755-2760`) |
| `MM=v` | queued | MsgID 14 write (`OTDirect.ino:2887-2898`) |
| `CH=1` / `CH=0` | queued | sets or clears bit 0 of the master status byte (`OTDirect.ino:2995-3005`) |
| `SW=v` | queued | MsgID 56 write (`OTDirect.ino:2857-2871`) |
| `TC=v` | queued | MsgID 16 plus MsgID 100 manual-override priority (`OTDirect.ino:2774-2794`) |
| `TP=11:12=v` | queued | MsgID 11 index 12 write (`OTDirect.ino:3591-3620`) |
| `PM=id` | queued; consecutive `PM=` calls collapse into one slot (A.5) | poll MsgID now or one-shot READ (`OTDirect.ino:3369-3392`) |
| `MI=500` | queued | sets the OT-Direct message interval (`OTDirect.ino:3572-3582`) |
| `HW=0/1` (dhw_enable option) | queued with `forceQueue` true | bit 1 of the master status byte (`OTDirect.ino:2957-2994`) |

Transport rules that matter for SAT:

- **Ownership of the control setpoint.** While `settings.sat.bEnabled` is true, SAT owns MsgID 1 (`SATcontrol.ino:2150-2157`). Any `CS=` that does not carry SAT's in-flight marker is refused, on both paths (`OTGW-Core.ino:3654-3658`, `OTDirect.ino:2748-2752`). SAT sets the marker around its own CS sends (`SATcontrol.ino:375-379`). Bytes written directly to the PIC through the serial bridge do not pass this check (`OTGW-Core.ino:3650-3653`).
- **CS expiry.** On a PIC board the firmware re-sends the last CS every 30 s (B, C.9). On OT-Direct the 60 s CS heartbeat expiry is skipped while SAT owns the setpoint (`OTDirect.ino:2148-2153`, `OTDirect.ino:456`), and the OT-Direct heating-curve writer is paused (`OTDirect.ino:2191`).
- **Value formatting.** `CS=%.1f` (`SATcontrol.ino:390`), `MM=%u` (`SATcontrol.ino:421`), `TC=%.1f` (`SATcontrol.ino:448`), `SW=%d` with the setpoint truncated to an integer (`SATcontrol.ino:4645`), `TP=11:12=%.0f` (`SATcontrol.ino:4988`). The keepalive re-sends the stored CS, already rounded to 0.1 (`SATcontrol.ino:392`, `SATcontrol.ino:411`).
- **Errors.** `addCommandToQueue()` drops a command, with only a debug log, when no PIC or OT-Direct exists, when it is malformed or too long, or when the queue of 20 is full (`OTGW-Core.ino:3618-3649`, `OTGW-Core.ino:3679-3683`, `OTGW-Core.h:772`). A command not acknowledged after 5 sends is dropped and a `[dropped]` event goes to the web socket (`OTGW-Core.ino:3748-3754`). SAT is not told about either case. SAT's own safety counter only checks whether a command interface exists (`SATcontrol.ino:4978`, `SATcontrol.ino:4996-4997`).

#### E. The simulation option

When `settings.sat.bSimulation` is on:

- SAT reads room, outside, flow, return, flame and modulation from its synthetic model instead of the OT bus (`SATcontrol.ino:1195-1203`, `SATcontrol.ino:1286-1288`, `SATcontrol.ino:1325-1347`).
- Commands still go through `addCommandToQueue()` and SAT's own logic, but they are blocked at the last step before the bus: in `sendPICSerial()` on the PIC path (`OTGW-Core.ino:3830-3844`) and before the master transmit on OT-Direct (`OTDirect.ino:1359-1373`). The blocked command is logged and kept in a trace ring (`SATcontrol.ino:1361-1382`).
- On OT-Direct, `handleOTDirectCommand()` still updates its local state before the transmit is blocked (for example the master status bits for `CH=`, `OTDirect.ino:2995-3005`). Whether that local state matters during simulation was not traced.
- Simulation cannot be switched on while a real boiler is present (`settingStuff.ino:1060-1077`, `SATcontrol.ino:1412-1425`). If a real boiler appears while it is on, simulation is switched off and saved (`SATcontrol.ino:1442-1466`, `SATcontrol.ino:4525-4526`).
- Compare: thermo-nova skips the write on the MQTT, serial, ESPHome and switch paths but not on the serial unload release; in the firmware every SAT write, including `CS=0` from `satDisable()`, goes through the same blocked transmit path.

#### F. DHW handling

- **What SAT writes for DHW.**
  - On the control tick where DHW turns active, SAT sends `SW=<fDhwSetpoint>` once (`SATcontrol.ino:4640-4648`). Conditions: DHW was not active on the previous tick, `bDhwEnabled` is true, `fDhwSetpoint` is between 30 and 60 °C, and a command interface exists (`SATcontrol.ino:4640-4643`, `SATtypes.h:433-434`).
  - Defaults: `bDhwEnabled` false and `fDhwSetpoint` 0 (`SATtypes.h:457-458`). So with defaults no `SW=` is ever sent.
  - `SW=` is not repeated while DHW stays active, and nothing is sent when DHW ends (`SATcontrol.ino:4649`, `SATcontrol.ino:4653`).
  - The `dhw_enable` option (`bDhwEnable`, default true, `SATtypes.h:459`) sends `HW=1` or `HW=0` when its value changes, but only if the boiler reported MsgID 3 bit HB3 (storage tank) (`settingStuff.ino:1018-1031`). It is not tied to the control loop.
  - Compare: thermo-nova never writes a DHW setpoint from the control loop; it writes `SW` only from the user number entity.
- **Effect of DHW active on control.**
  - The control tick returns right after the DHW check (`SATcontrol.ino:4865-4882 [lines refreshed 2026-10-24, TASK-1351]`). No CS, MM or CH is written, no PID update runs, and the 300 s write-on-change refresh does not happen.
  - The last sent values stay in effect as far as the firmware is concerned. On a PIC board the CS keepalive keeps re-sending the last CS during DHW, because it runs before the timer gate (`SATcontrol.ino:4608`, `SATcontrol.ino:405-414`).
  - The comment says "DHW has priority, don't adjust CH setpoint, boiler manages itself" (`SATcontrol.ino:4636`).
  - Compare: thermo-nova keeps writing during DHW and sets MM to the configured maximum (HOT_WATER modulation state); the firmware writes nothing.
  - The PWM suppression refresh also stops while DHW is active, read directly from slave status bit 2 (`SATcontrol.ino:900-901`).
- **DHW guard of the sustained switches (TASK-1420).** `satCycleNoteDhwTick()` (`SATcycles.ino:199-202`, called at `SATcontrol.ino:5240`) refreshes the DHW end timestamp with the tick time on every DHW tick and clears the sustained starts; `satDhwGuardActive()` (`SATcycles.ino:164-167`, `SATcycles.ino:33`) blocks the continuous-to-PWM switch and resets the PWM exits during the draw and for 300 s after it. Before TASK-1420 the timestamp was refreshed inside a function the DHW tick never reached, so the guard never armed. Not checked on hardware.
- **DHW in monitoring.** The published modulation state is `HOT_WATER` while SAT is active and DHW is active (`SATcontrol.ino:3026-3028`). Power uses `fDhwCapacity` during DHW when it is above 0 (`SATcontrol.ino:3276`).

#### G. Limits and safety fallbacks (what the code actually does)

- **Effective maximum setpoint** `satEffectiveMaxSetpoint()` (`SATcontrol.ino:240-252`):
  - start from the boiler's `MaxTSet`; if it is below 30 (for example 0 before MsgID 57 arrives), use 75 (`SATcontrol.ino:242-243`, `SATcontrol.ino:57`);
  - cap at the system cap: 62 radiators, 45 underfloor, 40 for a heat-pump source (`SATcontrol.ino:228-235`);
  - cap at the hard ceiling: 80 radiators, 50 underfloor (`SATcontrol.ino:245-247`, `SATcontrol.ino:62-63`);
  - cap at `settings.sat.fMaxSetpoint`, or 65 if that is below 30 or above 80 (`SATcontrol.ino:248-250`, `SATcontrol.ino:64`). Default 65 (`SATtypes.h:514`). No settings key, MQTT command or settings-file entry for `fMaxSetpoint` was found (search for `fMaxSetpoint` and `SATmaxsetpoint` in all firmware files), so it stays 65.
  - Result with defaults: 62 °C for radiators (or less when the boiler reports a lower `MaxTSet`), 45 °C for underfloor.
  - Compare: thermo-nova uses the configured maximum (default 55 radiators, 50 underfloor) and ignores the boiler's `MaxTSet` for the clamp.
- **Where the clamps apply.**
  - `pidOutput` to [10, effective maximum] (`SATcontrol.ino:4911-4912`).
  - The final setpoint to [10, effective maximum] after the PWM or continuous branch (`SATcontrol.ino:4936-4937`). Compare: thermo-nova has no final clamp.
  - The PWM hold and suppression setpoints are clamped to the system cap only, before the final clamp (`SATcontrol.ino:879`, `SATcontrol.ino:701-717`).
  - The 3 s refresh clamps to the effective maximum (`SATcontrol.ino:907-909`).
  - Lower bound everywhere is `SAT_MIN_SETPOINT` = 10 (`SATcontrol.ino:54`).
- **Maximum setpoint versus PWM.** `pidOutput >= sysMax` (the system cap, not the effective maximum) leaves PWM for CONTINUOUS (`SATcontrol.ino:4909`, `SATcontrol.ino:4917-4920`), unless `bForcePWM` is on (`SATcontrol.ino:4923-4925`). (Removed in TASK-1332, 2026-10-23: the Force PWM option no longer exists.)
- **Open point (TASK-1364): solar gain during a loss.** `satUpdateSolarGain()` keeps its last state while the room temperature is NaN (it returns and publishes the held rise rate). Before TASK-1364 a non-NaN out-of-range value (a sensor fault such as 85 or -127) reached its rise-rate EMA; it is now skipped like NaN through `satRoomTempValid()`, so it cannot switch solar gain on. Whether solar gain should switch off during a loss is not decided.
- **Room temperature lost.** NaN or out of range: SAT keeps heating on the heating curve alone, P, I and D are 0, no trip and no time limit (TASK-1364, entries 38 and 46). The room low-pass filter, the MQTT-takeover thermal block (and its 2 h safe 45 °C setpoint), auto-tune, the curve recommendation, the daily median sample and a new PV boost are skipped while it lasts. When a valid reading returns, the output stays the curve alone (P, I and D 0) until the next 60 s PID step stores a real error, up to 60 s later. The integral restarts from 0. The first D after recovery is computed over the whole gap, from the last pre-loss temperature to the new one, as thermo-nova does (`pid.py:134-137` keeps `_last_temperature`, `pid.py:209-232`), so D need not start at 0. The status field `room_temp_lost` (REST `sat/status`, MQTT `sat/room_temp_lost`) is true while the loss lasts. The sources mark themselves stale as listed in Inputs; the stale time per source (entry 27) is still open.
- **OpenTherm lost (TASK-1365, entries 39 and 47).** "No valid OpenTherm" is no boiler reply (B frame) for 30 s. Any B frame counts: a Data-Invalid reply (OT-Direct bridges it as `B`, `OTDirect.ino:1510-1512`; the PIC prints it as a B line), an OT-Direct loopback answer and a replayed line, the same frames that set `bBoilerState`. processOT() stamps `state.otBus.iBoilerLastSeenMs` with `millis()` (never 0, one store) for every B frame (`OTGW-Core.ino:5244-5245`, `OTBustypes.h:44`); `time()` is not used because it jumps at NTP sync. Silence is counted from the later of the last B frame and the last pass on which SAT was disabled, a flash ran (`_sat_otLinkBaseMs`, `SATcontrol.ino:4904`), SAT was tripped (`SATcontrol.ino:4915`) or the check was exempt, and from SAT's first pass after boot (seeded in `satOtLinkStep()`), so a boiler that never answers counts as lost 30 s after SAT's first pass. On entering the loss, the loop task calls `satDisable()` once: `CS=0` through `satEnqueueOwnCS()`, `CH=1`, `MM=T`, mode OFF, PWM and PID reset, command cache reset. It then sets `state.sat.bOtLinkLost` and keeps the boiler stamp of that moment. SAT stays enabled and no setting is written. While the flag is set every pass returns before any sender: no `CS`, `MM`, `CH`, `TC`, `SW=` or `TP=`, no PIC keepalive and no PWM refresh. The first B frame after the hand-back (a stamp different from the kept one, so a `millis()` wrap after about 49.7 days of loss cannot fake a reply) clears the flag and resets the command cache (`satResetCmdCache()`), so the next control tick sends `CS`, `MM`, `CH` (and `TC=` with push setpoint on) again; the PID starts from 0 because `satDisable()` reset it. Exempt: SAT simulation (`satSimulationBlocksBusTx()` keeps every SAT command off the bus; a simulation started during a loss clears the flag) and PS=1 summary mode on a PIC (the PIC prints no B lines; the flag is held, so only a boiler reply resumes, and leaving PS=1 starts a fresh 30 s window). OT-Direct in PS=1 still bridges every B frame (`OTDirect.ino` `bridgeFrameToParser()`), so the check runs there. So on a PIC in PS=1 a loss is not detected. Without a command interface (no PIC, not OT-Direct; `SATcontrol.ino:2475`) the check is skipped as well, so the 5-tick interface safety trip (`SAT_MAX_PIC_FAILS`) still latches as before TASK-1365; a loss set earlier is held. During the loss SAT stays enabled, so `satOwnsControlSetpoint()` stays true: external `CS=` is still refused (ADR-179, TASK-1236) and the OT-Direct heating curve stays disarmed (to be named against entries 26 and 47 in Q34). `satDisable()` as the hand-back also resets PWM and the daily curve recommendation, writes the energy totals and persists the zeroed PID on every loss of 30 s or more, except a loss before the first room reading (`satPidRoomSeen()` false, entry 115) or before NTP time (`isNTPtimeSet()` false: zeros stamped with an unsynced time would make the NTP-gated restore discard the file; TASK-1365 AC#4): then `satDisable(false)` skips the PID file write, so the saved PID state stays on flash whether the NTP-gated `satLoadPidState()` has run yet or not. `satOtLinkStep()` reads `satPidRoomSeen()` before `satDisable()`, because `satPidReset()` clears it; every other `satDisable()` caller persists as before. A user switch-off during the loss (`settingStuff.ino:989`) and the MQTT-takeover lift (`satMqttFallbackStep()`) call `satDisable()`, which sees the flag (`SATcontrol.ino:2388`, `:2406-2410`) and skips `CS=0`, `CH=1` and `MM=T`, so the hand-back goes out once per loss; it still saves the PID and energy files and clears the flag. The loop's disabled branch also clears it. Status: `ot_link_lost` (REST `sat/status`, MQTT `sat/ot_link_lost`, v2 SAT page hint "OpenTherm lost: boiler handed back"). Entry 39 is an exception to entry 29: thermo-nova has no stop on a lost link. Decided: the 30 s definition with Data-Invalid as a reply is Q33 A (Robert, 2026-10-26), and the automatic resume on the first boiler reply is Q34 A (Sergeant D, entry 79); see `open-questions.md`.
- **Safety trip.** Sets `bSafetyTripped`, narrates "SAFETY TRIPPED", and calls `satDisable()` (`SATcontrol.ino:4678-4680`, `SATcontrol.ino:5000-5002`). `satDisable()` (`SATcontrol.ino:2160-2187`):
  - mode OFF, PWM reset, `bActive` false, final setpoint 0;
  - PID reset and saved as zeros; energy saved; daily recommendation reset;
  - sends `CS=0` through the owner path (`SATcontrol.ino:2179`);
  - invalidates the write-on-change cache.
  - It sends no `MM` release and no `CH=0`. On OT-Direct, `CS=0` leaves the CH bit as it was (`OTDirect.ino:2755-2758`).
  - While tripped, the pass returns before any write, including the PIC keepalive (`SATcontrol.ino:4581`, before `SATcontrol.ino:4608`).
  - The trip clears only on an explicit enable: MQTT/REST `enabled`, or a false-to-true change of the setting (`SATcontrol.ino:1941-1958`, `settingStuff.ino:988-991`). `initSAT()` also clears it at boot (`SATcontrol.ino:3692`).
  - SAT still owns the control setpoint while tripped, because ownership follows `bEnabled` (`SATcontrol.ino:2138-2157`). So an external `CS=` stays refused.
- **Command interface lost.** 5 consecutive control ticks without a PIC or OT-Direct trip the safety (`SATcontrol.ino:4996-5004`, `SATcontrol.ino:69`).
- **Boiler fault flag.** The tick is skipped with no write while slave status bit 0 is set (`SATcontrol.ino:4716-4719`). The last sent values stay. On a PIC board the CS keepalive continues (`SATcontrol.ino:4608`).
- **Outside temperature.** Out of range [-40, 50] gives 10 °C (`SATcontrol.ino:4721-4723`). No source gives `Toutside` = 0.0 (`OTGW-Core.h:82`). There is no write stop for a missing outside temperature. Compare: thermo-nova stops writing when the outside temperature is None.
- **MQTT lost (fallback).** See C.3. The fallback reason enum also has `SAT_FB_THERMOSTAT_LOST` (`SATtypes.h:60`), but no code sets it (search of all firmware files).
- **Thermal safe fallback.** Fixed `CS=45` after 2 h in fallback without room data (C.16).
- **Overheating.** No code was found that compares the flow temperature with a hard limit and cuts the burner. The only hard limits are the setpoint clamps above.
- **Pressure.** `satUpdatePressure()` computes an EMA (alpha 0.05), a least-squares drop rate over 30 s samples (at least 3 samples spanning 300 s, window 3600 s), and an alarm confirmed after 120 s (`SATcontrol.ino:3338-3483`). The drop rate is suspended for 600 s after SAT's own `bActive` changes (`SATcontrol.ino:3344-3354`). It sends no command; no `addCommandToQueue` or `satDisable` call exists in `SATpressure.ino` or in `satUpdatePressure()` (search). It runs only inside the control tick, after the DHW, room and fault returns (`SATcontrol.ino:4758-4759`). With no pressure reading it returns early and raises nothing (`SATcontrol.ino:3358`).
  - Compare: thermo-nova also raises an alarm when pressure stops arriving (`pressure_max_value_age`); no such timeout was found in the firmware.
- **Setpoint synchronisation.** A mismatch of more than 1.0 °C between the sent setpoint and the reported `TSet`, lasting 60 s, sets `bSetpointMismatch` (`SATcontrol.ino:3590-3613`). Display only; no command follows.
- **Manufacturer limits.**
  - Geminox: MM at least 10, also for MM=0 (`SATcontrol.ino:311`).
  - Immergas: MM at most 80, and the extra `TP=11:12` command (`SATcontrol.ino:312`, `SATcontrol.ino:4986-4990`).
  - Ideal, Intergas, Geminox, Nefit (`SAT_QUIRK_NO_REL_MOD`): only the modulation-reliability tracker is skipped; MM is still written (`SATcontrol.ino:127-132`, `SATcontrol.ino:4802-4805`).
  - Auto-detection takes the first table match, so member ID 4 gives Atag and 27 gives Immergas (`SATcontrol.ino:120-140`, `SATcontrol.ino:178-187`). Unknown IDs give OTHER (`SATcontrol.ino:188`).

<a id="s8-formulas"></a>

### Formulas

Variable names are as in the code.

- Effective maximum: `m = MaxTSet; if m < 30: m = 75; m = min(m, sysCap, hardMax, globalMax)` with `sysCap` = 45 underfloor or 62, then 40 for heat pump; `hardMax` = 50 underfloor or 80; `globalMax` = `fMaxSetpoint`, or 65 when outside [30, 80] (`SATcontrol.ino:228-252`).
- Cold setpoint: 21.0 underfloor, 28.2 otherwise (`SATcontrol.ino:264-268`).
- Written setpoint: `clamp(branch(pidOutput), 10, effectiveMax)`, where `pidOutput` already carries the solar offset, and 10 when `pidOutput <= coldSetpoint` (`SATcontrol.ino:5137-5183`, `SATcontrol.ino:5206-5214`) (corrected 2026-10-24, TASK-1351).
- Written CH: `satChEnableDecide(pidOutput, coldSetpoint, pwmDrivesSetpoint, pwmOn)`: 0 at or below the cold setpoint or in the PWM OFF phase, else 1 (`SATchEnable.h:46-52`, `SATcontrol.ino:5223`) (corrected 2026-10-24, TASK-1351).
- Written MM: `satComputeMM(pwmMode, heatPump, heatPumpOrHybrid, iMaxRelModulation, quirks)`, or 100 under the cold-setpoint cutoff (`SATcontrol.ino:304-314`, `SATcontrol.ino:4973`).
- PWM hold: `clamp(return + fFlameOffOffset, coldSetpoint, sysCap)` (`SATcontrol.ino:701-707`).
- PWM suppression: `clamp(flow - fModSupOffset, 10, sysCap)` (`SATcontrol.ino:711-717`).
- Continuous: `max(pidOutput, flow - fFlowOffset)` only when the flame is on and `pidOutput < flow <= 100` (`SATcontrol.ino:1161-1186`).
- Write-on-change test for CS: send if `round(new*10) != round(last*10)`, or `now - lastSentMs >= 300000`, or the cache is invalid (`SATcontrol.ino:383-388`).

<a id="s8-constants-and-defaults"></a>

### Constants and defaults

**Constants in code.**

| Name | Value | Unit | Citation | Used for |
|---|---|---|---|---|
| `SAT_MIN_SETPOINT` | 10.0 | °C | `SATcontrol.ino:54` | lower clamp, off setpoint |
| `SAT_COLD_SETPOINT_RAD` / `_FLOOR` | 28.2 / 21.0 | °C | `SATcontrol.ino:55-56` | cold cutoff, PWM hold floor |
| `SAT_MAX_SETPOINT_DEFAULT` | 75.0 | °C | `SATcontrol.ino:57` | used when `MaxTSet` < 30 |
| System cap | 62 radiators, 45 underfloor, 40 heat pump | °C | `SATcontrol.ino:231-233` | max setpoint |
| `SAT_HARD_MAX_RAD` / `_FLOOR` | 80 / 50 | °C | `SATcontrol.ino:62-63` | max setpoint |
| `SAT_GLOBAL_MAX_SETPOINT` | 65 | °C | `SATcontrol.ino:64` | fallback for `fMaxSetpoint` |
| `SAT_STALE_TEMP_BLE_MS`, `SAT_STALE_TEMP_MS` | 300000 | ms | `SATcontrol.ino:65-66` | BLE and thermostat `Tr` staleness |
| `SAT_STALE_OUTDOOR_MS` | 600000 | ms | `SATcontrol.ino:67` | external outdoor staleness |
| `SAT_MAX_SKIP_COUNT` | removed (TASK-1364) | | |
| `SAT_MAX_PIC_FAILS` | 5 | ticks | `SATcontrol.ino:69` | command-interface safety trip |
| `SAT_THERMAL_SAFE_FLOW` | 45.0 | °C | `SATcontrol.ino:82` | thermal safe fallback CS |
| `SAT_THERMAL_MAX_EST_MS` | 7200000 | ms | `SATcontrol.ino:83` | time before the safe CS |
| `SAT_CMD_REFRESH_MS` | 300000 | ms | `SATcontrol.ino:351` | write-on-change refresh |
| `SAT_PIC_CS_KEEPALIVE_MS` | 30000 | ms | `SATcontrol.ino:403` | PIC CS keepalive |
| Keepalive threshold | 8.0 | °C | `SATcontrol.ino:407` | CS below this is not kept alive |
| Suppression refresh | 3 | s | `SATcontrol.ino:480` | PWM CS refresh |
| MQTT loss before fallback | 300000 | ms | `SATcontrol.ino:4547` | auto-enable |
| Room temperature valid range | -10 to 50 | °C | `SATcontrol.ino:4669` | room temperature lost (curve only) |
| Outside temperature valid range, fallback | -40 to 50, 10 | °C | `SATcontrol.ino:4721-4723` | outside sanity |
| `SAT_DHW_SETPOINT_MIN` / `_MAX` | 30 / 60 | °C | `SATtypes.h:433-434` | `SW=` window, setting clamp |
| `SAT_DHW_OVERSHOOT_GUARD_MS` | 300000 | ms | `SATcycles.ino:33` | 300 s hot-water guard of the overshoot switch and the PWM exits (armed by `satCycleNoteDhwTick`, TASK-1420) |
| `SAT_PID_STALE_SEC` | 1800 | s | `SATcontrol.ino:1993` | PID restore |
| `OTGW_CMD_RETRY` / `OTGW_CMD_INTERVAL_MS` | 5 / 5000 | sends / ms | `OTGW-Core.ino:3588-3589` | PIC command queue |
| `SER2NET_QUIET_MS` | 2000 | ms | `OTGW-Core.ino:3590` | queue pause after bridge traffic |
| `CMDQUEUE_MAX` | 20 | entries | `OTGW-Core.h:772` | PIC command queue |
| `OT_CSC2_EXPIRY_MS` | 60000 | ms | `OTDirect.ino:456` | OT-Direct CS heartbeat (skipped while SAT owns CS) |
| Pressure EMA alpha; confirm; drop-rate suspend; min window; sample; eviction | 0.05; 120; 600; 300; 30; 3600 | -; s; s; s; s; s | `SATcontrol.ino:3366`, `:3467`, `:3349`, `:3414`, `:3371`, `:3383` | display |
| Setpoint sync tolerance; delay | 1.0; 60 | °C; s | `SATcontrol.ino:3600`, `:3607` | display |
| `MI=` value for quirk | 500 | ms (OT-Direct range 100 to 1275) | `SATcontrol.ino:3731`, `OTDirect.ino:3574-3575` | boot |
| Manufacturer member IDs | Atag 4, Baxi 4, Brotge 4, DeDietrich 4, Ferroli 9, Geminox 4, Ideal 6, Immergas 27, Intergas 173, Itho 29, Nefit 131, Radiant 41, Remeha 11, Sime 27, Vaillant 24, Viessmann 33, Worcester 95, Other 0 | - | `SATcontrol.ino:120-140` | detection |

**Configuration options and their defaults** (struct defaults in `SATtypes.h`, accepted range in `updateSetting()` in `settingStuff.ino`, MQTT command in `MQTTstuff.ino`).

| Setting (settings key) | Default | Unit | Accepted range | Citation of default | Citation of range | MQTT `sat/` command |
|---|---|---|---|---|---|---|
| `bEnabled` (`SATenabled`) | false | - | bool | `SATtypes.h:437` | `settingStuff.ino:981-992` | `enabled` (`MQTTstuff.ino:664`) |
| `iHeatingSystem` (`SATsystem`) | AUTO (= radiators) | - | 0-2 | `SATtypes.h:438`, `SATcontrol.ino:198-199` | `settingStuff.ino:994` | `heating_system` |
| `iHeatingSource` (`SATsource`) | AUTO (= gas boiler) | - | 0-3 | `SATtypes.h:439`, `SATcontrol.ino:211-212` | `settingStuff.ino:995` | none found in `kSatMqttCmds` |
| `iControlInterval` (`SATinterval`) | 30 | s | 10-300 | `SATtypes.h:444` | `settingStuff.ino:1000-1003` | `interval` |
| `fTargetTemp` (`SATtargettemp`) | 20.0 | °C | 5-30 | `SATtypes.h:441` | `settingStuff.ino:997` | `target` (handler) |
| `iMaxRelModulation` (`SATmaxmodulation`) | 100 | % | 0-100 | `SATtypes.h:453` | `settingStuff.ino:1012` | `max_modulation` |
| (removed in TASK-1332) `bForcePWM` (`SATforcepwm`) | false | - | bool | `SATtypes.h:465` | `settingStuff.ino:1038` | `force_pwm` |
| `bOvershootProtection` (`SATovershootprotection`) | **true** | - | bool | `SATtypes.h:466` | `settingStuff.ino:1039` | `overshoot_protection` |
| `fFlameOffOffset` (`SATflameoffset`) | 18.0 | °C | 0-30 | `SATtypes.h:462` | `settingStuff.ino:1034` | `flame_off_offset` |
| `fModSupDelay` (`SATmodsupdelay`) | 20.0 | s | 0-120 | `SATtypes.h:455` | `settingStuff.ino:1014` | `mod_sup_delay` |
| `fModSupOffset` (`SATmodsupoffset`) | 1.0 | °C | 0-5 | `SATtypes.h:456` | `settingStuff.ino:1015` | `mod_sup_offset` |
| `fFlowOffset` (`SATflowoffset`) | 2.0 | °C | 0.5-10 | `SATtypes.h:467` | `settingStuff.ino:1040` | `flow_offset` |
| `fOvershootMargin` (`SATovershootmargin`) | 2.0 | °C | 0.5-5 | `SATtypes.h:454` | `settingStuff.ino:1013` | `overshoot_margin` |
| `iCyclesPerHour` (`SATcyclesperhour`) | 3 | 1/h | 2-6 | `SATtypes.h:525` | `settingStuff.ino:1105` | `cycles_per_hour` |
| `fMaxSetpoint` | 65.0 | °C | no setter found | `SATtypes.h:514` | - | none |
| `fDhwSetpoint` (`SATdhwsetpoint`) | 0 (inactive) | °C | 0-60; `SW=` only for 30-60 | `SATtypes.h:457` | `settingStuff.ino:1016`, `SATcontrol.ino:4641-4642` | `dhw_setpoint` |
| `bDhwEnabled` (`SATdhwenabled`) | false | - | bool | `SATtypes.h:458` | `settingStuff.ino:1017` | `dhw_enabled` |
| `bDhwEnable` (`SATdhwenable`) | true | - | bool; `HW=` only with MsgID 3 HB3 | `SATtypes.h:459` | `settingStuff.ino:1018-1031` | `dhw_enable` |
| `bPushSetpoint` (`SATpushsetpoint`) | false | - | bool | `SATtypes.h:460` | `settingStuff.ino:1032` | `push_setpoint` |
| `bFollowThermostat` (`SATfollowthermostat`) | false; equals `bPushSetpoint` when the key is absent from the file | - | bool | `SATtypes.h:461`, `settingStuff.ino:607-610` | `settingStuff.ino:1033` | `follow_thermostat` |
| `iManufacturer` (`SATmanufacturer`) | AUTO | - | 0-18 | `SATtypes.h:472` | `settingStuff.ino:1044` | `manufacturer` |
| `bSimulation` (`SATsimulation`) | false | - | bool, refused with a boiler present | `SATtypes.h:488` | `settingStuff.ino:1060-1077` | `simulation` |
| `iSensorMaxAgeS` (`SATsensormaxage`) | 21600 | s | 60-86400 | `SATtypes.h:515` | `settingStuff.ino:1101` | `sensor_max_age` |
| `bUseExternalTemp` (`SATexternaltemp`) | false | - | bool | `SATtypes.h:445` | `settingStuff.ino:1004` | `external_temp` |
| `bErrorMonitoring` (`SATerrormon`) | false | - | bool | `SATtypes.h:516` | `settingStuff.ino:1102` | `error_monitoring` |
| `fMinPressure` / `fMaxPressure` | 0.8 / 2.5 | bar | 0-3 / 1-4 | `SATtypes.h:469-470` | `settingStuff.ino:1041-1042` | `min_pressure`, `max_pressure` |
| `fMaxPressureDrop` (`SATmaxpressdrop`) | 0.3 | bar/h | 0.05-2 | `SATtypes.h:471` | `settingStuff.ino:1043` | `max_pressure_drop` |
| `fBoilerCapacity` / `fDhwCapacity` | 24.0 / 0.0 | kW | 1-100 / 0-100 | `SATtypes.h:479-480` | `settingStuff.ino:1054-1055` | `boiler_capacity`, `dhw_capacity` |
| `fValveOffset` (`SATvalveoffset`) | 0.0 | °C | -1 to 1 | `SATtypes.h:526` | `settingStuff.ino:1106` | `valve_offset` |
| `fTargetTempStep` (`SATtempstep`) | 0.5 | °C | 0.1-1 | `SATtypes.h:468` | `settingStuff.ino:1037` | `target_temp_step` |
| `fHeatingCurveCoeff` (`SATcoefficient`) | 1.5 | - | 0.1-5 | `SATtypes.h:442` | `settingStuff.ino:998` | `heating_curve` |
| `iHeatingMode` (`SATheatingmode`) | 0 (comfort) | - | 0-1 | `SATtypes.h:524` | `settingStuff.ino:1104` | `heating_mode` (handler) |
| `fAutoGainsValue` (`SATautogains`) | 2.0 | - | 0.1-10 | `SATtypes.h:517` | `settingStuff.ino:1103` | `auto_gains_value` |
| presets comfort / eco / away / sleep / activity / home | 21 / 18 / 15 / 16 / 10 / 18 | °C | see citations | `SATtypes.h:446-451` | `settingStuff.ino:1005-1010` | `preset_*` |
| `bSummerSimmer` (`SATsummersimmer`) | false | - | bool | `SATtypes.h:499` | `settingStuff.ino:1086` | `summer_simmer` |
| `bSolarGainEnable` / `fSolarSetpointOffset` | false / 2.0 | - / °C | bool / 0.5-10 | `SATtypes.h:494`, `SATtypes.h:496` | `settingStuff.ino:1082`, `settingStuff.ino:1084` | `solar_gain` |
| `bThermalComfort` (`SATthermalcomfort`) | false | - | bool | `SATtypes.h:522` | `settingStuff.ino:1107` | `thermal_comfort` |

Compare (defaults that differ from thermo-nova): overshoot protection true here, false there; cycles per hour 3 here, 4 there; heating-curve coefficient 1.5 here, 2.0 there; overshoot margin 2.0 here, 3.0 there; maximum setpoint 62/45 here (via caps), 55/50 there; control interval 30 s here, 5 s there; presets comfort 21 / sleep 16 here, 20 / 15 there.

<a id="s8-state-kept-between-passes"></a>

### State kept between passes

- Write-on-change cache: `bLastSentValid`, last CS, MM, CH, TC and their send times (`SATtypes.h:390-405`, `SATcontrol.ino:356-362`). In RAM only.
- Safety: `bSafetyTripped`, `_sat_roomLostActive` (narration only), `_sat_picFailCount`, `_sat_bootCS0sent` (`SATtypes.h:233`, `SATcontrol.ino:340-342`). In RAM only; reset at boot (`SATcontrol.ino:3692-3695`).
- DHW transition: `_sat_prevDhwActive` (`SATcontrol.ino:343`).
- OT-bus online edge: `_sat_prevOTOnline` (`SATcontrol.ino:344`).
- Fallback: `bFallbackActive`, `eFallbackReason` (`SATtypes.h:235-236`). The fallback sets `bEnabled` in RAM without saving (`SATcontrol.ino:4551`).
- PWM: previous control setpoint `_sat_lastControlCS`, hold setpoint, ignition wait (`SATcontrol.ino:686-694`). Reset by `satDisable()` and mode changes (`SATcontrol.ino:2165-2167`, `SATcontrol.ino:934-938`).
- Detected manufacturer and member ID, from MsgID 3 (`SATcontrol.ino:175-190`).
- Persisted to LittleFS: PID state every 300 s and on disable (`SATcontrol.ino:1995-2006`, `SATcontrol.ino:5010-5012`, `SATcontrol.ino:2171`); energy totals (`SATcontrol.ino:2055-2065`, `SATcontrol.ino:2088-2098`). The control setpoint and modulation are not persisted; after a reboot SAT starts with `CS=0` and recomputes (`SATcontrol.ino:3706-3710`).
  - Compare: thermo-nova restores the last control setpoint and modulation from the climate state at start-up.
- Settings are persisted through the settings file (`settingStuff.ino:315-433`).

<a id="s8-outputs-to-the-boiler-to-monitoring"></a>

### Outputs (to the boiler, to monitoring)

**To the boiler.**

- Boot: `CS=0` always; `PM=3`, `PM=15`, `PM=48` and optionally `MI=500` when enabled (Behaviour A).
- Each control tick that reaches the end: `CS`, `MM`, `CH` (write-on-change), `TP=11:12` for Immergas, `TC` if push is on (`SATcontrol.ino:4978-4994`).
- Special paths: summer simmer and valves closed (`CS=10`, `MM=max`, `CH=0`); thermal safe fallback (`CS=45`, `MM=max`, `CH=1`) (`SATcontrol.ino:4738-4743`, `SATcontrol.ino:4768-4773`, `SATcontrol.ino:4781-4786`).
- Between ticks: PIC CS keepalive every 30 s; PWM suppression CS every 3 s (`SATcontrol.ino:4608`, `SATcontrol.ino:4611`).
- DHW entry: `SW=` once, if configured (`SATcontrol.ino:4640-4648`).
- Option change: `HW=0/1` from `dhw_enable` with a storage-tank boiler (`settingStuff.ino:1024-1029`).
- Disable, safety trip, fallback lifted: `CS=0` (`SATcontrol.ino:2179`).

**To monitoring.** MQTT publish at the end of the tick and of the special paths (`SATcontrol.ino:5018`, `SATcontrol.ino:4650`, `SATcontrol.ino:4744`, `SATcontrol.ino:4774`, `SATcontrol.ino:4788`), and the core-state publish after every pass (`SATcontrol.ino:4504-4508`). Examples: `sat/dhw_setpoint`, `sat/dhw_enabled`, `sat/pressure_alarm` (`SATcontrol.ino:3050`, `SATcontrol.ino:3168`, `SATcontrol.ino:2884`), and the modulation state string (`SATcontrol.ino:3022-3035`). The full topic list belongs to another section.

<a id="s8-edge-cases"></a>

### Edge cases

- **`PM=3`, `PM=15`, `PM=48` reach a PIC one at a time at boot** (TASK-1315). See A.5 (`OTGW-Core.ino:3895-3982`, `SATcontrol.ino:3946-3948`) (corrected 2026-10-24, TASK-1351): before TASK-1315 only `PM=48` reached a PIC.
- **First MM=0 or CH=0 within 300 s of boot can be skipped.** `satResetCmdCache()` sets the send times to 0 but keeps the last values (`SATcontrol.ino:356-362`), and those start at 0 (`SATtypes.h:397-398`). The first CS send sets `bLastSentValid` true (`SATcontrol.ino:394`). The MM and CH checks then see "valid, same value, not stale" while `millis()` is below 300000, and return without sending (`SATcontrol.ino:418-419`, `SATcontrol.ino:432-433`). So a first commanded `MM=0` or `CH=0` in the first 5 minutes of uptime is not sent until the refresh. From reading the code; not checked on hardware.
- **CH follows `satChEnableDecide`.** CH=0 when the requested setpoint is at or below 28.2 / 21 or in the PWM OFF phase, otherwise CH=1, the PWM ON phase included whatever CS keeps the flame on (`SATchEnable.h:46-52`, `SATcontrol.ino:5223`). Compare: thermo-nova gives CH=0 for a control setpoint of 22 or less (entries 7, 9, 11, 17) (corrected 2026-10-24, TASK-1351).
- **All off paths set MM=max.** Cold cutoff: `iMaxRelModulation` (`SATcontrol.ino:1108`). Summer simmer and valves closed: `iMaxRelModulation` (`SATcontrol.ino:5001`, `SATcontrol.ino:5015`) (corrected 2026-10-24, TASK-1351); TASK-1327. Raw value: the entry-21 guards (heat pump/hybrid never 0, Geminox at least 10) do not apply on these paths. Open, see not-found item 15.
- **PWM with overshoot protection off** keeps the previous setpoint while MM is still 0 in PWM (`SATcontrol.ino:872-874`, `SATcontrol.ino:308`). The firmware default is on (`SATtypes.h:466`).
- **Immergas `TP` is sent every tick**, outside the write-on-change cache (`SATcontrol.ino:4984-4990`).
- **Fallback re-enable does not send the boot `PM=` commands** (`SATcontrol.ino:4549-4552`; `PM=` only in `SATcontrol.ino:3725-3733`).
- **Safety trip keeps ownership.** External `CS=` stays refused while tripped, because ownership follows `bEnabled` (`SATcontrol.ino:2150-2157`, `OTGW-Core.ino:3654-3658`).
- **No CS keepalive while tripped or disabled.** The keepalive is after the enabled and trip returns (`SATcontrol.ino:4568-4581`, `SATcontrol.ino:4608`). After the `CS=0` release this is consistent.
- **Weather feed is used only when `Toutside` is exactly 0.0** (`SATcontrol.ino:1299`). A real outdoor reading of 0.0 °C from the bus would also be replaced.
- **Changing `SATinterval` resets the timer** to one full new interval from now (`settingStuff.ino:1000-1003`, `safeTimers.h:131-135`).

<a id="s8-not-found-in-the-code-unclear"></a>

### Not found in the code / unclear

1. **What the PIC or boiler does with MM while SAT sends nothing (DHW, fault, trip).** The firmware does not re-send MM in those states (C.11, C.14). Whether the PIC keeps or expires an MM override was not checked in PIC code.
2. **Conflict on `CS=0` and the CH bit.** The OT-Direct handler comment says `gateway.asm` never changes bit 0 on `CS=0` (`OTDirect.ino:2756-2758`). The OTGW command documentation says `CS=0` clears the CHenable bit (`other-projects/OTGW-documentation/OTGW-Commands.md:492`). The two sources disagree; `gateway.asm` itself was not read for this section.
3. **PIC CS expiry window.** The firmware comment says about 63 to 126 s (`SATcontrol.ino:398-402`); the OTGW documentation says a CS of 8 or more must be repeated "at least every minute" (`other-projects/OTGW-documentation/OTGW-Commands.md:493`). Not checked in `gateway.asm`.
4. **`SAT_FB_THERMOSTAT_LOST`** is declared (`SATtypes.h:60`) but never assigned (search of all firmware files).
5. **`fMaxSetpoint`** has no settings key, MQTT command or file entry (search for `fMaxSetpoint` and `SATmaxsetpoint`), so the "global" ceiling is fixed at its default 65 (`SATtypes.h:514`).
6. **No `PM=25`** after CS was found in the firmware (search). Thermo-nova sends it on its OTGW-MQTT path without a stated reason.
7. **No hard overheat cutoff and no frost protection** were found in the SAT code. Only the setpoint clamps and the cold cutoff exist (Behaviour G). A full search for frost logic in SAT files was not done beyond the files read here.
8. **`iHeatingSource` has no MQTT command** in `kSatMqttCmds` (`MQTTstuff.ino:655-747`). Whether REST exposes it was not checked.
9. **Boot `PM=` collapse and the first-MM/CH skip** (Edge cases) are derived from reading the code. Neither was reproduced on a bench.
10. **Effect of OT-Direct local state changes during simulation** (Behaviour E) was not traced.
