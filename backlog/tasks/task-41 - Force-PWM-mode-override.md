---
id: TASK-41
title: Force PWM mode override
status: To Do
assignee: []
created_date: '2026-04-05 21:05'
labels:
  - sat
  - feature
dependencies: []
references:
  - 'other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py:80'
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/entry_data.py:149
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add manual toggle CONF_FORCE_PULSE_WIDTH_MODULATION to force PWM operation even when boiler is in continuous modulation mode. User-facing setting in WebUI.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 CONF_FORCE_PULSE_WIDTH_MODULATION setting added to configuration
- [ ] #2 Toggle accessible from WebUI settings page
- [ ] #3 When enabled, PWM mode is forced regardless of boiler modulation mode
- [ ] #4 Setting persisted across reboots
<!-- AC:END -->
