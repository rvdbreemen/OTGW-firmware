---
id: TASK-840
title: Add transcript prefix and firmware version to MQTT capture filename
status: In Progress
assignee:
  - '@codex'
created_date: '2026-06-07 10:37'
labels:
  - tooling
  - diagnostics
dependencies: []
references:
  - scripts/capture-mqtt-debug.bat
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow up the MQTT capture transcript naming so the final artifact is clearly labeled for users and includes firmware version metadata.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The final merged transcript filename starts with transcript.
- [ ] #2 The final merged transcript filename includes the firmware version when discoverable.
- [ ] #3 Filename generation falls back safely when firmware version metadata is unavailable.
- [ ] #4 Validation covers syntax, help/runtime path, and metadata-derived filename formatting.
<!-- AC:END -->
