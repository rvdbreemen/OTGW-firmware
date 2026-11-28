---
id: TASK-3106
title: >-
  SAT self-tune: indigo 24 h beta.428 capture: coefficient never moved (window
  11/12 h), evening hours below target excluded as min_mod (MsgID 15 never seen,
  default 5 %): decide whether the tune should count low-modulation continuous
  hours
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-26T13:36:00Z'
updated_date: '2026-11-28T19:45:00Z'
labels:
  - sat
  - research
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1166000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-11-26 (otgw-webui, read-only analysis for the coordinator) in indigo_light's 25 h capture on beta.428+4f303da. Facts file: OTGW-field-data/_comms/the earlier self-tune facts file in OTGW-field-data/_comms (indigo and will_85903 with sources, counters and inference labels); draft question for Sergeant D (on HOLD): OTGW-field-data/_comms/q_integral_held_under_target_DRAFT.md. Observation: five evening hours (25 Nov 17:00 to 22:00) the room sat 0.12 to 0.26 C under the target in continuous mode, flame 97 to 100 %, flow 43 to 46 C; the integral is held at 0.33 outside the 0.1 band by design (entries 329, 333, 414, ADR-259, SATpid.ino _pidUpdateIntegral). The heating curve self-tune did not move the coefficient in 25 h: 2.3 from first to last sample; window 11 of 12 hours (never full); last decision hold; the evening hours were excluded as min_mod (boiler modulation at or below its minimum; the boiler never reported MsgID 15, so the 5 % default applied; relative modulation in those hours 5 to 13 % mean). will_85903 (beta.418, 18 h 40) shows another cause: every hour excluded as no_sun (weather off, no sun elevation), window 0, coefficient 2.0. Open: whether the tune should count low-modulation continuous hours; the exclusion is ADR-205 / entries 322, 340. No code change before Sergeant D's answer is recorded as a register entry. The question waits for the beta.451 capture (entries 494/499/500 changed SAT behaviour since beta.428).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Check will_85903's captures for the same pattern, facts only (the beta.428 run does not exist on disk; beta.410 and beta.418 do): coefficient values, window_hours, excluded_hours per reason, in the facts file
- [ ] #2 Compare with the beta.451 capture of indigo_light when it arrives (same extraction, same facts file): does the coefficient move, which hours are excluded
- [ ] #3 Question to Sergeant D (plain words, scope 2.0.0 or 2.1.0, our recommendation) when the two checks above are in; BlueFlame writes the text from the draft
- [ ] #4 Implement only after his answer is recorded as a register entry before the change (CLAUDE.md SAT rules); host test red on the old code, green on the fix
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-26 AC1 facts (otgw-webui, read-only): will_85903 has no beta.428 run on disk (captures beta.410 and beta.418); beta.418 used. Coefficient 2.0 in all four device-state snapshots and in sat/heating_curve_coeff; tune on and running; window_hours 0; last_decision hold; last_reason day_not_counted; every hour in the covered 8 h excluded as no_sun (counter 5 -> 15; weather off, no sun elevation), min_mod 0. Different cause than indigo (min_mod). Coverage: tune MQTT lines only for 22 Nov 23:42 to 23 Nov 04:26 and 23 Nov 07:26 to 10:19. Full table with sources and inference labels: OTGW-field-data/_comms/the earlier self-tune facts file in OTGW-field-data/_comms.

Facts only (otgw-webui, 2026-11-26; firmware lines are v2.0.0-beta.428, both captures ran beta.428; source file OTGW-field-data/_comms/sat_evening_offset_two_captures.md). (1) Design: a steady offset of 0.1 to 0.3 C under the target is what the current design gives. Final request = curve + P + I + D. Inside |error| <= 0.1 (SAT_PID_DEADBAND) the integral accumulates; at the first tick outside the band it is halved once and then held (SATpid.ino@428:281-310; entries 329, 333, 414; ADR-259). thermo-nova resets it to 0 outside the band (pid.py:187-189, pinned 7beaaf5); the halve-and-hold is the deliberate deviation. In both designs, with the room 0.1 to 0.3 C under target, the integral does not grow, so P (Kp x error) plus the curve carries the offset. Kp = coeff x curve / divisor, Ki = Kp / 8400 (SATpid.ino@428:265-266). The tolerance band (ADR-250, SATcontrol.ino@428:8503-8520) acts only above the target, so it does nothing for a room below it.

