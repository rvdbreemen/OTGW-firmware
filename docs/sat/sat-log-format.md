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
| `SATTUNEH` | every tuner hour (heating curve self-tune, ADR-205; 3600 s blocks on the device clock, 60 s minimum in simulation) | `hour` counter since the tuner started, `mean` mean signal of the hour in C (`na` without a sample), `ticks` control ticks seen, `counted` 1 when the hour is clean, `reason` first exclusion that applied: `none`, `not_sat`, `dhw`, `no_sun`, `sun`, `pwm`, `flame_off`, `min_mod`, `coeff_changed`, `no_ticks`, `not_controlling` (a control path returned early: boiler fault, thermal-safe fallback, summer simmer, valves closed, OT link lost, restart hold, MQTT fallback, safety trip), `few_ticks` (fewer than 75 % of the expected ticks, hour length / control interval), `no_room` (only when `SAT_CT_EXCLUDE_ROOM_LOST` is built as 1; default 0: hours with a lost room temperature still count, pending Sergeant D Q-1525a). `reason` is the first exclusion in time |
| `SATTUNED` | every 24th tuner hour | `day`, `clean` clean hours, `counted` 1 with at least 6 clean hours, `days_in_window` counted days the decision used, `mean3` hour-weighted mean of the clean hours of the last 3 counted days (`na` before 3), `decision` `raise` / `lower` / `hold`, `reason` `day_not_counted`, `few_days`, `inside_band`, `at_limit` or `outside_band` |
| `SATTUNER` | once per boot, when the tuner first starts outside simulation | `restored` counted days taken back from `/sat_tune.json` (0 = none: no file, or it belongs to another coefficient), `coeff` |
| `SATTUNE` | the self-tune changed the coefficient (at most once a day) | `old`, `new` coefficient, `mean3` that caused it. The same change is also a narration line: "Heating curve self-tune: coefficient 2.0 -> 2.1 (3-day mean +1.50 C above the curve)" |

Self-tune window file (`/sat_tune.json`, Sergeant D log entry 183): the counted days of the window (clean hours and signal sum per day) and the coefficient they belong to, written once a day at the daily decision (removed when the window is empty). It is restored once per boot only when its coefficient equals the current one; a manual coefficient change, switching the tuner off, or leaving simulation removes it. Simulation never reads or writes it. The partial counters of the current day are not kept. In simulation with short hours (`tune_hour`), set the control interval to 10 s so an hour still reaches 75 % of its expected ticks.

Self-tune signal (TASK-1525): per control tick `clamp(praw, 10, system maximum) - curve`, where `praw` is the SATTICK field (curve + P + I + D before zones, solar offset and the MaxTSet clamp). The SATTICK `pid` field is not used. A hot-water tick is not a SATTICK line but is noted by the tuner as `dhw`.

An adopted thermostat setpoint keeps its existing line `SAT: follow thermostat: target ...`.

## Turning a capture into CSV

```
python scripts/sat_log_to_csv.py <transcript.txt> --out-dir <dir>
```

Writes `<transcript>-sattick.csv`, `-satboot.csv`, `-satmode.csv`, `-sattc.csv`, `-satfollow.csv`, `-satdhw.csv`, `-sattuneh.csv`, `-sattuned.csv`, `-sattune.csv`, one row per line and the keys as columns. `tests/test_sat_log_to_csv.py` checks that the firmware's format string and the parser agree.

## Inventory (alpha.568 source, before TASK-1521)

The full inventory of SAT log calls and decision points (which level each call got, which decisions logged nothing) is in the TASK-1521 task notes. Decisions that still log only through `SATTICK` values: the MM decision, the CH rule, the cold-setpoint cutoff and the max/min setpoint clamp (their result is in `cs`, `mm` and `ch` of every tick).
