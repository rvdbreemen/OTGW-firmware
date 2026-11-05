# Round 25: can SAT keep the room inside the comfort band under sun and mild weather? (TASK-1566)

Date: 2026-11-02. Design and simulation only: nothing in the firmware changes.

## Question

The room must stay within the comfort band (target 20 C, band +-0.3 C), sun included. The winter 2024-25
trace (NIBE S2125-8, 2015 house on a heavy screed, SAT continuous) showed the room above 20.3 C in 20.4 %
of the season's hours. Which heating strategy keeps it in the band, and at what cost in starts and
electricity?

## Method

- Round 21 library (`tools/research/sat/sat_hp_library_study21.py`) with SAT through the proven port
  (`tools/research/sat/sat_sim_port.py`, `tests/test_sat_sim_port_equivalence.py`).
- Script: `tools/research/sat/sat_comfort_band_study25.py`, one result file per run under `round25_parts/<season>/`,
  merged in `round25_runs.json`.
- Score: share of ALL minutes after the 14-day warm-up with the room above 20.3 C, below 19.7 C, and within
  the band. Heating and non-heating hours are both counted, because the complaint is about the room, not
  about the heating.
- Every strategy runs paired against the baseline on the same house, weather and household gains.
- Strategies:
  - `base`: today's SAT.
  - `cut`, `cut0`: room cutoff. Heating stops above 20.1 / 20.0 C and resumes below 19.9 / 19.8 C.
  - `f6`, `f12`, `f24`: the heating curve uses the mean outdoor temperature of the coming 6 / 12 / 24 hours,
    plus the expected solar gain expressed in degrees. These runs use the season's own weather, so the
    forecast is perfect: they are the upper bound of forecasting.
  - `f6err`, `f24err`: the same, with a forecast error that drifts (AR(1), sd 1.5 K).
  - `f12s2`, `f12s3`, `f6s3`: the solar term weighted 2x and 3x.
  - Combinations of the above.

## Step 1: where the time above the band comes from (2024-25, NIBE, 2015 house)

- 19 % of the season's minutes are above the band within 24 h after heating. Another 1.2 % are above the
  band after more than 24 h without heating.
- Counterfactual: the same house and weather with **no heating at all** never gets above 18.7 C.
  Sun and household gains alone therefore do not overheat the room. The overshoot is heat that the
  heating put into the screed and walls earlier, and that arrives when the sun or milder weather removes
  the need for it.
- The mechanism, seen on 27 April 2025 in the dashboard's "Last week" view:
  - SAT heats at 04:00, the coldest hour (6 C outside, room 20.2 C).
  - The heat pump runs for 2 h at its 2.4 kW minimum and charges the screed from 21.4 to 24 C.
  - Outside it is 18.6 C by 13:00, with up to 800 W/m2 of sun.
  - The room climbs to 21.3 C by mid-afternoon (21.9 C at the 18:00 cooking peak). There is no heating
    after 10:00.

## Step 2: strategy sweep (2024-25, NIBE, 2015 house, SAT continuous)

| Strategy | Within | Above | Below | Max | Starts/day | Electricity kWh |
|---|---|---|---|---|---|---|
| base | 79.0 % | 21.0 % | 0.0 % | 21.82 | 2.5 | 1007 |
| cut0 | 81.5 % | 18.2 % | 0.2 % | 21.78 | 1.5 | 1016 |
| f12 | 83.7 % | 14.4 % | 1.9 % | 21.49 | 3.0 | |
| f6 | **87.7 %** | 12.1 % | 0.2 % | 21.48 | 3.0 | 955 |
| f12s3 | 78.9 % | 13.0 % | 8.1 % | 21.79 | 3.0 | 950 |
| f12s3cut0 | 79.7 % | 9.0 % | 11.3 % | 21.72 | 2.2 | 966 |

Heavier anticipation only trades "too warm" for "too cold". The short 6 h horizon wins: it sees the
afternoon sun without averaging the curve over a whole day. The PWM variants scored worse than SAT
continuous throughout (round 25 quick run).

## Step 3: ten winters, 3 heat pumps, 4 houses (600 runs, paired)

Units: Mitsubishi Ecodan PUZ-WM50, NIBE S2125-8 and Daikin Altherma 3 R ERGA04. Houses: 1992-2012 on a
heavy screed, 2015 on a heavy screed, BENG 2021 on a heavy screed, and BENG 2021 on a **light** screed.
Winters 2015-16 to 2024-25.

