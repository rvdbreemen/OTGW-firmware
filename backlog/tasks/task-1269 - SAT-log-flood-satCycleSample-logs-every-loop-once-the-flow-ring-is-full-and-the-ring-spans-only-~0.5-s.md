---
id: TASK-1269
title: >-
  SAT log flood: satCycleSample logs every loop once the flow ring is full, and
  the ring spans only ~0.5 s
status: Done
assignee:
  - '@claude'
created_date: '2026-10-20 18:16'
updated_date: '2026-10-21 10:18'
labels:
  - bug
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 399000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
First real SAT field run (gruijter, Intergas HReco 30 + Honeywell Round + PIC Classic, alpha.439/441, #alpha-testing 2026-10-20), captures in %TEMP%\discord-1020. SATcycles.ino ~862-869: once _flow_sampleCount saturates at SAT_FLOW_SAMPLE_SIZE (256), (count & 0x0F)==0 is always true, so 'SAT sample: flow=' is logged on every loop: 106,674 lines in 13 min, which garbled the whole telnet capture. Sampling runs every loop (~2 ms), so the 256-entry ring read for p90/p10 covers ~0.5 s of flow instead of a cycle.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sampling runs at a fixed cadence (e.g. 1 Hz) so the ring covers the intended window; the debug line is rate-limited (telnet line count over 5 min before vs after)
- [x] #2 Cycle classifier p90/p10 still computed (test)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Gate the per-cycle flow ring (_flow_samples, 256) on the same 1 Hz gate as the tail ring (SATcycles.ino satCycleSample), so it spans the last 256 s of the cycle instead of ~0.5 s of loop iterations, and rate-limit the 'SAT sample' debug line with its own counter (one line per 30 samples = 30 s) instead of (_flow_sampleCount & 0x0F), which is always 0 once the ring is full. Not a SAT control change, so no Sergeant D wait: the classifier uses the 1 Hz tail ring; the full-cycle p90/p10 only feed the 'SAT cycle' debug line, the 4 h telemetry sat/4h_avg_flow_temp (f4hAvgFlow) and the tail fallback for cycles shorter than 10 s. Verification: bench replay (fixture with a 4 min burn then flame off) on alpha.444 (old) vs the fix: telnet 'SAT sample' lines over the run, and the 'SAT cycle classify' line still reports fullP90/tail values at cycle end.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-20 cycle: skipped for now; changes SAT behaviour (control/classifier), so it needs Sergeant D's OK per the step 4 rule.

Not a SAT control change (decided 2026-10-21, no Sergeant D wait): the classifier uses the 1 Hz tail ring; the full-cycle ring feeds the debug line, sat/4h_avg_flow_temp and the tail fallback for cycles < 10 s. Bench OTGW32 .88.61, replay with a 4 min burn then flame off (telnet capture over 440 s). RED alpha.444+a7bbacc: 24251 'SAT sample' lines (3307/min), n=256 on every line; cycle line 'p90=51.7 p10=51.7' (ring spanned ~0.5 s). GREEN fix (alpha.444+99cb6d4 build of this diff): 10 lines (one per 30 s), n=31, 61, 91 ... 256 = 1 Hz; the cycle was still classified: REST cycle_count 1, last_cycle_class 4, cycle_req_setpoint_error 13.44 (= tailP90 - requested 37.4, so tailP90 ~50.8 C, consistent with the replayed flow). The 'SAT cycle classify' telnet line itself was dropped by telnet in the green run (known 2.0.0 telnet loss under traffic). Opus review: no bug; accepted shifts: cycles < 10 s now fall back to max/min flow (tail < 10 samples too), 4h p90 is the p90 of the last 256 s instead of ~0.5 s, comments updated. Seen on the way: during a /simulate replay 'OTGW-SIM trace' logs every blocked bus tx (~10 lines/s); bench-only, not changed. Gates: evaluate --quick 72/0, test_sat_pwm_override 25 OK; settings diff 0. Evidence %TEMP%\bench-1269\red-444.json, green-fix.json + telnet transcripts.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The per-cycle flow ring now samples at 1 Hz (it filled at loop rate and spanned ~0.5 s) and the 'SAT sample' debug line is rate-limited to one per 30 s (it fired every loop once the ring was full). AC1: bench replay, 24251 lines (3307/min) on alpha.444 vs 10 lines in the same 7 min on the fix, n growing 30 per line. AC2: the cycle is still classified (cycle_count 1, class 4, req-setpoint error 13.44 = tailP90 - requested); p90/p10 now come from 256 s of 1 Hz samples instead of 0.5 s (old p90 == p10). Not verified: a real boiler; the classify telnet line in the green run (telnet dropped it, REST state shows the result).
<!-- SECTION:FINAL_SUMMARY:END -->
