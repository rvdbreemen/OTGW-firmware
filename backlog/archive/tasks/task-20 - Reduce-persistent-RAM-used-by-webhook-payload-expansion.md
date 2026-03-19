---
id: TASK-20
title: Reduce persistent RAM used by webhook payload expansion
status: Done
assignee:
  - '@copilot'
created_date: '2026-03-18 19:44'
updated_date: '2026-03-18 21:37'
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
- [x] #1 Persistent RAM used by webhook payload expansion is reduced by at least 256 bytes
- [x] #2 Webhook payload expansion remains safe for the currently supported placeholder set
- [x] #3 No silent truncation regression is introduced
- [x] #4 GET behavior for empty payload templates remains unchanged
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect webhook payload expansion and the TASK-8 safety constraints to identify the smallest change that removes retained RAM without changing webhook behavior.
2. Refactor webhook send logic so the expansion workspace is caller-owned during the send attempt instead of being retained for the full runtime, while keeping the bounded 384-byte safety margin.
3. Add explicit truncation detection and logging for expanded payloads so oversized templates remain observable instead of silently regressing.
4. Build the firmware to verify compile success and confirm the RAM reduction is reflected in the resulting memory usage.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Moved the webhook POST expansion workspace from static storage into a helper-local buffer so it only exists during an active POST send attempt.
Added explicit truncation detection in expandPayload() and a timestamped debug warning when the expanded payload hits the 384-byte bound.
Validated with python.exe .\build.py --firmware: global RAM dropped from 58452 bytes to 58068 bytes, reclaiming 384 bytes while keeping the GET path unchanged.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reduced retained webhook RAM by moving the 384-byte expanded payload buffer out of static lifetime and into the active POST send path in src/OTGW-firmware/webhook.ino. The supported placeholder set and GET-with-empty-payload behavior are unchanged, and payload expansion now emits an explicit truncation warning instead of truncating silently. 

Validation:
- python.exe .\build.py --firmware
- Global RAM: 58452 -> 58068 bytes (-384)
- Remaining dynamic memory: 23852 bytes
<!-- SECTION:FINAL_SUMMARY:END -->
