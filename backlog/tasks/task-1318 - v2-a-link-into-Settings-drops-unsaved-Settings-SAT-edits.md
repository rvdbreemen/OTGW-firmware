---
id: TASK-1318
title: 'v2: a link into Settings drops unsaved Settings/SAT edits'
status: To Do
assignee: []
created_date: '2026-10-23T12:39:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 441000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1312 review (pre-existing, not caused by it): showPage('settings') calls fetchSettings(), which resets setDirty = {}. A BLE 'Assign' or sensor-roster link while the user has unsaved edits on Settings or the SAT page silently discards them. Expected: keep the dirty map across a refetch (merge), or ask before discarding.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduction in a webui test: edit a field, follow the BLE Assign link, the edit is still dirty (red on current code)
- [ ] #2 Fix keeps unsaved edits across the refetch or asks first; the test goes green
<!-- AC:END -->
