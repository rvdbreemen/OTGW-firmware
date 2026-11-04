---
id: TASK-269
title: 'Fix: PIC firmware v6.6 upgrade via web UI fails'
status: Done
assignee: []
created_date: '2026-04-13 22:18'
updated_date: '2026-04-17 07:12'
labels:
  - bug
  - needs-info
  - pic
dependencies: []
references:
  - 'https://gathering.tweakers.net/forum/list_message/85026024#85026024'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by Tomba on Tweakers forum (2026-04-09). Upgrading PIC firmware to version 6.6 via the web interface fails with an error (screenshot shared, content not visible in RSS). Reporter asked if this is a known issue. A separate post from the same user confirms they successfully updated the ESP firmware OTA, and noted the PIC was behind. The PIC flash failure may be a web UI or file-transfer bug, or a PIC firmware compatibility issue.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 PIC firmware v6.6 can be flashed successfully via the web UI
- [ ] #2 Error message/log from reporter obtained
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Waiting for: error message/screenshot from reporter — not readable from RSS feed. Need to contact Tomba on Tweakers for details.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Duplicate of TASK-240. Both report PIC firmware v6.6 upgrade failure by Tomba on Tweakers. Merged into TASK-240.
<!-- SECTION:FINAL_SUMMARY:END -->
