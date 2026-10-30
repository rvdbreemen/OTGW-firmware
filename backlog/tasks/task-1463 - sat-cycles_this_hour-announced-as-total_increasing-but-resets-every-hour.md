---
id: TASK-1463
title: sat/cycles_this_hour announced as total_increasing but resets every hour
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T07:21:00Z'
updated_date: '2026-10-30T20:12:00Z'
labels:
  - sat
  - mqtt
  - ha-discovery
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 623000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. MQTTHaDiscovery.cpp:1341 announces state_class total_increasing; the value resets hourly, which HA reads as a meter reset each hour.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 state_class matches the hourly-reset semantics (measurement), with the reason recorded
<!-- AC:END -->
