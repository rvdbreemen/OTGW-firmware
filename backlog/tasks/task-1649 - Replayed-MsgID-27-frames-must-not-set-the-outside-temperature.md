---
id: TASK-1649
title: Replayed MsgID 27 frames must not set the outside temperature
status: To Do
assignee: []
created_date: '2026-11-05T06:57:00Z'
labels:
  - bug
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 773000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
https://discord.com none; found by TASK-1412.20 (R-3): OTGW-Core.ino:6076-6078 stamps the outside temperature from a replayed MsgID 27 frame (no bReplayed check), while docs/api/MQTT.md says live Read-Ack only. Robert 2026-11-05: bug task.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Replayed MsgID 27 leaves the outside temperature unchanged; live Read-Ack still sets it
- [ ] #2 Host test red on old / green on fix calling the real code path; evaluate --quick green
<!-- AC:END -->
