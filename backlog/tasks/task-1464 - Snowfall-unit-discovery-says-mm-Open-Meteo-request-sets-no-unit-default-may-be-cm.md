---
id: TASK-1464
title: >-
  Snowfall unit: discovery says mm, Open-Meteo request sets no unit (default may
  be cm)
status: To Do
assignee: []
created_date: '2026-10-29T07:22:00Z'
labels:
  - sat
  - weather
  - mqtt
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 624000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. MQTTHaDiscovery.cpp:1369 announces mm; SATweather.ino:68-75 sets no snowfall unit. The Open-Meteo default unit for snowfall is unverified (the auditor wrote it from memory).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Open-Meteo documentation checked (link in notes); the request or the announced unit fixed so they match
<!-- AC:END -->
