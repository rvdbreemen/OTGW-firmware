---
id: TASK-1396
title: >-
  capture-mqtt-debug: send q/D only after setup finished, keep hd_min_* in the
  device/info probe
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-25T18:40:00Z'
updated_date: '2026-10-29T21:42:00Z'
labels:
  - tooling
  - capture
milestone: 2.0.0
dependencies: []
ordinal: 535000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From TASK-1339 (2026-10-25): the capture scripts send 'q' and 'D' right after a telnet reconnect (capture-mqtt-debug.sh:1018, :920-923), which can run during setup() and stall telnet (see sibling task); and the HTTP probe log cuts each body at 200 characters (capture-mqtt-debug.sh:574), so hd_min_free_heap / hd_min_max_block (restAPI.ino:3325-3326) never reach the transcript. Fix both in .sh, macOS .sh and .bat.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 q/D are sent only after 'Setup finished!' is seen (or uptime > 0)
- [ ] #2 The device/info probe keeps hd_min_free_heap and hd_min_max_block in the transcript
<!-- AC:END -->
