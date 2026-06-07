---
id: TASK-839
title: Quiet MQTT capture tool errors
status: In Progress
assignee:
  - '@codex'
created_date: '2026-06-07 10:06'
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
Update the MQTT diagnostic capture script so noisy tool stderr is captured separately, folded into the final merged transcript, and the merged artifact filename identifies the capture context.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Tool stderr is captured to a separate error.txt file instead of being printed to the console during normal capture.
- [ ] #2 The merged transcript includes the captured error log content so a single artifact contains the full diagnostic context.
- [ ] #3 The final transcript filename is based on date-time, hostname, and the unique device ID.
- [ ] #4 Existing clean-stop behavior remains available and validation covers the script path.
<!-- AC:END -->
