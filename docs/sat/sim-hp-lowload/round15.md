# TASK-1527 round 15: heat-pump options over full heating seasons (1 October .. 30 April)

Date 2026-10-31. Robert asked whether the simulations used a whole winter data set. Rounds 1-14 used whole winters
of 1 November .. 28 February (120 days, real hourly weather). October, March and April were missing, and those carry
the lowest loads, where heat pumps switch most. This round runs the heat-pump options of rounds 13-14 over three
full heating seasons of 212 days each: 2010-11 (cold), 2013-14 (mild), 2025-26 (average), Open-Meteo archive, same
grid cell (`weather_season/`, 5088 hours each, no gaps). Script `scripts/sat_hp_season_study15.py`, data
`round15_hp_season_runs.json` (504 seasons).

**Model fix against rounds 12-14:** the underfloor emitter constant K was calibrated on the season mean of the
weather being simulated, so the "same" house differed slightly per winter. Comparisons between options inside one
winter were paired and are not affected; comparisons of absolute numbers between winters in rounds 12-14 are.
Here K is calibrated once on the reference winter (Nov-Feb 2025-26 mean 4.69 C). The radiator plant (B2) already used
fixed radiator sizes. A check: the seasons differ as they should (degree minutes -120, heavy screed: 1619 / 1258 /
1376 kWh in the cold / mild / average season), while the starts per day stay almost equal (2.6), because degree
minutes start at a fixed heat deficit: a colder season gives longer runs, not more of them.

Starts per day are averaged over all 212 days, including days without heating; comfort is scored over heating
minutes only.

## B1, on/off heat pump on underfloor heating (3 heavy houses, 2 seeds, 3 seasons)

| option | starts/day heavy screed | starts/day light screed | median run heavy / light (min) | mean abs(room - target) heavy / light | max starts in one day |
|---|---|---|---|---|---|
| today's PWM 30 min (1800 s) | 33.7 | 36.9 | 14 / 12 | 0.325 / 0.312 | 48 |
| today's PWM 40 min (2400 s) | 25.4 | 27.7 | 18 / 17 | 0.331 / 0.322 | 36 |
| third option, PWM 120 min (7200 s) | 8.2 | 7.5 | 55 / 69 | 0.374 / 0.401 | 12 |
| time-proportional 120 min | 9.0 | 10.0 | 50 / 45 | 0.283 / 0.221 | 13 |
| time-proportional 240 min | 4.7 | 5.3 | 89 / 84 | 0.248 / 0.217 | 7 |
| degree minutes -120 | 2.6 | 4.5 | 180 / 102 | 0.203 / 0.169 | 9 |
| degree minutes -240 | 1.8 | 3.4 | 266 / 136 | 0.253 / 0.206 | 7 |
| room hysteresis 0.2 C | 1.7 | 3.8 | 337 / 143 | 0.243 / 0.170 | 9 |
| heat pump's own control | 2.0 | 5.0 | 278 / 108 | 0.206 / 0.150 | 13 |

Electricity per season differs by under 6 % between the options (heavy screed 934-962 kWh mean over the houses).

## B2, modulating heat pumps on radiators (5 houses, 2 seeds, 3 seasons), SAT PWM as today

| period | hp30 starts/day | hp30 RMS | hp30 minutes/day outside +-0.3 C | hp60 starts/day | hp60 RMS | hp60 minutes/day outside |
|---|---|---|---|---|---|---|
| 1800 s | 8.0 | 0.204 | 120 | 28.5 | 0.231 | 152 |
| 2400 s | 6.4 | 0.218 | 135 | 26.2 | 0.256 | 188 |
| 7200 s | 3.6 | 0.365 | 259 | 17.5 | 0.466 | 398 |

## Findings, compared with rounds 13-14 (November .. February)

1. **The ranking holds over the full season.** Degree minutes, room hysteresis and own control give the fewest
   starts at the best comfort; today's 30/40-minute PWM gives the most starts; the fixed 120-minute PWM cuts starts
   but has the worst comfort.
2. **Comfort is worse over the full season for every option** (heavy screed: degree minutes -120 0.136 -> 0.203 C,
   PWM 40 min 0.288 -> 0.331 C). October and April bring sun and mild days on which an on/off heat pump at full
   power overheats the floor; that is the low-load problem showing up, and no option escapes it fully.
3. **Starts per day are lower on the season average** because the mild weeks include days without heating; the
   longest-run options stay at 2-5 starts a day, today's PWM at 25-37.
4. **Modulating heat pumps on radiators:** the third option halves the starts (hp30 8.0 -> 3.6, hp60 28.5 -> 17.5)
   and almost doubles the RMS (0.20 -> 0.37, 0.23 -> 0.47), as in round 14.
5. Degree minutes -120 remains the best single rule on a heavy floor (2.6 starts, 0.20 C); on a light floor the heat
   pump's own control is slightly better in comfort (0.150 vs 0.169 C) at similar starts.

## Limits

As rounds 12-14 (plant assumptions, compressor protection 20 / 10 min assumed, no defrost, no capture validation),
plus: heating is on below 15 C outside only; no summer mode or hot-water heating by the heat pump.
