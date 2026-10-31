# TASK-1527 round 14: a third heat-pump PWM option of 120 minutes (Sergeant D's request)

Date 2026-10-31. Sergeant D, #dev-sat-mqtt 14:46 UTC (msg 1566100973019996253): "explore the possibility to add a
3rd option for heat pumps a) 2x hour b) 1.5x hour c) 0.5x hour. This will give a third option with PWM of 120
minutes. Run some tests and see if the PWM mechanism is not braking"; 14:47 UTC (msg 1566101327644459050): "as far as
I remember the code the min cycles we can go is 1x/ hour".

## 1. His memory of the code: confirmed

thermo-nova `pwm.py:54`: `self._on_time_upper_threshold: float = 3600 / max(1, self._config.cycles_per_hour)`, so
the longest period is 3600 s (1 per hour); 0.5 per hour would be clamped to 1. For heat pumps the options flow offers
only 2 or 3 per hour (`config_flow.py:686-692`). The firmware offers heat pumps 1800 s or 2400 s
(`SATcontrol.ino:440-446`, `settingStuff.ino:1048`). A 7200 s option is a deviation from thermo-nova, which entry 182
allows for heat pumps.

## 2. Does the PWM mechanism break at 7200 s? Tested on the real firmware code

`round14/test_hp7200_scratch.py` runs the existing real-code suite `tests/test_sat_hp_pwm.py` (it cuts the getters,
the PWM block and the setters out of SATcontrol.ino / settingStuff.ino and compiles them with a host C++ compiler) on
copies patched with a third option 7200 s (period getter and setter only), plus 9 cases at 7200 s whose expected
values follow from thermo-nova's duty formula (`pwm.py:247-301`, P = 7200: lower 0.025, upper 0.975):

- duty 0.60 -> 4320 / 2880 s, 0.30 -> 2160 / 5040 s, 0.02 -> 180 / 8820 s, 0.98 -> 8820 / 180 s, 0.01 with flame
  -> 180 / 14220 s, without flame -> 0 / 14400 s, 0.99 -> 14400 / 0 s; the ON phase ends after its on time
  (duty 0.30, flame on 2200 s) and holds before it (1000 s).
- Unmodified firmware: the 7200 s cases fail (2 of 2 run). Patched copies: all 9 pass; 26 of the 27 existing tests
  pass, gas included. The one failure is `test_setter_snaps_to_two_options` (9999 now snaps to 7200 instead of
  2400), the intended contract change of a third option, not a break.
- Other uses of the period: only the duty mapper (`SATcontrol.ino:1179`). `iHpCycleSeconds` is a uint16_t (7200
  fits). An implementation must also widen the REST metadata (`restAPI.ino:3868`, max 2400) and the UI choices.
- Not covered by the harness: hours of repeated cycles, and the exits (underheat 180 s, saturation 300 s). Those
  timers do not depend on the period; their behaviour over hours is in the simulation below.

## 3. Simulation (script `scripts/sat_hp_period_study14.py`, data `round14_hp_period_runs.json`, 360 winters)

SAT PWM as today (ON time fixed at the start of each period, minimum ON 180 s), three winters (2010-11, 2013-14,
2025-26), 2 seeds. Indicative: plant assumptions as rounds 12-13 and 4-5b.

**B1, on/off heat pump on underfloor heating** (3 heavy houses; screed 8 MJ/K heavy, 3 MJ/K light):

| option | starts/day heavy / light | median run heavy / light (min) | mean abs(room - target) heavy / light | minutes/day outside +-0.3 C heavy / light |
|---|---|---|---|---|
| a) 1800 s | 41.9 / 44.0 | 14 / 14 | 0.285 / 0.296 | 457 / 513 |
| b) 2400 s | 31.5 / 32.9 | 19 / 18 | 0.288 / 0.305 | 470 / 592 |
| c) 7200 s | 9.8 / 8.8 | 58 / 76 | 0.310 / 0.357 | 630 / 904 |
| time-proportional 120 min (round 13 rule) | 10.8 / 11.6 | 54 / 51 | 0.226 / 0.179 | 324 / 172 |
| degree minutes -120 (round 13 rule) | 3.1 / 5.3 | 183 / 111 | 0.134 / 0.139 | 142 / 121 |

**B2, modulating heat pumps on radiators** (5 houses, calibrated radiators, coefficient at the balance, no tuner):

| period | hp30 starts/day | hp30 RMS | hp30 minutes/day outside +-0.3 C | hp60 starts/day | hp60 RMS | hp60 minutes/day outside |
|---|---|---|---|---|---|---|
| a) 1800 s | 6.4 | 0.155 | 79 | 30.4 | 0.183 | 102 |
| b) 2400 s | 5.3 | 0.171 | 89 | 28.2 | 0.209 | 133 |
| c) 7200 s | 3.4 | 0.331 | 202 | 18.9 | 0.452 | 346 |

## 4. Findings

1. The mechanism does not break: the real duty mapper gives the expected on/off times at 7200 s in every duty range,
   and the ON phase ends and holds as it should. The change is small (getter, setter, REST metadata, UI).
2. Option c) cuts starts a lot (on/off underfloor 42 -> 10 a day; hp30 6.4 -> 3.4; hp60 30 -> 19) but comfort gets
   clearly worse, most of all on radiators (RMS 0.155 -> 0.331 for hp30, 0.183 -> 0.452 for hp60). The cause is the
   ON time fixed at the start of a 2-hour period: SAT reacts to the room only once per 2 hours.
3. With the same 120-minute period, re-evaluating the ON time inside the period (time-proportional) keeps the
   starts (10.8-11.6 a day) and improves comfort (0.18-0.23 C). Degree minutes beats both (3-5 starts, 0.13-0.14 C).
4. For modulating 60 % heat pumps on radiators most starts do not come from the PWM period alone (19 a day even at
   7200 s); that case needs its own look.

## 5. Limits

Model plant, not validated against a heat-pump capture; compressor minimum on/off times assumed for the round 13
rules (20 / 10 min) and 180 s for SAT PWM; no defrost; modulating heat pumps on underfloor heating not simulated.
