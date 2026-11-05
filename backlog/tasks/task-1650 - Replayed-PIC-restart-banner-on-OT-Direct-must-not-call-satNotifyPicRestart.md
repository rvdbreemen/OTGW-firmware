---
id: TASK-1650
title: Replayed PIC restart banner on OT-Direct must not call satNotifyPicRestart
status: Done
assignee: []
created_date: '2026-11-05T06:58:00Z'
updated_date: '2026-11-05T09:30:00Z'
labels:
  - bug
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 774000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by TASK-1412.20 (R-4): OTGW-Core.ino:6188 calls satNotifyPicRestart() for a replayed banner before the !isOTDirectEnabled() guard. Robert 2026-11-05: bug task.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On OT-Direct a replayed PIC banner does not call satNotifyPicRestart(); on a PIC board a real banner still does
- [x] #2 Host test red on old / green on fix; evaluate --quick green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05: fix (wt-1649): banner path OTGW-Core.ino ~6192 now 'if (!isOTDirectEnabled()) satNotifyPicRestart();' (guard reused from the PIC recovery lines below). A replayed banner on a PIC board still notifies (bench scenario SB15 relies on it). Test: same harness, OT-Direct banner case red on old (got 1 want 0), green on fix; PIC banner case green on both.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
On OT-Direct a replayed PIC banner no longer calls satNotifyPicRestart() (guard isOTDirectEnabled()); a PIC board still does (bench scenario SB15). Evidence: same harness, OT-Direct case red on old (got 1 want 0) / green on fix; evaluate 0 failed. Not verified on the device.
<!-- SECTION:FINAL_SUMMARY:END -->
