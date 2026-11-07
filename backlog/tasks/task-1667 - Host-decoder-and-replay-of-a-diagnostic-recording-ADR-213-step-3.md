---
id: TASK-1667
title: Host decoder and replay of a diagnostic recording (ADR-213 step 3)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T20:35:00Z'
updated_date: '2026-11-07T11:34:00Z'
labels:
  - diagnostics
milestone: 2.0.0
dependencies:
  - TASK-1470
priority: medium
ordinal: 790000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python decoder for the ADR-213 format (tools/), export of OT frames to the ADR-209 replay format, and a diff between two recordings (TASK-1294 basis). Web UI start/stop/download button in the diagnostics panel.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decoder round-trips the host-test fixtures and a bench recording
- [ ] #2 A bench recording replays through the existing replay path
<!-- AC:END -->
