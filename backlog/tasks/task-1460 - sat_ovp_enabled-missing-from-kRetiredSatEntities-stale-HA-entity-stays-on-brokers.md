---
id: TASK-1460
title: >-
  sat_ovp_enabled missing from kRetiredSatEntities: stale HA entity stays on
  brokers
status: To Do
assignee: []
created_date: '2026-10-29T07:19:00Z'
labels:
  - sat
  - mqtt
  - ha-discovery
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 620000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. OVP was removed in ad8f7fb9d (TASK-891.6) but sat_ovp_enabled is not in kRetiredSatEntities (MQTTHaDiscovery.cpp:3744-3748), so brokers keep its retained discovery config. Check the other removed OVP entities too.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The OVP entities are listed in kRetiredSatEntities and test/host/test_ha_discovery_json.py covers their empty retained publish
<!-- AC:END -->
