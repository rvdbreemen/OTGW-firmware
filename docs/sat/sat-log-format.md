# SAT log format and levels (TASK-1521)

SAT logs on two telnet levels and in the SAT narration log.

| Level | Telnet key | Default | What it prints |
|---|---|---|---|
| SAT (low volume) | `5` | on | decisions, one `SATTICK` line per control tick (30 s), boot milestones, event records below |
| SAT trace (high volume) | `8` | off | per-pass and per-value detail: loop start line, MQTT publish passes, thermal-comfort room value, sensor and MQTT value pushes, zone values, 4 h / 24 h stats, cycle samples and classifier detail, HCR samples, simulation flame edges |
| Narration (SAT log in the web UI) | none (always on) | on | plain-language decisions: restart and hand-back, outside temperature missing or back, OT link lost or back, control mode changes, PWM ON / OFF, hot water start / end, room temperature lost or back, safety |

Both telnet keys are toggles. The REST debug dump (`/api/v2/debug`) shows `state.debug.sat` and `state.debug.sat_trace`.

## Record lines (low volume, machine-readable)

Every line starts with the device time `HH:MM:SS.uuuuuu` and the usual debug prefix. The record is `KIND key=value ...`.

| Record | When | Keys |
|---|---|---|
| `SATTICK` | every control tick that reaches the end of the tick (not during hot water, a boiler fault or the summer / valves-closed shortcuts) | `room` filtered room °C, `target`, `out` outside, `curve`, `p`, `i`, `d`, `pid` (= curve + P + I + D), `cs` final control setpoint, `mm` max modulation sent, `ch` CH enable sent, `mode` 0 off / 1 continuous / 2 PWM, `pwm` 1 when this tick's CS comes from the PWM ON phase, `duty`, `flame`, `flow`, `ret` |
| `SATBOOT phase=decide` | once per boot, at the restart decision | `ms` since boot, `valid` restored state usable, `pidload` (0 none, 1 restored, 2 waiting for NTP), `ntp`, `tc` push setpoint on |
| `SATBOOT phase=takeover` | once per boot, first pass through all gates | `ms` since boot, `since_sat_start_ms` |
| `SATMODE` | every control mode change | `from`, `to` |
| `SATTC push=` | every TC= push to the thermostat | pushed value |
| `SATTC refused=` | when the push-refused verdict changes | `refused`, `thermostat`, `pushed` |
| `SATFOLLOW` | a new thermostat setpoint recorded but not adopted (first value, echo of SAT's TC=, or no TC= since boot) | `recorded`, `adopted=0`, `last_tc`, `push` |
| `SATDHW start` / `SATDHW end` | hot water starts or ends (SAT pauses heating control) | `event` column in the CSV |

An adopted thermostat setpoint keeps its existing line `SAT: follow thermostat: target ...`.

## Turning a capture into CSV

```
python scripts/sat_log_to_csv.py <transcript.txt> --out-dir <dir>
```

Writes `<transcript>-sattick.csv`, `-satboot.csv`, `-satmode.csv`, `-sattc.csv`, `-satfollow.csv`, `-satdhw.csv`, one row per line and the keys as columns. `tests/test_sat_log_to_csv.py` checks that the firmware's format string and the parser agree.

## Inventory (alpha.568 source, before TASK-1521)

The full inventory of SAT log calls and decision points (which level each call got, which decisions logged nothing) is in the TASK-1521 task notes. Decisions that still log only through `SATTICK` values: the MM decision, the CH rule, the cold-setpoint cutoff and the max/min setpoint clamp (their result is in `cs`, `mm` and `ch` of every tick).
