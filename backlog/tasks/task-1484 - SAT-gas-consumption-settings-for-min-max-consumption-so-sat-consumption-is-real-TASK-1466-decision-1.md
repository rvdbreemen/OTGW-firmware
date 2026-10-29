---
id: TASK-1484
title: >-
  SAT gas consumption: settings for min/max consumption so sat/consumption is
  real (TASK-1466 decision 1)
status: To Do
assignee: []
created_date: '2026-10-29T20:29:00Z'
labels:
  - sat
  - mqtt
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 643000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-29: wire up sat/consumption. Today minCons/maxCons are hard-coded 0 (SATcontrol.ino:3708-3720), so the topic is always 0. Add two persisted settings (min and max gas consumption in m3/h at min/max modulation), REST + v2 UI fields (Technical), settingsHelp, MQTT.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With min/max set, sat/consumption follows the modulation formula (host test red on HEAD: always 0); with 0/0 the topic is not published or stays 0 by design, documented; settings persist and load
<!-- AC:END -->
