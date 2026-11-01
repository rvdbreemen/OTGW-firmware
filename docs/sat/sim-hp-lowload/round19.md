# TASK-1527 round 19: SAT against best-practice control of a heat pump with large thermal mass

Date 2026-11-01. Question (Robert): does the design of SAT conflict with best-practice control of a heat pump with a
large thermal mass? Each point is marked by how it was checked: **code** (read in the firmware and thermo-nova at the
pin), **sim** (this round), **literature** (source given), **unverified** (needs a capture or field data).

Plant: rounds 15/18 (on/off heat pump, underfloor heating, 3 houses, heavy 8 MJ/K and light 3 MJ/K screed). Runs:
- round 19: room-feedback and setback variants at curve 1.8, 3 seasons x 2 seeds, 864 runs
  (`scripts/sat_hp_bestpractice_study19.py`, `round19_bestpractice_runs.json`);
- round 19b: curve sweep 0.6-1.8 on 2025-26, seed 1, 378 runs (`scripts/sat_hp_curve_sweep_study19b.py`,
  `round19b_curve_sweep_runs.json`);
- round 19c: out-of-sample check, the chosen curves fixed, 3 seasons x 2 seeds, 576 runs
  (`scripts/sat_hp_tuned_curve_study19c.py`, `round19c_tuned_curve_runs.json`).
Regression: defaults reproduce round 17/18 exactly (pwm 30: 38.4 starts, 0.248 C). Hooks added to
`sat_hp_onoff_study13.py`: `KP_MODE`, `SETBACK`, `COEFF` (defaults unchanged).

**Model limits that apply to every finding:** on/off heat pump only (its flow comes from its power, not from CS:
mean flow while running is about 27 C in every variant, so a higher CS costs no COP in this model); COP depends on
flow only, not on outdoor temperature; no start losses, no defrost; one perfect, noise-free room sensor for the whole
house; no thermostatic valves; no buffer tank.

## Best practice (literature)

- Weather compensation with a low, well-tuned curve is the main control. Room feedback is small or capped, because
  room thermostats make heat pumps cycle like boilers (Stiebel Eltron installer newsletter issue 67; Heat Geek;
  Squote). NIBE "factor system": range 0.0-6.0, default 2.0, "A higher value gives a greater change of the heating
  curve's set offset" (F1245 manual menu 1.9.4, manualslib p.45, read). Reading "factor 2" as "2 K flow per K room
  error" is an assumption from a Danfoss/York manual's description, not a NIBE formula.
- Setback: none, or at most about 2 K with weather compensation (Grant UK FAQ).
- Compressor protection: NIBE F1345 p.18: 5 min stop-to-start, 20 min start-to-start (read).

## Findings

1. **In SAT PWM a heat pump needs a much lower curve, and SAT cannot learn it** (sim, out of sample; code).
   - The best coefficient in SAT PWM is 0.8-1.0, against 1.4-1.8 for degree minutes and the heat pump's own control,
     in the same houses.
   - With 1.8 in PWM the house runs warm in every season: 0.30-0.34 vs 0.16-0.18 C mean abs error (heavy), and
     +4 % electricity (952 vs 915 kWh per season).
   - The PID does not remove the offset: the integral resets outside the 0.1 C band (SATpid.ino:154-178), so only P
     holds it.
   - The self-tune cannot fix it: it ignores PWM hours (ADR-205, entry 174), and an on/off heat pump is in PWM almost
     always (entry 87).
   - Mechanism (reading, ours): the duty formula (setpoint - base) / (effective flow - base), thermo-nova `pwm.py:238`,
     firmware SATcontrol.ino:1187, treats the curve's flow as a fraction of the ON flow. A full-power on/off heat pump
     delivers more heat per ON minute than that linear mapping assumes.
   - A limit that probably makes it worse in reality: the firmware holds the effective flow from the first 180 s of
     the ON phase (EMA, `pwm.py:144-149`). A heat pump's flow rises slowly, so the held value sits below the steady ON
     flow the model uses, which gives a higher duty and more heat.
   - Needs field data.
