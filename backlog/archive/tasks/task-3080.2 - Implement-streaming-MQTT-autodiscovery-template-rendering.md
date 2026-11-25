---
id: TASK-22.2
title: Implement streaming MQTT autodiscovery template rendering
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
Add helpers to measure rendered discovery payload length and stream rendered template content directly to PubSubClient so large JSON payloads are no longer materialized in a dedicated msg buffer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Rendered payload length is measured before beginPublish
- [x] #2 Rendered payload bytes are written directly from the template plus replacement tokens
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented streaming MQTT template rendering helpers that measure rendered payload length before beginPublish() and write rendered chunks directly to PubSubClient without materializing a full message buffer.
<!-- SECTION:NOTES:END -->
