---
id: TASK-1888
title: SAT continuous->PWM threshold 1.5 C / 30 s (entry 327)
status: To Do
assignee: []
created_date: '2026-11-13T07:28:00Z'
updated_date: '2026-11-15T21:11:00Z'
labels:
  - sat
  - adr
  - blocked
milestone: 2.0.0
dependencies: []
ordinal: 961000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D decision 2026-11-13 (https://discord.com/channels/812969634638725140/1105556725714649128/1570699609200787459), entry 327 in docs/sat/sergeant-d-corrections.md: switch from continuous to PWM when flow - requested setpoint >= 1.5 C for 30 s, superseding entry 22 (2.0 C / 60 s). Today: settings.sat.fOvershootMargin (2.0) and SAT_OVERSHOOT_SUSTAIN_MS (60 s), SATcycles.ino:1409-1415; entry-289 fail-safe 3.5 C / 30 s at SATcycles.ino:1416-1419. thermo-nova: heating_control.py:262-300, OVERSHOOT_MARGIN_CELSIUS 3.0 at cycles/const.py:10, OVERSHOOT_SUSTAIN_SECONDS 60 at const.py:21. Entry 37 (classifier margin) not touched. Blocked on Sergeant D's answers to the open points (fail-safe of entry 289; fOvershootMargin key and default), asked one at a time after Q-326a.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Plan with the thermo-nova citations (heating_control.py:262-300, cycles/const.py:10, const.py:21) and SATcycles.ino:1409-1419, and Sergeant D's OK on the open points (entry-289 fail-safe; fOvershootMargin key and default) before code
- [ ] #2 ADR that supersedes the entry-22 rule (2.0 C / 60 s)
- [ ] #3 Host test on the real code: red on the old rule (2.0 C / 60 s), green on the new rule (1.5 C / 30 s)
- [ ] #4 Bench test by the bench-tester
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Q-327a and Q-327b decided 2026-11-15 by the coordinator under Robert's directive (CLI) 'Do not wait for Sergeant D. Finish everything that can be finished.'; Sergeant D may overrule, his answer later becomes a follow-up task; asked in the 18-question batch in #dev-sat-mqtt (items 6-7, https://discord.com/channels/812969634638725140/1105556725714649128/1571633456381694074): Q-327a A (keep the 3.5 C / 30 s fail-safe next to the 1.5 C rule of entry 327), Q-327b A (a separate value for the switch, fOvershootMargin keeps 2.0 for the classifier, entry 37 unchanged). Unblocked.
<!-- SECTION:NOTES:END -->
