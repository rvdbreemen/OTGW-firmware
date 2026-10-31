# TASK-1527 round 12: an on/off heat pump on underfloor heating, fewest starts with comfort

Date 2026-10-31. Trigger: maarten_41762 (#alpha-testing msg 1566089657492705431): on/off ground-source heat pump,
underfloor heating, well-insulated home with much internal mass; a 3-hour PWM period gives longer runs, few starts
and a stable room. Robert: for heat pumps the goal is the fewest starts with maximum comfort.

Script `scripts/sat_hp_onoff_study12.py`, data `round12_hp_onoff_runs.json` (540 winters: 30 variants x 3 houses
(1992-2012, 2015, BENG, all heavy) x 2 seeds x 3 winters (2010-11 cold, 2013-14 mild, 2025-26)). Model assumptions
(not validated against any heat pump): on/off at full power sized for -10 C; screed 8 MJ/K as the emitter node,
emission exponent 1.1, curve base 20 C (`SATcontrol.ino:127`), max flow 40 C (entry 155), COP 0.5 x Carnot from a
5 C brine; SAT curve and PID as rounds 1-11 (underfloor gain divisor 4, thermo-nova `pid.py:69`).

Variants:
- **own**: SAT never switches to PWM; the heat pump's own control cycles on the SAT control setpoint (on below
  CS - 5 K flow, off above CS + 5 K; the hysteresis is an assumption).
- **pwm P**: SAT PWM as today, the ON time fixed at the start of each P-minute period from SAT's duty formula.
- **pwmlive P**: time-proportional, the ON time re-evaluated every minute inside the period, at most one start per
  period. Minimum ON time 3 min (firmware) or 20 min (typical compressor protection, assumption).

Results (means over houses, seeds and winters; minimum ON 3 min unless noted):

| variant | starts per day | median run (min) | mean abs(room - target) | minutes/day outside +-0.3 C | electricity kWh/winter |
|---|---|---|---|---|---|
| own | **2.4** | 286 | **0.144 C** | 119 | 687 |
| pwmlive 240 | 5.6 | 100 | 0.196 C | 275 | 692 |
| pwmlive 180 | 7.5 | 78 | 0.214 C | 311 | 694 |
| pwmlive 120 | 11.0 | 54 | 0.221 C | 313 | 696 |
| pwm 180 | 6.0 | 87 | 0.332 C | 742 | 710 |
| pwm 40 (today's option, 2400 s) | 31.5 | 19 | 0.288 C | 470 | 702 |
| pwm 30 (today's default, 1800 s) | **41.9** | 14 | 0.285 C | 457 | 701 |

The cold and the mild winter give the same ranking of the variants (starts differ by up to 5.4 per day between the
two winters, for pwm 30 with a 20-minute minimum ON time; for own and pwmlive 180 / 240 by under 0.2 per day). A 20-minute minimum ON
time lowers starts for the short periods (pwm 30: 41.9 -> 28.8) but worsens comfort; for periods of 90 min and more
it changes nothing.

Findings:
1. Today's 30-minute heat-pump PWM is the worst case for this heat pump: ~42 starts a day and the poorest comfort.
2. A long period alone does not help comfort when the ON time is fixed at the start of the period (pwm 180: 0.332 C);
   it does when the ON time follows the room within the period (pwmlive 180: 0.214 C). This matches Maarten's
   floor-heating controller and his experience.
3. Letting the heat pump cycle on its own control gives the fewest starts and the best comfort in this model, because
   the screed buffers hours of heat. It depends on the heat pump's own control, which the model only assumes.
4. Electricity differs by under 4 % between the variants.

Limits: one emitter type (underfloor, on/off heat pump); modulating heat pumps on radiators not rerun in this round;
no compressor minimum off-time, defrost or brine temperature dynamics; no validation capture.
