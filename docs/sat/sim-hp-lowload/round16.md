# TASK-1527 round 16: degree minutes on every heat-pump type, compared with today's SAT control

Date 2026-10-31. Robert (#dev-sat-mqtt 15:10 UTC, msg 1566107128047865907): "I really like the degree minutes
version. How does that work and also simulate all other types of heatpumps we have modeled." and in session: "Also
add a comparison to our current control strategy".

## How degree minutes work (the NIBE method, applied to SAT)

1. SAT computes the flow temperature it wants, as today (heating curve + PID).
2. Every minute the controller adds the difference between the actual and the wanted flow: DM += (flow - wanted).
   Water 2 C too cold for one minute adds -2.
3. While the heat pump is off the water cools, DM runs negative: a heat deficit builds up.
4. At -S degree minutes (for example -120 = 60 minutes at 2 C too cold) the compressor starts.
5. While it runs the water is warmer than wanted and DM climbs back; at 0 the deficit is repaid and it stops.
6. Compressor protection: at least 20 minutes on and 10 minutes off (assumption; Maarten's heat pump protects
   10 minutes on by itself). A modulating heat pump sets its own power on SAT's setpoint while it runs; degree
   minutes only decide when it starts and stops, as on NIBE inverter units.
Source: NIBE F1145 manual, menu 4.9.3, start compressor default -60, range -1000..-30
(https://www.manualslib.com/manual/1146229/Nibe-F1145.html?page=49); stop at 0 from community sources.

## Setup

Script `scripts/sat_hp_dm_study16.py`, data `round16_hp_dm_runs.json` (630 seasons), comparison script
`scripts/sat_hp_compare_current.py`, charts `round16_tradeoff.png` and `round16_vs_current.png`.
Heat pumps on radiators (round 4/5b plant, radiators calibrated so the curve balances at 1.8): modulating down to
30 %, modulating down to 60 %, on/off. Full heating seasons 1 Oct .. 30 Apr (2010-11, 2013-14, 2025-26), 5 houses,
2 seeds. Modes: today's SAT PWM 1800 s (the current control) and 2400 s; "own" (SAT never switches to PWM, the heat
pump modulates and cycles itself); degree minutes -60 / -120 / -240 / -480. The on/off heat pump on underfloor
heating comes from round 15 (same seasons). Every strategy is paired with the current control on the same house,
season and seed.

## Comparison with today's SAT control (SAT PWM 30 min)

| plant | strategy | starts/day | starts vs today | room error vs today (C) | min/day outside +-0.3 C vs today | energy vs today |
|---|---|---|---|---|---|---|
| radiators, modulating 30 % | today | 8.0 | | 0.204 RMS | | |
| | DM -120 | 7.3 | -9 % | +0.001 | +7 | -0.2 % |
| | DM -240 | 5.2 | -35 % | +0.008 | +21 | -0.4 % |
| | DM -480 | 3.7 | -53 % | +0.020 | +45 | -0.4 % |
| | own | 9.4 | +17 % | +0.002 | -15 | -0.1 % |
| radiators, modulating 60 % | today | 28.5 | | 0.231 RMS | | |
| | DM -120 | 16.5 | -42 % | +0.031 | +85 | +0.1 % |
| | DM -240 | 12.3 | -57 % | +0.060 | +189 | +1.1 % |
| | own | 38.8 | +36 % | -0.036 | -67 | -0.1 % |
| radiators, on/off | today | 35.8 | | 0.273 RMS | | |
| | DM -120 | 18.4 | -49 % | +0.074 | +254 | +2.5 % |
| | DM -240 | 14.8 | -59 % | +0.114 | +368 | +4.7 % |
| | own | 51.7 | +45 % | -0.073 | -157 | -1.2 % |
| underfloor, on/off, heavy floor | today | 33.7 | | 0.325 mean abs | | |
| | DM -120 | 2.6 | -92 % | -0.123 | -274 | -1.7 % |
| | own | 2.0 | -94 % | -0.120 | -289 | -1.9 % |
| underfloor, on/off, light floor | today | 36.9 | | 0.312 mean abs | | |
| | DM -120 | 4.5 | -88 % | -0.143 | -362 | -1.9 % |
| | own | 5.0 | -87 % | -0.161 | -417 | -2.6 % |

(all rows: `python scripts/sat_hp_compare_current.py`)

## Findings

1. **Degree minutes work best where there is thermal mass between the heat pump and the room.** On underfloor
   heating they cut starts by ~90 % and improve comfort at the same time. On radiators the water holds little heat,
   so a long run overshoots the room: fewer starts, but comfort gets worse, most for on/off and 60 % heat pumps.
2. **Modulating 30 % heat pumps on radiators** (the most common modern case): degree minutes -240 cut starts by 35 %
   at almost no comfort cost (+0.008 C); -480 by 53 % for +0.02 C. A clean improvement over today.
3. **Modulating 60 % heat pumps on radiators:** a real trade-off: -42 % starts for +0.03 C (DM -120), -57 % for +0.06 C.
4. **On/off heat pumps on radiators:** degree minutes halve the starts but cost clearly in comfort (+0.07 C, +254 min/
   day outside the band) and energy (+2.5 %). In practice such systems usually need a buffer tank; the model has none.
5. **"Own control" on radiators gives more starts than today** (+17 to +45 %) with equal or better comfort: the heat
   pump's own hysteresis (assumed +-5 K) cycles faster than SAT's PWM. On underfloor heating it is as good as degree
   minutes.
6. The best degree-minute setting depends on the system's mass: about -120 on underfloor heating, -240 to -480 for
   a 30 % heat pump on radiators. One fixed default does not fit all.

## Limits

Model plant, not validated against a heat-pump capture; no buffer tank, no defrost, no brine or outdoor-air source
dynamics; compressor protection and the heat pump's own hysteresis are assumptions; the starts of the radiator
plant come from the boiler-style flow loop of round 4 (not validated, round 4 notes).
