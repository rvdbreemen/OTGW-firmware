---
id: TASK-20
title: Reduce persistent RAM used by webhook payload expansion
status: To Do
assignee: []
created_date: '2026-03-18 19:44'
labels:
  - memory webhook safety
dependencies: []
references:
  - 'src/OTGW-firmware/webhook.ino:186'
  - >-
    backlog/tasks/task-8%20-%20Fix-undersized-buffers-overflowCountBuf-MQTT-payload-webhook-expansion.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
webhook.ino keeps a permanent expandedPayload[384] static buffer to build POST bodies from the webhook template. TASK-8 increased this buffer for safety, but the cost is now permanent RAM. This task evaluates whether webhook expansion can use a caller-owned scratch buffer, a smaller bounded workspace, or streamed/template-segment emission while preserving safe expansion and without reintroducing truncation bugs. The target is to reclaim roughly 256-384 bytes of persistent RAM.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Persistent RAM used by webhook payload expansion is reduced by at least 256 bytes
- [ ] #2 Webhook payload expansion remains safe for the currently supported placeholder set
- [ ] #3 No silent truncation regression is introduced
- [ ] #4 GET behavior for empty payload templates remains unchanged
<!-- AC:END -->
