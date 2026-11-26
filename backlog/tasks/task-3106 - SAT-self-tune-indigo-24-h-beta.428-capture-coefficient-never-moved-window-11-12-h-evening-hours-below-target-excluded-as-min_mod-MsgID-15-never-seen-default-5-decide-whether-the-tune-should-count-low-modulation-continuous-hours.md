---
id: TASK-3106
title: >-
  SAT self-tune: indigo 24 h beta.428 capture: coefficient never moved (window
  11/12 h), evening hours below target excluded as min_mod (MsgID 15 never seen,
  default 5 %): decide whether the tune should count low-modulation continuous
  hours
status: To Do
assignee: []
created_date: '2026-11-26T13:36:00Z'
updated_date: '2026-11-26T13:38:00Z'
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
Found 2026-11-26 (otgw-webui, read-only analysis for the coordinator) in indigo_light's 25 h capture on beta.428+4f303da. Facts file: OTGW-field-data/_comms/selftune_indigo_will_facts.md (indigo and will_85903 with sources, counters and inference labels); draft question for Sergeant D (on HOLD): OTGW-field-data/_comms/q_integral_held_under_target_DRAFT.md. Observation: five evening hours (25 Nov 17:00 to 22:00) the room sat 0.12 to 0.26 C under the target in continuous mode, flame 97 to 100 %, flow 43 to 46 C; the integral is held at 0.33 outside the 0.1 band by design (entries 329, 333, 414, ADR-259, SATpid.ino _pidUpdateIntegral). The heating curve self-tune did not move the coefficient in 25 h: 2.3 from first to last sample; window 11 of 12 hours (never full); last decision hold; the evening hours were excluded as min_mod (boiler modulation at or below its minimum; the boiler never reported MsgID 15, so the 5 % default applied; relative modulation in those hours 5 to 13 % mean). will_85903 (beta.418, 18 h 40) shows another cause: every hour excluded as no_sun (weather off, no sun elevation), window 0, coefficient 2.0. Open: whether the tune should count low-modulation continuous hours; the exclusion is ADR-205 / entries 322, 340. No code change before Sergeant D's answer is recorded as a register entry. The question waits for the beta.451 capture (entries 494/499/500 changed SAT behaviour since beta.428).
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
2026-11-26 AC1 facts (otgw-webui, read-only): will_85903 has no beta.428 run on disk (captures beta.410 and beta.418); beta.418 used. Coefficient 2.0 in all four device-state snapshots and in sat/heating_curve_coeff; tune on and running; window_hours 0; last_decision hold; last_reason day_not_counted; every hour in the covered 8 h excluded as no_sun (counter 5 -> 15; weather off, no sun elevation), min_mod 0. Different cause than indigo (min_mod). Coverage: tune MQTT lines only for 22 Nov 23:42 to 23 Nov 04:26 and 23 Nov 07:26 to 10:19. Full table with sources and inference labels: OTGW-field-data/_comms/selftune_indigo_will_facts.md.
<!-- SECTION:NOTES:END -->
