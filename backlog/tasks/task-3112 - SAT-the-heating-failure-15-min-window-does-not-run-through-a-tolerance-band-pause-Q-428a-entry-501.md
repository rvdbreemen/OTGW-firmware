---
id: TASK-3112
title: >-
  SAT: the heating-failure 15 min window does not run through a tolerance-band
  pause (Q-428a, entry 501)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-27T05:22:00Z'
updated_date: '2026-11-27T05:31:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1169000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert chose A on Q-428a (2026-11-27, entry 501, Sergeant D asked, not answered): the heating-failure warning must not count a tolerance-band pause; hours are then not flagged/dropped from the self-tune because of it (entry 358). satDiagEventsStep: bandOff restarts the window, no new flag in a pause, a set flag stays until the next full window.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test red on the old code (a band pause cooling is flagged as a failure) and green on the fix, same test file
- [ ] #2 Sergeant D's answer recorded before push (entry 501 is the maintainer's choice)
- [ ] #3 Not verified on a unit; check on the next capture with a band pause at maximum duty
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-27 (otgw-webui): code in lane ba-428a, local, NOT pushed. satDiagEventsStep: SatDiagIn.bandOff (the band holds CH off; not a PWM OFF phase, which would otherwise stop the warning at max PWM duty) restarts the window, no new flag in a pause, a flag already set stays until the next full window. Test: tests/test_sat_diag_events.py red on old (hf_band flagged a failure at tick 30 from the band-pause cooling), green 16/16; test_sat_ch_enable_band_required 4, test_sat_cold_cut_min_burn 8. Risk: a real failure that starts inside a band pause is flagged 15 min after CH is asked again. Entries 358 and 501. Push only after Sergeant D answers or Robert says go.
<!-- SECTION:NOTES:END -->
