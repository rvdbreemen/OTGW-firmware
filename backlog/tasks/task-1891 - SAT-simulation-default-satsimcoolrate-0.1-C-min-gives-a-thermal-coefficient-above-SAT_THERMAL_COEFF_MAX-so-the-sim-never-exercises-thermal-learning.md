---
id: TASK-1891
title: >-
  SAT simulation: default satsimcoolrate 0.1 C/min gives a thermal coefficient
  above SAT_THERMAL_COEFF_MAX, so the sim never exercises thermal learning
status: In Review
assignee:
  - '@claude'
created_date: '2026-11-13T08:27:00Z'
updated_date: '2026-11-14T01:05:00Z'
labels:
  - sat
  - simulation
dependencies: []
priority: high
ordinal: 959000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-bench-tester (09) on the TASK-1869 bench of beta.220 (.61). fSimCoolRate defaults to 0.1 C/min at SAT_SIM_COOL_REF_K 10 K (SATtypes.h:558, SATcontrol.ino:185, :5841), about 0.6 C/h per K, while satUpdateThermalLearning() accepts samples only up to SAT_THERMAL_COEFF_MAX 0.3 (SATcontrol.ino:158, :6683). With the default every sample is rejected, so a sim run never tests the thermal model; 09 had to lower the cool rate. Decide: lower the sim default, or document the recipe. Simulation only, no effect on real control.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The default sim produces accepted thermal samples (host test or bench log), or the docs name the cool rate the sim needs
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 d3 analysis (lane-1891, wt-1891): confirmed in code. Sim room: fSimRoomTemp += (heat - coolRate x (room - outside) / SAT_SIM_COOL_REF_K 10) x dtMin (SATcontrol.ino ~6111-6114); learner sample = drop per hour / (room - outside) (satUpdateThermalLearning, ~6902+67/68). With the flame off every sample = 6 x satsimcoolrate regardless of the delta: default 0.1 -> 0.6 > SAT_THERMAL_COEFF_MAX 0.3 (:158), all rejected. The sim house is ~6x faster than a real one (tau ~1.7 h vs ~10 h at coefficient ~0.1) to keep bench runs short; lowering only the cool rate breaks the heat balance, so the heat rate must scale with it. A new default only reaches units without a stored SATsimcoolrate. Sergeant D's corrections hold no entry on the firmware sim cool rate (sim entries there concern the offline studies, e.g. entries 166, 170-172): no SAT control behaviour involved.

Choice put to Robert by DM via the coordinator: A) realistic defaults (coolrate 0.016, heatrate 0.08; learning works, sim runs ~6x slower, stored 0.1 stays), B) keep the fast sim + recipe in manual/bench recipe + a once-per-run log line that the cool rate puts thermal learning out of range (recommended, coordinator agrees), C) other. Host-test frame ready for both: tests/test_sat_sim_thermal_learning.py (real learner + the real sim room statement, pasted verbatim; defaults read from SATtypes.h): default sim 0 accepted samples in 6 h; 0.016/0.08 accepts samples at 0.096; boundary 0.045 accepted (0.27), 0.06 rejected (0.36). 3/3 OK.

2026-11-13 Robert's choice, relayed by the coordinator from his DM reply to the TASK-1891 question: "B" (keep the fast sim defaults 0.1/0.5; recipe in the manual and bench recipe; the firmware logs once per sim run that the cool rate puts thermal learning out of range). Implemented in wt-1907 (rides along with TASK-1907 in one build): SATcontrol.ino satUpdateThermalLearning narrates once per simulation run (re-armed on every sim on/off switch) 'Simulation: thermal learning skips samples (coefficient X > 0.30 from satsimcoolrate Y); for a thermal-learning test set satsimcoolrate 0.016 and satsimheatrate 0.08'; recipe in docs/manuals/en/ch05 + nl/h05 and docs/guides/agent-build-and-bench.md. Defaults unchanged.

Evidence: tests/test_sat_sim_thermal_learning.py 4/4 on the fix (default sim 0 accepted samples and exactly 1 note in 6 h; 0.016/0.08 accepts samples at 0.096 and no note; boundary 0.045 in, 0.06 out); on the old code the note test fails (0 notes). tests/test_sat_thermal_sim_persist.py OK (stub added for the new narration call). evaluate --quick 85/0. Bench check for 09: with the default sim the telnet/narration shows the note once per run.

2026-11-14 bench finding by 09 on 231+9cede93 (via the coordinator): the narration fired once (01:05:42) but was cut at 127 characters (satNarratef_P buf SAT_NARRATE_BUF 128, SATcontrol.ino:33/:110-113; my text was 172), and it printed the measured sample (0.49) next to 'from satsimcoolrate 0.100', which read as derived. My miss: tests/test_sat_narrate_fits.py already gates every narration format against SAT_NARRATE_BUF - 1 and was red on that text (worst 181 > 127); I had run only the tests I picked. Fix (not pushed yet): 'Sim: thermal learning skips samples (measured %.2f > %.2f); test with satsimcoolrate 0.016 + satsimheatrate 0.08' (gate worst case 118, coordinator's worst case coef 99.99 -> 113 chars); shared buffer unchanged. Gate OK, test_sat_sim_thermal_learning OK, evaluate 86/0. Rebased onto cfd389c17, re-bumped to beta.232 (commit 408e62d7f). 09 re-checks only the 1891 line on the rebuild.

2026-11-14 shipped in 2.0.0-beta.232 (9c3224077, post 1570967740544450631, zip sha256 3ed5847165f6e25ba28596dc83683ebf130f645a5896c2bdf06f5048bd3d6c64). Bench by 09 on .61 (evidence 09 scratchpad b232d3): the narration is complete at 117 chars, once per run, no repeat after 21 min; satthermalcoeff unchanged; settings diff none. Earlier on 231+9cede93 the line fired once but was cut at 127 chars (fixed in 232, see above). In Review for 09's bench ACs.
<!-- SECTION:NOTES:END -->
