---
id: TASK-48
title: Target temperature step configuration
status: To Do
assignee: []
created_date: '2026-04-05 21:06'
labels:
  - sat
  - feature
dependencies: []
references:
  - 'other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py:81'
  - 'other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py:348'
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add CONF_TARGET_TEMPERATURE_STEP setting (default 0.5C) to control the UI stepping granularity for target temperature in WebUI. Some users prefer 0.1C precision, others prefer 1.0C steps.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 CONF_TARGET_TEMPERATURE_STEP setting added with default 0.5C
- [ ] #2 WebUI target temperature controls use configured step size
- [ ] #3 Valid range enforced (0.1C to 1.0C)
- [ ] #4 Setting persisted across reboots
<!-- AC:END -->
