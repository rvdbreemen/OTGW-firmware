---
id: TASK-1081
title: Add device_class water to HA DHW flowrate sensor
status: To Do
assignee: []
created_date: '2026-08-24 18:12'
updated_date: '2026-08-24 18:13'
labels:
  - enhancement
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/675'
priority: low
ordinal: 183000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
GitHub #675 (Jeroenll): DHW Water Flow Rate In DHW Circuit MQTT discovery config lacks device_class: water. Adding it lets the entity be added to the HA Energy dashboard.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 DHW flowrate sensor discovery config includes device_class: water
- [ ] #2 Entity remains selectable/usable in HA Energy dashboard water section
<!-- AC:END -->
