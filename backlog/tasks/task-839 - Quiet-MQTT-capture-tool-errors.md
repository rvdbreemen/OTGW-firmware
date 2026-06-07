---
id: TASK-839
title: Quiet MQTT capture tool errors
status: In Progress
assignee:
  - '@codex'
created_date: '2026-06-07 10:06'
updated_date: '2026-06-07 10:07'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect the embedded PowerShell capture flow for process stderr and transcript merge behavior.
2. Add a capture-wide error.txt log and redirect noisy tool stderr into it while keeping console status output readable.
3. Rename the final merged transcript using timestamp, device host, and unique device ID metadata when available.
4. Validate help/syntax and a short noninteractive capture path, then update acceptance criteria and notes.
<!-- SECTION:PLAN:END -->
