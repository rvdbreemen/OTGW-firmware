---
id: TASK-2036
title: >-
  SAT web UI: the hand-back messages ('... boiler handed back') read as cryptic;
  tester suggests saying SAT is paused/disabled (gruijter)
status: To Do
assignee: []
created_date: '2026-11-18T21:24:00Z'
labels:
  - sat
  - webui
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1044000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter, #alpha-testing 2026-11-18 21:06 UTC (https://discord.com/channels/812969634638725140/1514720723980259460/1572719799446802463), on beta.375 after 'No outside temperature for 1 hour: boiler handed back': 'Boiler handed back is cryptic. SAT disabled would make more sense to me.' The same phrase is used for the OpenTherm loss (v2.html satOtLost, satOutsideMissing; v2.js sat_ot_lost, sat_outside_missing; narration in SATcontrol.ino:4484). Note: SAT is not disabled in that state, it stays enabled and resumes by itself (entries 56, 57; SATcontrol.ino:4476-4492), so 'disabled' would be inaccurate; candidate wording e.g. 'SAT paused: no outside temperature for 1 hour; your thermostat controls the boiler'. Tester feedback, needs Robert's OK.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert decides the wording (recorded in the task notes)
- [ ] #2 The web UI texts sat_ot_lost and sat_outside_missing (and the NL/DE packs) use the chosen wording; web UI tests green
<!-- AC:END -->
