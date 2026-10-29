---
id: TASK-1396
title: >-
  capture-mqtt-debug: send q/D only after setup finished, keep hd_min_* in the
  device/info probe
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T18:40:00Z'
updated_date: '2026-10-29T23:03:00Z'
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
- [x] #1 q/D are sent only after 'Setup finished!' is seen (or uptime > 0)
- [x] #2 The device/info probe keeps hd_min_free_heap and hd_min_max_block in the transcript
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed in the new capture tool (TASK-1475, ADR-202), which replaces the .bat/.sh payloads. AC#1: TelnetCapture.maybe_send_dump sends q/D only after 'Setup finished!' is seen on the connection or /api/v2/health reports uptime > 0 min. Red/green in docs/evidence/task1475/red-1396.txt: against a device still in setup, the old .sh worker sent '13qD', the new tool '1313' (no q/D). Host test test_i also shows the release once 'Setup finished!' arrives. AC#2: the /api/v2/device/info and /api/v2/health probe bodies are kept whole up to 8 KB. On bench .63, hd_min_free_heap is in 0 of 2 old transcripts and 2 of 2 new ones (docs/evidence/task1475/README.md); host test test_j. Residual: if setup() takes longer than 1 minute and the 'Setup finished!' line is missed, the uptime rule can release q/D early.
<!-- SECTION:FINAL_SUMMARY:END -->
