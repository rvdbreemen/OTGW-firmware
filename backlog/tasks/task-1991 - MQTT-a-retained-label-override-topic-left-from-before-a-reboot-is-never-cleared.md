---
id: TASK-1991
title: >-
  MQTT: a retained <label>/override topic left from before a reboot is never
  cleared
status: To Do
assignee: []
created_date: '2026-11-15T22:35:00Z'
labels:
  - mqtt
  - bug
dependencies: []
priority: low
ordinal: 1014000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Edge left by TASK-1985 (coordinator 2026-11-15: separate, low). The ADR-118 override store is RAM only; after a reboot it is empty, so a <label>/override topic that was retained before the reboot is never cleared by publishActiveOverrides() (it only clears entries it holds). HA keeps showing that old value. Possible approach: at MQTT connect (connect burst), publish an empty retained message once for each label that can carry an override (the ids recordOTOverride() is called for), before the first periodic publish; an override still active re-publishes its value within 60 s. Check the burst windowing rules (ADR-088) for the connect path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After a reboot with a retained <label>/override on the broker and no active override, the topic is gone within the first minute after connect (host test on the connect path, red on the current code)
- [ ] #2 An override active after the reboot is published again within the 60 s timer
<!-- AC:END -->
