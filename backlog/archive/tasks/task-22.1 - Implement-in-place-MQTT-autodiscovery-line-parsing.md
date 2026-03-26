---
id: TASK-22.1
title: Implement in-place MQTT autodiscovery line parsing
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
Refactor MQTT autodiscovery parsing to keep the raw mqttha.cfg line in one buffer and expose parsed topic/message template pointers without copying into a second large scratch buffer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Bulk and per-msgid autodiscovery can parse config lines without a dedicated rendered message buffer
- [x] #2 Parsing preserves current comment trimming and field validation behavior
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented in-place mqttha.cfg line parsing via MQTTAutoConfigLineView so topic and message templates are referenced directly inside the shared line buffer. Comment trimming and delimiter validation remain in parseAutoConfigLine().
<!-- SECTION:NOTES:END -->
