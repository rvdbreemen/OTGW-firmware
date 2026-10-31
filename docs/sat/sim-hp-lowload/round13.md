# TASK-1527 round 13: control options for long runs, heat-pump mode only

Date 2026-10-31. Robert: research a new way of control that gives long runs instead of PWM with a 40-minute cycle,
specifically for on/off heat pumps, only in the heat-pump mode and never in the general SAT control.

Script `scripts/sat_hp_onoff_study13.py`, data `round13_hp_longrun_runs.json` (576 winters: 16 options x 3 heavy
houses (1992-2012, 2015, BENG) x 2 screeds (8 and 3 MJ/K) x 2 seeds x 3 winters (2010-11 cold, 2013-14 mild,
2025-26)). Plant and SAT curve/PID as round 12 (on/off ground-source heat pump on underfloor heating; all
assumptions in `round12.md`). Every new option has compressor protection of 20 min minimum on and 10 min minimum
off (assumption: Trane Symbio uses 5 / 3 min, https://support.trane.com/hc/en-us/articles/12151502231437 ; a forum
source cites 10 min for scroll lubrication; no ground-source datasheet was found).

Options:
- **pwm 30 / 40**: today's heat-pump PWM (`SATcontrol.ino:440-446`, entries 40/50), ON time fixed per period.
- **own**: SAT stays continuous, the heat pump cycles on its own control (+-5 K around the CS, assumed).
- **tp 180 / 240**: time-proportional PWM (round 12).
- **dm S (degree minutes, the NIBE method)**: DM += (flow - SAT setpoint) every minute; the compressor starts when
  DM <= -S and stops when DM >= 0. NIBE F1145 manual, menu 4.9.3: "Degree minutes are a measurement of the current
  heating requirement in the house", start compressor default -60, range -1000..-30, "a higher value ... more
  compressor starts, which increases wear" (https://www.manualslib.com/manual/1146229/Nibe-F1145.html?page=49).
  The stop at 0 DM comes from community sources (OpenEnergyMonitor forum), not from the manual.
- **rh h (room hysteresis)**: on below target - h, off above target + h.
- **mr R (minimum run)**: every run R minutes, the off time after it from SAT's duty d: R (1 - d) / d.

Results (means over all runs; "light" = 3 MJ/K screed):

| option | starts/day | max starts in a day | median run (min) | mean abs(room - target) | light screed abs err | minutes/day outside +-0.3 C | kWh/winter | starts heavy / light screed |
|---|---|---|---|---|---|---|---|---|
| pwm 30 (today's default) | 43.0 | 48 | 14 | 0.290 | 0.296 | 485 | 702 | 41.9 / 44.0 |
| pwm 40 (today's option) | 32.2 | 36 | 19 | 0.296 | 0.305 | 531 | 703 | 31.5 / 32.9 |
| own | 4.2 | 13 | 200 | 0.134 | 0.124 | 97 | 687 | 2.4 / 6.0 |
| tp 180 | 7.7 | 9 | 76 | 0.196 | 0.176 | 260 | 691 | 7.5 / 7.9 |
| tp 240 | 5.8 | 7 | 99 | 0.192 | 0.186 | 282 | 688 | 5.6 / 5.9 |
| dm 60 | 6.0 | 13 | 102 | 0.114 | 0.116 | 100 | 686 | 4.9 / 7.1 |
| **dm 120** | **4.2** | 9 | 147 | **0.136** | 0.139 | 132 | 689 | 3.1 / 5.3 |
| dm 240 | 3.0 | 7 | 214 | 0.174 | 0.171 | 234 | 695 | 2.1 / 3.9 |
| dm 480 | 2.2 | 5 | 314 | 0.236 | 0.221 | 430 | 705 | 1.5 / 2.9 |
| **rh 0.1** | 5.3 | 14 | 141 | **0.102** | 0.094 | 65 | 684 | 3.4 / 7.1 |
| rh 0.2 | 3.3 | 10 | 260 | 0.161 | 0.141 | 128 | 690 | 2.0 / 4.5 |
| rh 0.3 | 2.4 | 8 | 334 | 0.219 | 0.187 | 384 | 696 | 1.5 / 3.3 |
| mr 60 | 9.7 | 21 | 60 | 0.314 | 0.334 | 490 | 686 | 9.8 / 9.7 |
| mr 90 | 6.5 | 15 | 90 | 0.317 | 0.347 | 516 | 687 | 6.5 / 6.4 |
| mr 120 | 4.8 | 11 | 120 | 0.345 | 0.392 | 563 | 686 | 4.9 / 4.8 |

Findings:
1. Any of the long-run methods cuts starts by a factor 5-15 against today's 30/40-minute heat-pump PWM, at better
   comfort. Electricity differs by under 3 %.
2. **Degree minutes and room hysteresis dominate.** They beat the long PWM (tp) and the minimum-run PWM (mr) on starts
   and comfort at the same time. The trade-off is set by one parameter: DM 60 -> 480 or hysteresis 0.1 -> 0.3 C
   moves from 6 starts / 0.11 C to 2 starts / 0.22-0.24 C.
3. **Fixed long runs (mr) give the worst comfort.** The run length ignores the demand, so the screed overshoots.
4. **own** looks good in a heavy screed (2.4 starts) but has 6.0 starts on a light screed, and it depends on each
   heat pump's own control, which the model only assumes. Degree minutes gives similar or better numbers with a rule
   SAT controls itself.
5. Degree minutes keeps SAT's weather-compensated flow setpoint (curve + PID) as the reference, so the curve and its
   self-tune (ADR-205) stay meaningful. Room hysteresis ignores the curve and is a plain thermostat on the room
   sensor, which an underfloor screed makes overshoot-prone in the field.

Recommendation (for Sergeant D, Q-1527a): for on/off heat pumps, a degree-minute mode with one setting (start at
-120 DM by default, stop at 0, minimum 20 min on / 10 min off), in the heat-pump mode only.

Limits: one plant type (on/off ground-source heat pump on underfloor heating); compressor protection times and the
heat pump's own control are assumptions; no defrost, no brine dynamics; not validated against a capture.
