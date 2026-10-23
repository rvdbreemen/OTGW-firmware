---
id: TASK-1310
title: >-
  SAT page: put end-user controls first, move Hot water and Burner cycle mode
  down or behind an advanced toggle
status: To Do
assignee: []
created_date: '2026-10-23T07:51:00Z'
labels:
  - webui
  - sat
  - ux
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 433000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter 2026-10-23 07:40 (#alpha-testing, screenshot of the SAT Thermostat tab): the Hot water card (standalone DHW control + DHW setpoint) and the Burner cycle mode card sit prominently near the top although they are for edge cases; burner cycle mode is not something an end user should need. General principle from the tester: the interface targets OTGW users, not developers: end-user functions and information first, advanced info and configuration further down or hidden until opened. UI-only change (no SAT control behaviour change).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On the SAT Thermostat (simple) tab the dial, presets and the user-facing status come first; Hot water and Burner cycle mode move below the history or into a collapsed 'Advanced' section (or to the Control tab), decision recorded in the task (before/after screenshot)
- [ ] #2 No control behaviour or setting changes: the same REST calls are made by the moved controls (tests/webui or bench check)
<!-- AC:END -->
