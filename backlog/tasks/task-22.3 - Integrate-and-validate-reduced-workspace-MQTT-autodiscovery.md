---
id: TASK-22.3
title: Integrate and validate reduced-workspace MQTT autodiscovery
status: Done
assignee:
  - '@github-copilot'
created_date: '2026-03-18 20:25'
updated_date: '2026-03-18 20:58'
labels:
  - mqtt memory
dependencies: []
parent_task_id: TASK-22
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Wire the new parsing and streaming helpers into bulk discovery, JIT discovery, and source-template expansion, then validate the firmware build and runtime-safety assumptions.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTT autodiscovery workspace no longer keeps dedicated msg and savedTopic buffers
- [x] #2 Firmware build passes after the refactor
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Integrated the reduced-workspace helpers into bulk discovery, per-msgid discovery, and source-template expansion. Removed dedicated msg and savedTopic buffers from MQTTAutoConfigBuffers and validated the refactor with python build.py --firmware.
<!-- SECTION:NOTES:END -->