Capture A (beta.428, 25 Nov 20:00 to 24:00 local, PWM, target 19.0 all evening; sattick.csv, sattuneh.csv, mqtt.csv): mean error 0.17, 0.17, 0.13, 0.10 C per hour; I range 0.05 to 0.22; P mean 1.5 to 2.6; Kp about 14.4 to 15.0 (sat/kp); requested 30.5 to 31.5 against curve 28.5 to 28.8; duty 0.53 to 0.60 (boiler not saturated, CH on 52 to 56 % of ticks, flow about 38 to 40 in ON phases). Coefficient 2.1 for the whole capture (sat/heating_curve_coeff).

IMPORTANT for TASK-3106: the tune band of 1.0 C (SAT_CT_BAND_C, SATcurveTune.h@428:26; verdict SATcontrol.ino@428:7014-7019, inside_band at :7076) is on the FLOW signal (praw - curve, SATcontrol.ino@428:6824, a flow temperature in C), not on the room error. In capture A the evening hours WERE counted (reason none): flow-signal means +2.59, +2.86, +1.81, +1.40 for the four evening hours, then +0.09, -1.19, (diag_event hour), +0.17, -1.09, -2.23, -0.96, -0.41. The 12-hour window (full 26 Nov 06:02) is diluted by the night and morning hours (-1 to -2) and the decision is hold, reason inside_band (again at 09:02 and 15:02, sat/curve_tune/last_reason). Capture B (beta.428, 25 Nov 17:00 to 22:00, continuous; from the earlier facts file the earlier self-tune facts file in OTGW-field-data/_comms, NOT re-extracted today): room 0.12 to 0.26 C under 20.5, flame 97 to 100 %, flow 43 to 46 (limit 55 not reached), I climbs 0.14 to 0.66 inside the band, halved to 0.33 and held until 22:00, P/error about 29, coefficient 2.3 for 25 h, window 7 to 11 of 12 (never full), evening hours excluded as min_mod (MsgID 15 never seen, 5 % default). So there are two different reasons the curve does not move: capture A counted its evening hours but the 12-hour window mean stays inside the band (inside_band / hold); capture B did not count them (min_mod) and its window never fills. Option C of the draft question (count low-modulation hours) only fixes the capture B case.

Not shown: whether the offset is acceptable (a SAT behaviour question for Sergeant D, not decided here); whether capture A's tune would move the curve if the evening hours were weighted alone, or whether the house needs a higher curve in the evening only (a daily pattern a 12-hour window averages out); capture B's signal on beta.451 (not analysed); whether capture B's boiler could deliver more at flow 43 to 46. Capture B numbers come from the earlier facts file, not from a re-extraction.

Side fact from the Q-428a draft (queued in the SAT decision register, not asked): in capture A the heating-failure check (15 min at the maximum, room rose less than 0.5 C) fired 4 times inside tolerance-band pauses; the self-tune excludes ticks while that event is set (SATcontrol.ino@428 about 8872 to 7006, SATT_DIAG_EVENT, third in order before no_sun, flame_off, min_mod), so hours 02, 11 and 12 of that run drop from this alone and hour 14 probably not (computed from event times, not from a counter). So the self-tune hours are excluded by something other than the heating itself in both captures (capture B: no_sun / min_mod; capture A: heating-failure ticks). Fact only, no conclusion.

