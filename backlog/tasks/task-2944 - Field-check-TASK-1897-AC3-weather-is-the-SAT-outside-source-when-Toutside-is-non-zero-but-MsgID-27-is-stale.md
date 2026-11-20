---
id: TASK-2944
title: >-
  Field check TASK-1897 AC3: weather is the SAT outside source when Toutside is
  non-zero but MsgID 27 is stale
status: To Do
assignee: []
created_date: '2026-11-20T01:29:00Z'
labels:
  - sat
  - field
milestone: 2.1.0
dependencies: []
ordinal: 1075000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1897 AC3 (host-proven by tests/test_weather_worker_handoff.py C6, red on the old gate; AC2 confirmed by the reporter; not benchable on .61, loopback never asks MsgID 27). Needs a unit where the boiler's MsgID 27 stops for over 1 h while Toutside stays non-zero. Waived for 2.0.0 by Robert, Discord DM 1573146152524587149 ('A', reply to the coordinator's question 1573145337458065529): close the 2.0.0 task with a field waiver and one 2.1.0 follow-up per field AC.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With Toutside non-zero but no MsgID 27 stamp younger than 1 h (sensor gone, or before the first stamp), /api/v2/sat/weather valid=true after the first fetch and the SAT outside source is the weather (osrc=W) (field or bench capture)
<!-- AC:END -->
