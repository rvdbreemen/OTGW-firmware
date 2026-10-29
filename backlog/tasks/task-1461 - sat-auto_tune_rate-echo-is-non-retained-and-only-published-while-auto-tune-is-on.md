---
id: TASK-1461
title: >-
  sat/auto_tune_rate echo is non-retained and only published while auto-tune is
  on
status: To Do
assignee: []
created_date: '2026-10-29T07:20:00Z'
labels:
  - sat
  - mqtt
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 621000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. Unlike the other settings echoes (SATcontrol.ino:3538), auto_tune_rate is published non-retained and only while auto-tune runs (:3545-3551).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 sat/auto_tune_rate is retained and published unconditionally like the other settings echoes
<!-- AC:END -->
