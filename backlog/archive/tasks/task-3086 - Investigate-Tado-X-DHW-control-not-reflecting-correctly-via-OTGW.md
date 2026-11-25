---
id: TASK-3086
title: 'Investigate: Tado X DHW control not reflecting correctly via OTGW'
status: To Do
assignee: []
created_date: '2026-04-09 20:17'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'Discord #beta-testing, user crashevans'
  - 'Discord #english-support, user crashevans'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
User "crashevans" in #beta-testing and #english-support reported that DHW (domestic hot water) setpoint is not propagating correctly when using Tado X with OTGW. Two sub-issues: (1) DHW setpoint not propagating correctly with Tado X thermostat, (2) MaxRelModLevelSetting cycling between values (noted by Sergeant D as a known Ideal boiler limitation, not an OTGW bug). User shared 8 log attachments in Discord but these are not readable via MCP image attachments.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 DHW setpoint from Tado X is correctly forwarded and reflected in MQTT/HA
- [ ] #2 MaxRelModLevelSetting cycling behavior is documented or resolved
<!-- AC:END -->

<!-- renumbered from TASK-243 (duplicate id; Robert DM 1575165989010804807, 2026-11-25). The file in backlog/tasks keeps TASK-243. -->
