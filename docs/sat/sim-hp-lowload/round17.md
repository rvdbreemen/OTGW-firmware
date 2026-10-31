# TASK-1527 round 17: SAT's own PWM period extended beyond today's limit

Date 2026-10-31. Robert: "Vergelijk de aanpak met SAT en laat PWM oplopen verder dan nu mogelijk is" (compare the
approach with SAT and let the PWM period go further than is possible today).

Today SAT offers heat pumps a PWM period of 30 or 40 minutes (1800 / 2400 s, `SATcontrol.ino:440-446`, entries
40/50). This round extends SAT's own PWM, unchanged otherwise (ON time fixed at the start of each period, thermo-nova
duty formula, minimum ON 180 s; round 14 showed the real mapper works at 7200 s), to 60, 90, 120, 180 and 240
minutes, on every heat-pump type, over full heating seasons (1 Oct .. 30 Apr: 2010-11, 2013-14, 2025-26), 2 seeds.
On underfloor heating the time-proportional variant (ON time re-evaluated inside the period) runs alongside.
Degree minutes come from rounds 15 (underfloor) and 16 (radiators), same seasons and houses.

Script `scripts/sat_hp_period_sweep_study17.py`, data `round17_hp_period_sweep_runs.json` (1,134 seasons), chart
`round17_period_sweep.png`.

## Radiators (5 houses, calibrated radiators)

| type | SAT PWM period | starts/day | vs today | room RMS (C) | vs today | min/day outside +-0.3 C | energy vs today |
|---|---|---|---|---|---|---|---|
| modulating 30 % | 30 (today) | 8.0 | | 0.204 | | 120 | |
| | 60 | 5.0 | -38 % | 0.245 | +0.041 | 161 | 0.0 % |
| | 120 | 3.6 | -54 % | 0.365 | +0.161 | 259 | +0.6 % |
| | 240 | 2.7 | -66 % | 0.689 | +0.485 | 454 | +0.8 % |
| | degree minutes -480 | 3.7 | -53 % | 0.224 | +0.020 | 165 | -0.4 % |
| modulating 60 % | 30 (today) | 28.5 | | 0.231 | | 152 | |
| | 60 | 23.1 | -19 % | 0.305 | +0.074 | 245 | +0.4 % |
| | 120 | 17.5 | -39 % | 0.466 | +0.235 | 398 | +1.5 % |
| | 240 | 11.8 | -59 % | 0.833 | +0.602 | 650 | +1.7 % |
| | degree minutes -120 | 16.5 | -42 % | 0.262 | +0.031 | 237 | +0.1 % |
| on/off | 30 (today) | 35.8 | | 0.273 | | 250 | |
| | 120 | 29.8 | -17 % | 0.511 | +0.238 | 452 | +2.0 % |
| | 240 | 26.3 | -27 % | 0.821 | +0.548 | 603 | +2.1 % |
| | degree minutes -120 | 18.4 | -49 % | 0.347 | +0.074 | 504 | +2.5 % |

## Underfloor heating, on/off heat pump (3 heavy houses; heavy and light screed)

| floor | strategy | 30 | 60 | 120 | 180 | 240 (period in minutes) |
|---|---|---|---|---|---|---|
| heavy | SAT PWM: starts/day / mean abs error (C) | 33.7 / 0.325 | 16.8 / 0.339 | 8.2 / 0.374 | 4.7 / 0.395 | 3.2 / 0.438 |
| heavy | time-proportional | 22.6 / 0.341 | 16.1 / 0.315 | 9.0 / 0.283 | 6.2 / 0.264 | 4.7 / 0.248 |
| heavy | degree minutes -120 / -240 | | | 2.6 / 0.203 | | 1.8 / 0.253 |
| light | SAT PWM | 36.9 / 0.312 | 18.1 / 0.340 | 7.5 / 0.401 | 4.1 / 0.467 | 2.6 / 0.556 |
| light | time-proportional | 23.1 / 0.347 | 17.8 / 0.279 | 10.0 / 0.221 | 6.9 / 0.211 | 5.3 / 0.217 |
| light | degree minutes -120 / -240 | | | 4.5 / 0.169 | | 3.4 / 0.206 |

## Findings

1. **Extending SAT's own PWM always trades starts for comfort.** Every step from 30 to 240 minutes lowers the starts
   and raises the room error, on every type. The cause is the ON time fixed at the start of the period: with a long
   period SAT reacts to the room only once per period.
2. **At the same number of starts degree minutes are far better on radiators.** Modulating 30 %: PWM 120 min gives
   3.6 starts at 0.365 C, degree minutes -480 give 3.7 starts at 0.224 C. Modulating 60 %: PWM 120 min 17.5 starts at
   0.466 C, degree minutes -120 16.5 starts at 0.262 C. On/off: PWM 240 min still 26 starts at 0.82 C, degree minutes
   -120 18 starts at 0.35 C.
3. **On underfloor heating the period only helps when the ON time follows the room inside the period.** The
   time-proportional variant gets better as the period grows (heavy floor 0.341 -> 0.248 C from 30 to 240 min);
   SAT's current mechanism gets worse (0.325 -> 0.438 C). Degree minutes beat both.
4. A longer SAT PWM period is the smallest code change (round 14: getter, setter, REST limits, UI) and a moderate
   step (60 min) is acceptable on modulating 30 % heat pumps (-38 % starts, +0.04 C). Beyond about 60 minutes the
   comfort cost grows quickly on radiators.

## Limits

Model plant, not validated against a heat-pump capture; no buffer tank, no defrost; compressor protection and the heat
pump's own control assumed; radiator-plant start counts from the round 4 flow loop (not validated).
