---
id: TASK-46
title: Climate preset sync to secondary entities
status: To Do
assignee: []
created_date: '2026-04-05 21:06'
labels:
  - sat
  - feature
dependencies: []
references:
  - 'other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py:79'
  - other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add CONF_SYNC_CLIMATES_WITH_PRESET setting. When the SAT climate entity changes preset (e.g. away, comfort, sleep), synchronize the preset to all configured secondary climate entities. Enables whole-home preset coordination.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 CONF_SYNC_CLIMATES_WITH_PRESET setting added to configuration
- [ ] #2 When SAT preset changes, all secondary climate entities receive the same preset
- [ ] #3 Secondary climate entities configurable via settings
- [ ] #4 Setting persisted across reboots
- [ ] #5 Graceful handling when secondary entity does not support the preset
<!-- AC:END -->
