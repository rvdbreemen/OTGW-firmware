---
id: TASK-1486
title: >-
  HA discovery audit: decide an entity per sat/* topic that has none (~90
  topics) (TASK-1466 decision 6)
status: To Do
assignee: []
created_date: '2026-10-29T20:31:00Z'
labels:
  - sat
  - mqtt
  - ha
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 645000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-29: full audit of all sat/* MQTT topics without a Home Assistant entity (153 published, 63 discovered, 2026-10-29 count). Per topic: keep MQTT-only (diagnostic) or add an entity (type, device class, entity category), with thermo-nova's HA entities (binary_sensor.py, sensor.py) as reference. Output: a table in docs, then a task for the additions. Health/sync entities follow the ON = problem convention (TASK-1466 decision 4).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A reviewed table lists every undiscovered sat/* topic with a decision and reason; additions filed as one implementation task; flash/heap cost estimated
<!-- AC:END -->
