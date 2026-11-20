---
id: TASK-2973
title: >-
  HA entity display names: curated plain-language names for Sensors and Controls
  (HA review H2)
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-20T15:56:00Z'
labels:
  - mqtt
  - ha
  - ux
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1105000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2970 HA review (docs/reviews/2026-11-20_ux-ha-entities/README.md, H2); umbrella TASK-2970. writeFriendlyName() (MQTTHaDiscovery.cpp:2359-2370) only replaces _ by a space and capitalises word starts, so HA shows code labels such as 'Max Capacity Min Mod Level HB U8', 'Status VH Master', 'S0 Power Kw'. Milestone 2.1.0 by coordinator decision (too big for 2.0.0 stable). Uses the H4 answer for role-based names.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every row that stays in Sensors or Controls after the H1 tidy-up has a curated display name in sentence case, without code abbreviations or units (units in unit_of_measurement)
- [ ] #2 Topics and uniq_id are unchanged (golden topic test green); existing entity ids stay, only the shown names change
- [ ] #3 Duplicate quantities get role-based names (for example 'Room temperature (thermostat)', 'Room temperature used by SAT') per the H4 answer
<!-- AC:END -->
