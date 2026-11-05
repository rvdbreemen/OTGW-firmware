---
id: TASK-1663
title: >-
  SAT narrate line truncated: 'Heating off waits for the 180 s minimum burn' cut
  at 'cold setpoint 2'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T20:23:00Z'
updated_date: '2026-11-05T20:38:00Z'
labels:
  - sat
  - logging
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 786000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench .61 beta.101 (TASK-1652): the telnet narration from satNarratef_ (SATcontrol.ino ~113) ends at '...is at or below the cold setpoint 2'; the buffer is too short for this message. Shorter narration lines print in full.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The longest satNarratef_ message prints in full (host test or bench line)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05: cause SAT_NARRATE_BUF 96 (95 chars) in SATcontrol.ino; the 1652 line and the self-tune line both reach 102 chars worst case (telnet 320 B and the WS log buffer 512 B do not cut). Fix: SAT_NARRATE_BUF 128 (+128 B static for the 4-slot WS ring, +32 B per transient stack buffer). New tests/test_sat_narrate_fits.py reads every satNarrate*_P format from the SAT sources and checks its worst-case expansion (every %s needs a stated bound) against SAT_NARRATE_BUF - 1: old code FAIL (lines 6262 and 7321 at 102 > 95), fix OK. test_sat_ws_event_ring, test_sat_pwm_anchors, test_sat_cold_cut_min_burn OK; evaluate --quick 0 failed. Not built or bench-tested: handed to the lead for bump/build/bench (bench: the 1652 narration prints in full on telnet).
<!-- SECTION:NOTES:END -->
