---
id: TASK-1663
title: >-
  SAT narrate line truncated: 'Heating off waits for the 180 s minimum burn' cut
  at 'cold setpoint 2'
status: To Do
assignee: []
created_date: '2026-11-05T20:23:00Z'
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
- [ ] #1 The longest satNarratef_ message prints in full (host test or bench line)
<!-- AC:END -->