2. **At a tuned curve SAT PWM gives the best comfort, but 2.5-4x the starts** (sim, out of sample). Heavy screed:
   SAT PWM 180 min 7.6 starts/day at 0.168 C; degree minutes 3.0 at 0.188 C; own control 2.0 at 0.206 C. Light screed:
   9.2 / 0.132 C, 4.6 / 0.157 C, 5.0 / 0.150 C. Electricity within 2 % (structural in this model). This corrects the
   comfort ranking of rounds 16-18, which compared SAT PWM at the too-high curve 1.8.
3. **Room feedback is strong in SAT; in this model it helps comfort** (sim, 19b, in sample). SAT's P gain is
   coeff x curve / 4 (SATpid.ino:135; about 13 K flow per K room error at curve 1.8 on underfloor). At each variant's
   best curve, SAT's PID beat a NIBE-style factor 2 and pure weather compensation in every control mode, at about the
   same starts in PWM. With the heat pump's own control, weaker feedback halves the starts (2.0 -> 1.0) but doubles
   the error (0.21 -> 0.39 C). This holds only for an on/off unit with a perfect room sensor. The model cannot show
   the main concern in the literature: on a modulating heat pump a high P gain raises the flow temperature and lowers
   the COP, and a noisy or badly placed sensor makes the floor swing.
4. **SAT drives a heat pump in PWM with gas-boiler setpoint tricks** (code, thermo-nova design; effect unverified).
   thermo-nova's overshoot protection (`heating_control.py:380-408`), ported as `satApplyPWM` (SATcontrol.ino:1366-1393),
   applies to every source:
   - ON phase before the compressor starts: CS = return + 18 C, capped (thermo-nova `heating_control.py:392-400`;
     firmware satPwmFlameOffHold, fFlameOffOffset 18, cap 40 C for heat pumps);
   - once it runs: CS = flow - 1 C, refreshed every 3 s (firmware satPwmSuppressionSetpoint, fModSupOffset 1.0), the
     OpenTherm way to hold a boiler at minimum modulation;
   - OFF phase: CS = 10 and CH = 0 (thermo-nova `heating_control.py:388-390`, `:238`).
   Risks, not measured: a modulating heat pump asked for 40 C at start runs harder than needed; a heat pump that stops
   when the flow exceeds CS may end SAT's ON phase early; with CH = 0 some heat pumps stop the circulation pump. Entry
   182 allows changing this for heat pumps. Needs an OpenTherm capture of a real heat pump in SAT PWM (MsgID 0 flame
   bit, MsgID 1, MsgID 17, MsgID 25).
5. **Night setback costs real comfort on a heavy floor** (sim, out of sample, at tuned curves; code). Presets:
   comfort 21, eco 18, sleep 16, away 15 C (SATtypes.h:446-451), so 3-6 K, above the about 2 K advised. SAT PWM at
   curve 0.8, setback 23:00-06:00: 1 K: 0.281 vs 0.168 C error in comfort hours, -3.8 % electricity; 3 K: 0.373 C,
   -5.1 %. Degree minutes and own control lose as much or more comfort. The round 19 numbers at curve 1.8 ("slightly
   worse comfort") were masked by the overheating. The saving is optimistic (COP does not drop at night in the model).
6. **On/off heat pumps always end in PWM** (code, known): the 60 s overshoot switch (entry 87) applies to heat pumps.
7. **Fixed already:** minimum ON 600 s and OFF 600 s for heat pumps (entries 197/198), matching NIBE's
   start-to-start rule.

Entries relied on: 87 (agree it is the current rule; its basis was thermo-nova parity, which entry 182 lifts for heat
pumps), 174 and ADR-205 (finding 1 shows a cost of that choice for heat pumps), 182, 197, 198 (agree).

## Not checked

Modulating heat pumps on underfloor heating (no plant model); radiators; real heat-pump response to CS and CH; sensor
noise; multiple rooms.
