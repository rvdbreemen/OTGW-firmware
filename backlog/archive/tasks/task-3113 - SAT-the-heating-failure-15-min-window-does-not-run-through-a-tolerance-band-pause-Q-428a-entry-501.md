---
id: TASK-3113
title: >-
  SAT: the heating-failure 15 min window does not run through a tolerance-band
  pause (Q-428a, entry 501)
status: To Do
assignee: []
created_date: '2026-11-27T05:27:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1170000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert chose A on Q-428a (2026-11-27, entry 501, Sergeant D asked, not answered): the heating-failure warning must not count a tolerance-band pause; hours are then not flagged/dropped from the self-tune because of it (entry 358). satDiagEventsStep: bandOff restarts the window, no new flag in a pause, a set flag stays until the next full window.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test red on the old code (a band pause cooling is flagged as a failure) and green on the fix, same test file
- [ ] #2 Sergeant D's answer recorded before push (entry 501 is the maintainer's choice)
- [ ] #3 Not verified on a unit; check on the next capture with a band pause at maximum duty
<!-- AC:END -->
