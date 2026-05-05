---
id: TASK-541
title: 'feat-2.0.0: port TASK-540 — add HA discovery for diagnostic MQTT topics'
status: To Do
assignee: []
created_date: '2026-05-05 05:45'
labels:
  - mqtt
  - ha-discovery
  - diagnostic
  - port
  - feat-2.0.0
dependencies:
  - TASK-540
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port TASK-540 (HA discovery for otgw-firmware/* and otgw-pic/* diagnostic topics) to feature-dev-2.0.0-otgw32-esp32-sat-support.\n\nCheck whether the 2.0.0 branch publishes additional or differently-named diagnostic topics (ESP32, OTDirect, SAT). If so, include those in the discovery set. Also confirm the discovery framework on 2.0.0 still uses MqttHaSensorCfg or has been refactored — adapt accordingly.\n\nReference dev-branch implementation: TASK-540.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All topic groups from TASK-540 also covered on 2.0.0 (firmware/*, firmware/stats/*, pic/*, pic/settings/*)
- [ ] #2 Any 2.0.0-specific diagnostic topics (ESP32 / OTDirect / SAT) added to the discovery set
- [ ] #3 All new entities attach to the OTGW HA device and use entity_category=diagnostic
- [ ] #4 Build 2.0.0 firmware + filesystem successfully
- [ ] #5 evaluate.py --quick shows no new failures
<!-- AC:END -->
