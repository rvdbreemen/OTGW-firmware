---
id: TASK-1035
title: >-
  Fix: gateway_mode + otgw_connected HA discovery never enqueued at boot (faux
  id 244)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-09 19:23'
labels: []
dependencies: []
ordinal: 155000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The 1.7.1 gateway_mode/otgw_connected binary-sensors (pseudo-ID 244) are added to the HA discovery table but publishNonOTDiscoveryConfigs() never calls setMQTTConfigPending(244), so the discovery configs are never published on boot/reconnect and the entities never appear in Home Assistant (only via manual /republish). Found by adversarial review of v1.7.0..HEAD; matches the faux-sensor-id-boot-publish trap (TASK-942).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A named pseudo-ID constant for 244 exists (no bare literal)
- [ ] #2 publishNonOTDiscoveryConfigs() enqueues the 244 constant so gateway_mode + otgw_connected discovery configs publish on boot and reconnect
- [ ] #3 python build.py --firmware exits 0
- [ ] #4 python evaluate.py --quick shows no new failures
<!-- AC:END -->
