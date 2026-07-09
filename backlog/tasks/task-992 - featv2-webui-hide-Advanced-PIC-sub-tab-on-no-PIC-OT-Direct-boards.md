---
id: TASK-992
title: 'feat(v2-webui): hide Advanced>PIC sub-tab on no-PIC (OT-Direct) boards'
status: Done
assignee: []
created_date: '2026-07-03 04:13'
updated_date: '2026-07-09 20:32'
labels: []
dependencies: []
ordinal: 204000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
otcommandinterface != PIC -> hide PIC subtab button + panel, fall back to Debug tab.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 PIC tab hidden on OTGW32/OT-Direct
- [x] #2 PIC tab visible and functional on PIC boards
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-09 drain verify ON DEVICE (S3 Mini Pro combo, .74): applyPicTabVisibility() gates the Advanced PIC subtab on otcommandinterface==='PIC'. AC#1: forced the board to OT-Direct (boardmode=2, reboot) -> JS DOM probe confirms the pic tab button has class 'hidden'=true while debug/files/system stay visible; the panel falls back to Debug. AC#2: on PIC mode (earlier this session) the PIC firmware card renders fully (device pic16f1847 / gateway / v6.6 + hex file list). The TASK-1033 retry fix is what makes the gating fetch reliable (a 503 no longer leaves the tab wrongly shown). Board restored to auto after the test.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 Advanced PIC subtab is hidden (falls back to Debug) on OTGW32/OT-Direct boards and shown+functional on PIC boards, gated on otcommandinterface via applyPicTabVisibility(). Verified on-device both ways on the S3 Mini Pro combo.
<!-- SECTION:FINAL_SUMMARY:END -->