| Strategy | Within | Above | Below | Max (mean) | Starts/day | Electricity | Paired: within | Runs better |
|---|---|---|---|---|---|---|---|---|
| base, continuous | 85.8 % | 14.1 % | 0.1 % | 21.92 | 3.4 | 1052 | | |
| **f6, continuous** | **90.2 %** | 9.6 % | 0.3 % | 21.68 | 3.8 | 1022 | **+4.4** (worst -0.6) | 118/120 |
| f6err, continuous | 89.4 % | 10.2 % | 0.4 % | 21.69 | 3.8 | 1023 | +3.6 (worst -1.0) | 101/120 |
| base, PWM | 82.5 % | 17.2 % | 0.3 % | 21.94 | 17.0 | 1058 | | |
| f6, PWM | 88.4 % | 11.1 % | 0.5 % | 21.73 | 19.1 | 1033 | +5.9 (worst -0.4) | 118/120 |

Electricity: f6 uses 3.3 % less than base (continuous) and 2.5 % less (PWM). Starts per day go up by 0.35
(continuous) and 2.05 (PWM).

Per house, SAT continuous, share of time within the band:

| House | base | f6 | f6err |
|---|---|---|---|
| 1992-2012, heavy | 93.3 % | 94.3 % | 93.7 % |
| 2015, heavy | 83.2 % | 89.5 % | 88.8 % |
| BENG, heavy | 79.0 % | 87.2 % | 86.4 % |
| BENG, light | 87.8 % | 89.6 % | 88.8 % |

All ten winters improve with f6, from +3.2 points (2017-18) to +6.0 points (2024-25).

## Step 4: sensitivity to the model's solar split

The model puts 50 % of the solar gain on the air node and 50 % on the walls, an assumption with no
measurement behind it. Rerun on 2024-25, NIBE, 2015 house, continuous:

| Solar share to air | base within | f6 within | f6 max |
|---|---|---|---|
| 0.2 | 79.8 % | 87.2 % | 21.48 |
| 0.5 (model) | 79.0 % | 87.7 % | 21.48 |
| 0.8 | 78.3 % | 86.8 % | 21.51 |

The conclusion does not depend on this assumption: every result moves by less than 1 point.

## Findings

1. **No heating strategy keeps the room inside +-0.3 C all the time.** The best single run reaches 93 %,
   and the mean is 90 %. A heating-only system cannot remove heat on a sunny afternoon. Holding the band
   at all times needs shading, cooling, or a lower setpoint in spring.
2. **The cause is mainly insulation, not only the screed.** The well-insulated BENG house overshoots more
   than the 1992-2012 house. A light screed helps (87.8 % against 79.0 % for the same BENG house on a heavy
   screed), but it does not reach the band either. The less a house loses, the longer the morning's heat
   stays in it.
3. **A 6 h weather-forecast term in the heating curve is the most effective change found.**
   - It adds +4.4 points within the band over ten winters (118 of 120 runs better) and cuts the time above
     the band by a third (14.1 % to 9.6 %).
   - It uses 3 % less electricity, at the cost of about one extra start every three days.
   - With a realistic forecast error it keeps most of the gain (+3.6 points).
   - The firmware already fetches a 24 h hourly forecast with temperature and cloud cover
     (`SATweather.ino:31-40`, HAS_WEATHER_FORECAST). The simulation's solar term uses irradiance in W/m2,
     which is not in that forecast. Firmware use would need irradiance added to the request, or a solar
     estimate derived from cloud cover, and that variant is not simulated. This finding is about what SAT
     should do with the forecast, not about new hardware.
4. **Longer horizons and heavier solar weights are worse.** They overcorrect and make the room too cold.
5. **A plain room cutoff helps a little** (+2.5 points in 2024-25) and halves the starts, but it does not
   address the cause.

## Field reference (not a SAT run)

maarten_41762, #alpha-testing msg 1566925806268186755 (written summary, no capture yet):
- House: 2017 detached, label A, underfloor heating on a high-mass floor.
- Heat pump: on/off, about 9 kW, under a Honeywell controller with a 180-min PWM.
- Behaviour: three runs a day of about 45 minutes at 29.5 C supply.
- Rooms: 20.5 to 21 C. That is a 0.5 C band with a setpoint of 21 C, consistent with the simulated
  heavy-screed behaviour.

A capture is planned after 2026-11-05.

## Limits

- One model house per era and three heat pumps. Hot-water blocks and shading are not modelled.
- The forecast term is the simulation's own construction: mean outdoor temperature over the horizon, plus
  the expected solar gain expressed in degrees. Nothing like it exists in thermo-nova; it is a design
  proposal, not a port.
- "Within the band" counts all hours, including the shoulder season, when no heating strategy can help.

## Next

Design question Q-1566a for Sergeant D (queued in `docs/sat/open-questions.md`): should SAT use a
short-horizon forecast term in the heating curve?
