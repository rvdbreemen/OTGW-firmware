# TASK-1933: pre-registered thresholds (committed before the first grid run)

Study: the PWM-to-continuous exit. Baseline "today": thermo-nova's sustained underheat exit (flow <= request - 3.0 C for
180 s while PWM status is ON; heating_control.py:308-356, const.py:22; firmware SATcycles.ino:39-40). Variant "e342":
Sergeant D's rule (entries 339, 342): flame on and request > flow for 60 s, same gates. Both use the entry-327 entry
rule (flow - request >= 1.5 C for 30 s) and keep the 300 s saturation exit. Script:
`tools/research/sat/sat_pwm_exit_sim_1933.py`, on the plant, boiler and fault model of
`tools/research/sat/sat_closedloop_sim_1412_19.py` (not edited). Every number is a MODEL RESULT; no result is boiler proof.

Field data first (TASK-1933 notes, EXPLORATORY, one house): in gruijter's 5 captures (about 70 h) the flow reached the
request within one 30 s tick in 90 of 91 PWM ON ignitions; the e342 rule would have fired once. That is the strongest
evidence on the toggling risk; the simulation checks the other house types.

## Grid

Classes very_low_load, thermal_lag, minimum_power, dhw, ignition (1412.19 definitions); eras 3_1983-92 and 5_2015;
radiators and underfloor; fixed and variable flow; Pmin 3.5 and 5.5 kW; MM honoured and ignored; seeds 1-5; 48 h per run.
800 runs per variant. Control interval 30 s; a sensitivity run at 60 s on the same grid.

## Validity check V1 (reported first; decides whether the simulation answers AC1)

On the gruijter-like plants (era 3_1983-92, radiators, fixed flow, Pmin 5.5 kW, MM ignored) under the baseline, PWM is
entered at least once in at least 60 % of the runs. If V1 fails, the simulation is reported as inconclusive for this
question and the field measurement stands alone.

## Measures (per run, then mean per variant and class)

- T1 toggles per day: PWM re-entries within 10 min after an underheat-type exit (baseline: the -3.0/180 s exit; variant:
  the e342 exit).
- T2 burner starts per day (S4 of 1412.19).
- T3 comfort: S1 share of the scored time within +-0.3 C; S3 worst undershoot.
- Reported, not judged: PWM entries per day, underheat-type exits per day, saturation exits per day.

## Decision rule (per class, the variant against the baseline on the same inputs)

The variant passes a class when all of these hold:
1. T1: mean toggles per day <= baseline mean + 1.0, and the worst run <= 4.0 per day.
2. T2: mean starts per day <= 1.10 x the baseline mean.
3. T3: mean S1 share >= baseline mean - 0.02, and the worst S3 not more than 0.2 C below the baseline worst.

Recommendation to Sergeant D: "ship e342" when V1 holds and the variant passes every class at 30 s; "ship with a caveat"
when it fails only the 60 s sensitivity run; otherwise report the failing class and measure, with no recommendation to
ship. Any analysis after the run (for example "also at baseline") is labelled as post-hoc.
