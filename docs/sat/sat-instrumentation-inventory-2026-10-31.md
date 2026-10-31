# SAT log and decision inventory (TASK-1521, 2026-10-31)

Taken from the alpha.568 source (`a6022396e`) before the TASK-1521 changes; line numbers refer to that commit.
Read-only inventory by a delegated reader, spot-checked (SATcontrol.ino:257, :2099, SATcycles.ino:1170 caller, SATpid.ino:115).
Level = what TASK-1521 does with it: LOW stays on key `5`, TRACE moved to key `8`.

## 1. SAT telnet log calls with a changed level (moved to TRACE)

| file:line | text | frequency |
|---|---|---|
| SATcontrol 2099 / 2103 | `SAT: thermal_comfort: raw=` / `no fresh indoor humidity` | several times per loop pass with thermal comfort on |
| SATcontrol 2258 | `%s trace: %s (src=%S)` | each blocked bus TX in simulation, about 10/s |
| SATcontrol 2389, 2406, 2507, 2524, 2553, 2581, 2680, 2699 | external indoor / PV surplus / external outdoor / humidity / sun elevation / area / zone value set | each MQTT or REST push |
| SATcontrol 2565 | `SAT: area %u temp set to %.1f (sensor)` | each BLE / Dallas reading |
| SATcontrol 2858 | `SAT: zones %u counted` | every tick with zones |
| SATcontrol 3160 | `SAT: estimated energy saved` | each 0.1 kWh |
| SATcontrol 3922 | `SAT: publishing MQTT state` | each tick, every 500 ms on retry |
| SATcontrol 5047 / 5054 | `SAT SIM: flame ON / OFF` | each simulated flame edge (duplicates the cycle lines) |
| SATcontrol 5871 | `SAT loop: room= target= mode= enabled=` | each tick (fields repeated in SATTICK) |
| SATcycles 469 / 550 | `SAT 4h:` / `SAT 24h:` stats | every 60 s |
| SATcycles 893 | `SAT cycle classify:` detail | each flame off |
| SATcycles 1035 | `SAT sample: flow=` | every 30 s while the flame burns |
| SATcycles 1347 | `SAT HCR: sample err=` | every tick |

Kept LOW (event-driven or once per boot): manufacturer detection (one line per 45 min in gruijter's capture), MaxTSet/SW=/MI=,
preset and target changes, follow-thermostat adopt, window, sensor stale, PV boost, PID / energy / HCR load and save, disable,
control mode command, pressure alarm, simulation events, thermal learning, solar gain, summer simmer, boiler fault skip (every
tick while the fault bit is set), PWM schedule per tick in PWM, PWM timeouts and limits, overshoot / underheat / saturation
switches, cycle flame ON/OFF and the cycle verdict, the end-of-tick summary (now `SATTICK`). SATpid 115 (`PID integral and raw
derivative reset`) was an ungated DebugTln and is now under key `5`. `satCycleCountClass` (SATcycles 1170) has no caller.

## 2. Decision points that logged nothing before TASK-1521, and what logs them now

| decision | location | now |
|---|---|---|
| Boot decision timing (valid / hand-back verdict existed as text) | satRestartStep 3410-3424 | `SATBOOT phase=decide` with ms, verdict, PID load, NTP, push |
| Takeover: first pass through all gates | 3451-3454 | `SATBOOT phase=takeover` with ms |
| TC= push | satEnqueueIfChangedTC 638-650 | `SATTC push=` |
| Push refused verdict | satPushRefusedDecide via 1888 | `SATTC refused=` on each change |
| Follow thermostat: first value, echo of TC=, no TC= since boot (recorded, not adopted) | satFollowThermostatDecide 1845-1853 | `SATFOLLOW recorded=` |
| All control mode changes (heater-off PWM disable, satDisable included) | satSetControlMode 1429-1445 | `SATMODE from= to=` + narration "Control mode: ..." |
| DHW start / end outside a PWM ON phase | 5847-5864 | `SATDHW start/end` + narration |
| CS, MM, CH, cold cutoff, max/min clamp per tick | 6099-6149 | values in every `SATTICK` (cs, mm, ch, pid, mode, pwm) |

Still without a dedicated line (visible only through SATTICK values or not at all): boot waits for NTP / for a command interface /
for the first boiler reply (the takeover `ms` shows the total wait), OT link exemptions, OT bus offline-to-online cache reset,
thermal-safe fallback, the per-tick summer-simmer and valves-closed skips, the no-return-temperature flood handover, the solar
offset on the requested setpoint, the safety-tripped and disabled early returns.

## 3. Boot milestones

| milestone | where | logged as |
|---|---|---|
| Restored-state verdict | satLoadPidState 3017-3070 via satRestartNotePidLoad 3398 | existing `SAT: PID state ...` lines + `pidload` in SATBOOT decide |
| Boot decision | satRestartStep 3412 | `SATBOOT phase=decide ms=` |
| First TC= | satEnqueueIfChangedTC (boot call since TASK-1520) | `SATTC push=` (device time) |
| Valid outside temperature + first boiler reply | satRestartStep 3431-3432 | implied by `SATBOOT phase=takeover ms=` |
| First CS= | satSendCS 557 via the first tick | the first `SATTICK` line |
