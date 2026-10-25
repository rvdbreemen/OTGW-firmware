---
id: TASK-1379
title: >-
  v2 Advanced > Debug Information: credit SAT's concept to George Ntellas and
  the original SAT Python version to Alex Wijnholds and George Ntellas
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-25T09:31:00Z'
updated_date: '2026-10-25T09:33:00Z'
labels:
  - webui
  - sat
milestone: 2.0.0
dependencies: []
ordinal: 512000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-25: add two fields to Advanced tab -> Debug Information: 'SAT concept by: George Ntellas' and 'Original SAT Python version: Authors Alex Wijnholds - George Ntellas'. It must be clear that the original idea of SAT is George Ntellas's.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Debug Information shows 'SAT concept by' = George Ntellas and 'Original SAT Python version' = Authors Alex Wijnholds - George Ntellas (screenshot)
- [ ] #2 Fields are static text, present on every board, no new REST field needed unless the page renders only REST data
- [ ] #3 Third field 'C/C++ version' = Robert + Claude (Robert 2026-10-25)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Static UI only (KISS): in v2.js renderDebugGroups() (~3634) add a group 'Credits' right after 'Firmware' with three rows: 'SAT concept by' = George Ntellas; 'Original SAT Python version' = Authors Alex Wijnholds - George Ntellas; 'C/C++ version' = Robert + Claude. No firmware/REST change: the rows are not device data. Sequencing: TASK-1366's workflow is the single writer in the dev tree right now and may touch v2.js; apply this edit after TASK-1366 is committed, then its own prerelease bump, one build for both, OTA to bench, screenshot of Advanced > Debug Information.
<!-- SECTION:PLAN:END -->
