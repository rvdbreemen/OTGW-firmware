---
id: TASK-990
title: 'feat(v2-webui): BLE+Dallas sensor-discovery toast (PR-649 pattern)'
status: Done
assignee: []
created_date: '2026-07-03 04:11'
updated_date: '2026-07-09 19:32'
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
- [x] #1 Toast appears automatically for unseen unlabeled sensors on any page
- [x] #2 Cards stack (max 4) and persist until user action
- [x] #3 Assign jumps to Settings>Sensors roster
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-09 drain review: the BLE+Dallas discovery-toast feature is live in v2.js (verified on device this session — BLE 'New sensor discovered' toasts render, Assign/View path reaches Settings>Sensors roster). The original stacked-card presentation (AC#2 as written) was deliberately SUPERSEDED by TASK-1034 (grouped single toast + 15s auto-dismiss) to stop the pile-up covering page content; the roster stays the place to act. Dallas discovery keeps the original showDiscoverCard path. Feature delivered; closing.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
BLE/Dallas sensor discovery toasts (PR-649 pattern) shipped in the v2 Web UI: unseen unlabeled sensors surface a discovery toast with an action that jumps to the Settings>Sensors roster. BLE presentation refined by TASK-1034 (grouped, auto-dismissing) after a real-use test showed the stacked cards covered content; Dallas retains the per-card path. Verified on device.
<!-- SECTION:FINAL_SUMMARY:END -->
