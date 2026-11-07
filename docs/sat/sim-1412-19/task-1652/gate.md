# TASK-1652 step-2 gate, written before the rule run (2026-11-05)

Paired comparison, same grid as TASK-1412.19 (1440 runs), flag off (baseline) vs --cold-cut-min-burn (rule).

Step 2 (firmware) goes ahead only if ALL hold:
1. No run fails a hard check H1-H4 with the rule that passes it in the baseline.
2. No class has a lower S2 pass share with the rule than in the baseline.
3. No run newly fails S2 (passes in baseline, fails with the rule).
Reported, not gating: S1, S3, S4, S5 deltas; cold-cutoff short phases (expected 0 with flame burning; a phase whose
flame went out before the cut is counted separately as cold_cutoff_flame_out); overall S6 (still fails on trv_pump).
Precondition: the baseline reproduces committed runs.json per run (ignoring the new S6_short_cause key and meta).
