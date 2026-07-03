---
id: TASK-990
title: 'feat(v2-webui): BLE+Dallas sensor-discovery toast (PR-649 pattern)'
status: To Do
assignee: []
created_date: '2026-07-03 04:11'
labels: []
dependencies: []
ordinal: 202000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Floating discover-card stack bottom-right, fed by /api/v2/sat/ble/discovery (BLE, --cold) and /api/v2/sensors dallas array (1-Wire, --hot). Stacked, no auto-expire, Assign/Ignore/dismiss. Implemented + validated on device this session.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Toast appears automatically for unseen unlabeled sensors on any page
- [ ] #2 Cards stack (max 4) and persist until user action
- [ ] #3 Assign jumps to Settings>Sensors roster
<!-- AC:END -->