2026-11-27 PLAN AC2/AC4 replay (otgw-webui; Sergeant D Q4 = C, entry 502, release 2.1.0, our reading: replay first, build only if the curve moves the right way and not on noise). (1) Harness: compile the REAL pure tuner block (satCurveTune begin/end) + SATcurveTune.h from origin/dev with the stubs of tests/test_sat_curve_selftune.py; a driver reads a per-tick CSV and calls satTuneAddTick/CloseHour/CloseDay exactly as the glue does. (2) Per-tick input from the captures: sattick.csv (room, target, curve, praw -> signal = clamp(praw)-curve, ch, pwm, flame, dhw) + otframes (MsgID 17 relative modulation, MsgID 15 min modulation) ; sun elevation and diag events are not in sattick: taken from the recorded hourly reason in sattuneh.csv (a recorded sun/no_sun/diag_event hour stays excluded in both runs). (3) Baseline must first reproduce the capture's own sattuneh.csv hour by hour (counted/reason/mean); if it does not, the gap is reported and no claim is made. (4) Variant: the min_mod exclusion applies only when the room is NOT under target (error <= 0); one condition in satTuneTickReason, nothing else. (5) Captures: indigo beta.428 (25 h 41 min), marceld91d beta.428, will_85903 beta.428 (no_sun: expected no change), indigo beta.451 run when it arrives. (6) Output: per capture counted hours, window, decisions and coefficient trajectory baseline vs variant, a chart, and a yes/no with the main risk (oversized boiler: low modulation under target may raise the curve). No firmware change before the replay holds (entry 502).

2026-11-27 REPLAY RESULT (otgw-webui; full report OTGW-field-data/_comms/selftune_replay/REPORT.md, chart indigo_beta428_replay.png, tool scripts/sat_selftune_replay.py in lane ba-tool, local). Baseline replay reproduces the captures' own hourly records (marcel 23/23, indigo 25/25 counted flags, will 25/25). Variant (min_mod only when the room is not under target): indigo beta.428 counts 14 instead of 4 hours, window fills at hour 16, window mean +3.06 C (warm-up +3.10, floating +2.85), raise 2.30 -> 2.35; marcel and will unchanged. Threshold sensitivity: error > 0 raises, error > 0.1 counts 9 hours and gives no decision in 25 h. Answer: yes, conditionally (right direction; one house, 25 h; threshold is a Sergeant D choice; beta.451 run not delivered, not analysed). Open-loop limit: only the first decision and the window means are meaningful. AC2 stays open until the beta.451 run is replayed; AC4 not started (no code before the replay holds).

2026-11-27 P4 (replay regression gate, local, lane ba-tool): tests/test_sat_selftune_replay_gate.py + anonymised fixtures tests/data/selftune_replay/{indigo,marcel,will}.txt.gz (8.4 + 6.5 + 5.7 KB gz; numbers only) built by scripts/sat_selftune_fixture.py. 4 tests green on dev: baseline reproduces every captured hourly counted flag, golden counts and decisions (indigo 4 hours / variant 14 hours and raise 2.30 to 2.35 at hour 16; marcel 15/15; will 0/0), fixture hygiene (size < 50 KB, no IP-like numbers, no host text), and two tuner mutants (default minimum modulation 5 to 1, decision band 1.0 to 0.2) make the gate fail. Report: OTGW-field-data/_comms/selftune_proposals_2026-11-27.md.

Register entry 546 (Robert, #dev-sat-mqtt 1576322428009975940, 2026-11-28 19:42 UTC: 'ADD the auto tune counting at very low power now.'): the scope changes from release 2.1.0 to the NEXT BETA. Behaviour unchanged: Sergeant D's entry 513 (Q-3106b B) stands: an hour at very low power counts only when the room is more than 0.1 C below the target; on indigo_light's 25 h capture only 9 hours count (replay, not re-run). The code is not part of the beta.496 build; it goes into the next combined candidate (57c9bf88c plus this task), banner to be named by poc-dev. Host tests first; unit soak before the beta notes say more. Also entry 546: 'Look at the curve advisor.' is a comparison order (5.1.0 has a curve advisor that only suggests a coefficient), no task yet.
<!-- SECTION:NOTES:END -->
